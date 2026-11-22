#include "adapter/ListLayout.h"

#include <algorithm>

namespace flume {

namespace {

// A row edge within half a point of the viewport's edge counts as on it.
constexpr float kEdgeSlop = 0.5f;

}  // namespace

void ListLayout::configure(const ListLayoutConfig& config, std::vector<int> fullSpan) {
  config_ = config;
  config_.lanes = std::max(1, config.lanes);
  fullSpan_ = std::move(fullSpan);
  rebuildRows();
}

ListLayout::Item ListLayout::make(int index, const TypeOf& typeOf) const {
  const int type = typeOf(index);
  return Item{type, estimate(type), false};
}

void ListLayout::reset(int count, const TypeOf& typeOf) {
  items_.clear();
  items_.reserve(std::max(0, count));
  for (int i = 0; i < count; i++) items_.push_back(make(i, typeOf));
  rebuildRows();
}

std::vector<int> ListLayout::apply(const ListOps& ops, int count, const TypeOf& typeOf, std::vector<int> fullSpan) {
  if (ops.reset) {
    reset(count, typeOf);
    return {};
  }
  fullSpan_ = std::move(fullSpan);
  for (const auto& range : ops.removed) {
    if (range.index < 0 || range.index + range.count > this->count()) continue;
    items_.erase(items_.begin() + range.index, items_.begin() + range.index + range.count);
  }
  for (const auto& range : ops.inserted) {
    if (range.index < 0 || range.index > this->count()) continue;
    std::vector<Item> fresh;
    for (int i = 0; i < range.count; i++) fresh.push_back(make(range.index + i, typeOf));
    items_.insert(items_.begin() + range.index, fresh.begin(), fresh.end());
  }
  for (const auto& [from, to] : ops.moved) {
    if (from < 0 || from >= this->count() || to < 0 || to >= this->count()) continue;
    Item item = items_[from];
    items_.erase(items_.begin() + from);
    items_.insert(items_.begin() + to, item);
  }
  if (this->count() != count) {
    reset(count, typeOf);
    return {};
  }
  std::vector<int> retyped;
  for (int index : ops.changed) {
    if (index < 0 || index >= count) continue;
    Item& item = items_[index];
    const int type = typeOf(index);
    if (type == item.type) continue;
    // Its measured length leaves the old type's mean; the new cell measures it again.
    if (item.measured) {
      Mean& mean = means_[item.type];
      mean.sum -= item.length;
      mean.count--;
      item.measured = false;
    }
    item.type = type;
    retyped.push_back(index);
  }
  rebuildRows();
  return retyped;
}

LengthChange ListLayout::setLength(int index, float length, int anchor, float viewportLength) {
  if (index < 0 || index >= count()) return LengthChange{0, 0};
  const bool hasAnchor = anchor >= 0 && anchor < count();
  const float before = hasAnchor ? anchorEdge(rowOf_[anchor], viewportLength) : 0;
  const float contentBefore = contentLength(viewportLength);
  Item& item = items_[index];
  Mean& mean = means_[item.type];
  if (item.measured) {
    mean.sum -= item.length;
  } else {
    mean.count++;
  }
  mean.sum += length;
  item.length = length;
  item.measured = true;
  updateRow(rowOf_[index]);
  const float moved = hasAnchor ? anchorEdge(rowOf_[anchor], viewportLength) - before : 0;
  return LengthChange{moved, contentLength(viewportLength) - contentBefore};
}

void ListLayout::invalidateLengths() {
  for (auto& item : items_) item.measured = false;
  means_.clear();
}

bool ListLayout::measured(int index) const { return index >= 0 && index < count() && items_[index].measured; }

IndexRange ListLayout::itemsOfRows(int firstRow, int lastRow) const {
  if (firstRow > lastRow) return {0, -1};
  return {rows_[firstRow].first, rows_[lastRow].first + rows_[lastRow].count - 1};
}

float ListLayout::estimate(int type) const {
  if (config_.estimatedItemLength >= 0) return config_.estimatedItemLength;
  auto it = means_.find(type);
  if (it == means_.end() || it->second.count == 0) return config_.estimateFloor;
  return std::max(config_.estimateFloor, static_cast<float>(it->second.sum / it->second.count));
}

float ListLayout::contentLength(float viewportLength) const {
  const float length = config_.paddingStart + rowStart(static_cast<int>(rows_.size())) + config_.paddingEnd;
  return bottomAnchored() ? std::max(viewportLength, length) : length;
}

float ListLayout::crossLength(int index, float listCrossLength) const {
  if (!fullSpan(index)) return laneLength(listCrossLength);
  const float insets = isData(index) ? config_.laneInsetStart + config_.laneInsetEnd : 0;
  return std::max(0.f, listCrossLength - config_.paddingCrossStart - config_.paddingCrossEnd - insets);
}

float ListLayout::laneLength(float listCrossLength) const {
  const float row = listCrossLength - config_.paddingCrossStart - config_.paddingCrossEnd - config_.laneInsetStart -
                    config_.laneInsetEnd - config_.crossGap * static_cast<float>(config_.lanes - 1);
  return std::max(0.f, row) / config_.lanes;
}

ItemRect ListLayout::rectOf(int index, float listCrossLength, float viewportLength) const {
  if (index < 0 || index >= count()) return ItemRect{0, 0, 0, 0};
  const int row = rowOf_[index];
  const float main = startOf(row, viewportLength);
  const float cross = crossLength(index, listCrossLength);
  const float lane = static_cast<float>(index - rows_[row].first);
  const float crossAt =
      config_.paddingCrossStart + (isData(index) ? config_.laneInsetStart : 0) + (cross + config_.crossGap) * lane;
  const float length = rows_[row].length;  // a grid row's cells all take its longest
  if (config_.horizontal) return ItemRect{main, crossAt, length, cross};
  return ItemRect{crossAt, main, cross, length};
}

int ListLayout::indexAt(float offset, float viewportLength) const {
  if (rows_.empty()) return -1;
  return rows_[rowAt(offset, viewportLength)].first;
}

// The first row that starts in the viewport, or the last that ends in it when bottom-anchored; a cut row grows unseen.
int ListLayout::anchorAt(float viewportStart, float viewportLength) const {
  if (rows_.empty()) return -1;
  const int last = static_cast<int>(rows_.size()) - 1;
  if (bottomAnchored()) {
    const float end = viewportStart + viewportLength;
    const int row = rowAt(end - kEdgeSlop, viewportLength);
    if (anchorEdge(row, viewportLength) <= end + kEdgeSlop) return rows_[row].first;
    // The row above it: the next one inverted, the previous one upright.
    return rows_[config_.inverted ? std::min(row + 1, last) : std::max(row - 1, 0)].first;
  }
  const int row = rowAt(viewportStart, viewportLength);
  const bool cut = startOf(row, viewportLength) < viewportStart - kEdgeSlop;
  return rows_[cut && row < last ? row + 1 : row].first;
}

float ListLayout::anchorEdgeOf(int index, float viewportLength) const {
  if (index < 0 || index >= count()) return 0;
  return anchorEdge(rowOf_[index], viewportLength);
}

std::vector<int> ListLayout::itemsIn(float start, float end, float viewportLength) const {
  std::vector<int> out;
  if (rows_.empty()) return out;
  const int a = rowAt(start, viewportLength);
  const int b = rowAt(end, viewportLength);
  for (int row = std::min(a, b); row <= std::max(a, b); row++) {
    // An inverted row's separator sits above it; either way a row whose separator shows is listed.
    const float rowFrom = startOf(row, viewportLength) - (config_.inverted ? rowStride(row) - rows_[row].length : 0);
    if (rowFrom >= end || rowFrom + rowStride(row) <= start) continue;
    for (int i = 0; i < rows_[row].count; i++) out.push_back(rows_[row].first + i);
  }
  return out;
}

std::optional<Pinned> ListLayout::pinned(const std::vector<int>& sticky, float viewportStart,
                                         float viewportLength) const {
  const int top = indexAt(viewportStart, viewportLength);
  if (top < 0) return std::nullopt;
  std::optional<int> index;
  std::optional<int> pusher;
  for (int s : sticky) {
    if (s < 0 || s >= count()) continue;
    if (config_.inverted ? s >= top && (!index || s < *index) : s <= top && (!index || s > *index)) index = s;
  }
  if (!index || startOf(rowOf_[*index], viewportLength) > viewportStart) return std::nullopt;
  for (int s : sticky) {
    if (s < 0 || s >= count()) continue;
    if (config_.inverted ? s < *index && (!pusher || s > *pusher) : s > *index && (!pusher || s < *pusher)) pusher = s;
  }
  float start = viewportStart;
  if (pusher) {
    // The pinned row ends at the pusher with the line between them, which is its own upright and the pusher's inverted.
    const float line = lineAfter(rowOf_[config_.inverted ? *pusher : *index]);
    start = std::min(start, startOf(rowOf_[*pusher], viewportLength) - line - items_[*index].length);
  }
  return Pinned{*index, start};
}

std::optional<ItemRect> ListLayout::separatorRect(int index, float listCrossLength) const {
  if (index < 0 || index >= count() || config_.separator <= 0) return std::nullopt;
  const int row = rowOf_[index];
  if (!gapAfter(row)) return std::nullopt;
  const ItemRect item = rectOf(index, listCrossLength, 0);
  const float crossAt = config_.horizontal ? item.y : item.x;
  // Each cell draws its share of the padded width, so the row's line runs across the column gaps and lane insets.
  const float padded = std::max(0.f, listCrossLength - config_.paddingCrossStart - config_.paddingCrossEnd);
  const bool alone = fullSpan(index);
  const float slot = alone ? padded : padded / config_.lanes;
  const float slotAt = config_.paddingCrossStart + slot * static_cast<float>(alone ? 0 : index - rows_[row].first);
  const float from = std::max(slotAt, config_.paddingCrossStart + config_.separatorInsetStart) - crossAt;
  const float to =
      std::min(slotAt + slot, listCrossLength - config_.paddingCrossEnd - config_.separatorInsetEnd) - crossAt;
  if (to <= from) return std::nullopt;
  const float main = config_.inverted ? -config_.separator : rows_[row].length;
  if (config_.horizontal) return ItemRect{main, from, config_.separator, to - from};
  return ItemRect{from, main, to - from, config_.separator};
}

float ListLayout::emptyMinLength(float viewportLength) const {
  if (config_.emptyIndex < 0 || config_.emptyIndex >= count()) return 0;
  const float used = config_.paddingStart + config_.paddingEnd + rowStart(static_cast<int>(rows_.size())) -
                     rows_[rowOf_[config_.emptyIndex]].length;
  return std::max(0.f, viewportLength - used);
}

bool ListLayout::fullSpan(int index) const { return std::binary_search(fullSpan_.begin(), fullSpan_.end(), index); }

void ListLayout::rebuildRows() {
  rows_.clear();
  rowOf_.assign(items_.size(), 0);
  for (int i = 0; i < count(); i++) {
    const bool alone = fullSpan(i);
    if (alone || rows_.empty() || rows_.back().count == config_.lanes || fullSpan(rows_.back().first)) {
      rows_.push_back(Row{i, 0, 0});
    }
    rows_.back().count++;
    rowOf_[i] = static_cast<int>(rows_.size()) - 1;
  }
  for (int row = 0; row < static_cast<int>(rows_.size()); row++) {
    for (int i = 0; i < rows_[row].count; i++) {
      rows_[row].length = std::max(rows_[row].length, items_[rows_[row].first + i].length);
    }
  }
  starts_.assign(rows_.size() + 1, 0);
  dirtyFrom_ = 0;
}

void ListLayout::updateRow(int row) {
  float length = 0;
  for (int i = 0; i < rows_[row].count; i++) length = std::max(length, items_[rows_[row].first + i].length);
  if (length == rows_[row].length) return;
  rows_[row].length = length;
  dirtyFrom_ = std::min(dirtyFrom_, row);
}

// The header, the footer and the last data row carry no gap.
bool ListLayout::gapAfter(int row) const {
  return rows_[row].first >= config_.dataStart && rows_[row].first + rows_[row].count < count() - config_.afterData;
}

bool ListLayout::isData(int index) const {
  return index >= config_.dataStart && index < count() - config_.afterData && index != config_.emptyIndex;
}

float ListLayout::lineAfter(int row) const { return gapAfter(row) ? config_.separator : 0; }

// Along the scroll axis: the row, its line, the grid row gap, then the main gap before the next row.
float ListLayout::rowStride(int row) const {
  const float data = gapAfter(row) ? config_.separator + config_.gridRowGap : 0;
  const float main = row + 1 < static_cast<int>(rows_.size()) ? config_.mainGap : 0;
  return rows_[row].length + data + main;
}

float ListLayout::rowStart(int row) const {
  for (; dirtyFrom_ < row; dirtyFrom_++) starts_[dirtyFrom_ + 1] = starts_[dirtyFrom_] + rowStride(dirtyFrom_);
  return starts_[row];
}

float ListLayout::slack(float viewportLength) const {
  if (!config_.stackFromEnd) return 0;
  const float length = config_.paddingStart + rowStart(static_cast<int>(rows_.size())) + config_.paddingEnd;
  return std::max(0.f, viewportLength - length);
}

float ListLayout::startOf(int row, float viewportLength) const {
  if (!config_.inverted) return config_.paddingStart + slack(viewportLength) + rowStart(row);
  return contentLength(viewportLength) - config_.paddingEnd - rowStart(row) - rows_[row].length;
}

float ListLayout::anchorEdge(int row, float viewportLength) const {
  const float start = startOf(row, viewportLength);
  return bottomAnchored() ? start + rows_[row].length : start;
}

int ListLayout::rowAt(float offset, float viewportLength) const {
  const int rows = static_cast<int>(rows_.size());
  rowStart(rows);
  if (!config_.inverted) {
    auto it = std::upper_bound(starts_.begin(), starts_.begin() + rows,
                               offset - config_.paddingStart - slack(viewportLength));
    return std::max(0, static_cast<int>(it - starts_.begin()) - 1);
  }
  // Inverted rows run up from the bottom: the row whose end, measured from item 0's bottom, reaches past the offset.
  const float fromBottom = contentLength(viewportLength) - config_.paddingEnd - offset;
  auto it = std::lower_bound(starts_.begin() + 1, starts_.begin() + rows + 1, fromBottom);
  return std::min(rows - 1, static_cast<int>(it - (starts_.begin() + 1)));
}

}  // namespace flume
