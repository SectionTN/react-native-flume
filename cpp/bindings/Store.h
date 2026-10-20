#pragma once

#include <jsi/jsi.h>
#include <worklets/Compat/StableApi.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
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

// One list: its order and items, the cell worklet, its holders and its runtime handles.
class Store : public std::enable_shared_from_this<Store> {
 public:
  explicit Store(int id) : id(id) {}
  const int id;

  ListStore list;

  // JS thread
  void setRenderCell(std::shared_ptr<worklets::Serializable> renderCell);

  // UI thread
  std::shared_ptr<worklets::Serializable> renderCell() const;
  uint64_t renderCellVersion() const;

  std::unordered_map<int, std::unique_ptr<Holder>> holders;
  StoreListener* listener = nullptr;
  std::unique_ptr<TextMeasurer> measurer;
  std::unique_ptr<Layout> layout;
  std::unique_ptr<Reconciler> reconciler;

  // Flume runtime only, inside host functions run through runSyncOnRuntime.
  struct RuntimeSlot {
    std::optional<facebook::jsi::Function> renderCellFn;
    uint64_t renderCellVersion = 0;
    ItemCache items;
    uint64_t retainedGeneration = 0;
  } slot;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<worklets::Serializable> renderCell_;
  uint64_t renderCellVersion_ = 0;
};

}  // namespace flume
