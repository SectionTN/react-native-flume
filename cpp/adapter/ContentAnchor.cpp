#include "ContentAnchor.h"

namespace flume {

AnchorDecision decideAnchor(const AnchorConfig& config, const AnchorState& before, bool insertedAtVisualTop,
                            bool insertedAtVisualBottom) {
  if (config.disabled) return {AnchorOutcome::KeepOffset};
  const double bottom = config.autoscrollToBottomThreshold;
  if (bottom >= 0 && insertedAtVisualBottom && before.distanceToVisualBottom <= bottom * before.viewportLength) {
    return {AnchorOutcome::ScrollToEdge, VisualEdge::Bottom, config.animateAutoScrollToBottom};
  }
  // React Native's iOS ScrollView always animates to the top.
  const double top = config.autoscrollToTopThreshold;
  if (top >= 0 && insertedAtVisualTop && before.distanceToVisualTop <= top * before.viewportLength) {
    return {AnchorOutcome::ScrollToEdge, VisualEdge::Top, true};
  }
  return {AnchorOutcome::KeepAnchor};
}

std::optional<int> mapIndex(const ListOps& ops, int oldIndex) {
  int index = oldIndex;
  for (const auto& range : ops.removed) {
    if (index >= range.index + range.count) {
      index -= range.count;
    } else if (index >= range.index) {
      return std::nullopt;
    }
  }
  for (const auto& range : ops.inserted) {
    if (index >= range.index) index += range.count;
  }
  for (const auto& [from, to] : ops.moved) {
    if (index == from) {
      index = to;
      continue;
    }
    if (index > from) index--;
    if (index >= to) index++;
  }
  return index;
}

EdgeInserts insertedAtEdges(const ListOps& ops, int newCount, bool inverted) {
  bool atStart = false;
  bool atEnd = false;
  for (const auto& range : ops.inserted) {
    if (range.index == 0) atStart = true;
    if (range.index + range.count == newCount) atEnd = true;
  }
  return inverted ? EdgeInserts{atEnd, atStart} : EdgeInserts{atStart, atEnd};
}

}  // namespace flume
