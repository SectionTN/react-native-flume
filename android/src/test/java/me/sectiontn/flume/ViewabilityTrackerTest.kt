package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ViewabilityTrackerTest {
  private val rows = listOf(ChildFrame(0, -50, 10), ChildFrame(1, 10, 60), ChildFrame(2, 60, 150), ChildFrame(3, 150, 200))

  @Test fun anyVisiblePixelCountsByDefault() = assertArrayEquals(intArrayOf(0, 1, 2), ViewabilityTracker(ViewabilityConfig()).compute(100, rows))

  @Test fun theItemThresholdNeedsThatMuchOfTheItem() =
    assertArrayEquals(intArrayOf(1), ViewabilityTracker(ViewabilityConfig(itemVisiblePercentThreshold = 50.0)).compute(100, rows))

  @Test fun theAreaThresholdNeedsThatMuchOfTheViewport() {
    val tracker = ViewabilityTracker(ViewabilityConfig(viewAreaCoveragePercentThreshold = 50.0))
    assertArrayEquals(intArrayOf(1), tracker.compute(100, listOf(ChildFrame(0, -50, 40), ChildFrame(1, 40, 160))))
  }

  @Test fun aZeroViewportReportsNothing() = assertArrayEquals(intArrayOf(), ViewabilityTracker(ViewabilityConfig()).compute(0, rows))

  @Test fun waitsForAnInteraction() {
    val tracker = ViewabilityTracker(ViewabilityConfig(waitForInteraction = true))
    assertArrayEquals(intArrayOf(), tracker.compute(100, rows))
    tracker.interacted = true
    assertArrayEquals(intArrayOf(0, 1, 2), tracker.compute(100, rows))
  }

  @Test fun unchangedSetsReportNothing() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    assertNotNull(tracker.report(intArrayOf(1, 2)))
    assertNull(tracker.report(intArrayOf(1, 2)))
  }

  @Test fun changedListsExitsAndEntries() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    tracker.report(intArrayOf(1, 2))
    val report = tracker.report(intArrayOf(2, 3))!!
    assertArrayEquals(intArrayOf(2, 3), report.viewable)
    assertEquals(listOf(ViewableChange(1, false), ViewableChange(3, true)), report.changed)
  }
}
