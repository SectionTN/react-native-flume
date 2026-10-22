package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class EdgeTrackerTest {
  private fun EdgeHits.fired() = start to end

  @Test
  fun firesOncePerContentAndReArmsOutsideTheThreshold() {
    val tracker = EdgeTracker()
    assertEquals(false to true, tracker.update(ScrollRange(1400, 2000, 500), 40, false, 500, 2.0, 2.0).fired())
    assertEquals(false to false, tracker.update(ScrollRange(1400, 2000, 500), 40, false, 500, 2.0, 2.0).fired())
    assertEquals(true to false, tracker.update(ScrollRange(0, 2000, 500), 40, false, 500, 0.5, 0.5).fired())
    assertEquals(false to true, tracker.update(ScrollRange(1400, 2000, 500), 40, false, 500, 2.0, 2.0).fired())
  }

  @Test
  fun firesAgainWhenItemsAreAdded() {
    val tracker = EdgeTracker()
    tracker.update(ScrollRange(1400, 2000, 500), 40, false, 500, 2.0, 2.0)
    assertTrue(tracker.update(ScrollRange(1400, 2200, 500), 44, false, 500, 2.0, 2.0).end)
  }

  // RecyclerView's range is the visible rows' average length times the count, so it moves as rows scroll by.
  @Test
  fun aRangeEstimateThatMovesWithTheRowsFiresOncePerItemCount() {
    val tracker = EdgeTracker()
    assertTrue(tracker.update(ScrollRange(1400, 2000, 500), 40, false, 500, 2.0, 2.0).end)
    assertFalse(tracker.update(ScrollRange(1480, 2100, 500), 40, false, 500, 2.0, 2.0).end)
  }

  @Test
  fun aShortListFiresBothEdgesOnce() {
    val tracker = EdgeTracker()
    assertEquals(true to true, tracker.update(ScrollRange(0, 300, 300), 5, false, 500, 2.0, 2.0).fired())
    assertEquals(false to false, tracker.update(ScrollRange(0, 300, 300), 5, false, 500, 2.0, 2.0).fired())
  }

  @Test
  fun anEmptyListFiresNothing() {
    assertEquals(false to false, EdgeTracker().update(ScrollRange(0, 0, 0), 0, false, 500, 2.0, 2.0).fired())
  }

  @Test
  fun invertedSwapsTheDistances() {
    assertEquals(EdgeDistances(600, 100), ListDistances.of(100, 1000, 300, inverted = true))
    assertEquals(EdgeDistances(100, 600), ListDistances.of(100, 1000, 300, inverted = false))
  }
}
