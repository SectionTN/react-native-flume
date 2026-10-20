#include "bindings/Engine.h"

#include <chrono>
#include <cstdio>

#include "bindings/JsiConverter.h"
#include "core/TreeParser.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace flume {

namespace jsi = facebook::jsi;

namespace {

int64_t nowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

Engine& Engine::instance() {
  static Engine engine;
  return engine;
}

void Engine::setScheduler(std::shared_ptr<worklets::UIScheduler> scheduler) {
  std::lock_guard<std::mutex> lock(mutex_);
  scheduler_ = std::move(scheduler);
}

int Engine::createStore() {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = nextStoreId_++;
  stores_[id] = std::make_shared<Store>(id);
  return id;
}

std::shared_ptr<Store> Engine::find(int storeId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = stores_.find(storeId);
  return it == stores_.end() ? nullptr : it->second;
}

void Engine::setData(int storeId, std::vector<std::string> keys, std::vector<std::string> types,
                     std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->list.setPending(std::move(keys), std::move(types), std::move(changedIndices), std::move(changedItems),
                         stableKeys);
  scheduleRefresh(store, false);
}

void Engine::setCell(int storeId, std::shared_ptr<worklets::Serializable> renderCell) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->setRenderCell(std::move(renderCell));
  scheduleRefresh(store, true);
}

void Engine::scheduleRefresh(const std::shared_ptr<Store>& store, bool always) {
  std::shared_ptr<worklets::UIScheduler> scheduler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    scheduler = scheduler_;
  }
  if (scheduler == nullptr) return;
  std::weak_ptr<Store> weak = store;
  worklets::scheduleOnUI(scheduler, [weak, always] {
    auto s = weak.lock();
    if (s == nullptr) return;
    std::optional<ListOps> ops = s->list.applyPending();
    if (s->listener == nullptr || (!ops && !always)) return;
    ListOps reset;
    reset.reset = true;
    s->listener->onListChanged(always ? reset : *ops, s->list.count());
  });
}

// Native resources die on the UI thread, runtime handles under the runtime lock; the JS thread only unregisters.
void Engine::releaseStore(int storeId) {
  std::shared_ptr<Store> store;
  std::shared_ptr<worklets::UIScheduler> scheduler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stores_.find(storeId);
    if (it == stores_.end()) return;
    store = it->second;
    stores_.erase(it);
    releasing_[storeId] = store;
    scheduler = scheduler_;
  }
  if (scheduler == nullptr) return;
  worklets::scheduleOnUI(scheduler, [this, storeId] {
    std::shared_ptr<Store> s;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = releasing_.find(storeId);
      if (it == releasing_.end()) return;
      s = it->second;
    }
    s->listener = nullptr;
    if (s->reconciler != nullptr) {
      for (auto& [id, holder] : s->holders) s->reconciler->release(*holder);
    }
    s->holders.clear();
    s->reconciler.reset();
    s->layout.reset();
    s->measurer.reset();
    if (runtime_ != nullptr && releaseFn_) worklets::runSyncOnRuntime(runtime_, *releaseFn_, jsi::Value(storeId));
    std::lock_guard<std::mutex> lock(mutex_);
    releasing_.erase(storeId);
  });
}

void Engine::onInstall(jsi::Runtime& rt) {
  runtime_ = worklets::getWeakRuntimeFromJSIRuntime(rt).lock();
  bindFn_ = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forAscii(rt, "__flumeBind"), 1,
      [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        bindOnRuntime(rt, count > 0 && args[0].isNumber() ? static_cast<int>(args[0].asNumber()) : -1);
        return jsi::Value::undefined();
      });
  releaseFn_ = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forAscii(rt, "__flumeRelease"), 1,
      [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* args, size_t count) {
        if (count > 0 && args[0].isNumber()) releaseOnRuntime(static_cast<int>(args[0].asNumber()));
        return jsi::Value::undefined();
      });
}

void Engine::releaseOnRuntime(int storeId) {
  std::shared_ptr<Store> store;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = releasing_.find(storeId);
    if (it == releasing_.end()) return;
    store = it->second;
  }
  store->slot.renderCellFn.reset();
  store->slot.items.clear();
}

int Engine::attach(int storeId, StoreListener* listener, std::unique_ptr<TextMeasurer> measurer, float pointScale) {
  auto store = find(storeId);
  if (store == nullptr) return 0;
  store->measurer = std::move(measurer);
  store->layout = std::make_unique<Layout>(*store->measurer, pointScale);
  store->reconciler = std::make_unique<Reconciler>(*store->layout);
  store->listener = listener;
  store->list.applyPending();
  return store->list.count();
}

void Engine::detach(int storeId) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->listener = nullptr;
  if (store->reconciler != nullptr) {
    for (auto& [id, holder] : store->holders) store->reconciler->release(*holder);
  }
  store->holders.clear();
  store->reconciler.reset();
  store->layout.reset();
  store->measurer.reset();
}

int Engine::count(int storeId) {
  auto store = find(storeId);
  return store == nullptr ? 0 : store->list.count();
}

void Engine::releaseHolder(int storeId, int holderId) {
  auto store = find(storeId);
  if (store == nullptr) return;
  auto it = store->holders.find(holderId);
  if (it == store->holders.end()) return;
  if (store->reconciler != nullptr) store->reconciler->release(*it->second);
  store->holders.erase(it);
}

bool Engine::bind(int storeId, int holderId, int index, float widthDp, OpBatch& out) {
  auto store = find(storeId);
  if (store == nullptr || runtime_ == nullptr || !bindFn_ || store->layout == nullptr) return false;
  auto& slot = store->holders[holderId];
  if (slot == nullptr) {
    slot = std::make_unique<Holder>();
    slot->id = holderId;
  }
  Holder& holder = *slot;
  bool first = holder.root == nullptr;
  out.clear();
  store->layout->measureCalls = 0;
  store->layout->measureHits = 0;

  int64_t t0 = nowNs();
  current_ = Current{store, nullptr, ""};
  try {
    worklets::runSyncOnRuntime(runtime_, *bindFn_, jsi::Value(index));
  } catch (const std::exception& e) {
    current_.error = e.what();
  }
  int64_t t1 = nowNs();
  if (current_.fresh != nullptr) {
    store->reconciler->reconcile(holder, std::move(current_.fresh), out);
  } else {
    logOnce(current_.error.empty() ? "renderCell produced nothing" : current_.error);
  }
  int64_t t2 = nowNs();
  if (holder.root != nullptr) {
    store->layout->layout(*holder.root, widthDp);
    store->layout->readFrames(*holder.root, out);
    store->reconciler->emitAfterLayout(holder, out);
  }
  int64_t t3 = nowNs();
  out.stats = {t1 - t0, t2 - t1, t3 - t2, store->layout->measureCalls, store->layout->measureHits, first ? 1 : 0};
  current_ = Current{};
  return true;
}

// Runs on the flume runtime with its lock held: materializes renderCell and the item, calls, parses.
void Engine::bindOnRuntime(jsi::Runtime& rt, int index) {
  std::shared_ptr<Store> store = current_.store;
  if (store == nullptr) return;
  Store::RuntimeSlot& slot = store->slot;
  try {
    if (!slot.renderCellFn || slot.renderCellVersion != store->renderCellVersion()) {
      auto serializable = store->renderCell();
      if (serializable == nullptr) {
        current_.error = "renderCell is not set";
        return;
      }
      slot.renderCellFn = serializable->toJSValue(rt).asObject(rt).asFunction(rt);
      slot.renderCellVersion = store->renderCellVersion();
    }
    if (index < 0 || index >= store->list.count()) {
      current_.error = "index out of range: " + std::to_string(index);
      return;
    }
    if (slot.retainedGeneration != store->list.keysGeneration()) {
      slot.items.retain(store->list.keys());
      slot.retainedGeneration = store->list.keysGeneration();
    }
    const jsi::Value& item =
        slot.items.materialize(rt, store->list.keyAt(index), store->list.itemVersion(index), store->list.itemAt(index));
    jsi::Value result = slot.renderCellFn->call(rt, {jsi::Value(rt, item), jsi::Value(index)});
    ParseResult parsed = parseTree(toValue(rt, result));
    if (parsed.root == nullptr) {
      current_.error = "renderCell returned an invalid tree: " + parsed.error;
      return;
    }
    current_.fresh = std::move(parsed.root);
  } catch (const jsi::JSError& e) {
    current_.error = "renderCell threw: " + e.getMessage();
  } catch (const std::exception& e) {
    current_.error = std::string("renderCell failed: ") + e.what();
  }
}

int Engine::viewType(int storeId, int position) {
  auto store = find(storeId);
  if (store == nullptr || position < 0 || position >= store->list.count()) return 0;
  return store->list.typeIdAt(position);
}

int64_t Engine::itemId(int storeId, int position) {
  auto store = find(storeId);
  if (store == nullptr || position < 0 || position >= store->list.count()) return -1;
  return store->list.itemIdAt(position);
}

std::string Engine::keyAt(int storeId, int position) {
  auto store = find(storeId);
  if (store == nullptr || position < 0 || position >= store->list.count()) return "";
  return store->list.keyAt(position);
}

void Engine::warn(const std::string& message) { logOnce(message); }

void Engine::logOnce(const std::string& message) {
  if (!logged_.insert(message).second) return;
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_WARN, "Flume", "%s", message.c_str());
#else
  std::fprintf(stderr, "Flume: %s\n", message.c_str());
#endif
}

}  // namespace flume
