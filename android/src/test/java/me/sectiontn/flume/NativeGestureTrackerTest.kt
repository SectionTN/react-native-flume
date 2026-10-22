package me.sectiontn.flume

import android.view.MotionEvent
import org.junit.Assert.assertEquals
import org.junit.Test

class NativeGestureTrackerTest {
  @Test
  fun aDragStartsOnceAndEndsAtRelease() {
    val tracker = NativeGestureTracker()
    assertEquals(GestureSignal.NONE, tracker.onEvent(MotionEvent.ACTION_DOWN, dragging = false, swallowing = false))
    assertEquals(GestureSignal.STARTED, tracker.onEvent(MotionEvent.ACTION_MOVE, dragging = true, swallowing = false))
    assertEquals(GestureSignal.NONE, tracker.onEvent(MotionEvent.ACTION_MOVE, dragging = true, swallowing = false))
    assertEquals(GestureSignal.ENDED, tracker.onEvent(MotionEvent.ACTION_UP, dragging = false, swallowing = false))
  }

  @Test
  fun aSwallowedTapStartsAtDown() {
    val tracker = NativeGestureTracker()
    assertEquals(GestureSignal.STARTED, tracker.onEvent(MotionEvent.ACTION_DOWN, dragging = false, swallowing = true))
    assertEquals(GestureSignal.ENDED, tracker.onEvent(MotionEvent.ACTION_UP, dragging = false, swallowing = false))
  }

  @Test
  fun aTapNobodyTookEndsNothing() {
    val tracker = NativeGestureTracker()
    tracker.onEvent(MotionEvent.ACTION_DOWN, dragging = false, swallowing = false)
    assertEquals(GestureSignal.NONE, tracker.onEvent(MotionEvent.ACTION_UP, dragging = false, swallowing = false))
  }
}
