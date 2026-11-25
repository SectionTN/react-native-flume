#pragma once

#include <cstdint>

namespace flume {

// The scroll command in flight: its id is reported once, when it settles at rest or is interrupted.
class ScrollCommands {
 public:
  struct Rest {
    bool moving = false;
    bool dragging = false;
    bool jumpWaiting = false;
    bool realignPending = false;
  };

  void begin(int32_t id);
  // The command's own movement has finished, so a list at rest ends it.
  void land();
  // Ends the command in flight and returns its id, or 0 when there is none.
  int32_t end();
  // Ends the command once it has landed and the list rests, else returns 0.
  int32_t settle(const Rest& rest);
  int32_t pending() const { return id_; }
  void reset();

 private:
  int32_t id_ = 0;
  bool landed_ = false;
};

}  // namespace flume
