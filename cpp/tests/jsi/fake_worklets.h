#pragma once

#include <ReactCommon/CallInvoker.h>
#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <memory>
#include <vector>

namespace worklets {

// Owns its Hermes runtime as the real one does, so the last strong reference destroys it.
class WorkletRuntime {
 public:
  WorkletRuntime();
  ~WorkletRuntime();
  std::unique_ptr<facebook::hermes::HermesRuntime> runtime;
};

}  // namespace worklets

// Queues JS tasks until the test drains them on the runtime they target.
struct QueueInvoker : facebook::react::CallInvoker {
  std::vector<facebook::react::CallFunc> tasks;
  void invokeAsync(facebook::react::CallFunc&& func) noexcept override { tasks.push_back(std::move(func)); }
  void invokeSync(facebook::react::CallFunc&& func) override { tasks.push_back(std::move(func)); }
  void drain(facebook::jsi::Runtime& rt) {
    auto pending = std::move(tasks);
    tasks.clear();
    for (auto& task : pending) task(rt);
  }
};

// A flume runtime the test holds only weakly, the way the engine ends up as the last owner on a device.
std::weak_ptr<worklets::WorkletRuntime> installFlumeRuntime(facebook::jsi::Runtime& reactRuntime);
