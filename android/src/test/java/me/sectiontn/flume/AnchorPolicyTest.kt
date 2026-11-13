package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class AnchorPolicyTest {
  private val before = AnchorState(firstVisiblePosition = 3, firstVisibleOffsetPx = -12, distanceToVisualTopPx = 900, distanceToVisualBottomPx = 50, viewportPx = 1000)

  @Test fun theDefaultDoesNothing() =
    assertEquals(AnchorDecision(), AnchorPolicy.decide(AnchorConfig(), before, insertedAtVisualTop = true, insertedAtVisualBottom = true))

  @Test fun disabledRestoresTheFirstVisibleRow() =
    assertEquals(AnchorDecision(restore = 3 to -12), AnchorPolicy.decide(AnchorConfig(disabled = true), before, true, false))

  @Test fun anInsertAtTheBottomWithinTheThresholdScrollsThere() {
    val config = AnchorConfig(autoscrollToBottomThreshold = 0.2, animateAutoScrollToBottom = false)
    assertEquals(AnchorDecision(scrollTo = VisualEdge.BOTTOM, animated = false), AnchorPolicy.decide(config, before, false, true))
    assertEquals(AnchorDecision(), AnchorPolicy.decide(config, before.copy(distanceToVisualBottomPx = 300), false, true))
    assertEquals(AnchorDecision(), AnchorPolicy.decide(config, before, true, false))
  }

  @Test fun anInsertAtTheTopWithinTheThresholdScrollsThereAlwaysAnimated() {
    val config = AnchorConfig(autoscrollToTopThreshold = 1.0, animateAutoScrollToBottom = false)
    assertEquals(AnchorDecision(scrollTo = VisualEdge.TOP, animated = true), AnchorPolicy.decide(config, before, true, false))
  }

  @Test fun aChangeThatRemovesEveryLaidOutRowKeepsThePosition() {
    assertEquals(AnchorDecision(restore = 3 to -12), AnchorPolicy.decide(AnchorConfig(), before, true, false, anchorLost = true))
    val bottom = AnchorConfig(autoscrollToBottomThreshold = 0.2)
    assertEquals(AnchorDecision(scrollTo = VisualEdge.BOTTOM, animated = true), AnchorPolicy.decide(bottom, before, false, true, anchorLost = true))
  }

  @Test fun theAnchorIsLostOnlyWhenNoLaidOutRowSurvives() {
    val everything = intArrayOf(0, 500)
    assertEquals(true, AnchorPolicy.losesEveryRow(intArrayOf(30, 31, 32), everything))
    assertEquals(false, AnchorPolicy.losesEveryRow(intArrayOf(-1, 0, 1), everything))
    assertEquals(false, AnchorPolicy.losesEveryRow(intArrayOf(30, 31, 32), intArrayOf(32, 1, 30, 1)))
    assertEquals(true, AnchorPolicy.losesEveryRow(intArrayOf(30, 31, 32), intArrayOf(32, 1, 30, 2)))
    assertEquals(false, AnchorPolicy.losesEveryRow(intArrayOf(), everything))
  }

  @Test fun nothingHappensWithoutAState() =
    assertEquals(AnchorDecision(), AnchorPolicy.decide(AnchorConfig(disabled = true), null, true, true))

  @Test fun edgesMapToContentOrder() {
    assertEquals(9 to 1f, AnchorTargets.indexFor(VisualEdge.BOTTOM, 10, inverted = false))
    assertEquals(0 to 0f, AnchorTargets.indexFor(VisualEdge.BOTTOM, 10, inverted = true))
    assertEquals(9 to 1f, AnchorTargets.indexFor(VisualEdge.TOP, 10, inverted = true))
  }
}
