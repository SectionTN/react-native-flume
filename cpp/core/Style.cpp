#include "core/Style.h"

#include <string>
#include <string_view>
#include <unordered_map>

#include "core/Color.h"
#include "core/Number.h"

namespace flume {

namespace {

bool isCssSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

}  // namespace

Dimension Dimension::fromValue(const Value& v, bool allowAuto) {
  if (v.kind() == Value::Kind::Number) return {Unit::Point, static_cast<float>(v.asNumber())};
  if (v.kind() != Value::Kind::String) return {};
  const std::string& s = v.asString();
  if (allowAuto && s == "auto") return {Unit::Auto, 0};
  // Fabric's CSS parser skips whitespace around the value.
  std::string_view text(s);
  while (!text.empty() && isCssSpace(text.front())) text.remove_prefix(1);
  while (!text.empty() && isCssSpace(text.back())) text.remove_suffix(1);
  if (text.size() < 2 || text.back() != '%') return {};
  auto n = parseNumber(text.substr(0, text.size() - 1), true);
  if (!n) return {};
  return {Unit::Percent, static_cast<float>(*n)};
}

namespace {

using Setter = void (*)(Style&, const Value&);

std::optional<float> number(const Value& v) {
  if (v.kind() != Value::Kind::Number) return std::nullopt;
  return static_cast<float>(v.asNumber());
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
                                {"baseline", YGAlignBaseline},
                                {"space-between", YGAlignSpaceBetween},
                                {"space-around", YGAlignSpaceAround},
                                {"space-evenly", YGAlignSpaceEvenly}});
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

template <std::optional<float> Style::*Field>
void setNumber(Style& s, const Value& v) {
  if (auto n = number(v)) s.*Field = *n;
}

template <Dimension Style::*Field, bool AllowAuto>
void setDimension(Style& s, const Value& v) {
  Dimension d = Dimension::fromValue(v, AllowAuto);
  if (d.unit != Dimension::Unit::Undefined) s.*Field = d;
}

template <std::array<Dimension, YGEdgeAll + 1> Style::*Field, YGEdge Edge, bool AllowAuto>
void setEdge(Style& s, const Value& v) {
  Dimension d = Dimension::fromValue(v, AllowAuto);
  if (d.unit != Dimension::Unit::Undefined) (s.*Field)[Edge] = d;
}

template <std::optional<uint32_t> Style::*Field>
void setColor(Style& s, const Value& v) {
  if (v.kind() != Value::Kind::String) return;
  if (auto c = parseColor(v.asString())) s.*Field = *c;
}

template <size_t Corner>
void setRadius(Style& s, const Value& v) {
  if (auto n = number(v)) s.borderRadii[Corner] = *n;
}

template <typename T, std::optional<T> Style::*Field, typename F>
Setter enumSetter(F) {
  return [](Style& s, const Value& v) {
    if (auto e = F{}(v)) s.*Field = *e;
  };
}

const std::unordered_map<std::string_view, Setter>& setters() {
  static const std::unordered_map<std::string_view, Setter> table = {
      {"flex", setNumber<&Style::flex>},
      {"flexGrow", setNumber<&Style::flexGrow>},
      {"flexShrink", setNumber<&Style::flexShrink>},
      {"aspectRatio", setNumber<&Style::aspectRatio>},
      {"gap", setNumber<&Style::gap>},
      {"rowGap", setNumber<&Style::rowGap>},
      {"columnGap", setNumber<&Style::columnGap>},
      {"flexBasis", setDimension<&Style::flexBasis, true>},
      {"width", setDimension<&Style::width, true>},
      {"height", setDimension<&Style::height, true>},
      {"minWidth", setDimension<&Style::minWidth, false>},
      {"minHeight", setDimension<&Style::minHeight, false>},
      {"maxWidth", setDimension<&Style::maxWidth, false>},
      {"maxHeight", setDimension<&Style::maxHeight, false>},
      {"top", setEdge<&Style::position, YGEdgeTop, false>},
      {"left", setEdge<&Style::position, YGEdgeLeft, false>},
      {"right", setEdge<&Style::position, YGEdgeRight, false>},
      {"bottom", setEdge<&Style::position, YGEdgeBottom, false>},
      {"start", setEdge<&Style::position, YGEdgeStart, false>},
      {"end", setEdge<&Style::position, YGEdgeEnd, false>},
      {"padding", setEdge<&Style::padding, YGEdgeAll, false>},
      {"paddingTop", setEdge<&Style::padding, YGEdgeTop, false>},
      {"paddingBottom", setEdge<&Style::padding, YGEdgeBottom, false>},
      {"paddingLeft", setEdge<&Style::padding, YGEdgeLeft, false>},
      {"paddingRight", setEdge<&Style::padding, YGEdgeRight, false>},
      {"paddingStart", setEdge<&Style::padding, YGEdgeStart, false>},
      {"paddingEnd", setEdge<&Style::padding, YGEdgeEnd, false>},
      {"paddingHorizontal", setEdge<&Style::padding, YGEdgeHorizontal, false>},
      {"paddingVertical", setEdge<&Style::padding, YGEdgeVertical, false>},
      {"margin", setEdge<&Style::margin, YGEdgeAll, true>},
      {"marginTop", setEdge<&Style::margin, YGEdgeTop, true>},
      {"marginBottom", setEdge<&Style::margin, YGEdgeBottom, true>},
      {"marginLeft", setEdge<&Style::margin, YGEdgeLeft, true>},
      {"marginRight", setEdge<&Style::margin, YGEdgeRight, true>},
      {"marginStart", setEdge<&Style::margin, YGEdgeStart, true>},
      {"marginEnd", setEdge<&Style::margin, YGEdgeEnd, true>},
      {"marginHorizontal", setEdge<&Style::margin, YGEdgeHorizontal, true>},
      {"marginVertical", setEdge<&Style::margin, YGEdgeVertical, true>},
      {"backgroundColor", setColor<&Style::backgroundColor>},
      {"color", setColor<&Style::color>},
      {"borderColor", setColor<&Style::borderColor>},
      {"shadowColor", setColor<&Style::shadowColor>},
      {"tintColor", setColor<&Style::tintColor>},
      {"opacity", setNumber<&Style::opacity>},
      {"borderWidth", setNumber<&Style::borderWidth>},
      {"shadowOpacity", setNumber<&Style::shadowOpacity>},
      {"shadowRadius", setNumber<&Style::shadowRadius>},
      {"elevation", setNumber<&Style::elevation>},
      {"lineHeight", setNumber<&Style::lineHeight>},
      {"letterSpacing", setNumber<&Style::letterSpacing>},
      {"borderTopLeftRadius", setRadius<0>},
      {"borderTopRightRadius", setRadius<1>},
      {"borderBottomRightRadius", setRadius<2>},
      {"borderBottomLeftRadius", setRadius<3>},
      {"borderRadius",
       [](Style& s, const Value& v) {
         if (auto n = number(v)) s.borderRadii.fill(*n);
       }},
      {"shadowOffset",
       [](Style& s, const Value& v) {
         const Value* w = v.get("width");
         const Value* h = v.get("height");
         if (w != nullptr && h != nullptr && number(*w) && number(*h)) s.shadowOffset = {*number(*w), *number(*h)};
       }},
      {"fontWeight", setFontWeight},
      {"fontSize",
       [](Style& s, const Value& v) {
         if (auto n = number(v)) s.fontSize = *n;
       }},
      {"numberOfLines",
       [](Style& s, const Value& v) {
         if (auto n = number(v)) s.numberOfLines = static_cast<int>(*n);
       }},
      {"fontFamily",
       [](Style& s, const Value& v) {
         if (v.kind() == Value::Kind::String) s.fontFamily = v.asString();
       }},
      {"fontStyle",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<bool>(v, {{"italic", true}, {"normal", false}})) s.italic = *e;
       }},
      {"textAlign",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<TextAlign>(v, {{"auto", TextAlign::Auto},
                                               {"left", TextAlign::Left},
                                               {"right", TextAlign::Right},
                                               {"center", TextAlign::Center},
                                               {"justify", TextAlign::Justify}}))
           s.textAlign = *e;
       }},
      {"textDecorationLine",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<uint8_t>(v, {{"none", 0}, {"underline", 1}, {"line-through", 2}, {"underline line-through", 3}}))
           s.decoration = *e;
       }},
      {"ellipsizeMode", [](Style&, const Value&) {}},
      {"resizeMode",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<ResizeMode>(v, {{"cover", ResizeMode::Cover},
                                                {"contain", ResizeMode::Contain},
                                                {"stretch", ResizeMode::Stretch},
                                                {"center", ResizeMode::Center}}))
           s.resizeMode = *e;
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
      {"alignContent",
       [](Style& s, const Value& v) {
         if (auto e = alignValue(v)) s.alignContent = *e;
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
           s.positionType = *e;
       }},
      {"overflow",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGOverflow>(v, {{"visible", YGOverflowVisible}, {"hidden", YGOverflowHidden}}))
           s.overflow = *e;
       }},
      {"display",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGDisplay>(v, {{"flex", YGDisplayFlex}, {"none", YGDisplayNone}})) s.display = *e;
       }},
      {"direction",
       [](Style& s, const Value& v) {
         if (auto e = enumValue<YGDirection>(v, {{"inherit", YGDirectionInherit}, {"ltr", YGDirectionLTR}, {"rtl", YGDirectionRTL}}))
           s.direction = *e;
       }},
  };
  return table;
}

}  // namespace

Style Style::fromValue(const Value& object, std::vector<std::string>* unknownKeys) {
  Style style;
  const auto& table = setters();
  for (const auto& [key, value] : object.asObject()) {
    auto it = table.find(key);
    if (it != table.end()) {
      it->second(style, value);
    } else if (unknownKeys != nullptr) {
      unknownKeys->push_back(key);
    }
  }
  return style;
}

}  // namespace flume

namespace flume {

void clearParentLayout(Style& style) {
  style.margin.fill(Dimension{});
  style.position.fill(Dimension{});
  style.positionType.reset();
  style.alignSelf.reset();
  style.flex.reset();
  style.flexGrow.reset();
  style.flexShrink.reset();
  style.flexBasis = Dimension{};
}

}  // namespace flume
