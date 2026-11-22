#include "core/SeparatorRule.h"

#include <algorithm>
#include <utility>

namespace flume {

int lastRowStart(int count, int lanes, const std::vector<int>& sortedFullSpans) {
  if (count <= 0) return 0;
  int last = count - 1;
  lanes = std::max(lanes, 1);
  // Only the last full span at or before the last index decides where the last run of lanes starts.
  auto after = std::upper_bound(sortedFullSpans.begin(), sortedFullSpans.end(), last);
  int runStart = 0;
  if (after != sortedFullSpans.begin()) {
    int span = *(after - 1);
    if (span == last) return last;
    runStart = span + 1;
  }
  return runStart + (last - runStart) / lanes * lanes;
}

std::unique_ptr<Node> composeRow(std::unique_ptr<Node> row, std::unique_ptr<Node> separator, bool horizontal,
                                 bool inverted) {
  auto root = std::make_unique<Node>();
  root->style.flexDirection = horizontal ? YGFlexDirectionRow : YGFlexDirectionColumn;
  if (row->type == NodeType::Pressable) {
    root->type = NodeType::Pressable;
    row->type = NodeType::View;
  }
  root->pressId = std::move(row->pressId);
  root->pressedOpacity = row->pressedOpacity;
  root->testID = std::move(row->testID);
  root->accessibilityLabel = std::move(row->accessibilityLabel);
  root->accessible = row->accessible;
  row->pressId.clear();
  row->testID.clear();
  row->accessibilityLabel.clear();
  row->accessible.reset();
  clearParentLayout(row->style);
  // Stretched to a grid row's length, an auto-length row grows like a FlatList row child and the line stays at the end.
  root->style.justifyContent = YGJustifySpaceBetween;
  const Dimension& length = horizontal ? row->style.width : row->style.height;
  const bool own = length.unit == Dimension::Unit::Point || length.unit == Dimension::Unit::Percent;
  if (!own && !row->style.aspectRatio) row->style.flexGrow = 1;
  row->key = "r";
  root->children.push_back(std::move(row));
  if (separator != nullptr) {
    separator->key = "s";
    separator->separator = true;
    root->children.insert(inverted ? root->children.begin() : root->children.end(), std::move(separator));
  }
  return root;
}

}  // namespace flume
