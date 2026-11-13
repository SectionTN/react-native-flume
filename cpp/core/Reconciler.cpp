#include "core/Reconciler.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace flume {
namespace {

ViewKind kindOf(NodeType type) {
  switch (type) {
    case NodeType::Text: return ViewKind::Text;
    case NodeType::Image: return ViewKind::Image;
    case NodeType::Pressable: return ViewKind::Pressable;
    default: return ViewKind::View;
  }
}

bool isContainer(NodeType type) { return type == NodeType::View || type == NodeType::Pressable; }

bool anyRadius(const Style& s) {
  return std::any_of(s.borderRadii.begin(), s.borderRadii.end(), [](const auto& r) { return r.has_value(); });
}

bool hasVisual(const Style& s) {
  return s.backgroundColor.has_value() || s.opacity.has_value() || anyRadius(s) || s.borderWidth.has_value() ||
         s.borderColor.has_value() || s.elevation.has_value() || s.shadowColor.has_value() ||
         s.shadowOpacity.has_value() || s.shadowRadius.has_value() || s.shadowOffset.has_value();
}

bool visualChanged(const Style& a, const Style& b) {
  return a.backgroundColor != b.backgroundColor || a.opacity != b.opacity || a.borderRadii != b.borderRadii ||
         a.borderWidth != b.borderWidth || a.borderColor != b.borderColor || a.elevation != b.elevation ||
         a.shadowColor != b.shadowColor || a.shadowOpacity != b.shadowOpacity || a.shadowRadius != b.shadowRadius ||
         a.shadowOffset != b.shadowOffset;
}

bool textStyleChanged(const Style& a, const Style& b) {
  return a.fontSize != b.fontSize || a.fontWeight != b.fontWeight || a.color != b.color ||
         a.numberOfLines != b.numberOfLines || a.fontFamily != b.fontFamily || a.italic != b.italic ||
         a.lineHeight != b.lineHeight || a.letterSpacing != b.letterSpacing || a.textAlign != b.textAlign ||
         a.decoration != b.decoration;
}

bool imageStyleChanged(const Style& a, const Style& b) {
  return a.width != b.width || a.height != b.height || a.borderRadii != b.borderRadii;
}

bool imageChanged(const Mounted& m, const Node& n) {
  return m.imageUri != n.imageUri || m.resizeMode != n.resizeMode || m.tintColor != n.tintColor ||
         m.imageHeaders != n.imageHeaders;
}

bool pressableChanged(const Mounted& m, const Node& n) {
  return m.pressId != n.pressId || m.pressedOpacity != n.pressedOpacity;
}

bool accessibilityChanged(const Mounted& m, const Node& n) {
  return m.accessibilityLabel != n.accessibilityLabel || m.testID != n.testID || m.accessible != n.accessible;
}

// A pressable is always exposed as a button; other nodes only when something is set.
bool hasAccessibility(const Mounted& m) {
  return m.type == NodeType::Pressable || !m.accessibilityLabel.empty() || !m.testID.empty() || m.accessible.has_value();
}

void copyProps(Mounted& mounted, Node& fresh) {
  mounted.style = fresh.style;
  mounted.text = std::move(fresh.text);
  mounted.allowFontScaling = fresh.allowFontScaling;
  mounted.imageUri = std::move(fresh.imageUri);
  mounted.resizeMode = fresh.resizeMode;
  mounted.tintColor = fresh.tintColor;
  mounted.imageHeaders = std::move(fresh.imageHeaders);
  mounted.pressId = std::move(fresh.pressId);
  mounted.pressedOpacity = fresh.pressedOpacity;
  mounted.key = std::move(fresh.key);
  mounted.testID = std::move(fresh.testID);
  mounted.accessibilityLabel = std::move(fresh.accessibilityLabel);
  mounted.accessible = fresh.accessible;
}

void appendTexts(const Mounted& node, std::string& out) {
  if (node.type == NodeType::Text && !node.text.empty()) {
    if (!out.empty()) out += ' ';
    out += node.text;
  }
  for (const auto& child : node.children) appendTexts(*child, out);
}

}  // namespace

void Reconciler::reconcile(Holder& holder, std::unique_ptr<Node> fresh, OpBatch& out) {
  if (fresh == nullptr) {
    if (holder.root != nullptr) {
      unmount(*holder.root, out);
      holder.root.reset();
    }
    return;
  }
  if (holder.root != nullptr && holder.root->type != fresh->type) {
    unmount(*holder.root, out);
    holder.root.reset();
  }
  if (holder.root == nullptr) {
    holder.root = mountNew(holder, *fresh, 0, -1, 0, out);
    return;
  }
  reconcileNode(holder, *holder.root, *fresh, out);
}

std::unique_ptr<Mounted> Reconciler::mountNew(Holder& holder, Node& fresh, int viewId, int parentId, int index,
                                              OpBatch& out) {
  auto mounted = std::make_unique<Mounted>();
  mounted->type = fresh.type;
  copyProps(*mounted, fresh);
  mounted->viewId = viewId;
  mounted->yoga = layout_.createNode(*mounted);
  layout_.applyStyle(mounted->yoga, mounted->style);
  if (viewId != 0) out.create(viewId, kindOf(mounted->type), parentId, index);
  if (isContainer(mounted->type) && hasVisual(mounted->style)) emitViewStyle(*mounted, out);
  if (mounted->type == NodeType::Image) emitImage(*mounted, out);
  if (mounted->type == NodeType::Pressable) emitPressable(*mounted, out);
  if (hasAccessibility(*mounted)) emitAccessibility(holder, *mounted, out);
  if (mounted->type == NodeType::Text) mounted->textDirty = true;
  mounted->children.reserve(fresh.children.size());
  for (size_t i = 0; i < fresh.children.size(); i++) {
    auto child = mountNew(holder, *fresh.children[i], holder.nextViewId++, viewId, static_cast<int>(i), out);
    YGNodeInsertChild(mounted->yoga, child->yoga, i);
    mounted->children.push_back(std::move(child));
  }
  return mounted;
}

void Reconciler::reconcileNode(Holder& holder, Mounted& mounted, Node& fresh, OpBatch& out) {
  bool imageEmitted = false;
  if (mounted.style != fresh.style) {
    bool visual = visualChanged(mounted.style, fresh.style);
    bool textStyle = textStyleChanged(mounted.style, fresh.style);
    bool imageStyle = imageStyleChanged(mounted.style, fresh.style);
    mounted.style = fresh.style;
    layout_.applyStyle(mounted.yoga, mounted.style);
    if (isContainer(mounted.type) && visual) emitViewStyle(mounted, out);
    if (mounted.type == NodeType::Text && textStyle) {
      mounted.textDirty = true;
      layout_.markTextDirty(mounted.yoga);
    }
    if (mounted.type == NodeType::Image && imageStyle) {
      emitImage(mounted, out);
      imageEmitted = true;
    }
  }
  if (mounted.type == NodeType::Text && mounted.text != fresh.text) {
    mounted.text = std::move(fresh.text);
    mounted.textDirty = true;
    layout_.markTextDirty(mounted.yoga);
  }
  if (mounted.type == NodeType::Text && mounted.allowFontScaling != fresh.allowFontScaling) {
    mounted.allowFontScaling = fresh.allowFontScaling;
    mounted.textDirty = true;
    layout_.markTextDirty(mounted.yoga);
  }
  if (mounted.type == NodeType::Image && imageChanged(mounted, fresh)) {
    mounted.imageUri = std::move(fresh.imageUri);
    mounted.resizeMode = fresh.resizeMode;
    mounted.tintColor = fresh.tintColor;
    mounted.imageHeaders = std::move(fresh.imageHeaders);
    if (!imageEmitted) emitImage(mounted, out);
  }
  if (mounted.type == NodeType::Pressable && pressableChanged(mounted, fresh)) {
    mounted.pressId = std::move(fresh.pressId);
    mounted.pressedOpacity = fresh.pressedOpacity;
    emitPressable(mounted, out);
  }
  if (accessibilityChanged(mounted, fresh)) {
    mounted.accessibilityLabel = std::move(fresh.accessibilityLabel);
    mounted.testID = std::move(fresh.testID);
    mounted.accessible = fresh.accessible;
    emitAccessibility(holder, mounted, out);
  }
  mounted.key = std::move(fresh.key);
  reconcileChildren(holder, mounted, fresh, out);
}

// Keyed children match by key; unkeyed ones pair by ordinal and type, as React does; the rest are removed.
void Reconciler::reconcileChildren(Holder& holder, Mounted& mounted, Node& fresh, OpBatch& out) {
  auto& old = mounted.children;
  std::vector<int> match(fresh.children.size(), -1);
  std::vector<bool> claimed(old.size(), false);
  std::unordered_map<std::string, size_t> byKey;
  for (size_t j = 0; j < old.size(); j++) {
    if (!old[j]->key.empty()) byKey.emplace(old[j]->key, j);
  }
  size_t cursor = 0;
  for (size_t i = 0; i < fresh.children.size(); i++) {
    const Node& f = *fresh.children[i];
    if (!f.key.empty()) {
      auto it = byKey.find(f.key);
      if (it != byKey.end() && !claimed[it->second] && old[it->second]->type == f.type) {
        match[i] = static_cast<int>(it->second);
        claimed[it->second] = true;
      }
      continue;
    }
    while (cursor < old.size() && !old[cursor]->key.empty()) cursor++;
    if (cursor < old.size()) {
      if (old[cursor]->type == f.type) {
        match[i] = static_cast<int>(cursor);
        claimed[cursor] = true;
      }
      cursor++;
    }
  }
  // Removes go first so the indices the platform sees stay valid; YGNodeFree detaches from Yoga.
  for (size_t j = 0; j < old.size(); j++) {
    if (!claimed[j]) unmount(*old[j], out);
  }
  std::vector<std::unique_ptr<Mounted>> next;
  next.reserve(fresh.children.size());
  for (size_t i = 0; i < fresh.children.size(); i++) {
    if (match[i] < 0) {
      auto child = mountNew(holder, *fresh.children[i], holder.nextViewId++, mounted.viewId, static_cast<int>(i), out);
      YGNodeInsertChild(mounted.yoga, child->yoga, i);
      next.push_back(std::move(child));
      continue;
    }
    std::unique_ptr<Mounted> child = std::move(old[match[i]]);
    reconcileNode(holder, *child, *fresh.children[i], out);
    // The children before i are already in place, so this one sits at i or after it.
    size_t current = i;
    const size_t count = YGNodeGetChildCount(mounted.yoga);
    while (current < count && YGNodeGetChild(mounted.yoga, current) != child->yoga) current++;
    if (current != i) {
      YGNodeRemoveChild(mounted.yoga, child->yoga);
      YGNodeInsertChild(mounted.yoga, child->yoga, i);
      out.move(child->viewId, static_cast<int>(i));
    }
    next.push_back(std::move(child));
  }
  old = std::move(next);
}

// Children first, so the platform removes leaves before their parent. YGNodeFree detaches from the owner.
void Reconciler::unmount(Mounted& mounted, OpBatch& out) {
  for (auto& child : mounted.children) unmount(*child, out);
  mounted.children.clear();
  if (mounted.viewId != 0) out.remove(mounted.viewId);
  layout_.destroyNode(mounted.yoga);
  mounted.yoga = nullptr;
}

void Reconciler::freeTree(Mounted& mounted) {
  for (auto& child : mounted.children) freeTree(*child);
  mounted.children.clear();
  layout_.destroyNode(mounted.yoga);
  mounted.yoga = nullptr;
}

void Reconciler::emitAfterLayout(Holder& holder, OpBatch& out) {
  if (holder.root == nullptr) return;
  emitTextOf(*holder.root, out);
  emitRootLabel(holder, out);
  emitPressableLabels(*holder.root, out);
}

void Reconciler::emitTextOf(Mounted& node, OpBatch& out) {
  if (node.type == NodeType::Text) {
    float maxWidth = node.measuredMaxWidth > 0 ? node.measuredMaxWidth : YGNodeLayoutGetWidth(node.yoga);
    float insetLeft = YGNodeLayoutGetPadding(node.yoga, YGEdgeLeft) + YGNodeLayoutGetBorder(node.yoga, YGEdgeLeft);
    float insetTop = YGNodeLayoutGetPadding(node.yoga, YGEdgeTop) + YGNodeLayoutGetBorder(node.yoga, YGEdgeTop);
    if (node.textDirty || maxWidth != node.emittedMaxWidth || insetLeft != node.emittedInsetLeft ||
        insetTop != node.emittedInsetTop) {
      out.text(node.viewId, node.text, node.style, maxWidth, node.allowFontScaling, insetLeft, insetTop);
      node.textDirty = false;
      node.emittedMaxWidth = maxWidth;
      node.emittedInsetLeft = insetLeft;
      node.emittedInsetTop = insetTop;
    }
  }
  for (auto& child : node.children) emitTextOf(*child, out);
}

// Without an explicit label the cell reads as its texts in order; sent only when that string changes.
void Reconciler::emitRootLabel(Holder& holder, OpBatch& out) {
  const Mounted& root = *holder.root;
  if (!root.accessibilityLabel.empty()) return;
  std::string joined;
  appendTexts(root, joined);
  if (joined == holder.rootLabel) return;
  holder.rootLabel = joined;
  out.accessibility(0, joined, root.testID, 0, root.accessible);
}

// A nested pressable without a label of its own reads as the texts inside it, as React Native's views do.
void Reconciler::emitPressableLabels(Mounted& node, OpBatch& out) {
  if (node.viewId != 0 && node.type == NodeType::Pressable && node.accessibilityLabel.empty()) {
    std::string joined;
    appendTexts(node, joined);
    if (joined != node.derivedLabel) {
      node.derivedLabel = joined;
      out.accessibility(node.viewId, joined, node.testID, 1, node.accessible);
    }
  }
  for (auto& child : node.children) emitPressableLabels(*child, out);
}

void Reconciler::emitViewStyle(const Mounted& node, OpBatch& out) { out.viewStyle(node.viewId, node.style); }

void Reconciler::emitImage(const Mounted& node, OpBatch& out) {
  out.image(node.viewId, node.imageUri, node.style, node.resizeMode, node.tintColor, node.imageHeaders);
}

void Reconciler::emitPressable(const Mounted& node, OpBatch& out) {
  out.pressable(node.viewId, node.pressId, node.pressedOpacity);
}

void Reconciler::emitAccessibility(Holder& holder, Mounted& node, OpBatch& out) {
  out.accessibility(node.viewId, node.accessibilityLabel, node.testID, node.type == NodeType::Pressable ? 1 : 0,
                    node.accessible);
  // The op carries the node's own label, empty when it derives one, so the derived one goes out again after layout.
  if (node.viewId == 0) {
    holder.rootLabel.clear();
  } else {
    node.derivedLabel.clear();
  }
}

void Reconciler::release(Holder& holder) {
  if (holder.root != nullptr) {
    freeTree(*holder.root);
    holder.root.reset();
  }
  holder.nextViewId = 1;
  holder.rootLabel.clear();
}

}  // namespace flume
