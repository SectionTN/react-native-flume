#include "core/Style.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/Color.h"

namespace flume {
namespace {

using Setter = void (*)(Style&, const Value&);

std::optional<float> number(const Value& v) {
  if (v.kind() != Value::Kind::Number) return std::nullopt;
  return static_cast<float>(v.asNumber());
}

std::optional<float> percent(const Value& v) {
  if (v.kind() != Value::Kind::String) return std::nullopt;
  const std::string& s = v.asString();
  if (s.size() < 2 || s.back() != '%') return std::nullopt;
  char* end = nullptr;
  float n = std::strtof(s.c_str(), &end);
  if (end != s.c_str() + s.size() - 1) return std::nullopt;
  return n;
}

template <typename T>
std::optional<T> enumValue(const Value& v, std::initializer_list<std::pair<std::string_view, T>> names) {
  if (v.kind() != Value::Kind::String) return std::nullopt;
  for (const auto& [name, value] : names) {
    if (v.asString() == name) return value;
  }
  return std::nullopt;
}

std::optional<YGAlign> alignValue(const Value& v) {
  return enumValue<YGAlign>(v, {{"auto", YGAlignAuto},
                                {"flex-start", YGAlignFlexStart},
                                {"flex-end", YGAlignFlexEnd},
                                {"center", YGAlignCenter},
                                {"stretch", YGAlignStretch},
                                {"baseline", YGAlignBaseline}});
}

void setFontWeight(Style& s, const Value& v) {
  if (v.kind() == Value::Kind::Number) {
    s.fontWeight = static_cast<int>(v.asNumber());
  } else if (v.kind() == Value::Kind::String) {
    const std::string& w = v.asString();
    if (w == "normal") s.fontWeight = 400;
    else if (w == "bold") s.fontWeight = 700;
    else if (w.size() == 3 && w[0] >= '1' && w[0] <= '9' && w[1] == '0' && w[2] == '0') s.fontWeight = (w[0] - '0') * 100;
  }
}

template <std::array<std::optional<float>, YGEdgeAll + 1> Style::*Field, YGEdge Edge>
void setEdge(Style& s, const Value& v) {
  if (auto n = number(v)) (s.*Field)[Edge] = *n;
}

template <std::optional<float> Style::*Field>
void setNumber(Style& s, const Value& v) {
  if (auto n = number(v)) s.*Field = *n;
}

template <std::optional<float> Style::*Field, std::optional<float> Style::*PercentField>
void setDimension(Style& s, const Value& v) {
  if (auto n = number(v)) {
    s.*Field = *n;
  } else if (auto p = percent(v)) {
    s.*PercentField = *p;
  }
}

template <std::optional<uint32_t> Style::*Field>
void setColor(Style& s, const Value& v) {
  if (v.kind() != Value::Kind::String) return;
  if (auto c = parseColor(v.asString())) s.*Field = *c;
}

const std::unordered_map<std::string_view, Setter>& setters() {
  static const std::unordered_map<std::string_view, Setter> table = {
      {"flex", setNumber<&Style::flex>},
      {"flexGrow", setNumber<&Style::flexGrow>},
      {"flexShrink", setNumber<&Style::flexShrink>},
      {"flexBasis", setNumber<&Style::flexBasis>},
      {"width", setDimension<&Style::width, &Style::widthPercent>},
      {"height", setDimension<&Style::height, &Style::heightPercent>},
      {"minWidth", setNumber<&Style::minWidth>},
      {"minHeight", setNumber<&Style::minHeight>},
      {"maxWidth", setNumber<&Style::maxWidth>},
      {"maxHeight", setNumber<&Style::maxHeight>},
      {"gap", setNumber<&Style::gap>},
      {"rowGap", setNumber<&Style::rowGap>},
      {"columnGap", setNumber<&Style::columnGap>},
      {"top", setNumber<&Style::top>},
      {"left", setNumber<&Style::left>},
      {"right", setNumber<&Style::right>},
      {"bottom", setNumber<&Style::bottom>},
      {"opacity", setNumber<&Style::opacity>},
      {"borderRadius", setNumber<&Style::borderRadius>},
      {"padding", setEdge<&Style::padding, YGEdgeAll>},
      {"paddingTop", setEdge<&Style::padding, YGEdgeTop>},
      {"paddingBottom", setEdge<&Style::padding, YGEdgeBottom>},
      {"paddingLeft", setEdge<&Style::padding, YGEdgeLeft>},
      {"paddingRight", setEdge<&Style::padding, YGEdgeRight>},
      {"paddingStart", setEdge<&Style::padding, YGEdgeStart>},
      {"paddingEnd", setEdge<&Style::padding, YGEdgeEnd>},
      {"paddingHorizontal", setEdge<&Style::padding, YGEdgeHorizontal>},
      {"paddingVertical", setEdge<&Style::padding, YGEdgeVertical>},
      {"margin", setEdge<&Style::margin, YGEdgeAll>},
      {"marginTop", setEdge<&Style::margin, YGEdgeTop>},
      {"marginBottom", setEdge<&Style::margin, YGEdgeBottom>},
      {"marginLeft", setEdge<&Style::margin, YGEdgeLeft>},
      {"marginRight", setEdge<&Style::margin, YGEdgeRight>},
      {"marginStart", setEdge<&Style::margin, YGEdgeStart>},
      {"marginEnd", setEdge<&Style::margin, YGEdgeEnd>},
      {"marginHorizontal", setEdge<&Style::margin, YGEdgeHorizontal>},
      {"marginVertical", setEdge<&Style::margin, YGEdgeVertical>},
      {"backgroundColor", setColor<&Style::backgroundColor>},
      {"color", setColor<&Style::color>},
      {"fontWeight", setFontWeight},
      {"fontSize",
       [](Style& s, const Value& v) {
         if (auto n = number(v)) s.fontSize = *n;
       }},
      {"numberOfLines",
       [](Style& s, const Value& v) {
         if (auto n = number(v)) s.numberOfLines = static_cast<int>(*n);
       }},
      {"flexDirection",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGFlexDirection>(v, {{"row", YGFlexDirectionRow},
                                                      {"column", YGFlexDirectionColumn},
                                                      {"row-reverse", YGFlexDirectionRowReverse},
                                                      {"column-reverse", YGFlexDirectionColumnReverse}}))
           s.flexDirection = *e;
       }},
      {"alignItems",
       [](Style& s, const Value& v) {
         if (auto e = alignValue(v)) s.alignItems = *e;
       }},
      {"alignSelf",
       [](Style& s, const Value& v) {
         if (auto e = alignValue(v)) s.alignSelf = *e;
       }},
      {"justifyContent",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGJustify>(v, {{"flex-start", YGJustifyFlexStart},
                                                {"flex-end", YGJustifyFlexEnd},
                                                {"center", YGJustifyCenter},
                                                {"space-between", YGJustifySpaceBetween},
                                                {"space-around", YGJustifySpaceAround},
                                                {"space-evenly", YGJustifySpaceEvenly}}))
           s.justifyContent = *e;
       }},
      {"flexWrap",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGWrap>(v, {{"wrap", YGWrapWrap},
                                             {"nowrap", YGWrapNoWrap},
                                             {"wrap-reverse", YGWrapWrapReverse}}))
           s.flexWrap = *e;
       }},
      {"position",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGPositionType>(v, {{"absolute", YGPositionTypeAbsolute},
                                                     {"relative", YGPositionTypeRelative}}))
           s.position = *e;
       }},
      {"overflow",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGOverflow>(v, {{"visible", YGOverflowVisible}, {"hidden", YGOverflowHidden}}))
           s.overflow = *e;
       }},
  };
  return table;
}

}  // namespace

Style Style::fromValue(const Value& object) {
  Style style;
  const auto& table = setters();
  for (const auto& [key, value] : object.asObject()) {
    auto it = table.find(key);
    if (it != table.end()) it->second(style, value);
  }
  return style;
}

}  // namespace flume
