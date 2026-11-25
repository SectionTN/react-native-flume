#include "adapter/ScrollCommands.h"

namespace flume {

void ScrollCommands::begin(int32_t id) {
  id_ = id;
  landed_ = false;
}

void ScrollCommands::land() { landed_ = id_ != 0; }

int32_t ScrollCommands::end() {
  const int32_t id = id_;
  reset();
  return id;
}

int32_t ScrollCommands::settle(const Rest& rest) {
  if (!landed_ || rest.moving || rest.dragging || rest.jumpWaiting || rest.realignPending) return 0;
  return end();
}

void ScrollCommands::reset() {
  id_ = 0;
  landed_ = false;
}

}  // namespace flume
