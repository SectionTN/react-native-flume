#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace flume {

// The viewPosition that brings a row into view: 0 before the fully visible rows, 1 after them, none when it is one.
// Positions count list rows. With no row fully in view, the row goes to the start.
std::optional<float> intoViewPosition(int position, int firstFull, int lastFull);

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

// The status bar tap that scrolls through the jump gate: the end of its glide reports the scroll to top, once.
class TopTap {
 public:
  void begin() { pending_ = true; }
  // A drag or a newer scroll took the list over.
  void cancel() { pending_ = false; }
  // True once, for the glide the tap started.
  bool glideEnded() { return std::exchange(pending_, false); }

 private:
  bool pending_ = false;
};

}  // namespace flume
