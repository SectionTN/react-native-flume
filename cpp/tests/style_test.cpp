#include "doctest.h"

#include "core/Style.h"
#include "values.h"

TEST_CASE("style from an object") {
  auto s = flume::Style::fromValue(obj({{"flexDirection", V("row")},
                                        {"padding", V(12)},
                                        {"gap", V(12)},
                                        {"alignItems", V("center")},
                                        {"fontWeight", V("bold")},
                                        {"width", V("50%")},
                                        {"backgroundColor", V("#fff")},
                                        {"foo", V(1)},
                                        {"color", V("blue")}}));
  CHECK(s.flexDirection == YGFlexDirectionRow);
  CHECK(s.padding[YGEdgeAll].value == 12.0f);
  CHECK(s.gap == 12.0f);
  CHECK(s.alignItems == YGAlignCenter);
  CHECK(s.fontWeight == 700);
  CHECK(s.width == flume::Dimension{flume::Dimension::Unit::Percent, 50});
  CHECK(s.backgroundColor == 0xFFFFFFFFu);
  CHECK(s.color == 0xFF0000FFu);
}

TEST_CASE("font weights") {
  CHECK(flume::Style::fromValue(obj({{"fontWeight", V("600")}})).fontWeight == 600);
  CHECK(flume::Style::fromValue(obj({{"fontWeight", V(500)}})).fontWeight == 500);
  CHECK(flume::Style::fromValue(obj({{"fontWeight", V("normal")}})).fontWeight == 400);
}

TEST_CASE("edges and percent heights") {
  auto s = flume::Style::fromValue(obj({{"paddingHorizontal", V(8)},
                                        {"marginStart", V(4)},
                                        {"height", V("25%")},
                                        {"numberOfLines", V(2)},
                                        {"opacity", V(0.5)}}));
  CHECK(s.padding[YGEdgeHorizontal].value == 8.0f);
  CHECK(s.margin[YGEdgeStart].value == 4.0f);
  CHECK(s.height == flume::Dimension{flume::Dimension::Unit::Percent, 25});
  CHECK(s.numberOfLines == 2);
  CHECK(s.opacity == 0.5f);
}

TEST_CASE("default styles are equal") { CHECK(flume::Style{} == flume::Style{}); }

TEST_CASE("parses the full vocabulary") {
  auto s = flume::Style::fromValue(obj({{"aspectRatio", V(2)}, {"display", V("none")}, {"direction", V("rtl")},
                                        {"alignContent", V("space-between")}, {"width", V("50%")}, {"minWidth", V(10)},
                                        {"maxHeight", V("30%")}, {"flexBasis", V("25%")}, {"marginLeft", V("auto")},
                                        {"marginHorizontal", V("10%")}, {"paddingStart", V(4)}, {"start", V(3)},
                                        {"end", V("5%")}, {"borderWidth", V(1.5)}, {"borderColor", V("#00f")},
                                        {"borderTopLeftRadius", V(2)}, {"shadowColor", V("#000")}, {"shadowOpacity", V(0.3)},
                                        {"shadowRadius", V(4)}, {"shadowOffset", obj({{"width", V(1)}, {"height", V(2)}})},
                                        {"elevation", V(3)}, {"fontFamily", V("Inter")}, {"fontStyle", V("italic")},
                                        {"lineHeight", V(20)}, {"letterSpacing", V(0.5)}, {"textAlign", V("center")},
                                        {"textDecorationLine", V("underline line-through")}, {"ellipsizeMode", V("tail")}}));
  CHECK(s.aspectRatio == 2.0f);
  CHECK(s.display == YGDisplayNone);
  CHECK(s.direction == YGDirectionRTL);
  CHECK(s.alignContent == YGAlignSpaceBetween);
  CHECK(s.width == flume::Dimension{flume::Dimension::Unit::Percent, 50});
  CHECK(s.minWidth == flume::Dimension{flume::Dimension::Unit::Point, 10});
  CHECK(s.maxHeight.unit == flume::Dimension::Unit::Percent);
  CHECK(s.flexBasis.unit == flume::Dimension::Unit::Percent);
  CHECK(s.margin[YGEdgeLeft].unit == flume::Dimension::Unit::Auto);
  CHECK(s.margin[YGEdgeHorizontal] == flume::Dimension{flume::Dimension::Unit::Percent, 10});
  CHECK(s.padding[YGEdgeStart].value == 4.0f);
  CHECK(s.position[YGEdgeStart].value == 3.0f);
  CHECK(s.position[YGEdgeEnd].unit == flume::Dimension::Unit::Percent);
  CHECK(s.borderWidth == 1.5f);
  CHECK(s.borderColor == 0xFF0000FFu);
  CHECK(s.borderRadii[0] == 2.0f);
  CHECK_FALSE(s.borderRadii[1].has_value());
  CHECK(s.shadowOffset == std::pair<float, float>{1, 2});
  CHECK(s.elevation == 3.0f);
  CHECK(s.fontFamily == "Inter");
  CHECK(s.italic);
  CHECK(s.lineHeight == 20.0f);
  CHECK(s.letterSpacing == 0.5f);
  CHECK(s.textAlign == flume::TextAlign::Center);
  CHECK(s.decoration == 3);
}

TEST_CASE("percent strings take Fabric's CSS number") {
  auto width = [](const char* text) { return flume::Style::fromValue(obj({{"width", V(text)}})).width; };
  CHECK(width("5e1%") == flume::Dimension{flume::Dimension::Unit::Percent, 50});
  CHECK(width("+.5%") == flume::Dimension{flume::Dimension::Unit::Percent, 0.5f});
  CHECK(width("50.%") == flume::Dimension{flume::Dimension::Unit::Percent, 50});
  CHECK(width("nan%") == flume::Dimension{});
  CHECK(width("0x10%") == flume::Dimension{});
  CHECK(width(" 50%\n") == flume::Dimension{flume::Dimension::Unit::Percent, 50});
  CHECK(width("50 %") == flume::Dimension{});
  CHECK(width("0e309%") == flume::Dimension{flume::Dimension::Unit::Percent, 0});
  CHECK(width(("1." + std::string(309, '0') + "%").c_str()) == flume::Dimension{flume::Dimension::Unit::Percent, 1});
}

TEST_CASE("unknown keys are collected") {
  std::vector<std::string> unknown;
  flume::Style::fromValue(obj({{"flex", V(1)}, {"foo", V(1)}}), &unknown);
  CHECK(unknown == std::vector<std::string>{"foo"});
}
