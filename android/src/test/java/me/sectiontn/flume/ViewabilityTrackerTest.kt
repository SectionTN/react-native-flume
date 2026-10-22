package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ViewabilityTrackerTest {
  private val rows = listOf(ChildFrame(0, -50, 10), ChildFrame(1, 10, 60), ChildFrame(2, 60, 150), ChildFrame(3, 150, 200))

  private fun tokens(vararg pairs: Pair<Int, String>) = pairs.map { ViewToken(it.first, it.second) }

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
    assertNotNull(tracker.report(tokens(1 to "b", 2 to "c")))
    assertNull(tracker.report(tokens(1 to "b", 2 to "c")))
  }

  @Test fun changedListsExitsAndEntries() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    tracker.report(tokens(1 to "b", 2 to "c"))
    val report = tracker.report(tokens(2 to "c", 3 to "d"))!!
    assertEquals(tokens(2 to "c", 3 to "d"), report.viewable)
    assertEquals(listOf(ViewableChange(ViewToken(1, "b"), false), ViewableChange(ViewToken(3, "d"), true)), report.changed)
  }

  // A prepend above the viewport shifts every index by one while the same rows stay on screen.
  @Test fun rowsThatOnlyMovedAreNotReported() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    tracker.report(tokens(0 to "a", 1 to "b"))
    assertNull(tracker.report(tokens(1 to "a", 2 to "b")))
  }

  // A message prepended to a chat that follows it keeps the indices: one row came in, one went out.
  @Test fun aDataChangeReportsTheRowsThatCameAndWentByKey() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    tracker.changed(intArrayOf(0, 1))
    tracker.report(tokens(0 to "a", 1 to "b"))
    tracker.invalidate()
    assertTrue(tracker.changed(intArrayOf(0, 1)))
    val report = tracker.report(tokens(0 to "new", 1 to "a"))!!
    assertEquals(listOf(ViewableChange(ViewToken(1, "b"), false), ViewableChange(ViewToken(0, "new"), true)), report.changed)
  }

  @Test fun clearingTheDataReportsTheExits() {
    val tracker = ViewabilityTracker(ViewabilityConfig())
    tracker.changed(intArrayOf(0))
    tracker.report(tokens(0 to "a"))
    tracker.invalidate()
    assertTrue(tracker.changed(IntArray(0)))
    assertEquals(listOf(ViewableChange(ViewToken(0, "a"), false)), tracker.report(emptyList())!!.changed)
  }
}
