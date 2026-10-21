package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SeparatorGeometryTest {
  @Test fun theLastRowHasNoGap() = assertNull(SeparatorGeometry.edge(row = 9, lastRow = 9, horizontal = false, inverted = false))

  @Test fun aListGapsBelow() = assertEquals(Edge.BOTTOM, SeparatorGeometry.edge(0, 9, horizontal = false, inverted = false))

  @Test fun anInvertedListGapsAbove() = assertEquals(Edge.TOP, SeparatorGeometry.edge(0, 9, horizontal = false, inverted = true))

  @Test fun aHorizontalListGapsToTheRight() = assertEquals(Edge.RIGHT, SeparatorGeometry.edge(0, 9, horizontal = true, inverted = false))

  @Test fun anInvertedHorizontalListGapsToTheLeft() = assertEquals(Edge.LEFT, SeparatorGeometry.edge(0, 9, horizontal = true, inverted = true))
}
