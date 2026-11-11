#include "adapter/SlotPool.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

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
  if (slot == nullptr) slot = firstIn(ahead_, group, kind, key);
  if (slot == nullptr) slot = firstIn(unadopted_, std::nullopt, kind, key);
  if (slot == nullptr) {
    const int id = nextId_++;
    slot = &slots_.emplace(id, Slot{id, group, kind, key}).first->second;
  }
  if (slot->owner != owner) {
    erase(free_, slot->id);
    erase(lent_, slot->id);
    erase(ahead_, slot->id);
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

void SlotPool::keepAhead(const std::vector<SlotRequest>& rows) {
  std::vector<int> kept;
  std::unordered_set<std::string> boundKeys;
  for (const auto& [id, slot] : slots_) {
    if (slot.owner != 0) boundKeys.insert(slot.key);
  }
  for (const SlotRequest& row : rows) {
    const bool bound = boundKeys.count(row.key) > 0 && std::any_of(slots_.begin(), slots_.end(), [&](const auto& item) {
                         return item.second.owner != 0 && matches(item.second, row.group, row.kind, row.key);
                       });
    if (bound) continue;
    Slot* slot = firstIn(ahead_, row.group, row.kind, row.key);
    if (slot == nullptr) slot = firstIn(free_, row.group, row.kind, row.key);
    if (slot == nullptr) slot = firstIn(lent_, row.group, row.kind, row.key);
    if (slot == nullptr) slot = firstIn(unadopted_, std::nullopt, row.kind, row.key);
    if (slot == nullptr) {
      const int id = nextId_++;
      slot = &slots_.emplace(id, Slot{id, row.group, row.kind, row.key}).first->second;
      dirty_ = true;
    }
    erase(free_, slot->id);
    erase(lent_, slot->id);
    erase(unadopted_, slot->id);
    slot->group = row.group;
    setIndex(slot->id, row.index);
    if (std::find(kept.begin(), kept.end(), slot->id) == kept.end()) kept.push_back(slot->id);
  }
  const std::vector<int> left = std::exchange(ahead_, std::move(kept));
  for (int slot : left) {
    if (std::find(ahead_.begin(), ahead_.end(), slot) == ahead_.end()) cache(free_, slot);
  }
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
  int headers = 0;
  int footers = 0;
  for (const auto& [id, slot] : slots_) {
    headers += slot.kind == kSlotHeader;
    footers += slot.kind == kSlotFooter;
  }
  // A shown header or footer stays mounted, as FlatList keeps it, while it is its kind's only slot.
  auto counted = [&](int id) {
    const int kind = slots_.at(id).kind;
    return kind == kSlotHeader ? headers > 1 : kind == kSlotFooter ? footers > 1 : true;
  };
  while (std::count_if(queue.begin(), queue.end(), counted) > maxFree_) {
    const auto oldest = std::find_if(queue.begin(), queue.end(), counted);
    const int kind = slots_.at(*oldest).kind;
    headers -= kind == kSlotHeader;
    footers -= kind == kSlotFooter;
    slots_.erase(*oldest);
    queue.erase(oldest);
    dirty_ = true;
  }
}

}  // namespace flume
