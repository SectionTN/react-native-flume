package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SeparatorGeometryTest {
  @Test fun theLastRowHasNoGap() = assertNull(SeparatorGeometry.edge(row = 9, lastRow = 9, horizontal = false, inverted = false))

  @Test fun aListGapsBelow() = assertEquals(Edge.BOTTOM, SeparatorGeometry.edge(0, 9, horizontal = false, inverted = false))

  @Test fun anInvertedListGapsAbove() = assertEquals(Edge.TOP, SeparatorGeometry.edge(0, 9, horizontal = false, inverted = true))

  @Test fun aHorizontalListGapsToTheRight() = assertEquals(Edge.RIGHT, SeparatorGeometry.edge(0, 9, horizontal = true, inverted = false))

  @Test fun anInvertedHorizontalListGapsToTheLeft() = assertEquals(Edge.LEFT, SeparatorGeometry.edge(0, 9, horizontal = true, inverted = true))

  @Test
  fun onlyDataRowsCarryAGap() {
    val rows = ListRows(header = true, dataCount = 3, footer = true)
    assertEquals(
      listOf(null, Edge.BOTTOM, Edge.BOTTOM, null, null),
      (0 until 5).map { SeparatorGeometry.edgeAt(it, rows, { p -> p }, horizontal = false, inverted = false) },
    )
  }

  // 300 px across three columns, 16 in from each side, 8 between columns: 84 wide at 16, 108 and 200.
  @Test
  fun gridColumnsSitInsideTheLaneInsetsWithTheirGaps() {
    assertArrayEquals(intArrayOf(16, 0), GapGeometry.laneOffsets(lane = 0, lanes = 3, innerPx = 300, insetStartPx = 16, insetEndPx = 16, gapPx = 8))
    assertArrayEquals(intArrayOf(8, 8), GapGeometry.laneOffsets(lane = 1, lanes = 3, innerPx = 300, insetStartPx = 16, insetEndPx = 16, gapPx = 8))
    assertArrayEquals(intArrayOf(0, 16), GapGeometry.laneOffsets(lane = 2, lanes = 3, innerPx = 300, insetStartPx = 16, insetEndPx = 16, gapPx = 8))
  }

  @Test
  fun everyColumnLeavesTheSameWidthWhenThePixelsDoNotDivide() {
    val sums = (0 until 3).map { GapGeometry.laneOffsets(it, 3, 301, 15, 15, 7).sum() }
    assertEquals(1, sums.distinct().size)
  }

  // A header, three rows and a footer.
  @Test
  fun theMainGapFollowsEveryRowButTheLastOneHeaderAndFooterIncluded() {
    assertEquals(
      listOf(8, 8, 8, 8, 0),
      (0..4).map { GapGeometry.after(row = it, lastRow = 4, dataGap = it in 1..2, mainGapPx = 8, gridRowGapPx = 0) },
    )
  }

  @Test
  fun gridRowsAddTheGridRowGapWhereTheyCarryALine() {
    assertEquals(12, GapGeometry.after(row = 0, lastRow = 2, dataGap = true, mainGapPx = 0, gridRowGapPx = 12))
    assertEquals(0, GapGeometry.after(row = 1, lastRow = 2, dataGap = false, mainGapPx = 0, gridRowGapPx = 12))
  }

  // The line touches its row and the gap sits beyond it. Across the axis it spans the decorated cell inside the clip.
  @Test
  fun theLineSitsNextToItsRowNotAcrossTheGap() {
    val child = intArrayOf(16, 0, 100, 50)
    val decorated = intArrayOf(0, 0, 100, 59)
    assertArrayEquals(intArrayOf(0, 50, 100, 51), SeparatorGeometry.line(Edge.BOTTOM, child, decorated, sizePx = 1, clipStart = 0, clipEnd = 300))
    assertArrayEquals(intArrayOf(10, -1, 100, 0), SeparatorGeometry.line(Edge.TOP, child, decorated, sizePx = 1, clipStart = 10, clipEnd = 300))
    assertArrayEquals(intArrayOf(100, 0, 101, 40), SeparatorGeometry.line(Edge.RIGHT, child, decorated, sizePx = 1, clipStart = 0, clipEnd = 40))
    assertArrayEquals(intArrayOf(15, 0, 16, 59), SeparatorGeometry.line(Edge.LEFT, child, decorated, sizePx = 1, clipStart = 0, clipEnd = 300))
  }
}
