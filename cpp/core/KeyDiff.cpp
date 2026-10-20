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

  // The order the platform holds after removes and inserts; moves bring it to the next order one at a time.
  Keys working;
  working.reserve(next.size());
  for (const std::string& key : previous) {
    if (inNext.count(key) != 0) working.push_back(key);
  }
  for (const auto& range : ops.inserted) {
    working.insert(working.begin() + range.index, next.begin() + range.index, next.begin() + range.index + range.count);
  }
  for (size_t i = 0; i < next.size() && i < working.size(); i++) {
    if (working[i] == next[i]) continue;
    size_t j = i + 1;
    while (j < working.size() && working[j] != next[i]) j++;
    if (j >= working.size()) break;
    std::string key = std::move(working[j]);
    working.erase(working.begin() + static_cast<long>(j));
    working.insert(working.begin() + static_cast<long>(i), std::move(key));
    ops.moved.emplace_back(static_cast<int>(j), static_cast<int>(i));
  }
  for (int index : changedIndices) {
    if (index >= 0 && index < static_cast<int>(next.size()) && inPrevious.count(next[index]) != 0) {
      ops.changed.push_back(index);
    }
  }
  return ops;
}

}  // namespace flume
