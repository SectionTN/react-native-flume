package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ScrollTargetsTest {
  @Test
  fun theOffsetPlacesTheItemByViewPosition() {
    assertEquals(0, ScrollTargets.offsetPx(0f, 0, 1000, 100))
    assertEquals(900, ScrollTargets.offsetPx(1f, 0, 1000, 100))
    assertEquals(460, ScrollTargets.offsetPx(0.5f, 10, 1000, 100))
  }

  @Test
  fun theSmoothDeltaAimsAtTheSameSpot() {
    assertEquals(-200, ScrollTargets.smoothDt(200, 300, 0, 1000, 0f, 0, reverse = false))
    assertEquals(700, ScrollTargets.smoothDt(200, 300, 0, 1000, 1f, 0, reverse = false))
    assertEquals(700, ScrollTargets.smoothDt(200, 300, 0, 1000, 0f, 0, reverse = true))
    assertEquals(-200, ScrollTargets.smoothDt(200, 300, 0, 1000, 1f, 0, reverse = true))
  }

  @Test
  fun indicesAreClampedToTheList() {
    assertEquals(9, ScrollTargets.clampIndex(12, 10))
    assertEquals(0, ScrollTargets.clampIndex(-1, 10))
    assertNull(ScrollTargets.clampIndex(0, 0))
  }

  @Test
  fun aLongJumpSpeedsUpToFitTheBudget() {
    assertEquals(0.06f, ScrollTargets.speedMsPerPx(0.06f, 1000, 400), 0.0001f)
    assertEquals(0.004f, ScrollTargets.speedMsPerPx(0.06f, 100000, 400), 0.0001f)
    assertEquals(0.06f, ScrollTargets.speedMsPerPx(0.06f, 0, 400), 0.0001f)
  }

  @Test
  fun aTargetAtOrBeforeTheStartIsTheFirstRow() {
    assertEquals(0, ScrollTargets.startRow(0))
    assertEquals(0, ScrollTargets.startRow(-40))
    assertNull(ScrollTargets.startRow(1))
  }

  @Test
  fun theInitialIndexCountsDataAndClampsToTheEnd() {
    val rows = ListRows(header = true, dataCount = 10, footer = true)
    assertEquals(4, ScrollTargets.initialPosition(3, rows))
    assertEquals(11, ScrollTargets.initialPosition(40, rows))
  }

  @Test
  fun aGridScrollCountsRows() {
    val spans = FullSpans(intArrayOf(0))
    assertEquals(3, ScrollTargets.rowSpan(1, 10) { spans.groupIndex(it, 3) })
  }

  @Test
  fun alignmentAtTheStartNeedsNoLength() {
    assertFalse(ScrollTargets.needsLength(0f))
    assertTrue(ScrollTargets.needsLength(0.5f))
  }
}
