package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class CellStretchTest {
  // GridLayoutManager measures the shorter cells of a row of 100, 150 and 120 again at 150, exactly.
  @Test
  fun aShorterCellStretchesToTheRowOnAnExactSpec() {
    assertEquals(listOf(150, 0, 150), listOf(100, 150, 120).map { CellStretch.target(exact = true, specPx = 150, naturalPx = it) })
  }

  @Test
  fun aStretchedCellStillReportsItsOwnLengthToAFreeMeasure() {
    assertEquals(0, CellStretch.target(exact = false, specPx = 150, naturalPx = 100))
  }
}
