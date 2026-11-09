#include "doctest.h"

#include "adapter/ScrollEvents.h"

using namespace flume;

namespace {

using K = ScrollEventKind;

}  // namespace

TEST_CASE("a throttle of a frame or less sends every scroll") {
  ScrollEvents events;
  events.setThrottleMs(16);
  CHECK(events.scrolled(1000));
  CHECK(events.scrolled(1005));
  CHECK(events.scrolled(1010));
}

TEST_CASE("a longer throttle sends the first scroll after the interval") {
  ScrollEvents events;
  events.setThrottleMs(100);
  CHECK(events.scrolled(1000));
  CHECK_FALSE(events.scrolled(1050));
  CHECK(events.scrolled(1101));
}

TEST_CASE("a negative throttle sends the first scroll and forced ones only") {
  ScrollEvents events;
  events.setThrottleMs(-1);
  CHECK(events.scrolled(1000));
  CHECK_FALSE(events.scrolled(9000));
  events.willBeginDragging();
  CHECK(events.scrolled(9100));
}

TEST_CASE("a drag edge sends the next scroll inside the interval") {
  ScrollEvents events;
  events.setThrottleMs(100);
  CHECK(events.scrolled(1000));
  events.didEndDragging(true);
  CHECK(events.scrolled(1010));
}

TEST_CASE("a drag that flings sends begin, end, momentum begin and momentum end") {
  ScrollEvents events;
  CHECK(events.willBeginDragging() == std::vector<K>{K::BeginDrag});
  CHECK(events.willEndDragging() == std::vector<K>{K::EndDrag});
  events.didEndDragging(true);
  CHECK(events.willBeginDecelerating() == std::vector<K>{K::MomentumBegin});
  CHECK(events.didEndDecelerating() == std::vector<K>{K::MomentumEnd});
}

TEST_CASE("a programmatic scroll ends with a scroll and a momentum end, never a begin") {
  ScrollEvents events;
  CHECK(events.scrolledProgrammatically(true).empty());
  CHECK(events.didEndScrollingAnimation() == std::vector<K>{K::Scroll, K::MomentumEnd});
  CHECK(events.scrolledProgrammatically(false) == std::vector<K>{K::Scroll, K::MomentumEnd});
}

TEST_CASE("leaving the window ends a fling, and at rest sends nothing") {
  ScrollEvents events;
  CHECK(events.leftWindow().empty());
  events.willBeginDragging();
  events.willEndDragging();
  events.didEndDragging(true);
  events.willBeginDecelerating();
  CHECK(events.leftWindow() == std::vector<K>{K::MomentumEnd});
  CHECK(events.leftWindow().empty());
}

TEST_CASE("an inverted offset counts from the content start") {
  CHECK(contentStartOffset(100, 1000, 300, false) == 100);
  CHECK(contentStartOffset(100, 1000, 300, true) == 600);
}
