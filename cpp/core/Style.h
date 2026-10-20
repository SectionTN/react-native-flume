#pragma once

#include <yoga/Yoga.h>

#include <array>
#include <cstdint>
#include <optional>

#include "core/Value.h"

namespace flume {

// Every field is the React Native style key of the same name, in dp.
struct Style {
  std::optional<float> flex, flexGrow, flexShrink, flexBasis, width, height, minWidth, minHeight,
      maxWidth, maxHeight, gap, rowGap, columnGap, top, left, right, bottom, widthPercent,
      heightPercent;
  std::optional<YGFlexDirection> flexDirection;
  std::optional<YGAlign> alignItems, alignSelf;
  std::optional<YGJustify> justifyContent;
  std::optional<YGWrap> flexWrap;
  std::optional<YGPositionType> position;
  std::optional<YGOverflow> overflow;
  std::array<std::optional<float>, YGEdgeAll + 1> padding, margin;  // indexed by YGEdge
  std::optional<uint32_t> backgroundColor, color;
  std::optional<float> opacity, borderRadius;
  float fontSize = 14;
  int fontWeight = 400;
  int numberOfLines = 0;

  // Unknown keys and values that do not parse are skipped.
  static Style fromValue(const Value& object);

  bool operator==(const Style&) const = default;
};

}  // namespace flume
