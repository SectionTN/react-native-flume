#include "adapter/SlotPool.h"

#include <algorithm>

namespace flume {

namespace {

void erase(std::vector<int>& queue, int slot) {
  queue.erase(std::remove(queue.begin(), queue.end(), slot), queue.end());
}

}  // namespace

bool SlotPool::matches(const Slot& slot, const std::optional<SlotGroup>& group, int kind, const std::string& key) {
  return slot.group == group && slot.kind == kind && slot.key == key;
}

SlotEntry SlotPool::toEntry(const Slot& slot) {
  return SlotEntry{slot.id, slot.kind, slot.key, slot.index, slot.owner != 0, slot.minMain};
}

SlotPool::Slot* SlotPool::firstIn(const std::vector<int>& queue, const std::optional<SlotGroup>& group, int kind,
                                  const std::string& key) {
  for (int id : queue) {
    auto found = slots_.find(id);
    if (found != slots_.end() && matches(found->second, group, kind, key)) return &found->second;
  }
  return nullptr;
}

void SlotPool::registerPrerendered(int slot, int kind, const std::string& key) {
  if (slots_.count(slot) > 0) return;
  if (!batchOpen_) {
    batchOpen_ = true;
    trim(unadopted_);
  }
  slots_.emplace(slot, Slot{slot, std::nullopt, kind, key});
  jsSeen_ = std::min(jsSeen_, slot);
  unadopted_.push_back(slot);
  dirty_ = true;
}

int SlotPool::acquire(int owner, SlotGroup group, int kind, const std::string& key, int index) {
  Slot* slot = nullptr;
  for (auto& [id, candidate] : slots_) {
    if (candidate.owner == owner && matches(candidate, group, kind, key)) {
      slot = &candidate;
      break;
    }
  }
  if (slot == nullptr) slot = firstIn(free_, group, kind, key);
  if (slot == nullptr) slot = firstIn(lent_, group, kind, key);
  if (slot == nullptr) slot = firstIn(unadopted_, std::nullopt, kind, key);
  if (slot == nullptr) {
    const int id = nextId_++;
    slot = &slots_.emplace(id, Slot{id, group, kind, key}).first->second;
  }
  if (slot->owner != owner) {
    erase(free_, slot->id);
    erase(lent_, slot->id);
    erase(unadopted_, slot->id);
    slot->group = group;
    slot->owner = owner;
    dirty_ = true;
  }
  setIndex(slot->id, index);
  return slot->id;
}

bool SlotPool::replace(int owner, int waiting, int prerendered) {
  auto old = slots_.find(waiting);
  auto next = slots_.find(prerendered);
  if (old == slots_.end() || old->second.owner != owner || next == slots_.end() ||
      !matches(next->second, std::nullopt, old->second.kind, old->second.key)) {
    return false;
  }
  Slot& adopted = next->second;
  adopted.group = old->second.group;
  adopted.owner = owner;
  adopted.index = old->second.index;
  adopted.minMain = old->second.minMain;
  slots_.erase(old);
  erase(unadopted_, prerendered);
  dirty_ = true;
  return true;
}

void SlotPool::release(int owner, int slot) {
  auto found = slots_.find(slot);
  if (found == slots_.end() || owner == 0 || found->second.owner != owner) return;
  found->second.owner = 0;
  cache(free_, slot);
}

void SlotPool::lend(int owner) {
  for (auto& [id, slot] : slots_) {
    if (slot.owner != owner) continue;
    slot.owner = 0;
    lent_.push_back(id);
    dirty_ = true;
  }
}

void SlotPool::reclaim() {
  const std::vector<int> waiting = std::move(lent_);
  lent_.clear();
  for (int slot : waiting) cache(free_, slot);
}

std::vector<int> SlotPool::slotsFor(int kind, const std::string& key) const {
  std::vector<int> out;
  for (const auto& [id, slot] : slots_) {
    if (slot.kind == kind && slot.key == key) out.push_back(id);
  }
  return out;
}

void SlotPool::setIndex(int slot, int index) {
  auto found = slots_.find(slot);
  if (found == slots_.end() || found->second.index == index) return;
  found->second.index = index;
  dirty_ = true;
}

void SlotPool::setMinMain(int slot, float minMain) {
  auto found = slots_.find(slot);
  if (found == slots_.end() || found->second.minMain == minMain) return;
  found->second.minMain = minMain;
  dirty_ = true;
}

std::optional<int> SlotPool::ownerOf(int slot) const {
  auto found = slots_.find(slot);
  if (found == slots_.end() || found->second.owner == 0) return std::nullopt;
  return found->second.owner;
}

std::optional<SlotGroup> SlotPool::groupOf(int slot) const {
  auto found = slots_.find(slot);
  return found == slots_.end() ? std::nullopt : found->second.group;
}

std::optional<SlotEntry> SlotPool::entry(int slot) const {
  auto found = slots_.find(slot);
  if (found == slots_.end()) return std::nullopt;
  return toEntry(found->second);
}

std::vector<SlotEntry> SlotPool::snapshot() {
  dirty_ = false;
  std::vector<SlotEntry> out;
  out.reserve(slots_.size());
  for (const auto& [id, slot] : slots_) out.push_back(toEntry(slot));
  return out;
}

void SlotPool::cache(std::vector<int>& queue, int slot) {
  if (std::find(queue.begin(), queue.end(), slot) == queue.end()) queue.push_back(slot);
  dirty_ = true;
  trim(queue);
}

void SlotPool::trim(std::vector<int>& queue) {
  while (static_cast<int>(queue.size()) > maxFree_) {
    const int oldest = queue.front();
    queue.erase(queue.begin());
    slots_.erase(oldest);
    dirty_ = true;
  }
}

}  // namespace flume
