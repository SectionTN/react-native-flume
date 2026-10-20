package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ViewportTrackerTest {
  @Test
  fun theFirstValidWidthIsAChange() {
    assertTrue(ViewportTracker().update(1080, 1f))
  }

  @Test
  fun aZeroWidthIsIgnored() {
    val tracker = ViewportTracker()
    assertFalse(tracker.update(0, 1f))
    assertEquals(0, tracker.widthPx)
  }

  @Test
  fun theSameViewportIsNotAChange() {
    val tracker = ViewportTracker()
    tracker.update(1080, 1f)
    assertFalse(tracker.update(1080, 1f))
  }

  @Test
  fun aFontScaleChangeIsAChange() {
    val tracker = ViewportTracker()
    tracker.update(1080, 1f)
    assertTrue(tracker.update(1080, 1.3f))
  }
}
