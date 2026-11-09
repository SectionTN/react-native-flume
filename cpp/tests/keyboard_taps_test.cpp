#include "doctest.h"

#include "adapter/KeyboardTaps.h"

using namespace flume;

// iOS reads the keyboard from its frame: a focused input with the keyboard hidden is keyboardUp false.
TEST_CASE("a focused input with the keyboard hidden lets taps through") {
  CHECK(decideTap(KeyboardTaps::Never, false, true) == TapDecision{false, false});
}

TEST_CASE("never dismisses and swallows the tap") {
  CHECK(decideTap(KeyboardTaps::Never, true, true) == TapDecision{true, true});
}

TEST_CASE("nothing happens without a keyboard") {
  CHECK(decideTap(KeyboardTaps::Never, false, false) == TapDecision{false, false});
}

TEST_CASE("handled keeps the keyboard for a pressable") {
  CHECK(decideTap(KeyboardTaps::Handled, true, true) == TapDecision{false, false});
}

TEST_CASE("handled dismisses elsewhere without swallowing") {
  CHECK(decideTap(KeyboardTaps::Handled, true, false) == TapDecision{true, false});
}

TEST_CASE("always never dismisses") {
  CHECK(decideTap(KeyboardTaps::Always, true, false) == TapDecision{false, false});
}
