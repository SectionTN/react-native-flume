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
