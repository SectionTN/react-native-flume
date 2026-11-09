#include "doctest.h"

#include "adapter/Viewability.h"

using namespace flume;

namespace {

const std::vector<ItemSpan> kRows = {{0, -50, 10}, {1, 10, 60}, {2, 60, 150}, {3, 150, 200}};

std::vector<ViewToken> tokens(std::initializer_list<std::pair<int, const char*>> pairs) {
  std::vector<ViewToken> out;
  for (const auto& [index, key] : pairs) out.push_back(ViewToken{index, key});
  return out;
}

}  // namespace

TEST_CASE("any visible pixel counts by default") {
  ViewabilityTracker tracker;
  CHECK(tracker.compute(100, kRows) == std::vector<int>{0, 1, 2});
}

TEST_CASE("the item threshold needs that much of the item") {
  ViewabilityTracker tracker;
  tracker.config.itemVisiblePercentThreshold = 50;
  CHECK(tracker.compute(100, kRows) == std::vector<int>{1});
}

TEST_CASE("the area threshold needs that much of the viewport") {
  ViewabilityTracker tracker;
  tracker.config.viewAreaCoveragePercentThreshold = 50;
  CHECK(tracker.compute(100, {{0, -50, 40}, {1, 40, 160}}) == std::vector<int>{1});
}

TEST_CASE("a zero viewport reports nothing") {
  ViewabilityTracker tracker;
  CHECK(tracker.compute(0, kRows).empty());
}

TEST_CASE("it waits for an interaction") {
  ViewabilityTracker tracker;
  tracker.config.waitForInteraction = true;
  CHECK(tracker.compute(100, kRows).empty());
  tracker.interacted = true;
  CHECK(tracker.compute(100, kRows) == std::vector<int>{0, 1, 2});
}

TEST_CASE("unchanged sets report nothing") {
  ViewabilityTracker tracker;
  CHECK(tracker.report(tokens({{1, "b"}, {2, "c"}})));
  CHECK_FALSE(tracker.report(tokens({{1, "b"}, {2, "c"}})));
}

TEST_CASE("a change lists exits and entries") {
  ViewabilityTracker tracker;
  tracker.report(tokens({{1, "b"}, {2, "c"}}));
  auto report = tracker.report(tokens({{2, "c"}, {3, "d"}}));
  REQUIRE(report);
  CHECK(report->viewable == tokens({{2, "c"}, {3, "d"}}));
  CHECK(report->changed == std::vector<ViewableChange>{{ViewToken{1, "b"}, false}, {ViewToken{3, "d"}, true}});
}

// A prepend above the viewport shifts every index by one while the same rows stay on screen.
TEST_CASE("rows that only moved are not reported") {
  ViewabilityTracker tracker;
  tracker.report(tokens({{0, "a"}, {1, "b"}}));
  CHECK_FALSE(tracker.report(tokens({{1, "a"}, {2, "b"}})));
}

// A message prepended to a chat that follows it keeps the indices: one row came in, one went out.
TEST_CASE("a data change reports the rows that came and went by key") {
  ViewabilityTracker tracker;
  tracker.changed({0, 1});
  tracker.report(tokens({{0, "a"}, {1, "b"}}));
  tracker.invalidate();
  CHECK(tracker.changed({0, 1}));
  auto report = tracker.report(tokens({{0, "new"}, {1, "a"}}));
  REQUIRE(report);
  CHECK(report->changed == std::vector<ViewableChange>{{ViewToken{1, "b"}, false}, {ViewToken{0, "new"}, true}});
}

// A fling carries rows past the viewport in less than minimumViewTime; only rows still in view at the deadline count.
TEST_CASE("a delayed report keeps only the rows still in view") {
  ViewabilityTracker tracker;
  tracker.config.minimumViewTimeMs = 250;
  auto key = [](int index) { return "k" + std::to_string(index); };
  auto scheduled = tracker.keyed({0, 1}, key);
  REQUIRE(scheduled);
  tracker.keyed({1, 2}, key);
  auto report = tracker.reportDelayed(*scheduled);
  REQUIRE(report);
  CHECK(report->viewable == tokens({{1, "k1"}}));
}

TEST_CASE("a delayed report carries the indices after a data change") {
  ViewabilityTracker tracker;
  tracker.config.minimumViewTimeMs = 250;
  const std::vector<std::string> before = {"a", "b"};
  const std::vector<std::string> after = {"new", "a", "b"};
  auto scheduled = tracker.keyed({0, 1}, [&](int index) { return before[index]; });
  REQUIRE(scheduled);
  tracker.invalidate();
  tracker.keyed({0, 1, 2}, [&](int index) { return after[index]; });
  auto report = tracker.reportDelayed(*scheduled);
  REQUIRE(report);
  CHECK(report->viewable == tokens({{1, "a"}, {2, "b"}}));
}

TEST_CASE("clearing the data reports the exits") {
  ViewabilityTracker tracker;
  tracker.changed({0});
  tracker.report(tokens({{0, "a"}}));
  tracker.invalidate();
  CHECK(tracker.changed({}));
  auto report = tracker.report({});
  REQUIRE(report);
  CHECK(report->changed == std::vector<ViewableChange>{{ViewToken{0, "a"}, false}});
}
