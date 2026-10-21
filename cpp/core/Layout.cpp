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

namespace {

using PointSetter = void (*)(YGNodeRef, float);
using AutoSetter = void (*)(YGNodeRef);
using EdgePointSetter = void (*)(YGNodeRef, YGEdge, float);
using EdgeAutoSetter = void (*)(YGNodeRef, YGEdge);

void setDimension(YGNodeRef node, const Dimension& d, PointSetter points, PointSetter percent, AutoSetter automatic) {
  switch (d.unit) {
    case Dimension::Unit::Point: points(node, d.value); break;
    case Dimension::Unit::Percent: percent(node, d.value); break;
    case Dimension::Unit::Auto:
      if (automatic != nullptr) automatic(node);
      else points(node, YGUndefined);
      break;
    default: points(node, YGUndefined);
  }
}

void setEdge(YGNodeRef node, YGEdge edge, const Dimension& d, EdgePointSetter points, EdgePointSetter percent,
             EdgeAutoSetter automatic) {
  switch (d.unit) {
    case Dimension::Unit::Point: points(node, edge, d.value); break;
    case Dimension::Unit::Percent: percent(node, edge, d.value); break;
    case Dimension::Unit::Auto:
      if (automatic != nullptr) automatic(node, edge);
      else points(node, edge, YGUndefined);
      break;
    default: points(node, edge, YGUndefined);
  }
}

}  // namespace

void Layout::applyStyle(YGNodeRef node, const Style& s) {
  YGNodeStyleSetFlex(node, s.flex.value_or(YGUndefined));
  YGNodeStyleSetFlexGrow(node, s.flexGrow.value_or(YGUndefined));
  YGNodeStyleSetFlexShrink(node, s.flexShrink.value_or(YGUndefined));
  YGNodeStyleSetAspectRatio(node, s.aspectRatio.value_or(YGUndefined));
  setDimension(node, s.flexBasis, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent, YGNodeStyleSetFlexBasisAuto);
  setDimension(node, s.width, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
  setDimension(node, s.height, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
  setDimension(node, s.minWidth, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent, nullptr);
  setDimension(node, s.minHeight, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent, nullptr);
  setDimension(node, s.maxWidth, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent, nullptr);
  setDimension(node, s.maxHeight, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent, nullptr);
  YGNodeStyleSetGap(node, YGGutterAll, s.gap.value_or(YGUndefined));
  YGNodeStyleSetGap(node, YGGutterRow, s.rowGap.value_or(YGUndefined));
  YGNodeStyleSetGap(node, YGGutterColumn, s.columnGap.value_or(YGUndefined));
  YGNodeStyleSetPositionType(node, s.positionType.value_or(YGPositionTypeRelative));
  YGNodeStyleSetFlexDirection(node, s.flexDirection.value_or(YGFlexDirectionColumn));
  YGNodeStyleSetAlignItems(node, s.alignItems.value_or(YGAlignStretch));
  YGNodeStyleSetAlignSelf(node, s.alignSelf.value_or(YGAlignAuto));
  YGNodeStyleSetAlignContent(node, s.alignContent.value_or(YGAlignFlexStart));
  YGNodeStyleSetJustifyContent(node, s.justifyContent.value_or(YGJustifyFlexStart));
  YGNodeStyleSetFlexWrap(node, s.flexWrap.value_or(YGWrapNoWrap));
  YGNodeStyleSetOverflow(node, s.overflow.value_or(YGOverflowVisible));
  YGNodeStyleSetDisplay(node, s.display.value_or(YGDisplayFlex));
  YGNodeStyleSetDirection(node, s.direction.value_or(YGDirectionInherit));
  YGNodeStyleSetBorder(node, YGEdgeAll, s.borderWidth.value_or(YGUndefined));
  for (int i = 0; i <= YGEdgeAll; i++) {
    auto edge = static_cast<YGEdge>(i);
    setEdge(node, edge, s.padding[i], YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent, nullptr);
    setEdge(node, edge, s.margin[i], YGNodeStyleSetMargin, YGNodeStyleSetMarginPercent, YGNodeStyleSetMarginAuto);
    setEdge(node, edge, s.position[i], YGNodeStyleSetPosition, YGNodeStyleSetPositionPercent, YGNodeStyleSetPositionAuto);
  }
}

// Yoga serves a constraint it has measured before from its own cache, so stale text is marked dirty by hand.
void Layout::invalidateText(Mounted& node) {
  if (node.type == NodeType::Text && node.yoga != nullptr) {
    YGNodeMarkDirty(node.yoga);
    node.textDirty = true;
  }
  for (auto& child : node.children) invalidateText(*child);
}


static bool sameConstraint(float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

// A root laid out at another size re-measures every text first, or Yoga reuses a width it measured before.
void Layout::layout(Mounted& root, float width, float height) {
  if (!sameConstraint(root.lastLayoutSize[0], width) || !sameConstraint(root.lastLayoutSize[1], height)) {
    invalidateText(root);
    root.lastLayoutSize = {width, height};
  }
  YGNodeCalculateLayout(root.yoga, width, height, YGDirectionLTR);
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
  const Style& s = mounted->style;
  TextMeasureRequest request{mounted->text,  s.fontSize,       s.fontWeight,
                             maxWidth,       s.numberOfLines,  s.fontFamily,
                             s.italic,       s.lineHeight.value_or(0), s.letterSpacing.value_or(0),
                             static_cast<int>(s.textAlign), static_cast<int>(s.decoration), mounted->allowFontScaling};
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
