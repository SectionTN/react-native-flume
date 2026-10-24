#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "bindings/Store.h"
#include "core/Node.h"
#include "core/OnceSet.h"
#include "core/OpBatch.h"
#include "core/TextMeasurer.h"

namespace flume {

// The process singleton: the store registry, the flume runtime and the bind pipeline with its stage timers.
class Engine {
 public:
  static Engine& instance();

  // JS thread (FlumeHost)
  void setScheduler(std::shared_ptr<worklets::UIScheduler> scheduler);
  int createStore();
  void releaseStore(int storeId);
  // Deferred data waits for refresh(), which the platform calls once it has the commit's pre-rendered cells.
  void setData(int storeId, std::vector<std::string> keys, std::vector<std::string> types,
               std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys, bool deferred);
  void setCell(int storeId, std::shared_ptr<worklets::Serializable> renderCell);
  void setCellKinds(int storeId, bool reactByDefault, std::vector<std::string> reactTypes);
  void setCallInvoker(std::shared_ptr<facebook::react::CallInvoker> invoker);
  void setReporter(facebook::jsi::Function reporter);  // a function on the React runtime, development only
  void report(int level, const std::string& message);  // once per message: the reporter when set, else logcat
  void warn(const std::string& message) { report(1, message); }

  // Flume runtime, inside the install host function
  void onInstall(facebook::jsi::Runtime& flumeRuntime);

  // UI thread (platform bridge)
  int attach(int storeId, StoreListener* listener, std::unique_ptr<TextMeasurer> measurer, float pointScale);
  void detach(int storeId);
  int count(int storeId);
  int viewType(int storeId, int position);     // type id * 2, plus 1 for React; 0 for an unknown position
  int64_t itemId(int storeId, int position);   // -1 for an unknown position
  std::string keyAt(int storeId, int position);
  bool bind(int storeId, int holderId, int index, float widthDp, float heightDp, OpBatch& out);
  void releaseHolder(int storeId, int holderId);
  void invalidateMeasures(int storeId);  // clears the text measure cache
  void refresh(int storeId);             // applies deferred data
  void setStatsEnabled(bool enabled) { statsEnabled_ = enabled; }  // any thread; binds read the clock only while on

 private:
  Engine() = default;
  std::shared_ptr<Store> find(int storeId);
  void scheduleRefresh(const std::shared_ptr<Store>& store, bool always);
  void bindOnRuntime(facebook::jsi::Runtime& rt, int index);
  void releaseOnRuntime(int storeId);
  void logLine(const std::string& message);

  struct Current {
    std::shared_ptr<Store> store;
    std::unique_ptr<Node> fresh;
    std::string error;
  };

  std::mutex mutex_;
  std::unordered_map<int, std::shared_ptr<Store>> stores_;
  std::unordered_map<int, std::shared_ptr<Store>> releasing_;
  int nextStoreId_ = 1;
  std::shared_ptr<worklets::UIScheduler> scheduler_;
  std::shared_ptr<facebook::react::CallInvoker> callInvoker_;
  std::shared_ptr<facebook::jsi::Function> reporter_;
  std::shared_ptr<worklets::WorkletRuntime> runtime_;
  std::optional<facebook::jsi::Function> bindFn_;
  std::optional<facebook::jsi::Function> releaseFn_;
  Current current_;
  std::atomic<bool> statsEnabled_{false};
  OnceSet logged_;
};

}  // namespace flume
