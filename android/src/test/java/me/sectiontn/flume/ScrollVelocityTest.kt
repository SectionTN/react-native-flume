package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class ScrollVelocityTest {
  private val none = Pair(0f, 0f)

  @Test
  fun aScrollCarriesTheOffsetChangePerMsSinceTheLastChange() {
    val velocity = ScrollVelocity()
    velocity.scrolled(100, 1_000)
    assertEquals(none, velocity.of(ScrollKind.SCROLL, horizontal = false, afterDrag = false))
    velocity.scrolled(130, 1_010)
    assertEquals(Pair(0f, 3f), velocity.of(ScrollKind.SCROLL, horizontal = false, afterDrag = false))
    assertEquals(Pair(3f, 0f), velocity.of(ScrollKind.SCROLL, horizontal = true, afterDrag = false))
    velocity.scrolled(110, 1_020)
    assertEquals(Pair(0f, -2f), velocity.of(ScrollKind.SCROLL, horizontal = false, afterDrag = false))
  }

  @Test
  fun aChangeInTheSameMsKeepsTheLastVelocity() {
    val velocity = ScrollVelocity()
    velocity.scrolled(0, 1_000)
    velocity.scrolled(40, 1_010)
    velocity.scrolled(50, 1_010)
    assertEquals(Pair(0f, 4f), velocity.of(ScrollKind.SCROLL, horizontal = false, afterDrag = false))
  }

  @Test
  fun theDragsEndCarriesTheFingerAndAnInvertedListFlipsItsAxis() {
    val velocity = ScrollVelocity()
    velocity.released(1f, -5f, horizontal = false, inverted = false)
    assertEquals(Pair(1f, -5f), velocity.of(ScrollKind.END_DRAG, horizontal = false, afterDrag = false))
    velocity.released(1f, -5f, horizontal = false, inverted = true)
    assertEquals(Pair(1f, 5f), velocity.of(ScrollKind.END_DRAG, horizontal = false, afterDrag = false))
    velocity.released(-4f, 2f, horizontal = true, inverted = true)
    assertEquals(Pair(4f, 2f), velocity.of(ScrollKind.END_DRAG, horizontal = true, afterDrag = false))
  }

  @Test
  fun momentumAfterADragIsTheReleaseInWholePixels() {
    val velocity = ScrollVelocity()
    velocity.released(0.4f, -5.6f, horizontal = false, inverted = false)
    assertEquals(Pair(0f, -6f), velocity.of(ScrollKind.MOMENTUM_BEGIN, horizontal = false, afterDrag = true))
    assertEquals(none, velocity.of(ScrollKind.MOMENTUM_BEGIN, horizontal = false, afterDrag = false))
  }

  @Test
  fun aNewTouchForgetsTheLastRelease() {
    val velocity = ScrollVelocity()
    velocity.released(0f, -5f, horizontal = false, inverted = false)
    velocity.touchStarted()
    assertEquals(none, velocity.of(ScrollKind.END_DRAG, horizontal = false, afterDrag = false))
  }

  @Test
  fun theDragsStartAndTheMomentumsEndCarryNone() {
    val velocity = ScrollVelocity()
    velocity.scrolled(0, 1_000)
    velocity.scrolled(30, 1_010)
    velocity.released(0f, -5f, horizontal = false, inverted = false)
    assertEquals(none, velocity.of(ScrollKind.BEGIN_DRAG, horizontal = false, afterDrag = false))
    assertEquals(none, velocity.of(ScrollKind.MOMENTUM_END, horizontal = false, afterDrag = true))
  }
}
