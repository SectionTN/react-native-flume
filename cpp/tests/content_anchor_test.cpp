#include "doctest.h"

#include "adapter/ContentAnchor.h"

using namespace flume;

namespace {

AnchorState at(float top, float bottom) { return AnchorState{top, bottom, 1000}; }

}  // namespace

TEST_CASE("the default keeps the anchor") {
  CHECK(decideAnchor(AnchorConfig{}, at(0, 0), true, true).outcome == AnchorOutcome::KeepAnchor);
}

TEST_CASE("disabled keeps the offset") {
  AnchorConfig config;
  config.disabled = true;
  config.autoscrollToBottomThreshold = 0.2;
  CHECK(decideAnchor(config, at(500, 0), false, true).outcome == AnchorOutcome::KeepOffset);
}

TEST_CASE("a batch before the opening lands keeps the offset the opening will set") {
  AnchorConfig config;
  config.autoscrollToBottomThreshold = 0.2;
  const AnchorState pending{0, 50000, 1000, true};
  CHECK(decideAnchor(config, pending, true, true).outcome == AnchorOutcome::KeepOffset);
}

TEST_CASE("an insert at the bottom within the threshold scrolls there") {
  AnchorConfig config;
  config.autoscrollToBottomThreshold = 0.2;
  const AnchorDecision near = decideAnchor(config, at(500, 150), false, true);
  CHECK(near.outcome == AnchorOutcome::ScrollToEdge);
  CHECK(near.edge == VisualEdge::Bottom);
  CHECK(near.animated == config.animateAutoScrollToBottom);
  CHECK(decideAnchor(config, at(500, 250), false, true).outcome == AnchorOutcome::KeepAnchor);
}

TEST_CASE("an insert at the top within the threshold scrolls there, always animated") {
  AnchorConfig config;
  config.autoscrollToTopThreshold = 0.2;
  config.animateAutoScrollToBottom = false;
  const AnchorDecision decision = decideAnchor(config, at(100, 500), true, false);
  CHECK(decision.outcome == AnchorOutcome::ScrollToEdge);
  CHECK(decision.edge == VisualEdge::Top);
  CHECK(decision.animated);
}

TEST_CASE("the bottom wins when both edges qualify") {
  AnchorConfig config;
  config.autoscrollToTopThreshold = 0.2;
  config.autoscrollToBottomThreshold = 0.2;
  const AnchorDecision decision = decideAnchor(config, at(0, 0), true, true);
  CHECK(decision.outcome == AnchorOutcome::ScrollToEdge);
  CHECK(decision.edge == VisualEdge::Bottom);
}

TEST_CASE("a zero threshold scrolls only from the edge itself") {
  AnchorConfig config;
  config.autoscrollToBottomThreshold = 0;
  CHECK(decideAnchor(config, at(500, 0), false, true).outcome == AnchorOutcome::ScrollToEdge);
  CHECK(decideAnchor(config, at(500, 1), false, true).outcome == AnchorOutcome::KeepAnchor);
}

TEST_CASE("an old index follows removes, inserts and moves as the model applies them") {
  ListOps ops;
  ops.removed = {{2, 2}};
  ops.inserted = {{0, 1}};
  ops.moved = {{5, 1}};
  CHECK(*mapIndex(ops, 6) == 1);
  CHECK(*mapIndex(ops, 0) == 2);
  CHECK(*mapIndex(ops, 4) == 4);
  CHECK(*mapIndex(ops, 9) == 8);
  CHECK_FALSE(mapIndex(ops, 2));
  CHECK_FALSE(mapIndex(ops, 3));
}

TEST_CASE("sticky headers keep their rows through a removal before their props catch up") {
  ListOps removeFirst;
  removeFirst.removed = {{0, 1}};
  CHECK(mapIndices(removeFirst, {17, 35}) == std::vector<int>{16, 34});
}

TEST_CASE("mapped indices drop removed rows and stay ascending") {
  ListOps ops;
  ops.removed = {{2, 1}};
  ops.moved = {{5, 0}};
  CHECK(mapIndices(ops, {1, 2, 6}) == std::vector<int>{0, 2});
}

TEST_CASE("edge inserts follow content order") {
  ListOps atStart;
  atStart.inserted = {{0, 1}};
  CHECK(insertedAtEdges(atStart, 10, false).visualTop);
  CHECK_FALSE(insertedAtEdges(atStart, 10, false).visualBottom);
  CHECK(insertedAtEdges(atStart, 10, true).visualBottom);
  CHECK_FALSE(insertedAtEdges(atStart, 10, true).visualTop);
  ListOps atEnd;
  atEnd.inserted = {{8, 2}};
  CHECK(insertedAtEdges(atEnd, 10, false).visualBottom);
  CHECK_FALSE(insertedAtEdges(atEnd, 10, false).visualTop);
}
