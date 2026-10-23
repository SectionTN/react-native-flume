package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test

class StickyPlacementTest {
  @Test
  fun theCopyPinsAtTheViewportEdgeInsideTheCrossPadding() {
    assertArrayEquals(intArrayOf(16, 0, 8, 4), StickyPlacement.margins(horizontal = false, left = 16, top = 32, right = 8, bottom = 4))
    assertArrayEquals(intArrayOf(0, 32, 8, 4), StickyPlacement.margins(horizontal = true, left = 16, top = 32, right = 8, bottom = 4))
  }

  @Test
  fun framesCountFromTheViewportEdge() {
    assertEquals(listOf(ChildFrame(3, 6, 46), ChildFrame(4, 46, 86)), StickyPlacement.fromViewportEdge(listOf(ChildFrame(3, -10, 30), ChildFrame(4, 30, 70)), 16))
  }
}
