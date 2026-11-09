#include "doctest.h"

#include "adapter/EdgeTracker.h"

using namespace flume;

namespace {

EdgeHits update(EdgeTracker& tracker, float offset, float range, float extent, int count, double threshold) {
  return tracker.update(edgeDistances(offset, range, extent, 0, 0, false), count, 500, threshold, threshold);
}

}  // namespace

TEST_CASE("an edge fires once per content and re-arms outside the threshold") {
  EdgeTracker tracker;
  EdgeHits hits = update(tracker, 1400, 2000, 500, 40, 2);
  CHECK((!hits.start && hits.end));
  hits = update(tracker, 1400, 2000, 500, 40, 2);
  CHECK((!hits.start && !hits.end));
  hits = update(tracker, 0, 2000, 500, 40, 0.5);
  CHECK((hits.start && !hits.end));
  hits = update(tracker, 1400, 2000, 500, 40, 2);
  CHECK((!hits.start && hits.end));
}

TEST_CASE("an edge fires again when items are added") {
  EdgeTracker tracker;
  update(tracker, 1400, 2000, 500, 40, 2);
  CHECK(update(tracker, 1400, 2200, 500, 44, 2).end);
}

// The content length moves as estimates resolve, so the rule counts items.
TEST_CASE("a content length that moves with the rows fires once per item count") {
  EdgeTracker tracker;
  CHECK(update(tracker, 1400, 2000, 500, 40, 2).end);
  CHECK_FALSE(update(tracker, 1480, 2100, 500, 40, 2).end);
}

TEST_CASE("a short list fires both edges once") {
  EdgeTracker tracker;
  EdgeHits hits = update(tracker, 0, 300, 300, 5, 2);
  CHECK((hits.start && hits.end));
  hits = update(tracker, 0, 300, 300, 5, 2);
  CHECK((!hits.start && !hits.end));
}

TEST_CASE("an empty list fires nothing") {
  EdgeTracker tracker;
  EdgeHits hits = update(tracker, 0, 0, 0, 0, 2);
  CHECK((!hits.start && !hits.end));
}

TEST_CASE("inverted swaps the distances") {
  EdgeDistances inverted = edgeDistances(100, 1000, 300, 0, 0, true);
  CHECK(inverted.fromStart == 600);
  CHECK(inverted.fromEnd == 100);
  EdgeDistances upright = edgeDistances(100, 1000, 300, 0, 0, false);
  CHECK(upright.fromStart == 100);
  CHECK(upright.fromEnd == 600);
}

TEST_CASE("distances count from the visible area inside the insets") {
  CHECK(edgeDistances(-50, 1000, 500, 50, 100, false).fromStart == 0);
  CHECK(edgeDistances(600, 1000, 500, 50, 100, false).fromEnd == 0);
}
