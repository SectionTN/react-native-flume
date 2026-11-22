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
  CHECK(layout.setLength(2, 150, 5).anchorMoved == 50);
  CHECK(y(layout, 5) == 550);
}

TEST_CASE("a length change at or below the anchor does not move it") {
  ListLayout layout = measured(10, 100);
  CHECK(layout.setLength(5, 150, 5).anchorMoved == 0);
  CHECK(layout.setLength(7, 150, 5).anchorMoved == 0);
}

TEST_CASE("removes, inserts and moves keep measured lengths with their items") {
  ListLayout layout = list(4);
  layout.setLength(0, 10, 0);
  layout.setLength(1, 20, 0);
  layout.setLength(2, 30, 0);
  layout.setLength(3, 40, 0);
  ListOps ops = diffKeys({"a", "b", "c", "d"}, {"x", "d", "a", "c"}, {}, true);
  REQUIRE_FALSE(ops.reset);
  layout.apply(ops, 4, kOneType, {});
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

TEST_CASE("a full-span header keeps its own row through a removal") {
  ListLayoutConfig config;
  config.lanes = 2;
  ListLayout layout = list(6, config, {2});
  ListOps removeFirst;
  removeFirst.removed = {{0, 1}};
  layout.apply(removeFirst, 5, kOneType, {1});
  CHECK(layout.itemsOfRows(1, 1) == IndexRange{1, 1});
  CHECK(layout.crossLength(1, 400) == 400);
}

TEST_CASE("a batch takes the full span of its own data, not the old rows mapped") {
  ListLayoutConfig config;
  config.lanes = 2;
  ListLayout layout = list(6, config, {2});
  ListOps prepend;
  prepend.inserted = {{0, 1}};
  layout.apply(prepend, 7, kOneType, {0});
  CHECK(layout.crossLength(0, 400) == 400);
  CHECK(layout.crossLength(3, 400) == 200);
}

TEST_CASE("a batch takes the footer's full-span row at the new count") {
  ListLayoutConfig config;
  config.lanes = 2;
  ListLayout layout = list(4, config, {0, 3});
  ListOps prepend;
  prepend.inserted = {{1, 1}};
  layout.apply(prepend, 5, kOneType, {0, 4});
  CHECK(layout.crossLength(3, 400) == 200);
  CHECK(layout.crossLength(4, 400) == 400);
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

namespace {

ListLayout inverted(int count, float length) {
  ListLayoutConfig config;
  config.inverted = true;
  return measured(count, length, config);
}

}  // namespace

TEST_CASE("an inverted list lays item 0 out at the bottom") {
  ListLayout layout = inverted(20, 100);
  CHECK(layout.contentLength(1000) == 2000);
  CHECK(y(layout, 0) == 1900);
  CHECK(y(layout, 19) == 0);
}

TEST_CASE("short inverted content sits at the bottom of the viewport") {
  ListLayout layout = inverted(3, 100);
  CHECK(layout.contentLength(1000) == 1000);
  CHECK(y(layout, 0) == 900);
  CHECK(y(layout, 2) == 700);
}

TEST_CASE("inverted: a change above the bottom anchor moves it and one below does not") {
  ListLayout layout = inverted(20, 100);
  CHECK(layout.setLength(10, 150, 3).anchorMoved == 50);
  CHECK(layout.setLength(1, 150, 3).anchorMoved == 0);
}

TEST_CASE("the sticky header at or above the viewport start pins there") {
  ListLayout layout = measured(10, 100);
  auto pin = layout.pinned({0, 5}, 250, 1000);
  REQUIRE(pin.has_value());
  CHECK(pin->index == 0);
  CHECK(pin->start == 250);
}

TEST_CASE("the next sticky header pushes the pinned one out") {
  ListLayout layout = measured(10, 100);
  auto pin = layout.pinned({0, 5}, 450, 1000);
  REQUIRE(pin.has_value());
  CHECK(pin->index == 0);
  CHECK(pin->start == 400);
}

TEST_CASE("no header pins while the first one is below the viewport start") {
  ListLayout layout = measured(10, 100);
  CHECK_FALSE(layout.pinned({3}, 100, 1000).has_value());
}

TEST_CASE("an inverted list pins the first header at or above the top row") {
  ListLayout layout = inverted(20, 100);
  auto pin = layout.pinned({4, 9, 14}, 900, 1000);
  REQUIRE(pin.has_value());
  CHECK(pin->index == 14);
  CHECK(pin->start == 900);
  auto pushed = layout.pinned({4, 9, 14}, 950, 1000);
  REQUIRE(pushed.has_value());
  CHECK(pushed->index == 14);
  CHECK(pushed->start == 900);
}

TEST_CASE("inverted: the bottom anchor's own growth keeps its bottom edge") {
  ListLayout layout = inverted(20, 100);
  CHECK(layout.setLength(3, 150, 3, 1000).anchorMoved == 50);
}

TEST_CASE("short inverted content does not move the anchor") {
  ListLayout layout = inverted(3, 100);
  CHECK(layout.setLength(0, 150, 0, 1000).anchorMoved == 0);
  CHECK(layout.setLength(2, 150, 0, 1000).anchorMoved == 0);
}

TEST_CASE("a row cut by the top edge is not the anchor, so it grows out of sight") {
  ListLayout layout = measured(10, 100);
  CHECK(layout.anchorAt(300, 1000) == 3);
  CHECK(layout.anchorAt(250, 1000) == 3);
  CHECK(layout.setLength(2, 150, layout.anchorAt(250, 1000), 1000).anchorMoved == 50);
}

TEST_CASE("inverted: a row cut by the bottom edge is not the anchor, so it grows out of sight") {
  ListLayout layout = inverted(20, 100);
  CHECK(layout.anchorAt(950, 1000) == 1);
  CHECK(layout.setLength(0, 150, layout.anchorAt(950, 1000), 1000).anchorMoved == 0);
}

TEST_CASE("inverted: the newest row ending at the viewport's end stays the anchor") {
  ListLayout layout = inverted(20, 100);
  CHECK(layout.anchorAt(1000, 1000) == 0);
}

TEST_CASE("apply reports the changed items whose type changed, to be measured again") {
  std::vector<int> types = {0, 0, 0};
  const ListLayout::TypeOf typeOf = [&types](int index) { return types[index]; };
  ListLayout layout;
  layout.configure({}, {});
  layout.reset(3, typeOf);
  for (int i = 0; i < 3; i++) layout.setLength(i, 100, 0);
  types[1] = 2;
  ListOps ops;
  ops.changed = {0, 1};
  CHECK(layout.apply(ops, 3, typeOf, {}) == std::vector<int>{1});
  CHECK(layout.measured(0));
  CHECK_FALSE(layout.measured(1));
}

TEST_CASE("in a grid an item shorter than its row leaves the content length") {
  ListLayoutConfig config;
  config.lanes = 2;
  ListLayout layout = list(4, config);
  CHECK(layout.setLength(0, 100, 0, 1000).contentMoved == 50);
  CHECK(layout.setLength(1, 80, 0, 1000).contentMoved == 0);
}

TEST_CASE("an inverted list shorter than its viewport keeps its content length") {
  ListLayoutConfig config;
  config.inverted = true;
  ListLayout layout = list(2, config);
  LengthChange change = layout.setLength(0, 100, 0, 500);
  CHECK(change.contentMoved == 0);
  CHECK(change.anchorMoved == 0);
}

namespace {

ListLayoutConfig separated(float size = 2) {
  ListLayoutConfig config;
  config.separator = size;
  return config;
}

}  // namespace

TEST_CASE("a separator moves the rows after it by its size") {
  ListLayout layout = measured(4, 50, separated());
  CHECK(y(layout, 1) == 52);
  CHECK(y(layout, 3) == 156);
  CHECK(layout.contentLength(1000) == 206);
}

TEST_CASE("the last row has no separator") {
  ListLayout layout = measured(4, 50, separated());
  CHECK_FALSE(layout.separatorRect(3, 400));
}

TEST_CASE("a list's separator sits below its row") {
  ListLayout layout = measured(4, 50, separated());
  CHECK(*layout.separatorRect(0, 400) == ItemRect{0, 50, 400, 2});
}

TEST_CASE("an inverted list's separator sits above its row") {
  ListLayoutConfig config = separated();
  config.inverted = true;
  ListLayout layout = measured(4, 50, config);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{0, -2, 400, 2});
}

TEST_CASE("a horizontal list's separator sits right of its row") {
  ListLayoutConfig config = separated();
  config.horizontal = true;
  ListLayout layout = measured(4, 50, config);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{50, 0, 2, 400});
}

TEST_CASE("an inverted horizontal list's separator sits left of its row") {
  ListLayoutConfig config = separated();
  config.horizontal = true;
  config.inverted = true;
  ListLayout layout = measured(4, 50, config);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{-2, 0, 2, 400});
}

TEST_CASE("the insets keep the line inside the padded cross range") {
  ListLayoutConfig config = separated();
  config.paddingCrossStart = 10;
  config.paddingCrossEnd = 10;
  config.separatorInsetStart = 16;
  config.separatorInsetEnd = 8;
  ListLayout layout = measured(4, 50, config);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{16, 50, 356, 2});
}

TEST_CASE("every cell of a grid row carries the row's separator") {
  ListLayoutConfig config = separated();
  config.lanes = 2;
  ListLayout layout = list(4, config);
  layout.setLength(0, 50, 0);
  layout.setLength(1, 80, 0);
  layout.setLength(2, 50, 0);
  layout.setLength(3, 50, 0);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{0, 80, 200, 2});
  CHECK(*layout.separatorRect(1, 400) == ItemRect{0, 80, 200, 2});
}

TEST_CASE("a partial last grid row has no separator") {
  ListLayoutConfig config = separated();
  config.lanes = 2;
  ListLayout layout = measured(3, 50, config);
  CHECK_FALSE(layout.separatorRect(2, 400));
  CHECK(layout.separatorRect(0, 400));
  CHECK(layout.separatorRect(1, 400));
}

TEST_CASE("separators run between data rows only") {
  ListLayoutConfig config = separated();
  config.dataStart = 1;
  config.afterData = 1;
  ListLayout layout = measured(5, 50, config);
  CHECK_FALSE(layout.separatorRect(0, 400));
  CHECK(layout.separatorRect(1, 400));
  CHECK(layout.separatorRect(2, 400));
  CHECK_FALSE(layout.separatorRect(3, 400));
  CHECK_FALSE(layout.separatorRect(4, 400));
  CHECK(y(layout, 2) == 102);
  CHECK(y(layout, 4) == 204);
  CHECK(layout.contentLength(1000) == 254);
}

TEST_CASE("the empty row fills the viewport the header and footer leave") {
  ListLayoutConfig config = separated();
  config.paddingStart = 10;
  config.paddingEnd = 20;
  config.dataStart = 1;
  config.afterData = 1;
  config.emptyIndex = 1;
  ListLayout layout = list(3, config);
  layout.setLength(0, 100, 0);
  layout.setLength(2, 60, 0);
  CHECK(layout.emptyMinLength(800) == 610);
  CHECK(layout.emptyMinLength(150) == 0);
  config.emptyIndex = -1;
  layout.configure(config, {});
  CHECK(layout.emptyMinLength(800) == 0);
}

TEST_CASE("a zero separator reserves nothing") {
  ListLayout layout = measured(4, 50);
  CHECK(y(layout, 1) == 50);
  CHECK_FALSE(layout.separatorRect(0, 400));
}

TEST_CASE("a grid target past the first column lands on its whole row") {
  ListLayoutConfig config;
  config.lanes = 3;
  ListLayout layout = list(7, config);
  CHECK(layout.rowCount() == 3);
  CHECK(layout.rowOf(4) == 1);
  CHECK(layout.itemsOfRows(1, 1) == IndexRange{3, 5});
  CHECK(layout.itemsOfRows(2, 2) == IndexRange{6, 6});
}

TEST_CASE("a short list that stacks from the end sits at the end") {
  ListLayoutConfig config;
  config.stackFromEnd = true;
  ListLayout layout = measured(2, 50, config);
  CHECK(layout.rectOf(0, 400, 500).y == 400);
  CHECK(layout.rectOf(1, 400, 500).y == 450);
  CHECK(layout.contentLength(500) == 500);
}

TEST_CASE("a long list that stacks from the end starts at the top") {
  ListLayoutConfig config;
  config.stackFromEnd = true;
  ListLayout layout = measured(20, 50, config);
  CHECK(layout.rectOf(0, 400, 500).y == 0);
  CHECK(layout.contentLength(500) == 1000);
}

TEST_CASE("a list that stacks from the end keeps its last row in view") {
  ListLayoutConfig config;
  config.stackFromEnd = true;
  ListLayout layout = measured(20, 50, config);
  CHECK(layout.anchorAt(100, 300) == 7);
  CHECK(layout.setLength(3, 70, 7, 300).anchorMoved == 20);
  ListLayout fresh = measured(20, 50, config);
  CHECK(fresh.setLength(7, 70, 7, 300).anchorMoved == 20);
}

TEST_CASE("the anchor edge is the bottom when inverted or stacking from the end") {
  CHECK(measured(4, 50).anchorEdgeOf(1, 1000) == 50);
  ListLayoutConfig inverted;
  inverted.inverted = true;
  CHECK(measured(4, 50, inverted).anchorEdgeOf(1, 1000) == 950);
  ListLayoutConfig stacked;
  stacked.stackFromEnd = true;
  CHECK(measured(4, 50, stacked).anchorEdgeOf(1, 1000) == 900);
}

namespace {

// A 3-lane grid 300 wide: 16 in from each side, 8 between columns.
ListLayoutConfig gapped() {
  ListLayoutConfig config;
  config.lanes = 3;
  config.crossGap = 8;
  config.laneInsetStart = 16;
  config.laneInsetEnd = 16;
  return config;
}

}  // namespace

TEST_CASE("lanes take the lane insets and the column gaps out of the row") {
  ListLayout layout = measured(3, 50, gapped());
  CHECK(layout.laneLength(300) == 84);
  CHECK(layout.crossLength(0, 300) == 84);
  CHECK(layout.rectOf(0, 300, 1000) == ItemRect{16, 0, 84, 50});
  CHECK(layout.rectOf(1, 300, 1000) == ItemRect{108, 0, 84, 50});
  CHECK(layout.rectOf(2, 300, 1000) == ItemRect{200, 0, 84, 50});
}

TEST_CASE("the header spans the padded width and a full-span data row keeps the lane insets") {
  ListLayoutConfig config = gapped();
  config.dataStart = 1;
  ListLayout layout = list(5, config, {0, 4});
  CHECK(layout.rectOf(0, 300, 1000).x == 0);
  CHECK(layout.rectOf(0, 300, 1000).width == 300);
  CHECK(layout.rectOf(1, 300, 1000).x == 16);
  CHECK(layout.rectOf(4, 300, 1000).x == 16);
  CHECK(layout.rectOf(4, 300, 1000).width == 268);
  CHECK(layout.crossLength(4, 300) == 268);
}

TEST_CASE("the main gap follows every row but the last, after the line, header and footer included") {
  ListLayoutConfig config = separated(1);
  config.mainGap = 8;
  config.dataStart = 1;
  config.afterData = 1;
  ListLayout layout = list(5, config, {0, 4});
  for (int i = 0; i < 5; i++) layout.setLength(i, 50, 0);
  CHECK(y(layout, 1) == 58);
  CHECK(y(layout, 2) == 117);
  CHECK(y(layout, 3) == 176);
  CHECK(y(layout, 4) == 234);
  CHECK(layout.contentLength(0) == 284);
  CHECK(layout.rowStride(1) == 59);
  CHECK(layout.rowStride(4) == 50);
  CHECK(*layout.separatorRect(1, 400) == ItemRect{0, 50, 400, 1});
}

TEST_CASE("grid rows add the grid row gap after the line") {
  ListLayoutConfig config = separated(1);
  config.lanes = 2;
  config.gridRowGap = 12;
  ListLayout layout = measured(4, 50, config);
  CHECK(y(layout, 2) == 63);
  CHECK(layout.contentLength(0) == 113);
}

TEST_CASE("an inverted list keeps the line next to its row and the gap beyond it") {
  ListLayoutConfig config = separated(1);
  config.inverted = true;
  config.mainGap = 8;
  ListLayout layout = measured(2, 50, config);
  CHECK(y(layout, 0) == 950);
  CHECK(y(layout, 1) == 891);
  CHECK(*layout.separatorRect(0, 400) == ItemRect{0, -1, 400, 1});
}

TEST_CASE("the line runs across the column gaps and the lane insets") {
  ListLayoutConfig config = gapped();
  config.separator = 1;
  ListLayout layout = measured(6, 50, config);
  CHECK(*layout.separatorRect(0, 300) == ItemRect{-16, 50, 100, 1});
  CHECK(*layout.separatorRect(1, 300) == ItemRect{-8, 50, 100, 1});
  CHECK(*layout.separatorRect(2, 300) == ItemRect{0, 50, 100, 1});
}

TEST_CASE("a pushed sticky header ends with its line at the pusher, over the gap") {
  ListLayout layout = measured(10, 100, separated(2));
  CHECK(layout.pinned({0, 5}, 450, 1000)->start == 408);
  ListLayoutConfig config = separated(2);
  config.mainGap = 8;
  ListLayout gapped = measured(10, 100, config);
  CHECK(gapped.pinned({0, 5}, 500, 1000)->start == 448);
}

TEST_CASE("the empty row leaves the viewport the other rows and every gap leave") {
  ListLayoutConfig config;
  config.mainGap = 8;
  config.dataStart = 1;
  config.afterData = 1;
  config.emptyIndex = 1;
  ListLayout layout = list(3, config, {0, 1, 2});
  layout.setLength(0, 50, 0);
  layout.setLength(2, 50, 0);
  CHECK(layout.emptyMinLength(1000) == 884);
}

TEST_CASE("an inverted pinned header ends at the pusher's line, which sits above the pusher") {
  ListLayoutConfig config = separated(2);
  config.inverted = true;
  ListLayout layout = measured(10, 100, config);
  CHECK(layout.pinned({2, 7}, 650, 1000)->start == 612);
}

TEST_CASE("every cell of a grid row is as long as the row's longest and keeps its own length") {
  ListLayoutConfig config;
  config.lanes = 3;
  ListLayout layout = list(3, config);
  layout.setLength(0, 100, 0);
  layout.setLength(1, 150, 0);
  layout.setLength(2, 120, 0);
  for (int i = 0; i < 3; i++) CHECK(length(layout, i) == 150);
  CHECK(layout.itemLength(0) == 100);
  layout.setLength(1, 90, 0);
  for (int i = 0; i < 3; i++) CHECK(length(layout, i) == 120);
  CHECK(layout.itemLength(1) == 90);
}
