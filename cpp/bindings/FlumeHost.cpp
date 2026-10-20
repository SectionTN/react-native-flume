#include "bindings/FlumeHost.h"

#include <worklets/Compat/StableApi.h>

#include <memory>
#include <string>

#include "bindings/Engine.h"
#include "bindings/JsiConverter.h"

namespace flume {

namespace jsi = facebook::jsi;

namespace {

const char* const kNames[] = {"install", "setScheduler", "createStore", "releaseStore", "setData", "setCell"};

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

}  // namespace

void FlumeHost::install(jsi::Runtime& rt) {
  rt.global().setProperty(rt, "__flume", jsi::Object::createFromHostObject(rt, std::make_shared<FlumeHost>()));
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
    return hostFunction(rt, "install", 0, [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
      engine.onInstall(rt);
      return jsi::Value::undefined();
    });
  }
  if (name == "setScheduler") {
    return hostFunction(rt, "setScheduler", 1,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 1) throw jsi::JSError(rt, "setScheduler needs the scheduler holder");
                          engine.setScheduler(worklets::getUISchedulerFromHolder(rt, args[0].asObject(rt)));
                          return jsi::Value::undefined();
                        });
  }
  if (name == "createStore") {
    return hostFunction(rt, "createStore", 0, [&engine](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
      return jsi::Value(engine.createStore());
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
    return hostFunction(rt, "setData", 5,
                        [&engine](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
                          if (count < 5) throw jsi::JSError(rt, "setData needs five arguments");
                          engine.setData(static_cast<int>(args[0].asNumber()), stringsOf(rt, args[1]),
                                         stringsOf(rt, args[2]), intsOf(rt, args[3]), valuesOf(rt, args[4], engine));
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
  return jsi::Value::undefined();
}

}  // namespace flume
