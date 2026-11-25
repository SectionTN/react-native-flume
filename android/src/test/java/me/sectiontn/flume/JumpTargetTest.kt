package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class JumpTargetTest {
  private val rows = ListRows(header = true, dataCount = 10, footer = true)

  @Test
  fun anIndexLandsOnItsDataRow() {
    assertEquals(4, JumpTarget.Index(3).position(rows))
    assertFalse(JumpTarget.Index(3).waitsForData(rows))
  }

  @Test
  fun anIndexPastTheDataLandsOnTheLastDataRowAndWaitsForData() {
    assertEquals(10, JumpTarget.Index(20).position(rows))
    assertTrue(JumpTarget.Index(20).waitsForData(rows))
    assertTrue(JumpTarget.Index(10).waitsForData(rows))
  }

  @Test
  fun theEndIsTheLastRowWithTheFooter() {
    assertEquals(11, JumpTarget.End.position(rows))
    assertFalse(JumpTarget.End.waitsForData(rows))
  }

  @Test
  fun aPositionStaysInsideTheRows() {
    assertEquals(0, JumpTarget.Position(0).position(rows))
    assertEquals(11, JumpTarget.Position(50).position(rows))
  }

  @Test
  fun noRowsHaveNoPosition() {
    val empty = ListRows()
    assertNull(JumpTarget.Index(0).position(empty))
    assertNull(JumpTarget.End.position(empty))
    assertNull(JumpTarget.Position(0).position(empty))
    assertTrue(JumpTarget.Index(0).waitsForData(empty))
  }
}
