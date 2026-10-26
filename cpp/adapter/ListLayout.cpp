#include "adapter/ListLayout.h"

#include <algorithm>

namespace flume {

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

void ListLayout::apply(const ListOps& ops, int count, const TypeOf& typeOf) {
  if (ops.reset) {
    reset(count, typeOf);
    return;
  }
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
    return;
  }
  for (int index : ops.changed) {
    if (index >= 0 && index < count) items_[index].type = typeOf(index);
  }
  rebuildRows();
}

float ListLayout::setLength(int index, float length, int anchor) {
  if (index < 0 || index >= count()) return 0;
  const bool hasAnchor = anchor >= 0 && anchor < count();
  const float before = hasAnchor ? rowStart(rowOf_[anchor]) : 0;
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
  return hasAnchor ? rowStart(rowOf_[anchor]) - before : 0;
}

void ListLayout::invalidateLengths() {
  for (auto& item : items_) item.measured = false;
  means_.clear();
}

bool ListLayout::measured(int index) const { return index >= 0 && index < count() && items_[index].measured; }

float ListLayout::estimate(int type) const {
  if (config_.estimatedItemLength >= 0) return config_.estimatedItemLength;
  auto it = means_.find(type);
  if (it == means_.end() || it->second.count == 0) return config_.estimateFloor;
  return std::max(config_.estimateFloor, static_cast<float>(it->second.sum / it->second.count));
}

float ListLayout::contentLength(float) const {
  return config_.paddingStart + rowStart(static_cast<int>(rows_.size())) + config_.paddingEnd;
}

float ListLayout::crossLength(int index, float listCrossLength) const {
  const float available = std::max(0.f, listCrossLength - config_.paddingCrossStart - config_.paddingCrossEnd);
  return fullSpan(index) ? available : available / config_.lanes;
}

ItemRect ListLayout::rectOf(int index, float listCrossLength, float) const {
  if (index < 0 || index >= count()) return ItemRect{0, 0, 0, 0};
  const int row = rowOf_[index];
  const float main = config_.paddingStart + rowStart(row);
  const float cross = crossLength(index, listCrossLength);
  const float crossAt = config_.paddingCrossStart + cross * static_cast<float>(index - rows_[row].first);
  const float length = items_[index].length;
  if (config_.horizontal) return ItemRect{main, crossAt, length, cross};
  return ItemRect{crossAt, main, cross, length};
}

int ListLayout::indexAt(float offset, float) const {
  if (rows_.empty()) return -1;
  return rows_[rowAt(offset - config_.paddingStart)].first;
}

std::vector<int> ListLayout::itemsIn(float start, float end, float) const {
  std::vector<int> out;
  if (rows_.empty()) return out;
  const float from = start - config_.paddingStart;
  const float to = end - config_.paddingStart;
  for (int row = rowAt(from); row < static_cast<int>(rows_.size()) && rowStart(row) < to; row++) {
    if (rowStart(row) + rows_[row].length <= from) continue;
    for (int i = 0; i < rows_[row].count; i++) out.push_back(rows_[row].first + i);
  }
  return out;
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

float ListLayout::rowStart(int row) const {
  for (; dirtyFrom_ < row; dirtyFrom_++) starts_[dirtyFrom_ + 1] = starts_[dirtyFrom_] + rows_[dirtyFrom_].length;
  return starts_[row];
}

int ListLayout::rowAt(float offset) const {
  const int rows = static_cast<int>(rows_.size());
  rowStart(rows);
  auto it = std::upper_bound(starts_.begin(), starts_.begin() + rows, offset);
  return std::max(0, static_cast<int>(it - starts_.begin()) - 1);
}

}  // namespace flume
