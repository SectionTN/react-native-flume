package me.sectiontn.flume

import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_DRAGGING
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_SETTLING
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ScrollEventsTest {
  @Test
  fun transitionsFollowTheScrollView() {
    assertEquals(listOf(ScrollKind.BEGIN_DRAG), ScrollStateMachine.transition(SCROLL_STATE_IDLE, SCROLL_STATE_DRAGGING))
    assertEquals(listOf(ScrollKind.END_DRAG, ScrollKind.MOMENTUM_BEGIN), ScrollStateMachine.transition(SCROLL_STATE_DRAGGING, SCROLL_STATE_SETTLING))
    // ScrollView runs its momentum pair after every release, also when no fling follows.
    assertEquals(
      listOf(ScrollKind.END_DRAG, ScrollKind.MOMENTUM_BEGIN, ScrollKind.MOMENTUM_END),
      ScrollStateMachine.transition(SCROLL_STATE_DRAGGING, SCROLL_STATE_IDLE),
    )
    assertEquals(listOf(ScrollKind.MOMENTUM_END), ScrollStateMachine.transition(SCROLL_STATE_SETTLING, SCROLL_STATE_IDLE))
    assertEquals(listOf(ScrollKind.MOMENTUM_END, ScrollKind.BEGIN_DRAG), ScrollStateMachine.transition(SCROLL_STATE_SETTLING, SCROLL_STATE_DRAGGING))
    assertEquals(listOf(ScrollKind.MOMENTUM_BEGIN), ScrollStateMachine.transition(SCROLL_STATE_IDLE, SCROLL_STATE_SETTLING))
    assertEquals(emptyList<ScrollKind>(), ScrollStateMachine.transition(SCROLL_STATE_IDLE, SCROLL_STATE_IDLE))
  }

  @Test
  fun theThrottleEmitsOncePerIntervalAndFlushesTheRest() {
    val throttle = ScrollThrottle(16)
    assertTrue(throttle.shouldEmit(0))
    assertFalse(throttle.shouldEmit(10))
    assertTrue(throttle.flush())
    assertFalse(throttle.flush())
    assertTrue(throttle.shouldEmit(16))
  }

  @Test
  fun anInvertedOffsetCountsFromTheContentStart() {
    assertEquals(100, ScrollMetrics.fromContentStart(100, 1000, 300, inverted = false))
    assertEquals(600, ScrollMetrics.fromContentStart(100, 1000, 300, inverted = true))
  }

  @Test
  fun theLastOffsetPlusTheViewLengthReachesTheContentSize() {
    // 1000 px of rows in a 300 px list padded 10 above and 20 below: extent 270, so the last offset is 730.
    val padding = intArrayOf(4, 10, 6, 20)
    assertEquals(Pair(360, 730 + 300), ScrollMetrics.contentSizePx(false, 1000, padding, 360, 300))
    // Across a horizontal list: 4 left and 6 right of a 360 px viewport leave an extent of 350.
    assertEquals(Pair(1000 - 350 + 360, 300), ScrollMetrics.contentSizePx(true, 1000, padding, 360, 300))
  }
}
