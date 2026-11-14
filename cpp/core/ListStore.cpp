#include "core/ListStore.h"

#include <algorithm>

namespace flume {

void ListStore::setPending(std::vector<std::string> keys, std::vector<std::string> types,
                           std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys,
                           bool deferred, std::vector<int> sticky) {
  std::lock_guard<std::mutex> lock(mutex_);
  // The snapshot carries the indices of its own data, newer than any sticky-only change.
  pendingSticky_.reset();
  // A second snapshot before the UI thread applied the first replaces it: items it carries are stale.
  if (pending_) {
    Pending& old = *pending_;
    std::unordered_map<std::string, size_t> oldIndex;
    for (size_t i = 0; i < old.changedIndices.size(); i++) oldIndex[old.keys[old.changedIndices[i]]] = i;
    std::vector<int> mergedIndices = changedIndices;
    std::vector<Value> mergedItems = std::move(changedItems);
    std::unordered_map<std::string, bool> covered;
    for (int index : mergedIndices) covered[keys[index]] = true;
    for (size_t i = 0; i < keys.size(); i++) {
      if (covered.count(keys[i]) != 0) continue;
      auto it = oldIndex.find(keys[i]);
      if (it == oldIndex.end()) continue;
      mergedIndices.push_back(static_cast<int>(i));
      mergedItems.push_back(std::move(old.changedItems[it->second]));
    }
    changedIndices = std::move(mergedIndices);
    changedItems = std::move(mergedItems);
    stableKeys = stableKeys && old.stableKeys;
  }
  pending_ = Pending{std::move(keys), std::move(types), std::move(changedIndices), std::move(changedItems), stableKeys,
                     deferred, std::move(sticky)};
}

bool ListStore::setSticky(std::vector<int> sticky) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (pending_) {
    pending_->sticky = std::move(sticky);
    return false;
  }
  pendingSticky_ = std::move(sticky);
  return true;
}

namespace {

std::vector<int> stickyWithin(const std::vector<int>& sticky, size_t count) {
  std::vector<int> out;
  for (int index : sticky) {
    if (index >= 0 && static_cast<size_t>(index) < count) out.push_back(index);
  }
  return out;
}

}  // namespace

void ListStore::setAttached(bool attached) {
  std::lock_guard<std::mutex> lock(mutex_);
  attached_ = attached;
}

bool ListStore::attached() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return attached_;
}

void ListStore::setKinds(bool reactByDefault, std::vector<std::string> reactTypes) {
  std::lock_guard<std::mutex> lock(mutex_);
  pendingKinds_ = Kinds{reactByDefault, {reactTypes.begin(), reactTypes.end()}};
}

void ListStore::resolveKinds() {
  reactByTypeId_.assign(typeRegistry_.size(), false);
  for (const auto& [name, id] : typeRegistry_) {
    reactByTypeId_[id] = kinds_.reactByDefault || kinds_.reactTypes.count(name) != 0;
  }
}

std::optional<ListOps> ListStore::applyPending() {
  std::optional<Pending> pending;
  std::optional<Kinds> kinds;
  std::optional<std::vector<int>> sticky;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending.swap(pending_);
    kinds.swap(pendingKinds_);
    sticky.swap(pendingSticky_);
  }
  if (kinds) {
    kinds_ = std::move(*kinds);
    resolveKinds();
  }
  if (!pending) {
    if (!sticky) return std::nullopt;
    sticky_ = stickyWithin(*sticky, keys_.size());
    ListOps ops;
    ops.sticky = sticky_;
    return ops;
  }

  std::unordered_map<std::string, size_t> previous;
  previous.reserve(keys_.size());
  for (size_t i = 0; i < keys_.size(); i++) previous[keys_[i]] = i;

  size_t size = pending->keys.size();
  std::vector<Value> items(size);
  std::vector<uint64_t> versions(size, 0);
  std::vector<int> typeIds(size);
  std::vector<int64_t> itemIds(size);
  std::vector<int> changed = pending->changedIndices;
  size_t carried = 0;
  for (size_t i = 0; i < size; i++) {
    typeIds[i] = typeRegistry_.try_emplace(pending->types[i], static_cast<int>(typeRegistry_.size())).first->second;
    itemIds[i] =
        idRegistry_.try_emplace(pending->keys[i], static_cast<int64_t>(idRegistry_.size()) + 1).first->second;
    auto it = previous.find(pending->keys[i]);
    if (it == previous.end()) continue;
    items[i] = std::move(items_[it->second]);
    versions[i] = versions_[it->second];
    carried++;
    // A new type can mean a new cell kind, so the platform rebinds the row.
    if (typeIds_[it->second] != typeIds[i]) changed.push_back(static_cast<int>(i));
  }
  if (reactByTypeId_.size() != typeRegistry_.size()) resolveKinds();
  std::sort(changed.begin(), changed.end());
  changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
  ListOps ops = diffKeys(keys_, pending->keys, changed, pending->stableKeys);
  ops.deferred = pending->deferred;
  sticky_ = stickyWithin(pending->sticky, size);
  ops.sticky = sticky_;

  for (size_t i = 0; i < pending->changedIndices.size(); i++) {
    size_t index = static_cast<size_t>(pending->changedIndices[i]);
    if (index >= size) continue;
    items[index] = std::move(pending->changedItems[i]);
    versions[index] = nextVersion_++;
  }
  if (carried != size || carried != keys_.size()) keysGeneration_++;
  keys_ = std::move(pending->keys);
  items_ = std::move(items);
  versions_ = std::move(versions);
  typeIds_ = std::move(typeIds);
  itemIds_ = std::move(itemIds);
  return ops;
}

}  // namespace flume
