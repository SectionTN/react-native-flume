#pragma once

namespace flume {

enum class KeyboardTaps { Always, Never, Handled };

struct TapDecision {
  bool dismiss;
  bool swallow;
  bool operator==(const TapDecision&) const = default;
};

// never: the first tap only closes the keyboard. handled: a tap off any pressable closes it. always: never. A text input always gets its tap.
TapDecision decideTap(KeyboardTaps mode, bool keyboardUp, bool pressableHit, bool textInputHit);

}  // namespace flume
