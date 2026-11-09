#include "ScrollTargets.h"

#include <algorithm>
#include <cmath>

namespace flume {

std::optional<int> clampIndex(int index, int count) {
  if (count <= 0) return std::nullopt;
  return std::clamp(index, 0, count - 1);
}

float alignedViewportStart(float itemStart, float itemLength, float viewportLength, float viewPosition, float viewOffset,
                           bool inverted) {
  const float before = viewPosition * (viewportLength - itemLength) + viewOffset;
  if (!inverted) return itemStart - before;
  return itemStart + itemLength + before - viewportLength;
}

float clampOffset(float offset, float contentLength, float boundsLength, float insetStart, float insetEnd) {
  const float low = -insetStart;
  return std::clamp(offset, low, std::max(low, contentLength - boundsLength + insetEnd));
}

IndexRange landingRows(int targetRow, float viewPosition, float viewportLength, int rowCount,
                       const std::function<float(int row)>& rowLength) {
  if (rowCount <= 0) return {0, -1};
  const int target = std::clamp(targetRow, 0, rowCount - 1);
  const float targetLength = rowLength(target);
  const float before = std::max(0.0f, viewPosition * (viewportLength - targetLength));
  int first = target;
  for (float covered = 0; first > 0 && covered < before;) covered += rowLength(--first);
  const float after = viewportLength - before - targetLength;
  int last = target;
  for (float covered = 0; last < rowCount - 1 && covered < after;) covered += rowLength(++last);
  return {std::max(0, first - kJumpMarginRows), std::min(rowCount - 1, last + kJumpMarginRows)};
}

IndexRange glideWindow(IndexRange from, IndexRange landing) {
  const IndexRange span{std::min(from.first, landing.first), std::max(from.last, landing.last)};
  if (span.last - span.first + 1 <= kMaxJumpRows) return span;
  if (from.first < landing.first) return {span.last - kMaxJumpRows + 1, span.last};
  return {span.first, span.first + kMaxJumpRows - 1};
}

std::optional<float> preJumpOffset(float current, float target, float viewportLength) {
  if (std::abs(target - current) <= 2 * viewportLength) return std::nullopt;
  return current < target ? target - viewportLength : target + viewportLength;
}

std::optional<Landing> openingLanding(int initialIndex, int count) {
  if (initialIndex < 0 || count <= 0) return std::nullopt;
  if (initialIndex >= count) return Landing{count - 1, 1, true};
  return Landing{initialIndex, 0, false};
}

}  // namespace flume
