#include "doctest.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "core/KeyDiff.h"

namespace {

using Keys = std::vector<std::string>;
using Range = flume::ListOps::Range;

// Replays the ops on the previous order the way the adapter does: removes, inserts, moves.
Keys apply(Keys order, const Keys& next, const flume::ListOps& ops) {
  if (ops.reset) return next;
  for (const auto& r : ops.removed) order.erase(order.begin() + r.index, order.begin() + r.index + r.count);
  for (const auto& r : ops.inserted) {
    order.insert(order.begin() + r.index, next.begin() + r.index, next.begin() + r.index + r.count);
  }
  for (const auto& [from, to] : ops.moved) {
    std::string key = order[from];
    order.erase(order.begin() + from);
    order.insert(order.begin() + to, key);
  }
  return order;
}

}  // namespace

TEST_CASE("append, prepend and remove") {
  auto a = flume::diffKeys({"a", "b"}, {"a", "b", "c"}, {2}, true);
  CHECK(a.inserted == std::vector<Range>{{2, 1}});
  CHECK(a.removed.empty());
  CHECK(a.changed.empty());
  auto p = flume::diffKeys({"a", "b"}, {"z", "a", "b"}, {0}, true);
  CHECK(p.inserted == std::vector<Range>{{0, 1}});
  CHECK(p.moved.empty());
  auto r = flume::diffKeys({"a", "b", "c"}, {"a", "c"}, {}, true);
  CHECK(r.removed == std::vector<Range>{{1, 1}});
  CHECK(r.inserted.empty());
}

TEST_CASE("a reversal moves one at a time") {
  auto ops = flume::diffKeys({"a", "b", "c"}, {"c", "b", "a"}, {}, true);
  CHECK(ops.moved == std::vector<std::pair<int, int>>{{2, 0}, {2, 1}});
  CHECK(apply({"a", "b", "c"}, {"c", "b", "a"}, ops) == Keys{"c", "b", "a"});
}

TEST_CASE("changed items that stayed are reported at their new index") {
  auto ops = flume::diffKeys({"a", "b"}, {"b", "a"}, {0, 1}, true);
  CHECK(ops.changed == std::vector<int>{0, 1});
  auto ins = flume::diffKeys({"a"}, {"a", "b"}, {1}, true);
  CHECK(ins.changed.empty());
}

TEST_CASE("unstable keys reset") {
  CHECK(flume::diffKeys({"0", "1"}, {"0", "1", "2"}, {2}, false).reset);
  CHECK_FALSE(flume::diffKeys({"a"}, {"a"}, {}, true).reset);
}

TEST_CASE("random orders replay to the next order") {
  std::mt19937 rng(42);
  auto randomOrder = [&]() {
    std::string letters = "abcdefghijkl";
    std::shuffle(letters.begin(), letters.end(), rng);
    Keys keys;
    size_t n = rng() % 9;
    for (size_t i = 0; i < n; i++) keys.push_back(std::string(1, letters[i]));
    return keys;
  };
  for (int i = 0; i < 300; i++) {
    Keys previous = randomOrder();
    Keys next = randomOrder();
    auto ops = flume::diffKeys(previous, next, {}, true);
    CHECK(apply(previous, next, ops) == next);
  }
}
