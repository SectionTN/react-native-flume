#pragma once

#include <yoga/Yoga.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "core/Node.h"

namespace flume {

class Layout;

// One element kept alive in a holder across binds, with its Yoga node and platform view id.
struct Mounted {
  NodeType type = NodeType::View;
  Style style;
  std::string text;
  std::string imageUri;
  int viewId = 0;              // 0 is the holder's own view
  YGNodeRef yoga = nullptr;
  Layout* layout = nullptr;
  float measuredMaxWidth = 0;  // the width constraint of the last text measure, for the Text op
  bool textDirty = false;      // set by the reconciler, cleared when the Text op is emitted after layout
  std::array<float, 4> lastFrame{NAN, NAN, NAN, NAN};  // the last Frame op sent; Yoga flags the root every pass
  std::vector<std::unique_ptr<Mounted>> children;
};

}  // namespace flume
