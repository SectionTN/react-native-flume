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

#include "core/Holder.h"
#include "core/Layout.h"
#include "core/Reconciler.h"
#include "core/TextMeasurer.h"
#include "core/Value.h"

namespace flume {

struct StoreListener {
  virtual ~StoreListener() = default;
  virtual void onDataChanged(int count) = 0;  // UI thread
};

// One list: its order and items, the pending snapshot from the JS thread, its holders and its runtime handles.
class Store : public std::enable_shared_from_this<Store> {
 public:
  explicit Store(int id) : id(id) {}
  const int id;

  // JS thread
  void setPending(std::vector<std::string> keys, std::vector<std::string> types, std::vector<int> changedIndices,
                  std::vector<Value> changedItems);
  void setRenderCell(std::shared_ptr<worklets::Serializable> renderCell);

  // UI thread
  bool applyPending();
  int count() const { return static_cast<int>(keys_.size()); }
  const std::string& keyAt(int index) const { return keys_[index]; }
  const Value& itemAt(int index) const { return items_[index]; }
  uint64_t itemVersion(int index) const { return versions_[index]; }
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
    std::unordered_map<std::string, std::pair<uint64_t, facebook::jsi::Value>> items;
  } slot;

 private:
  struct Pending {
    std::vector<std::string> keys;
    std::vector<std::string> types;
    std::vector<int> changedIndices;
    std::vector<Value> changedItems;
  };

  mutable std::mutex mutex_;
  std::optional<Pending> pending_;
  std::shared_ptr<worklets::Serializable> renderCell_;
  uint64_t renderCellVersion_ = 0;

  std::vector<std::string> keys_;
  std::vector<std::string> types_;
  std::vector<Value> items_;
  std::vector<uint64_t> versions_;
  uint64_t nextVersion_ = 1;
};

}  // namespace flume
