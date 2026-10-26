#include "doctest.h"

#include "adapter/ListLayout.h"
#include "core/KeyDiff.h"

using namespace flume;

namespace {

const ListLayout::TypeOf kOneType = [](int) { return 0; };

ListLayout list(int count, ListLayoutConfig config = {}, std::vector<int> fullSpan = {}) {
  ListLayout layout;
  layout.configure(config, std::move(fullSpan));
  layout.reset(count, kOneType);
  return layout;
}

ListLayout measured(int count, float length, ListLayoutConfig config = {}) {
  ListLayout layout = list(count, config);
  for (int i = 0; i < count; i++) layout.setLength(i, length, 0);
  return layout;
}

float y(const ListLayout& layout, int index) { return layout.rectOf(index, 400, 1000).y; }
float length(const ListLayout& layout, int index) { return layout.rectOf(index, 400, 1000).height; }

}  // namespace

TEST_CASE("unmeasured items stack at the floor") {
  ListLayout layout = list(3);
  CHECK(y(layout, 0) == 0);
  CHECK(y(layout, 1) == 50);
  CHECK(y(layout, 2) == 100);
  CHECK(layout.contentLength(500) == 150);
}

TEST_CASE("estimatedItemSize sets every estimate") {
  ListLayoutConfig config;
  config.estimatedItemLength = 80;
  ListLayout layout = list(3, config);
  CHECK(y(layout, 1) == 80);
  CHECK(y(layout, 2) == 160);
}

TEST_CASE("a type's estimate is the mean of its measured items above the floor") {
  ListLayout layout = list(4);
  layout.setLength(0, 80, 0);
  layout.setLength(1, 120, 0);
  CHECK(layout.estimate(0) == 100);
  ListLayout small = list(2);
  small.setLength(0, 30, 0);
  CHECK(small.estimate(0) == 50);
}

TEST_CASE("an item keeps its estimate until it is measured") {
  ListLayout layout = list(3);
  layout.setLength(0, 200, 0);
  CHECK(length(layout, 1) == 50);
  CHECK(y(layout, 2) == 250);
  CHECK_FALSE(layout.measured(1));
  CHECK(layout.measured(0));
}

TEST_CASE("a length change above the anchor moves the anchor by the change") {
  ListLayout layout = measured(10, 100);
  CHECK(layout.setLength(2, 150, 5) == 50);
  CHECK(y(layout, 5) == 550);
}

TEST_CASE("a length change at or below the anchor does not move it") {
  ListLayout layout = measured(10, 100);
  CHECK(layout.setLength(5, 150, 5) == 0);
  CHECK(layout.setLength(7, 150, 5) == 0);
}

TEST_CASE("removes, inserts and moves keep measured lengths with their items") {
  ListLayout layout = list(4);
  layout.setLength(0, 10, 0);
  layout.setLength(1, 20, 0);
  layout.setLength(2, 30, 0);
  layout.setLength(3, 40, 0);
  ListOps ops = diffKeys({"a", "b", "c", "d"}, {"x", "d", "a", "c"}, {}, true);
  REQUIRE_FALSE(ops.reset);
  layout.apply(ops, 4, kOneType);
  CHECK(layout.count() == 4);
  CHECK(length(layout, 0) == 50);
  CHECK(length(layout, 1) == 40);
  CHECK(length(layout, 2) == 10);
  CHECK(length(layout, 3) == 30);
  CHECK_FALSE(layout.measured(0));
  CHECK(layout.measured(1));
}

TEST_CASE("a reset makes every item unmeasured") {
  ListLayout layout = measured(3, 100);
  layout.reset(3, kOneType);
  CHECK_FALSE(layout.measured(0));
  CHECK(length(layout, 0) == 100);
}

TEST_CASE("items are found by offset inside the padding") {
  ListLayoutConfig config;
  config.paddingStart = 20;
  ListLayout layout = measured(10, 100, config);
  CHECK(layout.indexAt(0, 1000) == 0);
  CHECK(layout.indexAt(119, 1000) == 0);
  CHECK(layout.indexAt(120, 1000) == 1);
  CHECK(layout.indexAt(5000, 1000) == 9);
  CHECK(layout.itemsIn(150, 350, 1000) == std::vector<int>{1, 2, 3});
}

TEST_CASE("a grid row takes its longest cell and a full-span item a row of its own") {
  ListLayoutConfig config;
  config.lanes = 2;
  ListLayout layout = list(5, config, {0});
  const float lengths[] = {40, 100, 60, 80, 70};
  for (int i = 0; i < 5; i++) layout.setLength(i, lengths[i], 0);
  CHECK(y(layout, 0) == 0);
  CHECK(y(layout, 1) == 40);
  CHECK(y(layout, 2) == 40);
  CHECK(y(layout, 3) == 140);
  CHECK(y(layout, 4) == 140);
  CHECK(layout.contentLength(1000) == 220);
}

TEST_CASE("lanes split the cross axis inside the cross padding") {
  ListLayoutConfig config;
  config.lanes = 2;
  config.paddingCrossStart = 10;
  config.paddingCrossEnd = 10;
  ListLayout layout = list(3, config, {0});
  CHECK(layout.rectOf(1, 400, 1000).x == 10);
  CHECK(layout.rectOf(1, 400, 1000).width == 190);
  CHECK(layout.rectOf(2, 400, 1000).x == 200);
  CHECK(layout.rectOf(2, 400, 1000).width == 190);
  CHECK(layout.rectOf(0, 400, 1000).x == 10);
  CHECK(layout.rectOf(0, 400, 1000).width == 380);
  CHECK(layout.crossLength(1, 400) == 190);
  CHECK(layout.crossLength(0, 400) == 380);
}

TEST_CASE("a horizontal list lays rows out along x") {
  ListLayoutConfig config;
  config.horizontal = true;
  config.paddingCrossStart = 8;
  config.paddingCrossEnd = 8;
  ListLayout layout = measured(3, 100, config);
  ItemRect rect = layout.rectOf(1, 300, 1000);
  CHECK(rect.x == 100);
  CHECK(rect.y == 8);
  CHECK(rect.width == 100);
  CHECK(rect.height == 284);
}

TEST_CASE("invalidated lengths keep positions and become unmeasured") {
  ListLayout layout = measured(3, 100);
  layout.invalidateLengths();
  CHECK_FALSE(layout.measured(0));
  CHECK(y(layout, 2) == 200);
}
