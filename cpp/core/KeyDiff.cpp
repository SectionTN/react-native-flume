#include "core/KeyDiff.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace flume {
namespace {

using Keys = std::vector<std::string>;

// Runs of positions whose key the predicate rejects, ascending.
template <typename Absent>
std::vector<ListOps::Range> runs(const Keys& keys, Absent absent) {
  std::vector<ListOps::Range> out;
  for (int i = 0; i < static_cast<int>(keys.size()); i++) {
    if (!absent(keys[i])) continue;
    if (!out.empty() && out.back().index + out.back().count == i) out.back().count++;
    else out.push_back({i, 1});
  }
  return out;
}

// Marks one longest strictly increasing subsequence of distinct values (patience sorting, O(n log n)).
std::vector<char> longestIncreasing(const std::vector<int>& values) {
  std::vector<int> tails;
  std::vector<int> parent(values.size(), -1);
  for (int i = 0; i < static_cast<int>(values.size()); i++) {
    auto it = std::lower_bound(tails.begin(), tails.end(), values[i],
                               [&](int tail, int value) { return values[tail] < value; });
    if (it != tails.begin()) parent[i] = *(it - 1);
    if (it == tails.end()) tails.push_back(i);
    else *it = i;
  }
  std::vector<char> kept(values.size(), 0);
  for (int i = tails.empty() ? -1 : tails.back(); i >= 0; i = parent[i]) kept[i] = 1;
  return kept;
}

int indexOf(const Keys& keys, const std::string& key) {
  return static_cast<int>(std::find(keys.begin(), keys.end(), key) - keys.begin());
}

}  // namespace

ListOps diffKeys(const Keys& previous, const Keys& next, const std::vector<int>& changedIndices, bool stableKeys) {
  ListOps ops;
  if (!stableKeys) {
    ops.reset = true;
    return ops;
  }
  std::unordered_set<std::string_view> inNext(next.begin(), next.end());
  std::unordered_set<std::string_view> inPrevious(previous.begin(), previous.end());
  ops.removed = runs(previous, [&](const std::string& key) { return inNext.count(key) == 0; });
  std::reverse(ops.removed.begin(), ops.removed.end());
  ops.inserted = runs(next, [&](const std::string& key) { return inPrevious.count(key) == 0; });

  // The order the platform holds after removes and inserts; moves bring it to the next order.
  Keys working;
  working.reserve(next.size());
  for (const std::string& key : previous) {
    if (inNext.count(key) != 0) working.push_back(key);
  }
  for (const auto& range : ops.inserted) {
    working.insert(working.begin() + range.index, next.begin() + range.index, next.begin() + range.index + range.count);
  }
  std::vector<int> positions(next.size());
  {
    std::unordered_map<std::string_view, int> at;
    at.reserve(working.size());
    for (int i = 0; i < static_cast<int>(working.size()); i++) at.emplace(working[i], i);
    for (size_t i = 0; i < next.size(); i++) positions[i] = at.at(next[i]);
  }
  // Keys in one longest run already in order stay; every other key moves once.
  std::vector<char> kept = longestIncreasing(positions);
  if (std::count(kept.begin(), kept.end(), 0) > kMaxMoves) {
    ListOps reload;
    reload.reset = true;
    return reload;
  }
  // From the end, a key that moves goes just before the key after it in the next order, which is already placed.
  for (int i = static_cast<int>(next.size()) - 1; i >= 0; i--) {
    if (kept[i]) continue;
    int from = indexOf(working, next[i]);
    int to = static_cast<int>(working.size()) - 1;
    if (i + 1 < static_cast<int>(next.size())) {
      int anchor = indexOf(working, next[i + 1]);
      to = from < anchor ? anchor - 1 : anchor;
    }
    std::string key = std::move(working[from]);
    working.erase(working.begin() + from);
    working.insert(working.begin() + to, std::move(key));
    ops.moved.emplace_back(from, to);
  }
  for (int index : changedIndices) {
    if (index >= 0 && index < static_cast<int>(next.size()) && inPrevious.count(next[index]) != 0) {
      ops.changed.push_back(index);
    }
  }
  return ops;
}

std::vector<int> rebound(const ListOps& ops) {
  std::vector<int> rows = ops.changed;
  rows.insert(rows.end(), ops.neighbors.begin(), ops.neighbors.end());
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  return rows;
}

bool isNeighbor(const ListOps& ops, int index) {
  return std::binary_search(ops.neighbors.begin(), ops.neighbors.end(), index);
}

}  // namespace flume
