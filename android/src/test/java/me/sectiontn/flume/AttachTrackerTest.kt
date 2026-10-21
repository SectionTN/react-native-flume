package me.sectiontn.flume

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AttachTrackerTest {
  @Test
  fun theFirstAttachReloads() {
    assertTrue(AttachTracker().onAttach(1))
  }

  @Test
  fun theSameStoreAttachedAgainDoesNotReload() {
    val tracker = AttachTracker()
    tracker.onAttach(1)
    assertFalse(tracker.onAttach(1))
  }

  @Test
  fun aReattachAfterADetachReloads() {
    val tracker = AttachTracker()
    tracker.onAttach(1)
    tracker.onDetach()
    assertTrue(tracker.onAttach(1))
  }

  @Test
  fun anotherStoreReloads() {
    val tracker = AttachTracker()
    tracker.onAttach(1)
    assertTrue(tracker.onAttach(2))
  }
}
