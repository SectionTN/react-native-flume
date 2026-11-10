#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>

#include <memory>
#include <vector>

namespace flume {

// globalThis.__flume on the React runtime. A worklet that captures it reaches install() on the flume runtime.
class FlumeHost : public facebook::jsi::HostObject {
 public:
  // Starts the engine session of this React runtime, which ends when the runtime is torn down.
  static void install(facebook::jsi::Runtime& reactRuntime, std::shared_ptr<facebook::react::CallInvoker> invoker);

  explicit FlumeHost(int session) : session_(session) {}
  facebook::jsi::Value get(facebook::jsi::Runtime& rt, const facebook::jsi::PropNameID& name) override;
  std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime& rt) override;

 private:
  const int session_;
};

}  // namespace flume
