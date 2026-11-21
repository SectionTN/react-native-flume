#include "adapter/BatchUpdates.h"

#include <algorithm>

namespace flume {

namespace {

struct Entry {
  int previous;  // -1 for an insert
  bool moved;
};

}  // namespace

BatchUpdates toBatchUpdates(const ListOps& ops, int previousCount) {
  BatchUpdates batch;
  if (ops.reset) {
    batch.reload = true;
    return batch;
  }
  std::vector<Entry> order;
  order.reserve(std::max(0, previousCount));
  for (int i = 0; i < previousCount; i++) order.push_back(Entry{i, false});
  for (const auto& range : ops.removed) {
    for (int i = 0; i < range.count; i++) batch.deletes.push_back(range.index + i);
    order.erase(order.begin() + range.index, order.begin() + range.index + range.count);
  }
  for (const auto& range : ops.inserted) order.insert(order.begin() + range.index, range.count, Entry{-1, false});
  for (const auto& [from, to] : ops.moved) {
    Entry entry = order[from];
    entry.moved = true;
    order.erase(order.begin() + from);
    order.insert(order.begin() + to, entry);
  }
  for (int next = 0; next < static_cast<int>(order.size()); next++) {
    if (order[next].previous < 0) {
      batch.inserts.push_back(next);
    } else if (order[next].moved) {
      batch.moves.emplace_back(order[next].previous, next);
    }
  }
  std::sort(batch.deletes.begin(), batch.deletes.end());
  batch.reconfigures = rebound(ops);
  return batch;
}

}  // namespace flume
