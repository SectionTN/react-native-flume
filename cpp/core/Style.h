#pragma once

#include <yoga/Yoga.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Value.h"

namespace flume {

// A React Native dimension: points, a percentage string or "auto".
struct Dimension {
  enum class Unit : uint8_t { Undefined, Point, Percent, Auto };
  Unit unit = Unit::Undefined;
  float value = 0;

  static Dimension fromValue(const Value& v, bool allowAuto);
  float pointsOr(float fallback) const { return unit == Unit::Point ? value : fallback; }
  bool operator==(const Dimension&) const = default;
};

enum class TextAlign : uint8_t { Auto, Left, Right, Center, Justify };
enum class ResizeMode : uint8_t { Cover, Contain, Stretch, Center };

// Every field is the React Native style key of the same name, in dp.
struct Style {
  std::optional<float> flex, flexGrow, flexShrink, aspectRatio, gap, rowGap, columnGap;
  Dimension flexBasis, width, height, minWidth, minHeight, maxWidth, maxHeight;
  std::array<Dimension, YGEdgeAll + 1> margin, padding, position;  // indexed by YGEdge
  std::optional<YGFlexDirection> flexDirection;
  std::optional<YGAlign> alignItems, alignSelf, alignContent;
  std::optional<YGJustify> justifyContent;
  std::optional<YGWrap> flexWrap;
  std::optional<YGPositionType> positionType;
  std::optional<YGOverflow> overflow;
  std::optional<YGDisplay> display;
  std::optional<YGDirection> direction;
  std::optional<uint32_t> backgroundColor, color, borderColor, shadowColor, tintColor;
  std::optional<float> opacity, borderWidth, shadowOpacity, shadowRadius, elevation;
  std::optional<std::pair<float, float>> shadowOffset;
  std::array<std::optional<float>, 4> borderRadii;  // topLeft, topRight, bottomRight, bottomLeft
  std::optional<ResizeMode> resizeMode;
  float fontSize = 14;
  int fontWeight = 400;
  int numberOfLines = 0;
  std::string fontFamily;
  bool italic = false;
  std::optional<float> lineHeight, letterSpacing;
  TextAlign textAlign = TextAlign::Auto;
  uint8_t decoration = 0;  // 1 underline, 2 line-through

  // Unknown keys and values that do not parse are skipped; unknown keys are collected when a vector is given.
  static Style fromValue(const Value& object, std::vector<std::string>* unknownKeys = nullptr);

  bool operator==(const Style&) const = default;
};

}  // namespace flume
