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
  CHECK(s.padding[YGEdgeAll] == 12.0f);
  CHECK(s.gap == 12.0f);
  CHECK(s.alignItems == YGAlignCenter);
  CHECK(s.fontWeight == 700);
  CHECK(s.widthPercent == 50.0f);
  CHECK_FALSE(s.width.has_value());
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
  CHECK(s.padding[YGEdgeHorizontal] == 8.0f);
  CHECK(s.margin[YGEdgeStart] == 4.0f);
  CHECK(s.heightPercent == 25.0f);
  CHECK(s.numberOfLines == 2);
  CHECK(s.opacity == 0.5f);
}

TEST_CASE("default styles are equal") { CHECK(flume::Style{} == flume::Style{}); }
