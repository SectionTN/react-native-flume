package me.sectiontn.flume

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class KeyboardGestureTest {
  private val never = KeyboardTapPolicy.decide(KeyboardTaps.NEVER, keyboardUp = true, pressableHit = false)

  // A drag that starts while the keyboard is up scrolls the list and leaves the keyboard open, as in a ScrollView.
  @Test fun aDragKeepsTheKeyboard() {
    val gesture = KeyboardGesture()
    gesture.down(never)
    gesture.scrolled()
    assertFalse(gesture.release())
  }

  @Test fun aTapClosesTheKeyboardOnRelease() {
    val gesture = KeyboardGesture()
    gesture.down(never)
    assertTrue(gesture.release())
  }
}
