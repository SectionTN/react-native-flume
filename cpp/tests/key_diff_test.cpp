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
  CHECK(ops.moved.size() == 2);
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

namespace {

// The longest increasing run of next's keys by their previous positions, by the O(n^2) definition.
size_t longestInOrder(const Keys& previous, const Keys& next) {
  std::vector<int> positions;
  for (const std::string& key : next) {
    positions.push_back(static_cast<int>(std::find(previous.begin(), previous.end(), key) - previous.begin()));
  }
  std::vector<size_t> best(positions.size(), 1);
  size_t longest = 0;
  for (size_t i = 0; i < positions.size(); i++) {
    for (size_t j = 0; j < i; j++) {
      if (positions[j] < positions[i]) best[i] = std::max(best[i], best[j] + 1);
    }
    longest = std::max(longest, best[i]);
  }
  return longest;
}

Keys numbered(int count) {
  Keys keys;
  for (int i = 0; i < count; i++) keys.push_back(std::to_string(i));
  return keys;
}

}  // namespace

TEST_CASE("moving the first key to the end is one move") {
  Keys previous{"a", "b", "c", "d"};
  Keys next{"b", "c", "d", "a"};
  auto ops = flume::diffKeys(previous, next, {}, true);
  CHECK(ops.moved.size() == 1);
  CHECK(apply(previous, next, ops) == next);
}

TEST_CASE("random orders replay with the fewest moves") {
  std::mt19937 rng(7);
  for (int i = 0; i < 300; i++) {
    Keys previous = numbered(static_cast<int>(rng() % 13));
    Keys next = previous;
    std::shuffle(next.begin(), next.end(), rng);
    auto ops = flume::diffKeys(previous, next, {}, true);
    CHECK(apply(previous, next, ops) == next);
    CHECK(ops.moved.size() == next.size() - longestInOrder(previous, next));
  }
}

TEST_CASE("more moves than the cap reset") {
  auto reversed = [](Keys keys) {
    std::reverse(keys.begin(), keys.end());
    return keys;
  };
  Keys atCap = numbered(flume::kMaxMoves + 1);
  auto kept = flume::diffKeys(atCap, reversed(atCap), {}, true);
  CHECK_FALSE(kept.reset);
  CHECK(kept.moved.size() == static_cast<size_t>(flume::kMaxMoves));
  Keys pastCap = numbered(flume::kMaxMoves + 2);
  CHECK(flume::diffKeys(pastCap, reversed(pastCap), {}, true).reset);
  Keys long_ = numbered(10000);
  CHECK(flume::diffKeys(long_, reversed(long_), {}, true).reset);
}
