#include "doctest.h"

#include "core/Color.h"

using flume::parseColor;

TEST_CASE("hex colors") {
  CHECK(parseColor("#0066ff") == 0xFF0066FFu);
  CHECK(parseColor("#06f") == 0xFF0066FFu);
  CHECK(parseColor("#0066ff80") == 0x800066FFu);
  CHECK(parseColor("#06f8") == 0x880066FFu);
}

TEST_CASE("functional colors and transparent") {
  CHECK(parseColor("rgb(17, 17, 17)") == 0xFF111111u);
  CHECK(parseColor("rgba(0, 0, 0, 0.5)") == 0x80000000u);
  CHECK(parseColor("transparent") == 0x00000000u);
}

TEST_CASE("unsupported strings are skipped") {
  CHECK(parseColor("blue") == 0xFF0000FFu);
  CHECK_FALSE(parseColor("#12345").has_value());
  CHECK_FALSE(parseColor("").has_value());
}

TEST_CASE("keywords, hsl and hwb follow normalize-colors") {
  CHECK(flume::parseColor("red") == 0xFFFF0000u);
  CHECK(flume::parseColor("rebeccapurple") == 0xFF663399u);
  CHECK(flume::parseColor("hsl(120, 100%, 50%)") == 0xFF00FF00u);
  CHECK(flume::parseColor("hsla(120, 100%, 50%, 0.5)") == 0x8000FF00u);
  CHECK(flume::parseColor("hwb(0, 0%, 0%)") == 0xFFFF0000u);
  CHECK(flume::parseColor("rgba(255, 0, 0, .5)") == 0x80FF0000u);
  CHECK_FALSE(flume::parseColor("Red").has_value());
  CHECK_FALSE(flume::parseColor("blue ").has_value());
}
