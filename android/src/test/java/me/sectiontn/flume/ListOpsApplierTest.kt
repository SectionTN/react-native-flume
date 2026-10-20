package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class ListOpsApplierTest {
  private val log = mutableListOf<String>()
  private val notifier = object : ListNotifier {
    override fun reset() { log += "reset" }
    override fun removed(index: Int, count: Int) { log += "removed $index $count" }
    override fun inserted(index: Int, count: Int) { log += "inserted $index $count" }
    override fun moved(from: Int, to: Int) { log += "moved $from $to" }
    override fun changed(index: Int) { log += "changed $index" }
  }

  @Test
  fun appliesRemovesInsertsMovesThenChanges() {
    ListOpsApplier.apply(false, intArrayOf(3, 1, 0, 2), intArrayOf(1, 1), intArrayOf(2, 0), intArrayOf(1), notifier)
    assertEquals(listOf("removed 3 1", "removed 0 2", "inserted 1 1", "moved 2 0", "changed 1"), log)
  }

  @Test
  fun resetShortCircuits() {
    ListOpsApplier.apply(true, intArrayOf(0, 1), intArrayOf(), intArrayOf(), intArrayOf(), notifier)
    assertEquals(listOf("reset"), log)
  }
}
