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
  CHECK_FALSE(parseColor("blue").has_value());
  CHECK_FALSE(parseColor("#12345").has_value());
  CHECK_FALSE(parseColor("").has_value());
}
