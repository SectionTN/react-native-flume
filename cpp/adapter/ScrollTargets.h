#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "adapter/ListLayout.h"

namespace flume {

enum class VisualEdge { Top, Bottom };  // the left and right edges of a horizontal list

constexpr int kJumpMarginRows = 2;
constexpr int kMaxJumpRows = 48;
constexpr int64_t kJumpWaitMs = 1000;  // how long a jump waits for its React rows to render

std::optional<int> clampIndex(int index, int count);
// Where the viewport starts so the item sits as FlatList's scrollToIndex places it, viewPosition in content order.
float alignedViewportStart(float itemStart, float itemLength, float viewportLength, float viewPosition, float viewOffset,
                           bool inverted);
// UIScrollView's range, from the start inset to the content end past the end inset.
float clampOffset(float offset, float contentLength, float boundsLength, float insetStart, float insetEnd);
// After the viewport's end moves (a resize or an end inset), a list anchored at its end keeps that end in place.
float offsetAfterViewportChange(bool keepsEnd, float offset, float oldEnd, float newEnd, float low, float high);
// The rows a landing shows, two past each side; rowLength may measure the row it is asked for.
IndexRange landingRows(int targetRow, float viewPosition, float viewportLength, int rowCount,
                       const std::function<float(int row)>& rowLength);
// The rows a batch can bring into view, one viewport past each edge of the visible rows; rowLength may measure the row.
IndexRange batchRows(IndexRange visible, float viewportLength, int rowCount,
                     const std::function<float(int row)>& rowLength);
// An animated scroll's rows from where it starts to its landing; past the cap, the rows nearest the landing.
IndexRange glideWindow(IndexRange from, IndexRange landing);
// Past two viewports an animated scroll first jumps to one viewport before its target.
std::optional<float> preJumpOffset(float current, float target, float viewportLength);

struct Landing {
  int index;
  float viewPosition;
  bool contentEnd;
  bool operator==(const Landing&) const = default;
};

// FlatList's initialScrollIndex: none while negative or empty; past the data, the content end.
std::optional<Landing> openingLanding(int initialIndex, int count);

}  // namespace flume
