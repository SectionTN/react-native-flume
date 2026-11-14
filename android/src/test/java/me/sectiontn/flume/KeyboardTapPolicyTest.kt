package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class KeyboardTapPolicyTest {
  // Back hides the IME while the input keeps its focus; the next tap on a row has to reach the row.
  @Test fun aFocusedInputWithTheKeyboardHiddenLetsTapsThrough() =
    assertFalse(KeyboardState.dismissible(hasFocusedInput = true, imeVisible = false))

  @Test fun neverDismissesAndSwallowsTheTap() =
    assertEquals(TapDecision(dismiss = true, swallow = true), KeyboardTapPolicy.decide(KeyboardTaps.NEVER, keyboardUp = true, pressableHit = true, textInputHit = false))

  @Test fun nothingHappensWithoutAKeyboard() =
    assertEquals(TapDecision(false, false), KeyboardTapPolicy.decide(KeyboardTaps.NEVER, keyboardUp = false, pressableHit = false, textInputHit = false))

  @Test fun handledKeepsTheKeyboardForAPressable() =
    assertEquals(TapDecision(false, false), KeyboardTapPolicy.decide(KeyboardTaps.HANDLED, keyboardUp = true, pressableHit = true, textInputHit = false))

  @Test fun handledDismissesElsewhereWithoutSwallowing() =
    assertEquals(TapDecision(true, false), KeyboardTapPolicy.decide(KeyboardTaps.HANDLED, keyboardUp = true, pressableHit = false, textInputHit = false))

  @Test fun alwaysNeverDismisses() =
    assertEquals(TapDecision(false, false), KeyboardTapPolicy.decide(KeyboardTaps.ALWAYS, keyboardUp = true, pressableHit = false, textInputHit = false))

  // ScrollView captures a never-mode touch only when its target is not a TextInput.
  @Test fun neverLetsATapOnATextInputThroughAndKeepsTheKeyboard() =
    assertEquals(TapDecision(false, false), KeyboardTapPolicy.decide(KeyboardTaps.NEVER, keyboardUp = true, pressableHit = true, textInputHit = true))

  @Test fun handledKeepsTheKeyboardForATextInput() =
    assertEquals(TapDecision(false, false), KeyboardTapPolicy.decide(KeyboardTaps.HANDLED, keyboardUp = true, pressableHit = false, textInputHit = true))
}
