#include "doctest.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "adapter/BatchUpdates.h"
#include "core/KeyDiff.h"

using namespace flume;

namespace {

using Keys = std::vector<std::string>;

// Applies a batch the way UICollectionView does: moved-to and inserted items take their next indices, the rest fill in order.
Keys applyLikeUIKit(const Keys& previous, const Keys& next, const BatchUpdates& batch) {
  if (batch.reload) return next;
  const size_t size = previous.size() - batch.deletes.size() + batch.inserts.size();
  std::vector<std::string> out(size);
  std::vector<bool> placed(size, false);
  std::vector<bool> gone(previous.size(), false);
  for (int index : batch.deletes) gone[index] = true;
  for (int index : batch.inserts) {
    out[index] = next[index];
    placed[index] = true;
  }
  for (const auto& [from, to] : batch.moves) {
    out[to] = previous[from];
    placed[to] = true;
    gone[from] = true;
  }
  size_t slot = 0;
  for (size_t i = 0; i < previous.size(); i++) {
    if (gone[i]) continue;
    while (placed[slot]) slot++;
    out[slot++] = previous[i];
  }
  return out;
}

}  // namespace

TEST_CASE("removes and inserts become previous and next indices") {
  ListOps ops;
  ops.removed = {{1, 2}};
  ops.inserted = {{0, 1}};
  BatchUpdates batch = toBatchUpdates(ops, 5);
  CHECK_FALSE(batch.reload);
  CHECK(batch.deletes == std::vector<int>{1, 2});
  CHECK(batch.inserts == std::vector<int>{0});
  CHECK(batch.moves.empty());
}

TEST_CASE("a sequential move becomes one move from its previous to its next index") {
  ListOps ops = diffKeys({"a", "b", "c", "d"}, {"d", "a", "b", "c"}, {}, true);
  BatchUpdates batch = toBatchUpdates(ops, 4);
  CHECK(batch.moves == std::vector<std::pair<int, int>>{{3, 0}});
}

TEST_CASE("random orders replay to the next order") {
  std::mt19937 rng(7);
  auto randomOrder = [&]() {
    Keys pool;
    for (int i = 0; i < 40; i++) pool.push_back("k" + std::to_string(i));
    std::shuffle(pool.begin(), pool.end(), rng);
    pool.resize(rng() % 31);
    return pool;
  };
  for (int i = 0; i < 200; i++) {
    Keys previous = randomOrder();
    Keys next = randomOrder();
    ListOps ops = diffKeys(previous, next, {}, true);
    BatchUpdates batch = toBatchUpdates(ops, static_cast<int>(previous.size()));
    CHECK(applyLikeUIKit(previous, next, batch) == next);
  }
}

TEST_CASE("changed items are reconfigured at their next indices") {
  ListOps ops = diffKeys({"a", "b", "c"}, {"c", "a", "b"}, {0, 2}, true);
  BatchUpdates batch = toBatchUpdates(ops, 3);
  CHECK(batch.reconfigures == std::vector<int>{0, 2});
}

TEST_CASE("a reset reloads") {
  ListOps ops;
  ops.reset = true;
  CHECK(toBatchUpdates(ops, 3).reload);
}

TEST_CASE("a batch while a reload is still owed reloads too") {
  ListOps ops = diffKeys({"a", "b", "c"}, {"a", "c"}, {}, true);
  CHECK_FALSE(toBatchUpdates(ops, 3).reload);
  BatchUpdates batch = toBatchUpdates(ops, 3, true);
  CHECK(batch.reload);
  CHECK(batch.deletes.empty());
}

TEST_CASE("separator neighbors are reconfigured with the changed items") {
  ListOps ops = diffKeys({"a", "b", "c"}, {"a", "b", "c", "d"}, {}, true);
  ops.neighbors = {2};
  BatchUpdates batch = toBatchUpdates(ops, 3);
  CHECK(batch.reconfigures == std::vector<int>{2});
}
