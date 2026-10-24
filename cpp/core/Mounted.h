#pragma once

#include <yoga/Yoga.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/Node.h"
#include "core/OpBatch.h"

namespace flume {

class Layout;

// One element kept alive in a holder across binds, with its Yoga node and platform view id.
struct Mounted {
  NodeType type = NodeType::View;
  Style style;
  std::string text;
  bool allowFontScaling = true;
  std::string imageUri;
  ResizeMode resizeMode = ResizeMode::Cover;
  std::optional<uint32_t> tintColor;
  Headers imageHeaders;
  std::string pressId;
  float pressedOpacity = 0.7f;
  std::string key;
  std::string testID;
  std::string accessibilityLabel;
  std::optional<bool> accessible;
  int viewId = 0;              // 0 is the holder's own view
  YGNodeRef yoga = nullptr;
  Layout* layout = nullptr;
  float measuredMaxWidth = 0;  // the width constraint of the last text measure, for the Text op
  float emittedMaxWidth = -1;  // the width the last Text op carried; a text wrapped at another width is sent again
  float emittedInsetLeft = 0;  // the padding and border insets the last Text op carried
  float emittedInsetTop = 0;
  bool textDirty = false;      // set by the reconciler, cleared when the Text op is emitted after layout
  std::array<float, 4> lastFrame{NAN, NAN, NAN, NAN};  // the last Frame op sent; Yoga flags the root every pass
  std::array<float, 2> lastLayoutSize{NAN, NAN};  // the root's last constraints; another size re-measures every text
  std::vector<std::unique_ptr<Mounted>> children;
};

}  // namespace flume
