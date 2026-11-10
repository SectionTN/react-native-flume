#include "ListRows.h"

#include <algorithm>

namespace flume {

int ListRows::itemCount() const {
  return headerCount() + dataCount + (hasEmptyRow() ? 1 : 0) + (footer ? 1 : 0);
}

RowKind ListRows::kind(int position) const {
  if (header && position == 0) return RowKind::Header;
  if (position < headerCount() + dataCount) return RowKind::Data;
  if (hasEmptyRow() && position == headerCount()) return RowKind::Empty;
  return RowKind::Footer;
}

int ListRows::dataIndex(int position) const {
  int first = headerCount();
  return position >= first && position < first + dataCount ? position - first : -1;
}

ListOps ListRows::positions(const ListOps& dataOps) const {
  const int shift = headerCount();
  ListOps ops = dataOps;
  if (shift == 0) return ops;
  for (auto& range : ops.removed) range.index += shift;
  for (auto& range : ops.inserted) range.index += shift;
  for (auto& [from, to] : ops.moved) {
    from += shift;
    to += shift;
  }
  for (int& index : ops.changed) index += shift;
  return ops;
}

std::vector<int> ListRows::fullSpanPositions(const std::vector<int>& stickyDataIndices) const {
  std::vector<int> positions;
  positions.reserve(stickyDataIndices.size() + 3);
  if (header) positions.push_back(0);
  for (int index : stickyDataIndices) {
    if (index >= 0 && index < dataCount) positions.push_back(position(index));
  }
  if (hasEmptyRow()) positions.push_back(headerCount());
  if (footer) positions.push_back(itemCount() - 1);
  std::sort(positions.begin(), positions.end());
  positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
  return positions;
}

}  // namespace flume
