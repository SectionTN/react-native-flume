#include "FlumeShadowNodes.h"

namespace facebook::react {

extern const char FlumeViewComponentName[] = "FlumeView";
extern const char FlumeSlotViewComponentName[] = "FlumeSlotView";

namespace {

// An offset, not a point: w = 0 keeps the transform's translation, already applied to the frame, out of it.
Point transformed(const Transform &transform, Float x, Float y) {
  const auto result = transform * Vector{.x = x, .y = y, .z = 0.0f, .w = 0.0f};
  return {.x = result.x, .y = result.y};
}

} // namespace

Point FlumeViewShadowNode::getContentOriginOffset(bool includeTransform) const {
  const auto offset = getStateData().contentOffset;
  return transformed(includeTransform ? getTransform() : Transform::Identity(), -offset.x, -offset.y);
}

Point FlumeSlotViewShadowNode::getContentOriginOffset(bool includeTransform) const {
  const auto &state = getStateData();
  if (!state.placed) {
    return {};
  }
  const auto own = getLayoutMetrics().frame.origin;
  return transformed(
      includeTransform ? getTransform() : Transform::Identity(), state.origin.x - own.x, state.origin.y - own.y);
}

} // namespace facebook::react
