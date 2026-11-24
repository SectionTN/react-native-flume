#pragma once

#include <utility>
#include <vector>

#include "core/KeyDiff.h"

namespace flume {

// One UICollectionView performBatchUpdates: deletes and move sources at previous indices, inserts and move targets at next.
struct BatchUpdates {
  bool reload = false;                     // a reset
  std::vector<int> deletes;                // previous indices
  std::vector<int> inserts;                // next indices
  std::vector<std::pair<int, int>> moves;  // previous index, next index
  std::vector<int> reconfigures;           // next indices, applied after the batch
};

// A batch run before UIKit performs a pending reloadData lands on rows reloaded from the new data, so it reloads too.
BatchUpdates toBatchUpdates(const ListOps& ops, int previousCount, bool reloadPending = false);

}  // namespace flume
