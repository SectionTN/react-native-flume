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

int Engine::beginSession(std::shared_ptr<facebook::react::CallInvoker> invoker) {
  std::lock_guard<std::mutex> lock(mutex_);
  int session = nextSession_++;
  sessions_[session].invoker = std::move(invoker);
  return session;
}

// Moves the session and its stores out under the lock, so they die after it, each store's values before its runtime.
void Engine::endSession(int session) {
  std::vector<std::shared_ptr<Store>> stores;
  Session ended;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;
    ended = std::move(it->second);
    sessions_.erase(it);
    for (auto* map : {&stores_, &releasing_}) {
      for (auto entry = map->begin(); entry != map->end();) {
        if (entry->second->session != session) {
          ++entry;
          continue;
        }
        stores.push_back(std::move(entry->second));
        entry = map->erase(entry);
      }
    }
  }
}

void Engine::setScheduler(int session, std::shared_ptr<worklets::UIScheduler> scheduler) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(session);
  if (it != sessions_.end()) it->second.scheduler = std::move(scheduler);
}

int Engine::createStore(int session) {
  std::lock_guard<std::mutex> lock(mutex_);
  int id = nextStoreId_++;
  stores_[id] = std::make_shared<Store>(id, session);
  return id;
}

std::shared_ptr<Store> Engine::find(int storeId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = stores_.find(storeId);
  return it == stores_.end() ? nullptr : it->second;
}

std::shared_ptr<worklets::UIScheduler> Engine::schedulerOf(int session) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(session);
  return it == sessions_.end() ? nullptr : it->second.scheduler;
}

std::shared_ptr<const FlumeRuntime> Engine::flumeOf(int session) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(session);
  return it == sessions_.end() ? nullptr : it->second.flume;
}

void Engine::setData(int storeId, std::vector<std::string> keys, std::vector<std::string> types,
                     std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys,
                     bool deferred) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->list.setPending(std::move(keys), std::move(types), std::move(changedIndices), std::move(changedItems),
                         stableKeys, deferred);
  if (!deferred) scheduleRefresh(store, false);
}

void Engine::refresh(int storeId) {
  auto store = find(storeId);
  if (store != nullptr) scheduleRefresh(store, false);
}

void Engine::setCell(int storeId, std::shared_ptr<worklets::Serializable> renderCell) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->setRenderCell(std::move(renderCell));
  scheduleRefresh(store, true);
}

void Engine::setCellKinds(int storeId, bool reactByDefault, std::vector<std::string> reactTypes) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->list.setKinds(reactByDefault, std::move(reactTypes));
  scheduleRefresh(store, true);
}

// A wake makes the kernel run the UI thread in place of the waker, so a detached store, whose attach applies what is
// pending, never wakes it.
void Engine::scheduleRefresh(const std::shared_ptr<Store>& store, bool always) {
  if (!store->list.attached()) return;
  std::shared_ptr<worklets::UIScheduler> scheduler = schedulerOf(store->session);
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

// The JS thread only unregisters. The UI job releases runtime handles under the runtime lock, ~Store the native parts.
void Engine::releaseStore(int storeId) {
  std::shared_ptr<worklets::UIScheduler> scheduler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stores_.find(storeId);
    if (it == stores_.end()) return;
    std::shared_ptr<Store> store = std::move(it->second);
    stores_.erase(it);
    auto session = sessions_.find(store->session);
    if (session != sessions_.end()) scheduler = session->second.scheduler;
    releasing_[storeId] = std::move(store);
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
    // Binds write the slot on this thread too, so it is read here without the runtime lock.
    if (auto flume = s->slot.flume) worklets::runSyncOnRuntime(flume->runtime, flume->release, jsi::Value(storeId));
    std::lock_guard<std::mutex> lock(mutex_);
    releasing_.erase(storeId);
  });
}

void Engine::onInstall(int session, jsi::Runtime& rt) {
  auto flume = std::make_shared<const FlumeRuntime>(
      worklets::getWeakRuntimeFromJSIRuntime(rt).lock(),
      jsi::Function::createFromHostFunction(
          rt, jsi::PropNameID::forAscii(rt, "__flumeBind"), 1,
          [this](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
            bindOnRuntime(rt, count > 0 && args[0].isNumber() ? static_cast<int>(args[0].asNumber()) : -1);
            return jsi::Value::undefined();
          }),
      jsi::Function::createFromHostFunction(
          rt, jsi::PropNameID::forAscii(rt, "__flumeRelease"), 1,
          [this](jsi::Runtime&, const jsi::Value&, const jsi::Value* args, size_t count) {
            if (count > 0 && args[0].isNumber()) releaseOnRuntime(static_cast<int>(args[0].asNumber()));
            return jsi::Value::undefined();
          }));
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(session);
  if (it != sessions_.end()) std::swap(it->second.flume, flume);
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
  // The holders outlive a detach: the platform's recycled views keep their trees until the store is released.
  if (store->layout == nullptr) {
    store->measurer = std::move(measurer);
    store->layout = std::make_unique<Layout>(*store->measurer, pointScale);
    store->reconciler = std::make_unique<Reconciler>(*store->layout);
  }
  store->listener = listener;
  store->list.setAttached(true);
  store->list.applyPending();
  return store->list.count();
}

void Engine::detach(int storeId) {
  auto store = find(storeId);
  if (store == nullptr) return;
  store->listener = nullptr;
  store->list.setAttached(false);
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

void Engine::invalidateMeasures(int storeId) {
  auto store = find(storeId);
  if (store == nullptr || store->layout == nullptr) return;
  store->layout->cache().clear();
  for (auto& entry : store->holders) {
    if (entry.second != nullptr && entry.second->root != nullptr) store->layout->invalidateText(*entry.second->root);
  }
}

bool Engine::bind(int storeId, int holderId, int index, float widthDp, float heightDp, OpBatch& out) {
  auto store = find(storeId);
  if (store == nullptr || store->layout == nullptr) return false;
  if (index >= 0 && index < store->list.count() && store->list.isReactAt(index)) return false;
  std::shared_ptr<const FlumeRuntime> flume = flumeOf(store->session);
  if (flume == nullptr) return false;
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

  bool timed = statsEnabled_.load(std::memory_order_relaxed);
  int64_t t0 = timed ? nowNs() : 0;
  current_ = Current{store, flume, nullptr, ""};
  try {
    worklets::runSyncOnRuntime(flume->runtime, flume->bind, jsi::Value(index));
  } catch (const std::exception& e) {
    current_.error = e.what();
  }
  int64_t t1 = timed ? nowNs() : 0;
  if (current_.fresh != nullptr) {
    store->reconciler->reconcile(holder, std::move(current_.fresh), out);
  } else {
    report(2, current_.error.empty() ? "renderCell produced nothing" : current_.error);
  }
  int64_t t2 = timed ? nowNs() : 0;
  if (holder.root != nullptr) {
    store->layout->layout(*holder.root, widthDp, heightDp);
    store->layout->readFrames(*holder.root, out);
    store->reconciler->emitAfterLayout(holder, out);
  }
  out.first = first;
  if (timed) {
    int64_t t3 = nowNs();
    out.timed = true;
    out.stats = {t1 - t0, t2 - t1, t3 - t2, store->layout->measureCalls, store->layout->measureHits, first ? 1 : 0};
  }
  current_ = Current{};
  return true;
}

// Runs on the flume runtime with its lock held: materializes renderCell and the item, calls, parses.
void Engine::bindOnRuntime(jsi::Runtime& rt, int index) {
  std::shared_ptr<Store> store = current_.store;
  if (store == nullptr) return;
  Store::RuntimeSlot& slot = store->slot;
  if (slot.flume != current_.flume) {
    Store::RuntimeSlot stale;  // dies whole at the end of this block, its values before the runtime it holds
    std::swap(stale, slot);
    slot.flume = current_.flume;
  }
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
    std::vector<std::string> unknownKeys;
    ParseResult parsed = parseTree(toValue(rt, result), &unknownKeys);
    for (const std::string& key : unknownKeys) report(1, "unknown style key: " + key);
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
  return store->list.viewTypeAt(position);
}

bool Engine::hasReactKinds(int storeId) {
  auto store = find(storeId);
  return store != nullptr && store->list.hasReactKinds();
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

void Engine::setReporter(int session, jsi::Function reporter) {
  auto next = std::make_shared<jsi::Function>(std::move(reporter));
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(session);
  if (it != sessions_.end()) std::swap(it->second.reporter, next);
}

// The newest session's reporter and invoker, with only a weak reference leaving the lock (the session owns the function).
void Engine::report(int level, const std::string& message) {
  if (!logged_.first(message)) return;
  std::weak_ptr<jsi::Function> reporter;
  std::shared_ptr<facebook::react::CallInvoker> invoker;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sessions_.empty()) {
      const Session& newest = sessions_.rbegin()->second;
      reporter = newest.reporter;
      invoker = newest.invoker;
    }
  }
  if (reporter.expired() || invoker == nullptr) {
    logLine(message);
    return;
  }
  invoker->invokeAsync([reporter, level, message](jsi::Runtime& rt) {
    if (auto fn = reporter.lock()) fn->call(rt, jsi::Value(level), jsi::String::createFromUtf8(rt, message));
  });
}

void Engine::logLine(const std::string& message) {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_WARN, "Flume", "%s", message.c_str());
#else
  std::fprintf(stderr, "Flume: %s\n", message.c_str());
#endif
}

}  // namespace flume
