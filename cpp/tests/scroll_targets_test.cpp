#include "doctest.h"

#include "adapter/ScrollTargets.h"

using namespace flume;

namespace {

const std::function<float(int)> kRows100 = [](int) { return 100.0f; };

}  // namespace

TEST_CASE("an index past a list that shrank clamps to its last row") {
  CHECK(*clampIndex(7, 5) == 4);
  CHECK(*clampIndex(-1, 5) == 0);
  CHECK_FALSE(clampIndex(0, 0));
}

TEST_CASE("viewPosition places the item from the start edge in content order") {
  CHECK(alignedViewportStart(2000, 100, 1000, 0, 0, false) == 2000);
  CHECK(alignedViewportStart(2000, 100, 1000, 1, 0, false) == 1100);
  CHECK(alignedViewportStart(2000, 100, 1000, 0.5f, 10, false) == 1540);
  CHECK(alignedViewportStart(2000, 100, 1000, 0, 0, true) == 1100);
  CHECK(alignedViewportStart(2000, 100, 1000, 1, 0, true) == 2000);
}

TEST_CASE("an offset clamps to the scrollable range with the insets") {
  CHECK(clampOffset(-100, 2000, 800, 50, 30) == -50);
  CHECK(clampOffset(5000, 2000, 800, 50, 30) == 1230);
  CHECK(clampOffset(100, 300, 800, 50, 30) == -50);
}

TEST_CASE("a landing covers the screen after the target and two rows each side") {
  CHECK(landingRows(50, 0, 1000, 100, kRows100) == IndexRange{48, 61});
}

TEST_CASE("an end-aligned landing covers the screen above the target") {
  CHECK(landingRows(50, 1, 1000, 100, kRows100) == IndexRange{39, 52});
}

TEST_CASE("the landing stays inside the list") {
  CHECK(landingRows(0, 0, 1000, 100, kRows100).first == 0);
  CHECK(landingRows(99, 0, 1000, 100, kRows100).last == 99);
}

TEST_CASE("a glide keeps the rows nearest the landing") {
  CHECK(glideWindow({0, 11}, {500, 511}) == IndexRange{464, 511});
  CHECK(glideWindow({600, 611}, {500, 511}) == IndexRange{500, 547});
  CHECK(glideWindow({0, 11}, {19, 30}) == IndexRange{0, 30});
}

TEST_CASE("a removal at a chat's newest end finds the row it reveals measured") {
  CHECK(batchRows({50, 59}, 1000, 100, kRows100) == IndexRange{40, 69});
}

TEST_CASE("the rows past each edge count their measured lengths") {
  const std::function<float(int)> rows50 = [](int) { return 50.0f; };
  CHECK(batchRows({50, 59}, 1000, 100, rows50) == IndexRange{30, 79});
}

TEST_CASE("the rows a batch can reveal stay inside the list") {
  CHECK(batchRows({0, 9}, 1000, 15, kRows100) == IndexRange{0, 14});
}

TEST_CASE("a far target jumps to one viewport before it first") {
  CHECK(*preJumpOffset(0, 5000, 1000) == 4000);
  CHECK(*preJumpOffset(9930, 5000, 1000) == 6000);
  CHECK_FALSE(preJumpOffset(3500, 5000, 1000));
}

TEST_CASE("a chat keeps its newest rows in view when a keyboard shrinks the list") {
  CHECK(offsetAfterViewportChange(true, 1000, 800, 500, 0, 5000) == doctest::Approx(1300));
  CHECK(offsetAfterViewportChange(true, 1000, 500, 800, 0, 5000) == doctest::Approx(700));
}

TEST_CASE("an upright list keeps its offset when the viewport shrinks") {
  CHECK(offsetAfterViewportChange(false, 1000, 800, 500, 0, 5000) == doctest::Approx(1000));
}

TEST_CASE("the kept end stays inside the scroll range") {
  CHECK(offsetAfterViewportChange(true, 4900, 800, 500, 0, 5000) == doctest::Approx(5000));
  CHECK(offsetAfterViewportChange(true, 100, 500, 800, -40, 5000) == doctest::Approx(-40));
}

TEST_CASE("an initial index past the data opens at the content end") {
  CHECK(*openingLanding(3, 10) == Landing{3, 0, false});
  CHECK(openingLanding(40, 10)->contentEnd);
  CHECK_FALSE(openingLanding(-1, 10));
  CHECK_FALSE(openingLanding(3, 0));
}
