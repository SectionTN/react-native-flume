package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SlotPoolTest {
  private val list = SlotGroup(viewType = 1)
  private val cards = SlotGroup(viewType = 3)

  @Test
  fun aKeyComesBackToItsSlot() {
    val pool = SlotPool()
    val a = pool.acquire(owner = 1, list, SlotKinds.ROW, "a", index = 0)
    pool.release(1, a)
    assertEquals(a, pool.acquire(owner = 2, list, SlotKinds.ROW, "a", index = 4))
    assertEquals(SlotEntry(a, SlotKinds.ROW, "a", 4, bound = true), pool.entry(a))
  }

  @Test
  fun aSlotNeverShowsAnotherItemOrGroup() {
    val pool = SlotPool()
    val a = pool.acquire(1, list, SlotKinds.ROW, "a", 0)
    pool.release(1, a)
    assertNotEquals(a, pool.acquire(2, list, SlotKinds.ROW, "b", 1))
    assertNotEquals(a, pool.acquire(3, cards, SlotKinds.ROW, "a", 0))
    assertNotEquals(pool.acquire(4, list, SlotKinds.ROW, "c", 2), pool.acquire(5, list, SlotKinds.ROW, "c", 2))
  }

  @Test
  fun theOldestReleasedSlotLeavesPastTheCap() {
    val pool = SlotPool(maxFree = 2)
    val ids = listOf("a", "b", "c").mapIndexed { i, key ->
      pool.acquire(i + 1, list, SlotKinds.ROW, key, i).also { pool.release(i + 1, it) }
    }
    assertEquals(ids.drop(1), pool.snapshot().map { it.slot })
    assertTrue(pool.acquire(9, list, SlotKinds.ROW, "a", 0) > ids.last())
  }

  @Test
  fun onlyTheOwnerReleases() {
    val pool = SlotPool()
    val a = pool.acquire(1, list, SlotKinds.ROW, "a", 0)
    pool.release(2, a)
    assertEquals(1, pool.ownerOf(a))
  }

  @Test
  fun theSnapshotListsBoundAndReleasedSlotsAndTracksChanges() {
    val pool = SlotPool()
    val a = pool.acquire(1, list, SlotKinds.ROW, "a", 0)
    val b = pool.acquire(2, list, SlotKinds.ROW, "b", 1)
    pool.release(1, a)
    assertTrue(pool.dirty)
    assertEquals(listOf(false, true), pool.snapshot().map { it.bound })
    assertFalse(pool.dirty)
    pool.setIndex(b, 1)
    assertFalse(pool.dirty)
    pool.setIndex(b, 2)
    assertTrue(pool.dirty)
  }

  @Test
  fun aBindAdoptsAPreRenderedSlotForItsKey() {
    val pool = SlotPool()
    pool.register(-1, SlotKinds.ROW, "a")
    assertEquals(-1, pool.acquire(1, list, SlotKinds.ROW, "a", 0))
    assertEquals(-1, pool.jsSeen)
    assertNotEquals(-1, pool.acquire(2, cards, SlotKinds.ROW, "a", 0))
  }

  @Test
  fun aReleaseNeverEvictsAPreRenderedSlot() {
    val pool = SlotPool(maxFree = 2)
    pool.register(-1, SlotKinds.ROW, "a")
    listOf("x", "y", "z").forEachIndexed { i, key -> pool.release(i + 1, pool.acquire(i + 1, list, SlotKinds.ROW, key, i)) }
    assertEquals(-1, pool.acquire(9, list, SlotKinds.ROW, "a", 0))
  }

  @Test
  fun aMountBatchOfPreRenderedSlotsPastTheCapWaitsForItsBinds() {
    val pool = SlotPool(maxFree = 2)
    val keys = listOf("a", "b", "c", "d", "e")
    keys.forEachIndexed { i, key -> pool.register(-1 - i, SlotKinds.ROW, key) }
    keys.forEachIndexed { i, key -> assertEquals(-1 - i, pool.acquire(i + 1, list, SlotKinds.ROW, key, i)) }
  }

  @Test
  fun olderPreRenderedSlotsPastTheCapLeaveAsTheNextBatchRegisters() {
    val pool = SlotPool(maxFree = 2)
    listOf("a", "b", "c").forEachIndexed { i, key -> pool.register(-1 - i, SlotKinds.ROW, key) }
    assertEquals(listOf(-3, -2, -1), pool.snapshot().map { it.slot })
    pool.endBatch()
    pool.register(-4, SlotKinds.ROW, "d")
    assertEquals(listOf(-4, -3, -2), pool.snapshot().map { it.slot })
  }

  @Test
  fun aPreRenderedSlotReplacesOneStillWaiting() {
    val pool = SlotPool()
    val waiting = pool.acquire(1, list, SlotKinds.ROW, "a", 3)
    pool.register(-2, SlotKinds.ROW, "a")
    pool.replace(owner = 1, waiting = waiting, prerendered = -2)
    assertEquals(listOf(SlotEntry(-2, SlotKinds.ROW, "a", 3, bound = true)), pool.snapshot())
  }
}
