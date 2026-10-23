package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class LayoutShapeTest {
  @Test
  fun theSamePropsGiveTheSameShape() {
    assertEquals(LayoutShape.of(false, 2, false, false), LayoutShape.of(false, 2, false, false))
  }

  @Test
  fun theAxisColumnsAndDirectionEachChangeIt() {
    val list = LayoutShape.of(horizontal = false, numColumns = 1, inverted = false, startRenderingFromBottom = false)
    assertNotEquals(list, LayoutShape.of(true, 1, false, false))
    assertNotEquals(list, LayoutShape.of(false, 2, false, false))
    assertNotEquals(list, LayoutShape.of(false, 1, true, false))
  }

  @Test
  fun startRenderingFromBottomChangesOnlyAnUprightSingleColumn() {
    assertNotEquals(LayoutShape.of(false, 1, false, true), LayoutShape.of(false, 1, false, false))
    assertEquals(LayoutShape.of(false, 1, true, true), LayoutShape.of(false, 1, true, false))
    assertEquals(LayoutShape.of(false, 2, false, true), LayoutShape.of(false, 2, false, false))
  }
}
