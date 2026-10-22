package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class ListRowsTest {
  @Test
  fun dataSitsBetweenTheHeaderAndTheFooter() {
    val rows = ListRows(header = true, dataCount = 3, footer = true)
    assertEquals(5, rows.itemCount)
    assertEquals(
      listOf(RowKind.HEADER, RowKind.DATA, RowKind.DATA, RowKind.DATA, RowKind.FOOTER),
      (0 until 5).map(rows::kind),
    )
    assertEquals(listOf(-1, 0, 1, 2, -1), (0 until 5).map(rows::dataIndex))
    assertEquals(1, rows.position(0))
  }

  @Test
  fun theEmptyRowShowsOnlyWithoutData() {
    assertEquals(RowKind.EMPTY, ListRows(header = true, dataCount = 0, emptyComponent = true, footer = true).kind(1))
    assertFalse(ListRows(dataCount = 2, emptyComponent = true).hasEmptyRow)
    assertEquals(0, ListRows(dataCount = 0).itemCount)
  }

  @Test
  fun stickyRowsAndListComponentsSpanTheGrid() =
    assertArrayEquals(intArrayOf(0, 3, 5, 6), ListRows(header = true, dataCount = 5, footer = true).fullSpanPositions(intArrayOf(2, 4, 9)))
}
