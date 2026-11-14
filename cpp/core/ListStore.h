#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/KeyDiff.h"
#include "core/Value.h"

namespace flume {

// One list's order and items: the pending snapshot from the JS thread, the applied order on the UI thread.
class ListStore {
 public:
  // JS thread; a second snapshot before the first is applied merges into it.
  void setPending(std::vector<std::string> keys, std::vector<std::string> types, std::vector<int> changedIndices,
                  std::vector<Value> changedItems, bool stableKeys, std::vector<int> sticky = {});

  // JS thread. Joins pending data or applies alone: true when nothing was pending, so the caller schedules the apply.
  bool setSticky(std::vector<int> sticky);

  // JS thread; the next applyPending applies it, also when no data is pending.
  void setKinds(bool reactByDefault, std::vector<std::string> reactTypes);

  // UI thread, before the attach applies the pending data, so a snapshot that saw the store detached is still applied.
  void setAttached(bool attached);
  // Any thread; a detached store needs no UI wake, its attach applies what is pending.
  bool attached() const;

  // UI thread; nullopt when nothing was pending.
  std::optional<ListOps> applyPending();
  int count() const { return static_cast<int>(keys_.size()); }
  bool isReactAt(int index) const { return reactByTypeId_[typeIds_[index]]; }
  int viewTypeAt(int index) const { return typeIdAt(index) * 2 + (isReactAt(index) ? 1 : 0); }  // odd for React
  bool hasReactKinds() const { return kinds_.reactByDefault || !kinds_.reactTypes.empty(); }  // UI thread
  const std::string& keyAt(int index) const { return keys_[index]; }
  const Value& itemAt(int index) const { return items_[index]; }
  uint64_t itemVersion(int index) const { return versions_[index]; }
  int typeIdAt(int index) const { return typeIds_[index]; }      // first type seen is 0, then 1, ...
  int64_t itemIdAt(int index) const { return itemIds_[index]; }  // per key, stable for the store's life
  const std::vector<std::string>& keys() const { return keys_; }
  uint64_t keysGeneration() const { return keysGeneration_; }  // bumped when the key set changes
  const std::vector<int>& sticky() const { return sticky_; }    // UI thread, the applied data's sticky indices

 private:
  struct Pending {
    std::vector<std::string> keys;
    std::vector<std::string> types;
    std::vector<int> changedIndices;
    std::vector<Value> changedItems;
    bool stableKeys = true;
    std::vector<int> sticky;
  };
  struct Kinds {
    bool reactByDefault = false;
    std::unordered_set<std::string> reactTypes;
  };

  void resolveKinds();

  mutable std::mutex mutex_;
  bool attached_ = false;
  std::optional<Pending> pending_;
  std::optional<Kinds> pendingKinds_;
  std::optional<std::vector<int>> pendingSticky_;  // a sticky-only change, set only while no data is pending
  Kinds kinds_;
  std::vector<int> sticky_;
  std::vector<bool> reactByTypeId_;

  std::vector<std::string> keys_;
  std::vector<Value> items_;
  std::vector<uint64_t> versions_;
  std::vector<int> typeIds_;
  std::vector<int64_t> itemIds_;
  std::unordered_map<std::string, int> typeRegistry_;
  std::unordered_map<std::string, int64_t> idRegistry_;
  uint64_t nextVersion_ = 1;
  uint64_t keysGeneration_ = 0;
};

}  // namespace flume
