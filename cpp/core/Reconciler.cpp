#include "core/Reconciler.h"

#include <algorithm>

namespace flume {
namespace {

ViewKind kindOf(NodeType type) {
  switch (type) {
    case NodeType::Text: return ViewKind::Text;
    case NodeType::Image: return ViewKind::Image;
    default: return ViewKind::View;
  }
}

bool hasVisual(const Style& s) {
  return s.backgroundColor.has_value() || s.opacity.has_value() || s.borderRadius.has_value();
}

bool visualChanged(const Style& a, const Style& b) {
  return a.backgroundColor != b.backgroundColor || a.opacity != b.opacity || a.borderRadius != b.borderRadius;
}

bool textStyleChanged(const Style& a, const Style& b) {
  return a.fontSize != b.fontSize || a.fontWeight != b.fontWeight || a.color != b.color ||
         a.numberOfLines != b.numberOfLines;
}

bool imageStyleChanged(const Style& a, const Style& b) {
  return a.width != b.width || a.height != b.height || a.borderRadius != b.borderRadius;
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
  mounted->style = fresh.style;
  mounted->text = std::move(fresh.text);
  mounted->imageUri = std::move(fresh.imageUri);
  mounted->viewId = viewId;
  mounted->yoga = layout_.createNode(*mounted);
  layout_.applyStyle(mounted->yoga, mounted->style);
  if (viewId != 0) out.create(viewId, kindOf(mounted->type), parentId, index);
  if (mounted->type == NodeType::View && hasVisual(mounted->style)) emitViewStyle(*mounted, out);
  if (mounted->type == NodeType::Image) emitImage(*mounted, out);
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
    if (mounted.type == NodeType::View && visual) emitViewStyle(mounted, out);
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
  if (mounted.type == NodeType::Image && mounted.imageUri != fresh.imageUri) {
    mounted.imageUri = std::move(fresh.imageUri);
    if (!imageEmitted) emitImage(mounted, out);
  }

  size_t common = std::min(mounted.children.size(), fresh.children.size());
  for (size_t i = 0; i < common; i++) {
    Mounted& child = *mounted.children[i];
    Node& freshChild = *fresh.children[i];
    if (child.type != freshChild.type) {
      unmount(child, out);
      mounted.children[i] = mountNew(holder, freshChild, holder.nextViewId++, mounted.viewId, static_cast<int>(i), out);
      YGNodeInsertChild(mounted.yoga, mounted.children[i]->yoga, i);
    } else {
      reconcileNode(holder, child, freshChild, out);
    }
  }
  while (mounted.children.size() > fresh.children.size()) {
    unmount(*mounted.children.back(), out);
    mounted.children.pop_back();
  }
  for (size_t i = common; i < fresh.children.size(); i++) {
    auto child = mountNew(holder, *fresh.children[i], holder.nextViewId++, mounted.viewId, static_cast<int>(i), out);
    YGNodeInsertChild(mounted.yoga, child->yoga, i);
    mounted.children.push_back(std::move(child));
  }
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

void Reconciler::emitText(Holder& holder, OpBatch& out) {
  if (holder.root != nullptr) emitTextOf(*holder.root, out);
}

void Reconciler::emitTextOf(Mounted& node, OpBatch& out) {
  if (node.type == NodeType::Text && node.textDirty) {
    float maxWidth = node.measuredMaxWidth > 0 ? node.measuredMaxWidth : YGNodeLayoutGetWidth(node.yoga);
    out.text(node.viewId, node.text, node.style.color.value_or(0xFF000000u), node.style.fontWeight,
             node.style.numberOfLines, node.style.fontSize, maxWidth);
    node.textDirty = false;
  }
  for (auto& child : node.children) emitTextOf(*child, out);
}

void Reconciler::emitViewStyle(const Mounted& node, OpBatch& out) {
  out.viewStyle(node.viewId, node.style.backgroundColor.value_or(0), node.style.opacity.value_or(1),
                node.style.borderRadius.value_or(0));
}

void Reconciler::emitImage(const Mounted& node, OpBatch& out) {
  out.image(node.viewId, node.imageUri, node.style.width.value_or(0), node.style.height.value_or(0),
            node.style.borderRadius.value_or(0));
}

void Reconciler::release(Holder& holder) {
  if (holder.root != nullptr) {
    freeTree(*holder.root);
    holder.root.reset();
  }
  holder.nextViewId = 1;
}

}  // namespace flume
