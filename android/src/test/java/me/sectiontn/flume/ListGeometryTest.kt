package me.sectiontn.flume

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
}
