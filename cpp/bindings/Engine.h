#pragma once

#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bindings/Store.h"
#include "core/Node.h"
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
  void setData(int storeId, std::vector<std::string> keys, std::vector<std::string> types,
               std::vector<int> changedIndices, std::vector<Value> changedItems);
  void setCell(int storeId, std::shared_ptr<worklets::Serializable> renderCell);
  void warn(const std::string& message);  // once per message

  // Flume runtime, inside the install host function
  void onInstall(facebook::jsi::Runtime& flumeRuntime);

  // UI thread (platform bridge)
  int attach(int storeId, StoreListener* listener, std::unique_ptr<TextMeasurer> measurer, float pointScale);
  void detach(int storeId);
  int count(int storeId);
  bool bind(int storeId, int holderId, int index, float widthDp, OpBatch& out);
  void releaseHolder(int storeId, int holderId);

 private:
  Engine() = default;
  std::shared_ptr<Store> find(int storeId);
  void scheduleRefresh(const std::shared_ptr<Store>& store, bool always);
  void bindOnRuntime(facebook::jsi::Runtime& rt, int index);
  void releaseOnRuntime(int storeId);
  void logOnce(const std::string& message);

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
  std::shared_ptr<worklets::WorkletRuntime> runtime_;
  std::optional<facebook::jsi::Function> bindFn_;
  std::optional<facebook::jsi::Function> releaseFn_;
  Current current_;
  std::unordered_set<std::string> logged_;
};

}  // namespace flume
