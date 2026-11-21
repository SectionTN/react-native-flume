#pragma once

#include <string>
#include <utility>
#include <vector>

namespace flume {

// What a platform list applies to go from one key order to the next, in this order of application.
struct ListOps {
  struct Range {
    int index;
    int count;
    bool operator==(const Range&) const = default;
  };
  bool reset = false;                      // reload without animation (unstable keys or a new cell)
  std::vector<Range> removed;              // previous positions, descending
  std::vector<Range> inserted;             // next positions, ascending
  std::vector<std::pair<int, int>> moved;  // from, to; sequential on the order left by removes then inserts
  std::vector<int> changed;                // next positions of changed items that already existed
  std::vector<int> neighbors;  // next positions of worklet rows whose separator changed, none of them changed
  std::vector<int> sticky;  // the sticky header indices of the data these ops lead to
};

// The rows a platform rebinds in place: the changed rows and the neighbors, ascending.
std::vector<int> rebound(const ListOps& ops);

// Whether a next position gained or lost its separator, so a measured length no longer holds.
bool isNeighbor(const ListOps& ops, int index);

// Past this many moves the platform reloads: per-row move animations stop meaning anything, and the work stays bounded.
inline constexpr int kMaxMoves = 100;

ListOps diffKeys(const std::vector<std::string>& previous, const std::vector<std::string>& next,
                 const std::vector<int>& changedIndices, bool stableKeys);

}  // namespace flume
