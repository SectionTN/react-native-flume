#include "core/Layout.h"

namespace flume {

constexpr float kUnboundedWidth = 1e6f;

Layout::Layout(TextMeasurer& measurer, float pointScale) : measurer_(measurer), config_(YGConfigNew()) {
  YGConfigSetPointScaleFactor(config_, pointScale);
}

Layout::~Layout() { YGConfigFree(config_); }

YGNodeRef Layout::createNode(Mounted& owner) {
  YGNodeRef node = YGNodeNewWithConfig(config_);
  owner.layout = this;
  YGNodeSetContext(node, &owner);
  if (owner.type == NodeType::Text) YGNodeSetMeasureFunc(node, measureText);
  return node;
}

void Layout::destroyNode(YGNodeRef node) { YGNodeFree(node); }

void Layout::markTextDirty(YGNodeRef node) { YGNodeMarkDirty(node); }

void Layout::applyStyle(YGNodeRef node, const Style& s) {
  YGNodeStyleSetFlex(node, s.flex.value_or(YGUndefined));
  YGNodeStyleSetFlexGrow(node, s.flexGrow.value_or(YGUndefined));
  YGNodeStyleSetFlexShrink(node, s.flexShrink.value_or(YGUndefined));
  if (s.flexBasis) YGNodeStyleSetFlexBasis(node, *s.flexBasis);
  else YGNodeStyleSetFlexBasisAuto(node);
  if (s.widthPercent) YGNodeStyleSetWidthPercent(node, *s.widthPercent);
  else if (s.width) YGNodeStyleSetWidth(node, *s.width);
  else YGNodeStyleSetWidthAuto(node);
  if (s.heightPercent) YGNodeStyleSetHeightPercent(node, *s.heightPercent);
  else if (s.height) YGNodeStyleSetHeight(node, *s.height);
  else YGNodeStyleSetHeightAuto(node);
  YGNodeStyleSetMinWidth(node, s.minWidth.value_or(YGUndefined));
  YGNodeStyleSetMinHeight(node, s.minHeight.value_or(YGUndefined));
  YGNodeStyleSetMaxWidth(node, s.maxWidth.value_or(YGUndefined));
  YGNodeStyleSetMaxHeight(node, s.maxHeight.value_or(YGUndefined));
  YGNodeStyleSetGap(node, YGGutterAll, s.gap.value_or(YGUndefined));
  YGNodeStyleSetGap(node, YGGutterRow, s.rowGap.value_or(YGUndefined));
  YGNodeStyleSetGap(node, YGGutterColumn, s.columnGap.value_or(YGUndefined));
  YGNodeStyleSetPositionType(node, s.position.value_or(YGPositionTypeRelative));
  YGNodeStyleSetPosition(node, YGEdgeTop, s.top.value_or(YGUndefined));
  YGNodeStyleSetPosition(node, YGEdgeLeft, s.left.value_or(YGUndefined));
  YGNodeStyleSetPosition(node, YGEdgeRight, s.right.value_or(YGUndefined));
  YGNodeStyleSetPosition(node, YGEdgeBottom, s.bottom.value_or(YGUndefined));
  YGNodeStyleSetFlexDirection(node, s.flexDirection.value_or(YGFlexDirectionColumn));
  YGNodeStyleSetAlignItems(node, s.alignItems.value_or(YGAlignStretch));
  YGNodeStyleSetAlignSelf(node, s.alignSelf.value_or(YGAlignAuto));
  YGNodeStyleSetJustifyContent(node, s.justifyContent.value_or(YGJustifyFlexStart));
  YGNodeStyleSetFlexWrap(node, s.flexWrap.value_or(YGWrapNoWrap));
  YGNodeStyleSetOverflow(node, s.overflow.value_or(YGOverflowVisible));
  for (int edge = 0; edge <= YGEdgeAll; edge++) {
    YGNodeStyleSetPadding(node, static_cast<YGEdge>(edge), s.padding[edge].value_or(YGUndefined));
    YGNodeStyleSetMargin(node, static_cast<YGEdge>(edge), s.margin[edge].value_or(YGUndefined));
  }
}

void Layout::layout(Mounted& root, float width) {
  YGNodeCalculateLayout(root.yoga, width, YGUndefined, YGDirectionLTR);
}

void Layout::readFrames(Mounted& root, OpBatch& out) {
  out.rootWidth = YGNodeLayoutGetWidth(root.yoga);
  out.rootHeight = YGNodeLayoutGetHeight(root.yoga);
  readFramesOf(root, out);
}

void Layout::readFramesOf(Mounted& node, OpBatch& out) {
  if (YGNodeGetHasNewLayout(node.yoga)) {
    YGNodeSetHasNewLayout(node.yoga, false);
    std::array<float, 4> frame{YGNodeLayoutGetLeft(node.yoga), YGNodeLayoutGetTop(node.yoga),
                               YGNodeLayoutGetWidth(node.yoga), YGNodeLayoutGetHeight(node.yoga)};
    if (frame != node.lastFrame) {
      node.lastFrame = frame;
      out.frame(node.viewId, frame[0], frame[1], frame[2], frame[3]);
    }
  }
  for (auto& child : node.children) readFramesOf(*child, out);
}

YGSize Layout::measureText(YGNodeConstRef node, float width, YGMeasureMode widthMode, float, YGMeasureMode) {
  auto* mounted = static_cast<Mounted*>(YGNodeGetContext(node));
  Layout* self = mounted->layout;
  float maxWidth = widthMode == YGMeasureModeUndefined ? kUnboundedWidth : width;
  mounted->measuredMaxWidth = maxWidth;
  TextMeasureRequest request{mounted->text, mounted->style.fontSize, mounted->style.fontWeight, maxWidth,
                             mounted->style.numberOfLines};
  self->measureCalls++;
  if (const TextMetrics* hit = self->cache_.find(request)) {
    self->measureHits++;
    return {hit->width, hit->height};
  }
  TextMetrics metrics = self->measurer_.measure(request);
  self->cache_.put(request, metrics);
  return {metrics.width, metrics.height};
}

}  // namespace flume
