#include "KeyboardTaps.h"

namespace flume {

TapDecision decideTap(KeyboardTaps mode, bool keyboardUp, bool pressableHit) {
  if (!keyboardUp || mode == KeyboardTaps::Always) return {false, false};
  if (mode == KeyboardTaps::Never) return {true, true};
  return {!pressableHit, false};
}

}  // namespace flume
