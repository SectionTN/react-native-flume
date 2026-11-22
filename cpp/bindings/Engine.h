#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <atomic>
#include <map>
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

// The process singleton: the store registry, one session per React runtime and the bind pipeline with its stage timers.
class Engine {
 public:
  static Engine& instance();

  // A session holds what one React runtime installed and ends with that runtime (FlumeHost::install).
  int beginSession(std::shared_ptr<facebook::react::CallInvoker> invoker);
  void endSession(int session);  // inside the runtime's teardown, which still allows destroying its values

  // JS thread (FlumeHost)
  void setScheduler(int session, std::shared_ptr<worklets::UIScheduler> scheduler);
  int createStore(int session);
  void releaseStore(int storeId);
  // Deferred data waits for refresh(), which the platform calls once it has the commit's pre-rendered cells.
  void setData(int storeId, std::vector<std::string> keys, std::vector<std::string> types,
               std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys, bool deferred,
               std::vector<int> sticky);
  // A sticky-only change: it joins pending data, or applies alone.
  void setStickyHeaderIndices(int storeId, std::vector<int> sticky);
  void setCell(int storeId, std::shared_ptr<worklets::Serializable> renderCell);
  void setSeparatorCell(int storeId, std::shared_ptr<worklets::Serializable> separatorCell);  // null clears it
  void setCellKinds(int storeId, bool reactByDefault, std::vector<std::string> reactTypes);
  void setReporter(int session, facebook::jsi::Function reporter);  // a function on the React runtime, development only
  void report(int level, const std::string& message);  // once per message: the reporter when set, else logcat
  void warn(const std::string& message) { report(1, message); }

  // Flume runtime, inside the install host function
  void onInstall(int session, facebook::jsi::Runtime& flumeRuntime);

  // UI thread (platform bridge)
  int attach(int storeId, StoreListener* listener, std::unique_ptr<TextMeasurer> measurer, float pointScale);
  void detach(int storeId);
  void setRowShape(int storeId, RowShape shape);  // before any bind laid out with the new shape
  int count(int storeId);
  int viewType(int storeId, int position);     // type id * 2, plus 1 for React; 0 for an unknown position
  bool hasReactKinds(int storeId);             // some item type renders as a React row
  int64_t itemId(int storeId, int position);   // -1 for an unknown position
  std::string keyAt(int storeId, int position);
  std::vector<int> stickyIndices(int storeId);  // UI thread, the applied data's sticky indices
  bool bind(int storeId, int holderId, int index, float widthDp, float heightDp, OpBatch& out);
  // The holder's bound tree laid out again at a main length, NaN for its own, with no worklet call. False with no tree.
  bool relayout(int storeId, int holderId, float mainDp, OpBatch& out);
  void releaseHolder(int storeId, int holderId);
  void invalidateMeasures(int storeId);  // clears the text measure cache
  void refresh(int storeId);             // applies deferred data
  // UI thread: applies deferred data now, in the frame of the mount that brought its cells. False when none was pending.
  bool applyNow(int storeId);
  void setStatsEnabled(bool enabled) { statsEnabled_ = enabled; }  // any thread; binds read the clock only while on

 private:
  Engine() = default;

  struct Session {
    std::shared_ptr<facebook::react::CallInvoker> invoker;
    std::shared_ptr<worklets::UIScheduler> scheduler;
    std::shared_ptr<const FlumeRuntime> flume;
    std::shared_ptr<facebook::jsi::Function> reporter;  // the only strong reference, so it dies with the session
  };

  struct Current {
    std::shared_ptr<Store> store;
    std::shared_ptr<const FlumeRuntime> flume;
    std::unique_ptr<Node> fresh;
    std::string error;
  };

  std::shared_ptr<Store> find(int storeId);
  std::shared_ptr<worklets::UIScheduler> schedulerOf(int session);
  std::shared_ptr<const FlumeRuntime> flumeOf(int session);
  void scheduleRefresh(const std::shared_ptr<Store>& store, bool always);
  static bool applyPending(Store& store, bool always);
  void bindOnRuntime(facebook::jsi::Runtime& rt, int index);
  void composeOnRuntime(facebook::jsi::Runtime& rt, Store& store, int index);
  void releaseOnRuntime(int storeId);
  void logLine(const std::string& message);

  std::mutex mutex_;
  std::map<int, Session> sessions_;
  int nextSession_ = 1;
  std::unordered_map<int, std::shared_ptr<Store>> stores_;
  std::unordered_map<int, std::shared_ptr<Store>> releasing_;
  int nextStoreId_ = 1;
  Current current_;
  std::atomic<bool> statsEnabled_{false};
  OnceSet logged_;
};

}  // namespace flume
