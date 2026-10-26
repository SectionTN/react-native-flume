#pragma once

#include <functional>
#include <unordered_map>
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
};

struct ItemRect {
  float x, y, width, height;
};

// Item positions along the scroll axis from measured lengths and per-type estimates; units are points.
class ListLayout {
 public:
  using TypeOf = std::function<int(int index)>;

  void configure(const ListLayoutConfig& config, std::vector<int> fullSpan);  // fullSpan ascending
  void reset(int count, const TypeOf& typeOf);
  void apply(const ListOps& ops, int count, const TypeOf& typeOf);
  float setLength(int index, float length, int anchor);  // how far the anchor moved along the scroll axis
  void invalidateLengths();
  int count() const { return static_cast<int>(items_.size()); }
  bool measured(int index) const;
  float estimate(int type) const;
  float contentLength(float viewportLength) const;
  float crossLength(int index, float listCrossLength) const;  // the size a cell binds at across the scroll axis
  ItemRect rectOf(int index, float listCrossLength, float viewportLength) const;
  int indexAt(float offset, float viewportLength) const;
  std::vector<int> itemsIn(float start, float end, float viewportLength) const;

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
  float rowStart(int row) const;
  int rowAt(float offset) const;  // offset along the scroll axis inside the padding

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
