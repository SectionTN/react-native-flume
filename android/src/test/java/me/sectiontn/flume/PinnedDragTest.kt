package me.sectiontn.flume

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PinnedDragTest {
  @Test fun aTapOnThePinnedHeaderStaysWithIt() =
    assertFalse(PinnedDrag.takesOver(onPinned = true, dx = 3f, dy = 4f, slopPx = 8))

  @Test fun aDragOnThePinnedHeaderGoesToTheList() =
    assertTrue(PinnedDrag.takesOver(onPinned = true, dx = 0f, dy = 12f, slopPx = 8))

  @Test fun aDragElsewhereIsLeftAlone() =
    assertFalse(PinnedDrag.takesOver(onPinned = false, dx = 0f, dy = 40f, slopPx = 8))
}
