#pragma once

#include <vector>

#include "core/KeyDiff.h"

namespace flume {

enum class RowKind { Header, Data, Empty, Footer };

// View types for the list components: negative and odd, so React.
constexpr int kHeaderViewType = -1, kFooterViewType = -3, kEmptyViewType = -5;

// The list's rows: the header, then the data or the empty row, then the footer. Indices outside count data only.
struct ListRows {
  bool header = false;
  int dataCount = 0;
  bool emptyComponent = false;
  bool footer = false;

  int headerCount() const { return header ? 1 : 0; }
  bool hasEmptyRow() const { return emptyComponent && dataCount == 0; }
  int itemCount() const;
  RowKind kind(int position) const;
  int dataIndex(int position) const;  // -1 outside the data
  int position(int dataIndex) const { return dataIndex + headerCount(); }
  ListOps positions(const ListOps& dataOps) const;  // the store's ops, shifted past the header
  std::vector<int> fullSpanPositions(const std::vector<int>& stickyDataIndices) const;  // ascending, distinct

  bool operator==(const ListRows&) const = default;
};

}  // namespace flume
