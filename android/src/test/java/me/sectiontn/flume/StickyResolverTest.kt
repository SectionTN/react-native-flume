package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class StickyResolverTest {
  private fun rows(vararg frames: Triple<Int, Int, Int>) = frames.map { ChildFrame(it.first, it.second, it.third) }

  @Test
  fun nothingPinsBeforeTheFirstHeader() {
    assertNull(StickyResolver.resolve(intArrayOf(5), rows(Triple(0, 0, 40), Triple(1, 40, 80)), 10, inverted = false, pinnedLength = 40))
  }

  @Test
  fun theLastHeaderAtOrAboveTheTopRowPins() {
    val pinned = StickyResolver.resolve(intArrayOf(0, 4, 8), rows(Triple(5, -10, 30), Triple(6, 30, 70)), 10, false, 40)
    assertEquals(Pinned(4, 0), pinned)
  }

  @Test
  fun theNextHeaderPushesThePinnedCopy() {
    val pinned = StickyResolver.resolve(intArrayOf(0, 4), rows(Triple(2, -30, 20), Triple(3, 20, 60), Triple(4, 60, 100)), 10, false, 80)
    assertEquals(Pinned(0, -20), pinned)
  }

  @Test
  fun theHeaderRowAtTheTopIsNotPushed() {
    assertEquals(Pinned(4, 0), StickyResolver.resolve(intArrayOf(4, 8), rows(Triple(4, 0, 40), Triple(5, 40, 80)), 10, false, 40))
  }

  @Test
  fun invertedPinsTheFirstHeaderAtOrAfterTheTopRowAndThePreviousOnePushes() {
    val sticky = intArrayOf(3, 7)
    assertEquals(Pinned(7, 0), StickyResolver.resolve(sticky, rows(Triple(6, -10, 30), Triple(5, 30, 70), Triple(4, 70, 110), Triple(3, 110, 150)), 10, true, 40))
    assertEquals(Pinned(7, -10), StickyResolver.resolve(sticky, rows(Triple(5, -30, 10), Triple(4, 10, 30), Triple(3, 30, 70)), 10, true, 40))
  }

  @Test
  fun aHeaderBelowThePinLineHasNotPinned() {
    assertNull(StickyResolver.resolve(intArrayOf(0, 4), rows(Triple(0, 16, 56), Triple(1, 56, 96)), 10, false, 40))
    assertNull(StickyResolver.resolve(intArrayOf(3, 7), rows(Triple(7, 16, 56), Triple(6, 56, 96)), 10, true, 40))
    assertEquals(Pinned(0, 0), StickyResolver.resolve(intArrayOf(0, 4), rows(Triple(0, 0, 40), Triple(1, 40, 80)), 10, false, 40))
  }

  @Test
  fun indicesPastTheCountAreIgnored() {
    assertEquals(Pinned(2, 0), StickyResolver.resolve(intArrayOf(2, 50), rows(Triple(8, 0, 40), Triple(9, 40, 80)), 10, false, 40))
  }
}
