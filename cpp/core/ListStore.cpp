#include "core/ListStore.h"

namespace flume {

void ListStore::setPending(std::vector<std::string> keys, std::vector<std::string> types,
                           std::vector<int> changedIndices, std::vector<Value> changedItems, bool stableKeys) {
  std::lock_guard<std::mutex> lock(mutex_);
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
  pending_ = Pending{std::move(keys), std::move(types), std::move(changedIndices), std::move(changedItems), stableKeys};
}

std::optional<ListOps> ListStore::applyPending() {
  std::optional<Pending> pending;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending.swap(pending_);
  }
  if (!pending) return std::nullopt;

  ListOps ops = diffKeys(keys_, pending->keys, pending->changedIndices, pending->stableKeys);

  std::unordered_map<std::string, size_t> previous;
  previous.reserve(keys_.size());
  for (size_t i = 0; i < keys_.size(); i++) previous[keys_[i]] = i;

  size_t size = pending->keys.size();
  std::vector<Value> items(size);
  std::vector<uint64_t> versions(size, 0);
  std::vector<int> typeIds(size);
  std::vector<int64_t> itemIds(size);
  size_t carried = 0;
  for (size_t i = 0; i < size; i++) {
    auto it = previous.find(pending->keys[i]);
    if (it != previous.end()) {
      items[i] = std::move(items_[it->second]);
      versions[i] = versions_[it->second];
      carried++;
    }
    typeIds[i] = typeRegistry_.try_emplace(pending->types[i], static_cast<int>(typeRegistry_.size())).first->second;
    itemIds[i] =
        idRegistry_.try_emplace(pending->keys[i], static_cast<int64_t>(idRegistry_.size()) + 1).first->second;
  }
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
