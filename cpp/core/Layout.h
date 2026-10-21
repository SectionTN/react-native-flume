#pragma once

#include <yoga/Yoga.h>

#include "core/MeasureCache.h"
#include "core/Mounted.h"
#include "core/OpBatch.h"
#include "core/TextMeasurer.h"

namespace flume {

// Owns the Yoga config of one list and turns styles into Yoga nodes, layouts into frames.
class Layout {
 public:
  Layout(TextMeasurer& measurer, float pointScale);
  ~Layout();
  Layout(const Layout&) = delete;
  Layout& operator=(const Layout&) = delete;

  YGNodeRef createNode(Mounted& owner);  // Text nodes get a measure function whose context is the Mounted
  void destroyNode(YGNodeRef node);       // after the node left its parent
  void applyStyle(YGNodeRef node, const Style& style);
  void markTextDirty(YGNodeRef node);
  void invalidateText(Mounted& root);  // every text re-measures and is sent again, for a new font scale or size
  void layout(Mounted& root, float width, float height);  // NaN on an axis means automatic, as YGUndefined
  void readFrames(Mounted& root, OpBatch& out);  // Frame ops for nodes with a new layout, root size into out
  MeasureCache& cache() { return cache_; }

  int measureCalls = 0;  // reset by the caller per bind
  int measureHits = 0;

 private:
  static YGSize measureText(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
                            YGMeasureMode heightMode);
  void readFramesOf(Mounted& node, OpBatch& out);

  TextMeasurer& measurer_;
  YGConfigRef config_;
  MeasureCache cache_;
};

}  // namespace flume
