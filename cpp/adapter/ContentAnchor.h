#pragma once

#include <optional>

#include "adapter/ScrollTargets.h"
#include "core/KeyDiff.h"

namespace flume {

// maintainVisibleContentPosition, sent complete by the wrapper.
struct AnchorConfig {
  bool disabled = false;
  double autoscrollToTopThreshold = -1;  // in points, negative for none
  double autoscrollToBottomThreshold = -1;  // in viewports, negative for none
  bool animateAutoScrollToBottom = true;
};

struct AnchorState {
  float distanceToVisualTop;
  float distanceToVisualBottom;
  float viewportLength;
  bool positionPending = false;  // an opening or kept position still waits for the next layout pass
};

enum class AnchorOutcome { KeepOffset, KeepAnchor, ScrollToEdge };

struct AnchorDecision {
  AnchorOutcome outcome;
  VisualEdge edge = VisualEdge::Top;
  bool animated = false;
};

// disabled or a pending position keeps the offset; a near visual edge insert scrolls there, the bottom first; else the anchor stays.
AnchorDecision decideAnchor(const AnchorConfig& config, const AnchorState& before, bool insertedAtVisualTop,
                            bool insertedAtVisualBottom);
// Where an old index is after the ops, replayed as ListLayout::apply replays them; nullopt when removed.
std::optional<int> mapIndex(const ListOps& ops, int oldIndex);

struct EdgeInserts {
  bool visualTop;
  bool visualBottom;
};

EdgeInserts insertedAtEdges(const ListOps& ops, int newCount, bool inverted);

}  // namespace flume
