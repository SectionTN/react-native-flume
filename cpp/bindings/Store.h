#pragma once

#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bindings/ItemCache.h"
#include "core/Holder.h"
#include "core/KeyDiff.h"
#include "core/Layout.h"
#include "core/ListStore.h"
#include "core/Reconciler.h"
#include "core/TextMeasurer.h"
#include "core/Value.h"

namespace flume {

struct StoreListener {
  virtual ~StoreListener() = default;
  virtual void onListChanged(const ListOps& ops, int count) = 0;  // UI thread
};

// The flume runtime and its two entry points, never changed once built: the functions die before the runtime.
struct FlumeRuntime {
  FlumeRuntime(std::shared_ptr<worklets::WorkletRuntime> runtime, facebook::jsi::Function bind,
               facebook::jsi::Function release)
      : runtime(std::move(runtime)), bind(std::move(bind)), release(std::move(release)) {}
  const std::shared_ptr<worklets::WorkletRuntime> runtime;
  const facebook::jsi::Function bind;
  const facebook::jsi::Function release;
};

// How the platform lays rows out, which decides where a separator goes and which rows draw one.
struct RowShape {
  bool horizontal = false;
  bool inverted = false;
  int lanes = 1;
  bool operator==(const RowShape&) const = default;
};

// One list: its order and items, the cell worklet, its holders and its runtime handles.
class Store : public std::enable_shared_from_this<Store> {
 public:
  Store(int id, int session) : id(id), session(session) {}
  ~Store();  // frees the holders' trees before the layout, on whichever thread drops the last reference
  const int id;
  const int session;  // the React runtime that created it, see Engine::beginSession

  ListStore list;

  // JS thread
  void setRenderCell(std::shared_ptr<worklets::Serializable> renderCell);
  void setSeparatorCell(std::shared_ptr<worklets::Serializable> separatorCell);  // null clears it

  // UI thread
  std::shared_ptr<worklets::Serializable> renderCell() const;
  uint64_t renderCellVersion() const;
  // The separator worklet with its version, read together so a set between two reads cannot pair them wrong.
  std::pair<std::shared_ptr<worklets::Serializable>, uint64_t> separatorCell() const;
  RowShape shape;  // set by the platform view before it binds with a new shape

  std::unordered_map<int, std::unique_ptr<Holder>> holders;
  StoreListener* listener = nullptr;
  std::unique_ptr<TextMeasurer> measurer;
  std::unique_ptr<Layout> layout;
  std::unique_ptr<Reconciler> reconciler;

  // Flume runtime only, inside host functions run through runSyncOnRuntime. Replace it whole, never member by member.
  struct RuntimeSlot {
    std::shared_ptr<const FlumeRuntime> flume;  // the runtime of the values below, declared first so it dies last
    std::optional<facebook::jsi::Function> renderCellFn;
    uint64_t renderCellVersion = 0;
    std::optional<facebook::jsi::Function> separatorCellFn;
    uint64_t separatorCellVersion = 0;
    ItemCache items;
    uint64_t retainedGeneration = 0;
  } slot;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<worklets::Serializable> renderCell_;
  uint64_t renderCellVersion_ = 0;
  std::shared_ptr<worklets::Serializable> separatorCell_;
  uint64_t separatorCellVersion_ = 0;
};

}  // namespace flume
