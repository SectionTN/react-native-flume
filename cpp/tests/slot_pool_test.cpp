#include "doctest.h"

#include <algorithm>
#include <set>

#include "adapter/SlotPool.h"

using namespace flume;

namespace {

const SlotGroup kList{1};
const SlotGroup kCards{3};

std::vector<int> ids(const std::vector<SlotEntry>& entries) {
  std::vector<int> out;
  for (const SlotEntry& entry : entries) out.push_back(entry.slot);
  return out;
}

}  // namespace

TEST_CASE("a key comes back to its slot") {
  SlotPool pool;
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  pool.release(1, a);
  CHECK(pool.acquire(2, kList, kSlotRow, "a", 4) == a);
  CHECK(*pool.entry(a) == SlotEntry{a, kSlotRow, "a", 4, true});
}

TEST_CASE("a slot never shows another item or group") {
  SlotPool pool;
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  pool.release(1, a);
  CHECK(pool.acquire(2, kList, kSlotRow, "b", 1) != a);
  CHECK(pool.acquire(3, kCards, kSlotRow, "a", 0) != a);
  CHECK(pool.acquire(4, kList, kSlotRow, "c", 2) != pool.acquire(5, kList, kSlotRow, "c", 2));
}

TEST_CASE("the oldest released slot leaves past the cap") {
  SlotPool pool(2);
  std::vector<int> released;
  const std::vector<std::string> keys{"a", "b", "c"};
  for (int i = 0; i < 3; i++) {
    released.push_back(pool.acquire(i + 1, kList, kSlotRow, keys[i], i));
    pool.release(i + 1, released.back());
  }
  CHECK(ids(pool.snapshot()) == std::vector<int>{released[1], released[2]});
  CHECK(pool.acquire(9, kList, kSlotRow, "a", 0) > released.back());
}

TEST_CASE("only the owner releases") {
  SlotPool pool;
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  pool.release(2, a);
  CHECK(pool.ownerOf(a) == 1);
}

TEST_CASE("the snapshot lists bound and released slots and tracks changes") {
  SlotPool pool;
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  const int b = pool.acquire(2, kList, kSlotRow, "b", 1);
  pool.release(1, a);
  CHECK(pool.dirty());
  const std::vector<SlotEntry> snapshot = pool.snapshot();
  REQUIRE(snapshot.size() == 2);
  CHECK_FALSE(snapshot[0].bound);
  CHECK(snapshot[1].bound);
  CHECK_FALSE(pool.dirty());
  pool.setIndex(b, 1);
  CHECK_FALSE(pool.dirty());
  pool.setIndex(b, 2);
  CHECK(pool.dirty());
}

TEST_CASE("a bind adopts a pre-rendered slot for its key") {
  SlotPool pool;
  pool.registerPrerendered(-1, kSlotRow, "a");
  CHECK(pool.acquire(1, kList, kSlotRow, "a", 0) == -1);
  CHECK(pool.jsSeen() == -1);
  CHECK(pool.acquire(2, kCards, kSlotRow, "a", 0) != -1);
}

TEST_CASE("a release never evicts a pre-rendered slot") {
  SlotPool pool(2);
  pool.registerPrerendered(-1, kSlotRow, "a");
  const std::vector<std::string> keys{"x", "y", "z"};
  for (int i = 0; i < 3; i++) pool.release(i + 1, pool.acquire(i + 1, kList, kSlotRow, keys[i], i));
  CHECK(pool.acquire(9, kList, kSlotRow, "a", 0) == -1);
}

TEST_CASE("a mount batch of pre-rendered slots past the cap waits for its binds") {
  SlotPool pool(2);
  const std::vector<std::string> keys{"a", "b", "c", "d", "e"};
  for (int i = 0; i < 5; i++) pool.registerPrerendered(-1 - i, kSlotRow, keys[i]);
  for (int i = 0; i < 5; i++) CHECK(pool.acquire(i + 1, kList, kSlotRow, keys[i], i) == -1 - i);
}

TEST_CASE("older pre-rendered slots past the cap leave as the next batch registers") {
  SlotPool pool(2);
  const std::vector<std::string> keys{"a", "b", "c"};
  for (int i = 0; i < 3; i++) pool.registerPrerendered(-1 - i, kSlotRow, keys[i]);
  CHECK(ids(pool.snapshot()) == std::vector<int>{-3, -2, -1});
  pool.endBatch();
  pool.registerPrerendered(-4, kSlotRow, "d");
  CHECK(ids(pool.snapshot()) == std::vector<int>{-4, -3, -2});
}

TEST_CASE("a pre-rendered slot replaces one still waiting") {
  SlotPool pool;
  const int waiting = pool.acquire(1, kList, kSlotRow, "a", 3);
  pool.registerPrerendered(-2, kSlotRow, "a");
  CHECK(pool.replace(1, waiting, -2));
  CHECK(pool.snapshot() == std::vector<SlotEntry>{{-2, kSlotRow, "a", 3, true}});
}

TEST_CASE("a bind adopts a lent slot for its key and group") {
  SlotPool pool;
  const int a = pool.acquire(7, kList, kSlotRow, "a", 50);
  pool.lend(7);
  CHECK(pool.acquire(3, kList, kSlotRow, "a", 50) == a);
  CHECK(pool.ownerOf(a) == 3);
}

TEST_CASE("a lent slot never goes to another key or group") {
  SlotPool pool;
  const int a = pool.acquire(7, kList, kSlotRow, "a", 50);
  pool.lend(7);
  CHECK(pool.acquire(3, kList, kSlotRow, "b", 51) != a);
  CHECK(pool.acquire(4, kCards, kSlotRow, "a", 50) != a);
}

TEST_CASE("reclaim caches the lent slots no bind took") {
  SlotPool pool;
  std::vector<int> lent;
  for (int i = 0; i < 25; i++) lent.push_back(pool.acquire(7, kList, kSlotRow, "k" + std::to_string(i), i));
  pool.lend(7);
  pool.acquire(3, kList, kSlotRow, "k0", 0);
  pool.acquire(4, kList, kSlotRow, "k1", 1);
  pool.reclaim();
  std::vector<int> bound, released;
  for (const SlotEntry& entry : pool.snapshot()) (entry.bound ? bound : released).push_back(entry.slot);
  CHECK(bound == std::vector<int>(lent.begin(), lent.begin() + 2));
  CHECK(released == std::vector<int>(lent.begin() + 5, lent.end()));
}

TEST_CASE("slotsFor lists every slot of a key") {
  SlotPool pool;
  const int owned = pool.acquire(1, kList, kSlotRow, "a", 0);
  const int released = pool.acquire(2, kCards, kSlotRow, "a", 0);
  pool.release(2, released);
  pool.registerPrerendered(-1, kSlotRow, "a");
  pool.acquire(3, kList, kSlotRow, "b", 1);
  const std::vector<int> slots = pool.slotsFor(kSlotRow, "a");
  CHECK(std::set<int>(slots.begin(), slots.end()) == std::set<int>{owned, released, -1});
}

namespace {

SlotRequest row(const std::string& key, int index, SlotGroup group = kList) {
  return SlotRequest{group, kSlotRow, key, index};
}

}  // namespace

TEST_CASE("rows ahead each wait in a slot their bind adopts") {
  SlotPool pool;
  pool.keepAhead({row("a", 5), row("b", 6)});
  const std::vector<SlotEntry> entries = pool.snapshot();
  REQUIRE(entries.size() == 2);
  CHECK(entries[0] == SlotEntry{entries[0].slot, kSlotRow, "a", 5, false});
  CHECK(entries[1] == SlotEntry{entries[1].slot, kSlotRow, "b", 6, false});
  CHECK(pool.acquire(3, kList, kSlotRow, "a", 5) == entries[0].slot);
  CHECK(pool.acquire(4, kCards, kSlotRow, "b", 6) != entries[1].slot);
}

TEST_CASE("a row bound in its group gets no slot ahead") {
  SlotPool pool;
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  pool.keepAhead({row("a", 0)});
  CHECK(pool.slotsFor(kSlotRow, "a") == std::vector<int>{a});
}

TEST_CASE("a released or lent slot of a row ahead leaves the cap and the reclaim") {
  SlotPool pool(0);
  const int a = pool.acquire(1, kList, kSlotRow, "a", 0);
  pool.registerPrerendered(-1, kSlotRow, "b");
  pool.lend(1);
  pool.acquire(2, kList, kSlotRow, "c", 2);
  pool.keepAhead({row("a", 0), row("b", 1), row("c", 2)});
  pool.release(2, pool.slotsFor(kSlotRow, "c").front());
  pool.reclaim();
  CHECK(pool.slotsFor(kSlotRow, "a") == std::vector<int>{a});
  CHECK(pool.slotsFor(kSlotRow, "b") == std::vector<int>{-1});
  CHECK(pool.slotsFor(kSlotRow, "c").empty());
}

TEST_CASE("slots of rows that leave the look-ahead are released") {
  SlotPool pool(1);
  pool.keepAhead({row("a", 0), row("b", 1), row("c", 2)});
  pool.keepAhead({row("c", 2)});
  CHECK(pool.slotsFor(kSlotRow, "a").empty());
  CHECK(pool.slotsFor(kSlotRow, "b").size() == 1);
  CHECK(pool.slotsFor(kSlotRow, "c").size() == 1);
}

TEST_CASE("slots ahead follow their row's index, and an unchanged look-ahead changes nothing") {
  SlotPool pool;
  pool.keepAhead({row("a", 5)});
  pool.snapshot();
  pool.keepAhead({row("a", 5)});
  CHECK_FALSE(pool.dirty());
  pool.keepAhead({row("a", 7)});
  CHECK(pool.dirty());
  CHECK(pool.snapshot().front().index == 7);
}
