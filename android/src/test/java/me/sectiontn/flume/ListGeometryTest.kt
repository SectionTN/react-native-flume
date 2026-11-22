package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ListGeometryTest {
  private fun geometry() = ListGeometry(2f).apply { update(1080, 500, 1f) }

  @Test
  fun columnsShareTheInnerWidth() {
    val g = geometry()
    g.configure(horizontal = false, numColumns = 3, inverted = false, paddingPx = intArrayOf(20, 0, 20, 0))
    assertEquals(173.33f, g.cellWidthDp(fullSpan = false), 0.01f)
    assertEquals(520f, g.cellWidthDp(fullSpan = true), 0f)
    assertTrue(g.cellHeightDp().isNaN())
  }

  @Test
  fun aHorizontalListBindsAtTheInnerHeight() {
    val g = geometry()
    g.configure(horizontal = true, numColumns = 1, inverted = false, paddingPx = intArrayOf(0, 10, 0, 10))
    assertEquals(240f, g.cellHeightDp(), 0f)
    assertTrue(g.cellWidthDp(fullSpan = false).isNaN())
  }

  @Test
  fun theListWidthIsInDpOnceLaidOut() {
    val g = ListGeometry(2f)
    assertEquals(-1f, g.widthDp, 0f)
    g.update(1080, 500, 1f)
    assertEquals(540f, g.widthDp, 0f)
  }

  @Test
  fun aZeroBindAxisIsIgnored() {
    val g = ListGeometry(2f)
    assertFalse(g.update(0, 500, 1f))
    assertFalse(g.ready)
  }

  @Test
  fun theHeightOnlyMattersWhenHorizontal() {
    val g = geometry()
    assertFalse(g.update(1080, 600, 1f))
    g.configure(horizontal = true, numColumns = 1, inverted = false, paddingPx = IntArray(4))
    assertTrue(g.update(1080, 700, 1f))
  }

  @Test
  fun aColumnOrFontScaleChangeRebinds() {
    val g = geometry()
    assertTrue(g.configure(horizontal = false, numColumns = 2, inverted = false, paddingPx = IntArray(4)))
    assertFalse(g.configure(horizontal = false, numColumns = 2, inverted = false, paddingPx = IntArray(4)))
    assertTrue(g.update(1080, 500, 1.3f))
  }

  @Test
  fun gridCellsLeaveTheLaneInsetsAndTheColumnGaps() {
    val g = ListGeometry(1f).apply { update(300, 500, 1f) }
    g.configure(horizontal = false, numColumns = 3, inverted = false, paddingPx = IntArray(4), lanes = Lanes(insetStartPx = 16, insetEndPx = 16, gapPx = 8))
    assertEquals(84f, g.cellWidthDp(fullSpan = false), 0f)
    assertEquals(268f, g.cellWidthDp(fullSpan = true), 0f)
  }

  @Test
  fun aLaneChangeRebinds() {
    val g = geometry()
    val lanes = Lanes(insetStartPx = 16, insetEndPx = 16, gapPx = 8)
    assertTrue(g.configure(horizontal = false, numColumns = 3, inverted = false, paddingPx = IntArray(4), lanes = lanes))
    assertFalse(g.configure(horizontal = false, numColumns = 3, inverted = false, paddingPx = IntArray(4), lanes = lanes.copy()))
    assertTrue(g.configure(horizontal = false, numColumns = 3, inverted = false, paddingPx = IntArray(4), lanes = lanes.copy(gapPx = 4)))
  }

  @Test
  fun percentPaddingResolvesAgainstTheListWidthOnEveryEdge() {
    assertArrayEquals(intArrayOf(20, 20, 20, 20), ListGeometry.paddingPx(IntArray(4), FloatArray(4) { 0.05f }, widthPx = 400))
    assertArrayEquals(intArrayOf(12, 20, 0, 0), ListGeometry.paddingPx(intArrayOf(12, 0, 0, 0), floatArrayOf(0f, 0.05f, 0f, 0f), widthPx = 400))
  }

  // Three rows of 50 inside padding of 10 and 20, laid out from the top of the view.
  @Test
  fun theBackgroundSpansTheLaidOutContentAndItsPadding() {
    assertArrayEquals(intArrayOf(0, 180), ContentBackground.span(firstStart = 10, lastEnd = 160, hasFirst = true, hasLast = true, paddingStart = 10, paddingEnd = 20, viewLength = 1000))
    assertArrayEquals(intArrayOf(0, 1000), ContentBackground.span(firstStart = -40, lastEnd = 1200, hasFirst = false, hasLast = false, paddingStart = 10, paddingEnd = 20, viewLength = 1000))
  }
}
