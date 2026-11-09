#pragma once

namespace flume {

enum class KeyboardTaps { Always, Never, Handled };

struct TapDecision {
  bool dismiss;
  bool swallow;
  bool operator==(const TapDecision&) const = default;
};

// never: the first tap only closes the keyboard; handled: a tap off any pressable closes it; always: never.
TapDecision decideTap(KeyboardTaps mode, bool keyboardUp, bool pressableHit);

}  // namespace flume
