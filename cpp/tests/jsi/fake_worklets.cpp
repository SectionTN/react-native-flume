// Link-time stand-ins for the worklets stable API, so the engine runs on host Hermes runtimes.
#include "fake_worklets.h"

#include <string>
#include <unordered_map>

namespace jsi = facebook::jsi;

namespace worklets {

class UIScheduler {};

namespace {

std::unordered_map<jsi::Runtime*, std::weak_ptr<WorkletRuntime>>& registry() {
  // Never destroyed: the engine singleton may release a runtime during static destruction.
  static auto* runtimes = new std::unordered_map<jsi::Runtime*, std::weak_ptr<WorkletRuntime>>();
  return *runtimes;
}

// A worklet travels as its source ('show source', since Hermes prints compiled functions as bytecode), evaluated again.
class SourceSerializable : public Serializable {
 public:
  explicit SourceSerializable(std::string source) : Serializable(ValueType::WorkletType), source_(std::move(source)) {}
  jsi::Value toJSValue(jsi::Runtime& rt) override {
    return rt.evaluateJavaScript(std::make_shared<jsi::StringBuffer>("(" + source_ + ")"), "worklet");
  }

 private:
  std::string source_;
};

}  // namespace

WorkletRuntime::WorkletRuntime() : runtime(facebook::hermes::makeHermesRuntime()) {}

WorkletRuntime::~WorkletRuntime() { registry().erase(runtime.get()); }

Serializable::~Serializable() = default;

std::shared_ptr<UIScheduler> getUISchedulerFromHolder(jsi::Runtime&, const jsi::Object&) {
  return std::make_shared<UIScheduler>();
}

std::weak_ptr<WorkletRuntime> getWeakRuntimeFromJSIRuntime(jsi::Runtime& rt) {
  auto it = registry().find(&rt);
  return it == registry().end() ? std::weak_ptr<WorkletRuntime>() : it->second;
}

void scheduleOnUI(const std::shared_ptr<UIScheduler>&, const std::function<void()>& job) { job(); }

std::shared_ptr<Serializable> extractSerializable(jsi::Runtime& rt, const jsi::Value& value, const std::string&) {
  jsi::Function fn = value.asObject(rt).asFunction(rt);
  jsi::Function toString = fn.getPropertyAsFunction(rt, "toString");
  return std::make_shared<SourceSerializable>(toString.callWithThis(rt, fn).asString(rt).utf8(rt));
}

void runSyncOnRuntime(const std::shared_ptr<WorkletRuntime>& workletRuntime, const jsi::Function& worklet,
                      const jsi::Value& arg0) {
  jsi::Runtime& rt = *workletRuntime->runtime;
  worklet.call(rt, jsi::Value(rt, arg0));
}

}  // namespace worklets

std::weak_ptr<worklets::WorkletRuntime> installFlumeRuntime(jsi::Runtime& reactRuntime) {
  auto flume = std::make_shared<worklets::WorkletRuntime>();
  jsi::Runtime& rt = *flume->runtime;
  worklets::registry()[&rt] = flume;
  auto host = reactRuntime.global().getPropertyAsObject(reactRuntime, "__flume").getHostObject(reactRuntime);
  rt.global().setProperty(rt, "__flume", jsi::Object::createFromHostObject(rt, host));
  rt.evaluateJavaScript(std::make_shared<jsi::StringBuffer>("__flume.install()"), "install");
  return flume;
}
