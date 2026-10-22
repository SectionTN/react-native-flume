package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class FabricChildrenTest {
  @Test
  fun mirrorsFabricInsertsAndRemoves() {
    val children = FabricChildren<Pair<Int, String>> { it.first }
    children.add(0, 7 to "a")
    children.add(1, 9 to "b")
    children.add(1, 8 to "c")
    assertEquals(listOf(7, 8, 9), (0 until children.count).map { children.get(it)!!.first })
    assertEquals(8 to "c", children.removeAt(1))
    assertNull(children.bySlot(8))
    assertEquals(9 to "b", children.bySlot(9))
    assertNull(children.get(2))
  }
}
