#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/KeyDiff.h"

namespace flume {

struct ListLayoutConfig {
  bool horizontal = false;
  bool inverted = false;
  int lanes = 1;
  float paddingStart = 0, paddingEnd = 0;  // along the scroll axis, at the visual top or left and bottom or right
  float paddingCrossStart = 0, paddingCrossEnd = 0;
  float estimatedItemLength = -1;  // estimatedItemSize; negative for the per-type mean
  float estimateFloor = 50;
  float separator = 0;  // the gap after every data row but the last, along the scroll axis
  float mainGap = 0;  // after every row but the last, after the separator, the header and footer included
  float gridRowGap = 0;  // after the separator of every data row but the last
  float crossGap = 0;  // between the lanes of a grid row
  float laneInsetStart = 0, laneInsetEnd = 0;  // a data row's cross insets, inside the cross padding
  int dataStart = 0, afterData = 0;  // the items before and after the data rows: the header and the footer
  int emptyIndex = -1;  // the empty row, -1 for none
  float separatorInsetStart = 0, separatorInsetEnd = 0;  // across the scroll axis, from the padded edges
  // startRenderingFromBottom on an upright list; short content sits at the end, sizes keep the last row in view.
  bool stackFromEnd = false;
};

// Percent padding is a fraction of the list width on every edge, as Yoga resolves it.
inline float paddingOf(float points, float fraction, float listWidth) {
  return points + fraction * std::max(0.f, listWidth);
}

struct ItemRect {
  float x, y, width, height;
  bool operator==(const ItemRect&) const = default;
};

struct Pinned {
  int index;
  float start;  // along the scroll axis, in content coordinates
};

struct IndexRange {
  int first;
  int last;  // inclusive; empty when first > last
  bool operator==(const IndexRange&) const = default;
};

struct LengthChange {
  float anchorMoved;   // how far the anchor's edge moved along the scroll axis
  float contentMoved;  // how far the content length moved
};

// Item positions along the scroll axis from measured lengths and per-type estimates; units are points.
class ListLayout {
 public:
  using TypeOf = std::function<int(int index)>;

  void configure(const ListLayoutConfig& config, std::vector<int> fullSpan);  // fullSpan ascending
  void reset(int count, const TypeOf& typeOf);
  // Returns the changed items whose type changed (they need a reload). fullSpan is the new data's, ascending.
  std::vector<int> apply(const ListOps& ops, int count, const TypeOf& typeOf, std::vector<int> fullSpan);
  // The anchor's move is its top edge's, or its bottom edge's when inverted.
  LengthChange setLength(int index, float length, int anchor, float viewportLength = 0);
  void invalidateLengths();
  int count() const { return static_cast<int>(items_.size()); }
  int rowCount() const { return static_cast<int>(rows_.size()); }
  int rowOf(int index) const { return rowOf_[index]; }
  float rowLength(int row) const { return rows_[row].length; }
  float rowStride(int row) const;  // the row's length with the separator and the gaps after it
  float itemLength(int index) const { return items_[index].length; }  // its own, measured or estimated
  IndexRange itemsOfRows(int firstRow, int lastRow) const;
  bool measured(int index) const;
  float estimate(int type) const;
  float contentLength(float viewportLength) const;
  // From the start of the padding to the end of it, at the bottom of a short list that sits there.
  std::pair<float, float> contentSpan(float viewportLength) const;
  float crossLength(int index, float listCrossLength) const;  // the size a cell binds at across the scroll axis
  float laneLength(float listCrossLength) const;  // one column of a grid, the whole width with one lane
  ItemRect rectOf(int index, float listCrossLength, float viewportLength) const;
  int indexAt(float offset, float viewportLength) const;
  // The item whose edge stays put while rows measure, for setLength.
  int anchorAt(float viewportStart, float viewportLength) const;
  // The edge that stays in place, its bottom when inverted or stacking from the end, in content coordinates.
  float anchorEdgeOf(int index, float viewportLength) const;
  std::vector<int> itemsIn(float start, float end, float viewportLength) const;
  float emptyMinLength(float viewportLength) const;  // the viewport the padding and the other rows leave
  std::optional<Pinned> pinned(const std::vector<int>& sticky, float viewportStart, float viewportLength) const;
  // Relative to the item's origin: the gap after its row in content order, none after the last row.
  std::optional<ItemRect> separatorRect(int index, float listCrossLength) const;
  // Inverted or stacked from the end: the list keeps its end where it was.
  bool bottomAnchored() const { return config_.inverted || config_.stackFromEnd; }

 private:
  struct Item {
    int type;
    float length;
    bool measured;
  };
  struct Row {
    int first;
    int count;
    float length;
  };
  struct Mean {
    double sum = 0;
    int count = 0;
  };

  Item make(int index, const TypeOf& typeOf) const;
  bool fullSpan(int index) const;
  void rebuildRows();
  void updateRow(int row);
  bool gapAfter(int row) const;
  bool isData(int index) const;  // not the header, the footer or the empty row
  float lineAfter(int row) const;  // the separator after the row, 0 when it has none
  float rowStart(int row) const;  // from the start of item 0, inside the padding
  float slack(float viewportLength) const;  // the space a short list that stacks from the end leaves above its rows
  float startOf(int row, float viewportLength) const;  // in content coordinates
  float anchorEdge(int row, float viewportLength) const;
  int rowAt(float offset, float viewportLength) const;  // offset in content coordinates

  ListLayoutConfig config_;
  std::vector<int> fullSpan_;
  std::vector<Item> items_;
  std::unordered_map<int, Mean> means_;
  std::vector<Row> rows_;
  std::vector<int> rowOf_;
  mutable std::vector<float> starts_;  // rows_.size() + 1 prefix sums, valid up to dirtyFrom_
  mutable int dirtyFrom_ = 0;
};

}  // namespace flume
