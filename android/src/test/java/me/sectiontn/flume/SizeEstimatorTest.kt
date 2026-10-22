package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class SizeEstimatorTest {
  private val rows = SlotGroup(1)
  private val cards = SlotGroup(3)

  @Test
  fun estimatedItemSizeWins() {
    val estimator = SizeEstimator(floorPx = 100)
    estimator.estimatedPx = 60
    estimator.record(rows, 300)
    assertEquals(60, estimator.estimatePx(rows))
  }

  @Test
  fun aGroupTakesTheMeanOfItsMeasuredCellsAboveTheFloor() {
    val estimator = SizeEstimator(floorPx = 100)
    assertEquals(100, estimator.estimatePx(rows))
    estimator.record(rows, 300)
    estimator.record(rows, 500)
    assertEquals(400, estimator.estimatePx(rows))
    estimator.record(cards, 20)
    assertEquals(100, estimator.estimatePx(cards))
  }
}
