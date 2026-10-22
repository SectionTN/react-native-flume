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
}
