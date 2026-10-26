#include "doctest.h"

#include "core/OpBatch.h"

using flume::OpBatch;
using flume::Style;

TEST_CASE("a view style carries the shadow with React Native's defaults") {
  OpBatch batch;
  batch.viewStyle(3, Style{});
  CHECK(batch.ints == std::vector<int32_t>{4, 3, 0, int32_t(0xFF000000u), int32_t(0xFF000000u)});
  CHECK(batch.floats == std::vector<float>{1, 0, 0, 0, 0, 0, 0, 0, 3, 0, -3});
}

TEST_CASE("a view style carries a set shadow") {
  Style style;
  style.shadowColor = 0xFFFF0000u;
  style.shadowOpacity = 0.5f;
  style.shadowRadius = 6;
  style.shadowOffset = std::pair<float, float>{2, 4};
  OpBatch batch;
  batch.viewStyle(3, style);
  CHECK(batch.ints.back() == int32_t(0xFFFF0000u));
  CHECK(std::vector<float>(batch.floats.end() - 4, batch.floats.end()) == std::vector<float>{0.5f, 6, 2, 4});
}
