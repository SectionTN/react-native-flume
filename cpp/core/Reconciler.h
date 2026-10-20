#pragma once

#include <memory>
#include <string>

#include "core/Holder.h"
#include "core/Layout.h"
#include "core/Node.h"
#include "core/OpBatch.h"

namespace flume {

// Matches a fresh tree against a holder's mounted tree by position and type and emits the difference.
class Reconciler {
 public:
  explicit Reconciler(Layout& layout) : layout_(layout) {}

  void reconcile(Holder& holder, std::unique_ptr<Node> fresh, OpBatch& out);  // null fresh unmounts everything
  void emitAfterLayout(Holder& holder, OpBatch& out);  // Text ops for dirty or rewrapped texts, then the root label
  void release(Holder& holder);                        // destroys the mounted tree and its Yoga nodes, no ops

 private:
  std::unique_ptr<Mounted> mountNew(Holder& holder, Node& fresh, int viewId, int parentId, int index, OpBatch& out);
  void reconcileNode(Holder& holder, Mounted& mounted, Node& fresh, OpBatch& out);
  void unmount(Mounted& mounted, OpBatch& out);
  void freeTree(Mounted& mounted);
  void emitTextOf(Mounted& node, OpBatch& out);
  void emitRootLabel(Holder& holder, OpBatch& out);
  void emitViewStyle(const Mounted& node, OpBatch& out);
  void emitImage(const Mounted& node, OpBatch& out);
  void emitPressable(const Mounted& node, OpBatch& out);
  void emitAccessibility(const Mounted& node, OpBatch& out);

  Layout& layout_;
};

}  // namespace flume
