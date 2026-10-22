package me.sectiontn.flume

import androidx.recyclerview.widget.GridLayoutManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class FlumeSpanSizeLookupTest {
  private fun lookup(spanCount: Int, vararg fullSpans: Int) = FlumeSpanSizeLookup { spanCount }.apply { this.fullSpans = FullSpans(fullSpans) }

  // RecyclerView's own lookup walks every earlier position, which makes it the reference on small grids.
  private fun reference(spanCount: Int, vararg fullSpans: Int) = object : GridLayoutManager.SpanSizeLookup() {
    override fun getSpanSize(position: Int) = if (position in fullSpans) spanCount else 1
  }

  @Test fun matchesRecyclerViewsRowsAndColumns() {
    val layouts = listOf(intArrayOf(), intArrayOf(0), intArrayOf(0, 5), intArrayOf(2, 3), intArrayOf(4, 9, 10, 30), intArrayOf(7))
    for (spanCount in 2..4) {
      for (full in layouts) {
        val flume = lookup(spanCount, *full)
        val stock = reference(spanCount, *full)
        for (position in 0 until 40) {
          val case = "position $position, $spanCount columns, full spans ${full.toList()}"
          assertEquals("row of $case", stock.getSpanGroupIndex(position, spanCount), flume.getSpanGroupIndex(position, spanCount))
          assertEquals("column of $case", stock.getSpanIndex(position, spanCount), flume.getSpanIndex(position, spanCount))
        }
      }
    }
  }

  // GridLayoutManager asks for rows on every anchor and backward row, the separators for every child on every frame.
  @Test fun findsTheRowOfAFarPositionWithoutWalkingTheGrid() {
    val flume = lookup(3, 0, 50_000_000)
    val start = System.nanoTime()
    val row = flume.getSpanGroupIndex(99_999_999, 3)
    val column = flume.getSpanIndex(99_999_999, 3)
    val elapsedMs = (System.nanoTime() - start) / 1_000_000
    assertEquals(33_333_335, row)
    assertEquals(0, column)
    assertTrue("took $elapsedMs ms", elapsedMs < 100)
  }
}
