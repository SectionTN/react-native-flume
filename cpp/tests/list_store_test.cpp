#include "doctest.h"

#include "core/ListStore.h"
#include "values.h"

using Range = flume::ListOps::Range;

TEST_CASE("a first snapshot inserts everything") {
  flume::ListStore store;
  store.setPending({"a", "b", "c"}, {"t", "t", "u"}, {0, 1, 2}, {V(1), V(2), V(3)}, true);
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->inserted == std::vector<Range>{{0, 3}});
  CHECK(store.count() == 3);
  CHECK(store.typeIdAt(0) == 0);
  CHECK(store.typeIdAt(2) == 1);
  CHECK(store.itemAt(1).asNumber() == 2);
  CHECK(store.keyAt(2) == "c");
}

TEST_CASE("a reset carries the sticky indices of its own data") {
  flume::ListStore store;
  store.setPending({"a", "b", "c"}, {"t", "t", "t"}, {0, 1, 2}, {V(1), V(2), V(3)}, true, {0});
  store.applyPending();
  store.setPending({"c", "a", "b"}, {"t", "t", "t"}, {}, {}, false, {0, 2});
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->reset);
  CHECK(ops->sticky == std::vector<int>{0, 2});
  CHECK(store.sticky() == std::vector<int>{0, 2});
}

TEST_CASE("the newest sticky indices win across coalesced snapshots") {
  flume::ListStore store;
  const std::vector<std::string> keys{"a", "b", "c", "d"};
  const std::vector<std::string> types{"t", "t", "t", "t"};
  store.setPending(keys, types, {}, {}, true, {1});
  store.setPending(keys, types, {}, {}, true, {2});
  CHECK(store.applyPending()->sticky == std::vector<int>{2});
  store.setSticky({1});
  store.setPending(keys, types, {}, {}, true, {3});
  CHECK(store.applyPending()->sticky == std::vector<int>{3});
}

TEST_CASE("a sticky-only change applies as empty ops") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {}, {}, true, {0});
  store.applyPending();
  CHECK(store.setSticky({1}));
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK_FALSE(ops->reset);
  CHECK(ops->removed.empty());
  CHECK(ops->inserted.empty());
  CHECK(ops->moved.empty());
  CHECK(ops->changed.empty());
  CHECK(ops->sticky == std::vector<int>{1});
  CHECK(store.sticky() == std::vector<int>{1});
}

TEST_CASE("a sticky-only change joins pending data") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {}, {}, true, {});
  CHECK_FALSE(store.setSticky({0}));
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->sticky == std::vector<int>{0});
}

TEST_CASE("sticky indices outside the data are dropped") {
  flume::ListStore store;
  store.setPending({"a", "b", "c"}, {"t", "t", "t"}, {}, {}, true, {0, 3, 7});
  CHECK(store.applyPending()->sticky == std::vector<int>{0});
}

TEST_CASE("coalesced snapshots diff against the applied order") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {0, 1}, {V(1), V(2)}, true);
  store.applyPending();
  store.setPending({"a", "b", "c"}, {"t", "t", "t"}, {2}, {V(3)}, true);
  store.setPending({"c", "a"}, {"t", "t"}, {}, {}, true);
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->removed == std::vector<Range>{{1, 1}});
  CHECK(ops->inserted == std::vector<Range>{{0, 1}});
  CHECK(ops->moved.empty());
  CHECK(store.itemAt(0).asNumber() == 3);
  CHECK(store.itemAt(1).asNumber() == 1);
  CHECK_FALSE(store.applyPending().has_value());
}

TEST_CASE("item ids are stable across reorders") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {0, 1}, {V(1), V(2)}, true);
  store.applyPending();
  int64_t b = store.itemIdAt(1);
  CHECK(b != store.itemIdAt(0));
  store.setPending({"b", "a"}, {"t", "t"}, {}, {}, true);
  store.applyPending();
  CHECK(store.itemIdAt(0) == b);
  CHECK(store.keysGeneration() == 1);
  store.setPending({"b"}, {"t"}, {}, {}, true);
  store.applyPending();
  CHECK(store.keysGeneration() == 2);
}

TEST_CASE("cell kinds follow the type names, including types seen later") {
  flume::ListStore store;
  store.setKinds(false, {"voice", "card"});
  store.setPending({"a", "b"}, {"message", "voice"}, {0}, {V(1)}, true);
  store.applyPending();
  CHECK_FALSE(store.isReactAt(0));
  CHECK(store.isReactAt(1));
  CHECK(store.viewTypeAt(0) == 0);
  CHECK(store.viewTypeAt(1) == 3);
  store.setPending({"a", "b", "c"}, {"message", "voice", "card"}, {}, {}, true);
  store.applyPending();
  CHECK(store.isReactAt(2));
  store.setKinds(true, {});
  CHECK_FALSE(store.applyPending().has_value());
  CHECK(store.isReactAt(0));
  CHECK(store.viewTypeAt(0) == 1);
}

TEST_CASE("a store has React kinds once applied kinds name any") {
  flume::ListStore store;
  CHECK_FALSE(store.hasReactKinds());
  store.setKinds(false, {"card"});
  CHECK_FALSE(store.hasReactKinds());
  store.applyPending();
  CHECK(store.hasReactKinds());
  store.setKinds(true, {});
  store.applyPending();
  CHECK(store.hasReactKinds());
  store.setKinds(false, {});
  store.applyPending();
  CHECK_FALSE(store.hasReactKinds());
}

TEST_CASE("a key whose type changes is reported changed") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {0, 1}, {V(1), V(2)}, true);
  store.applyPending();
  store.setPending({"a", "b"}, {"t", "u"}, {}, {}, true);
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->changed == std::vector<int>{1});
  CHECK(ops->inserted.empty());
}

TEST_CASE("a store is detached until attached and again after detach") {
  flume::ListStore store;
  CHECK_FALSE(store.attached());
  store.setAttached(true);
  CHECK(store.attached());
  store.setAttached(false);
  CHECK_FALSE(store.attached());
}

TEST_CASE("data set while detached waits for the attach to apply it") {
  flume::ListStore store;
  store.setPending({"a", "b"}, {"t", "t"}, {0, 1}, {V(1), V(2)}, true);
  CHECK_FALSE(store.attached());
  store.setAttached(true);
  auto ops = store.applyPending();
  REQUIRE(ops);
  CHECK(ops->inserted == std::vector<Range>{{0, 2}});
  CHECK(store.count() == 2);
}
