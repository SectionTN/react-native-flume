#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace flume {

enum SlotKind { kSlotRow = 0, kSlotHeader = 1, kSlotFooter = 2, kSlotEmpty = 3, kSlotSticky = 4 };

// The cells a slot may move between: one view type, in the list or pinned.
struct SlotGroup {
  int viewType;
  bool pinned = false;
  bool operator==(const SlotGroup&) const = default;
};

struct SlotEntry {
  int slot;
  int kind;
  std::string key;
  int index;
  bool bound;
  float minMain = 0;
  bool operator==(const SlotEntry&) const = default;
};

// The slot a React row asks for when it binds: its cells' group, its kind, its key and its data index.
struct SlotRequest {
  SlotGroup group;
  int kind;
  std::string key;
  int index;
};

// Which slot each cell shows: a slot keeps one key for life, and a released or pre-rendered one waits for that key.
class SlotPool {
 public:
  static constexpr int kMaxFree = 20;

  explicit SlotPool(int maxFree = kMaxFree) : maxFree_(maxFree) {}

  // Every pre-rendered slot of one mount batch waits for its binds, older ones past the cap leave as the next registers.
  void registerPrerendered(int slot, int kind, const std::string& key);
  void endBatch() { batchOpen_ = false; }
  int acquire(int owner, SlotGroup group, int kind, const std::string& key, int index);
  // The owner's waiting slot leaves the pool and the pre-rendered one for the same key takes its place.
  bool replace(int owner, int waiting, int prerendered);
  void release(int owner, int slot);
  // The owner's slots wait, ownerless, for the next binds of their key and group, as a jump's rows rendered ahead do.
  void lend(int owner);
  // Lent slots no bind took become released slots, so the cap applies to them.
  void reclaim();
  // Rows rendered ahead of the edges keep slots out of the cap and the reclaim, released once no longer ahead.
  void keepAhead(const std::vector<SlotRequest>& rows);
  std::vector<int> slotsFor(int kind, const std::string& key) const;
  void setIndex(int slot, int index);
  void setMinMain(int slot, float minMain);
  std::optional<int> ownerOf(int slot) const;
  std::optional<SlotGroup> groupOf(int slot) const;  // nullopt for a pre-rendered slot no bind adopted
  std::optional<SlotEntry> entry(int slot) const;
  std::vector<SlotEntry> snapshot();  // clears dirty
  bool dirty() const { return dirty_; }
  int jsSeen() const { return jsSeen_; }  // the lowest pre-rendered id registered, JS drops a pending slot past it

 private:
  struct Slot {
    int id;
    std::optional<SlotGroup> group;
    int kind;
    std::string key;
    int index = -1;
    int owner = 0;
    float minMain = 0;
  };

  static bool matches(const Slot& slot, const std::optional<SlotGroup>& group, int kind, const std::string& key);
  static SlotEntry toEntry(const Slot& slot);
  Slot* firstIn(const std::vector<int>& queue, const std::optional<SlotGroup>& group, int kind, const std::string& key);
  void cache(std::vector<int>& queue, int slot);
  void trim(std::vector<int>& queue);

  int maxFree_;
  std::map<int, Slot> slots_;
  // Released, lent, ahead and pre-rendered slots wait in separate queues, oldest first.
  std::vector<int> free_;
  std::vector<int> lent_;
  std::vector<int> ahead_;
  std::vector<int> unadopted_;
  int nextId_ = 1;
  bool batchOpen_ = false;
  bool dirty_ = false;
  int jsSeen_ = 0;
};

}  // namespace flume
