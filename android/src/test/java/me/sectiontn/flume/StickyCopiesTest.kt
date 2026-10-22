package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Test

class StickyCopiesTest {
  private val frames = listOf(ChildFrame(4, 0, 50), ChildFrame(5, 50, 100), ChildFrame(9, 100, 150))

  @Test
  fun thePinnedRowAndLaidOutStickyRowsGetCopies() =
    assertArrayEquals(intArrayOf(2, 9), StickyCopies.wanted(intArrayOf(2, 9, 20), frames, pinned = 2, isReact = { true }))

  @Test
  fun workletRowsGetNone() =
    assertArrayEquals(intArrayOf(), StickyCopies.wanted(intArrayOf(2, 9), frames, pinned = 2, isReact = { false }))
}
