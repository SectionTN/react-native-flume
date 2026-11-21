#include "bindings/FlumeHost.h"

#include <worklets/Compat/StableApi.h>

#include <memory>
#include <string>

#include "bindings/Engine.h"
#include "bindings/JsiConverter.h"

namespace flume {

namespace jsi = facebook::jsi;

namespace {

const char* const kNames[] = {"install", "setScheduler", "createStore", "releaseStore", "setData",
                              "setCell", "setSeparatorCell", "setCellKinds", "setStickyHeaderIndices",
                              "setReporter"};

std::vector<std::string> stringsOf(jsi::Runtime& rt, const jsi::Value& value) {
  std::vector<std::string> out;
  jsi::Array array = value.asObject(rt).asArray(rt);
  size_t size = array.size(rt);
  out.reserve(size);
  for (size_t i = 0; i < size; i++) out.push_back(array.getValueAtIndex(rt, i).asString(rt).utf8(rt));
  return out;
}

std::vector<int> intsOf(jsi::Runtime& rt, const jsi::Value& value) {
  std::vector<int> out;
  jsi::Array array = value.asObject(rt).asArray(rt);
  size_t size = array.size(rt);
  out.reserve(size);
  for (size_t i = 0; i < size; i++) out.push_back(static_cast<int>(array.getValueAtIndex(rt, i).asNumber()));
  return out;
}

std::vector<Value> valuesOf(jsi::Runtime& rt, const jsi::Value& value, Engine& engine) {
  std::vector<Value> out;
  jsi::Array array = value.asObject(rt).asArray(rt);
  size_t size = array.size(rt);
  out.reserve(size);
  bool truncated = false;
  for (size_t i = 0; i < size; i++) out.push_back(toValue(rt, array.getValueAtIndex(rt, i), &truncated));
  if (truncated) engine.warn("an item nests deeper than 64 levels and was cut there, is there a cycle?");
  return out;
}

jsi::Function hostFunction(jsi::Runtime& rt, const char* name, unsigned argc, jsi::HostFunctionType body) {
  return jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, name), argc, std::move(body));
}

// Held by the React runtime only (FlumeHost is shared with the flume runtime), so it dies in that runtime's teardown.
class SessionEnd : public jsi::HostObject {
 public:
  explicit SessionEnd(int session) : session_(session) {}
  ~SessionEnd() override { Engine::instance().endSession(session_); }
  int session() const { return session_; }

 private:
  const int session_;
};

// Read-only, hidden and permanent, as jsi.cpp defines its own runtime data global, so JS cannot end the session.
void defineSessionGlobal(jsi::Runtime& rt, int session) {
  jsi::Object global = rt.global();
  jsi::Object descriptor(rt);
  descriptor.setProperty(rt, "value", jsi::Object::createFromHostObject(rt, std::make_shared<SessionEnd>(session)));
  descriptor.setProperty(rt, "writable", false);
  descriptor.setProperty(rt, "enumerable", false);
  descriptor.setProperty(rt, "configurable", false);
  global.getPropertyAsObject(rt, "Object")
      .getPropertyAsFunction(rt, "defineProperty")
      .call(rt, global, jsi::String::createFromAscii(rt, "__flumeSession"), descriptor);
}

}  // namespace

// A second install on the same runtime keeps its session: the global cannot be redefined.
void FlumeHost::install(jsi::Runtime& rt, std::shared_ptr<facebook::react::CallInvoker> invoker) {
  const jsi::Value existing = rt.global().getProperty(rt, "__flumeSession");
  int session;
  if (existing.isObject() && existing.getObject(rt).isHostObject<SessionEnd>(rt)) {
    session = existing.getObject(rt).getHostObject<SessionEnd>(rt)->session();
  } else {
    session = Engine::instance().beginSession(std::move(invoker));
    defineSessionGlobal(rt, session);
  }
  rt.global().setProperty(rt, "__flume", jsi::Object::createFromHostObject(rt, std::make_shared<FlumeHost>(session)));
}

std::vector<jsi::PropNameID> FlumeHost::getPropertyNames(jsi::Runtime& rt) {
  std::vector<jsi::PropNameID> names;
  for (const char* name : kNames) names.push_back(jsi::PropNameID::forAscii(rt, name));
  return names;
}

jsi::Value FlumeHost::get(jsi::Runtime& rt, const jsi::PropNameID& nameId) {
  std::string name = nameId.utf8(rt);
  Engine& engine = Engine::instance();
  if (name == "install") {
    return hostFunction(rt, "install", 0,
                        [&engine, session = session_](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
      engine.onInstall(session, rt);
      return jsi::Value::undefined();
    });
  }
  if (name == "setScheduler") {
    return hostFunction(rt, "setScheduler", 1,
                        [&engine, session = session_](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
                                                      size_t count) {
                          if (count < 1) throw jsi::JSError(rt, "setScheduler needs the scheduler holder");
                          engine.setScheduler(session, worklets::getUISchedulerFromHolder(rt, args[0].asObject(rt)));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "createStore") {
    return hostFunction(rt, "createStore", 0,
                        [&engine, session = session_](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      return jsi::Value(engine.createStore(session));
    });
  }
  if (name == "releaseStore") {
    return hostFunction(rt, "releaseStore", 1,
                        [&engine](jsi::Runtime&, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count >= 1) engine.releaseStore(static_cast<int>(args[0].asNumber()));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setData") {
    return hostFunction(rt, "setData", 9,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 7) throw jsi::JSError(rt, "setData needs seven arguments");
                          engine.setData(static_cast<int>(args[0].asNumber()), stringsOf(rt, args[1]),
                                         typesOf(rt, args[2], args[3]), intsOf(rt, args[4]),
                                         valuesOf(rt, args[5], engine), args[6].getBool(),
                                         count > 7 && args[7].getBool(),
                                         count > 8 ? intsOf(rt, args[8]) : std::vector<int>{});
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setStickyHeaderIndices") {
    return hostFunction(rt, "setStickyHeaderIndices", 2,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 2) throw jsi::JSError(rt, "setStickyHeaderIndices needs a store id and indices");
                          engine.setStickyHeaderIndices(static_cast<int>(args[0].asNumber()), intsOf(rt, args[1]));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setCell") {
    return hostFunction(rt, "setCell", 2,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 2) throw jsi::JSError(rt, "setCell needs a store id and a worklet");
                          auto worklet = worklets::extractSerializable(
                              rt, args[1], "react-native-flume: renderCell must be a worklet");
                          engine.setCell(static_cast<int>(args[0].asNumber()), std::move(worklet));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setSeparatorCell") {
    return hostFunction(rt, "setSeparatorCell", 2,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 1) throw jsi::JSError(rt, "setSeparatorCell needs a store id");
                          std::shared_ptr<worklets::Serializable> worklet;
                          if (count > 1 && !args[1].isNull() && !args[1].isUndefined()) {
                            worklet = worklets::extractSerializable(
                                rt, args[1], "react-native-flume: renderSeparatorCell must be a worklet");
                          }
                          engine.setSeparatorCell(static_cast<int>(args[0].asNumber()), std::move(worklet));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setCellKinds") {
    return hostFunction(rt, "setCellKinds", 3,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 3) throw jsi::JSError(rt, "setCellKinds needs a store id, a default and types");
                          engine.setCellKinds(static_cast<int>(args[0].asNumber()), args[1].getBool(),
                                              stringsOf(rt, args[2]));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "setReporter") {
    return hostFunction(rt, "setReporter", 1,
                        [&engine, session = session_](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args,
                                                      size_t count) {
                          if (count < 1 || !args[0].isObject()) throw jsi::JSError(rt, "setReporter needs a function");
                          engine.setReporter(session, args[0].asObject(rt).asFunction(rt));
                          return jsi::Value::undefined();
                        });
  }
  return jsi::Value::undefined();
}

}  // namespace flume
