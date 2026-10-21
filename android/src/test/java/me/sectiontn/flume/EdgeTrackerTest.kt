package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class EdgeTrackerTest {
  @Test
  fun firesOncePerContentLengthAndReArmsOutsideTheThreshold() {
    val tracker = EdgeTracker()
    assertEquals(EdgeHits(start = false, end = true), tracker.update(EdgeDistances(1500, 100), 500, 2000, 2.0, 2.0))
    assertEquals(EdgeHits(start = false, end = false), tracker.update(EdgeDistances(1500, 100), 500, 2000, 2.0, 2.0))
    assertEquals(EdgeHits(start = true, end = false), tracker.update(EdgeDistances(0, 1500), 500, 2000, 0.5, 0.5))
    assertEquals(EdgeHits(start = false, end = true), tracker.update(EdgeDistances(1500, 100), 500, 2000, 2.0, 2.0))
  }

  @Test
  fun firesAgainWhenTheContentGrows() {
    val tracker = EdgeTracker()
    tracker.update(EdgeDistances(1500, 100), 500, 2000, 2.0, 2.0)
    assertTrue(tracker.update(EdgeDistances(1500, 100), 500, 3000, 2.0, 2.0).end)
  }

  @Test
  fun aShortListFiresBothEdgesOnce() {
    val tracker = EdgeTracker()
    assertEquals(EdgeHits(start = true, end = true), tracker.update(EdgeDistances(0, 0), 500, 300, 2.0, 2.0))
    assertEquals(EdgeHits(start = false, end = false), tracker.update(EdgeDistances(0, 0), 500, 300, 2.0, 2.0))
  }

  @Test
  fun invertedSwapsTheDistances() {
    assertEquals(EdgeDistances(600, 100), ListDistances.of(100, 1000, 300, inverted = true))
    assertEquals(EdgeDistances(100, 600), ListDistances.of(100, 1000, 300, inverted = false))
  }
}
