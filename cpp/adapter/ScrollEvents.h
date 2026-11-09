#pragma once

#include <vector>

namespace flume {

enum class ScrollEventKind { Scroll, BeginDrag, EndDrag, MomentumBegin, MomentumEnd };

// React Native's iOS rules: the throttle, and which events each UIScrollView callback sends.
class ScrollEvents {
 public:
  void setThrottleMs(double ms);  // negative: forced scrolls only; one frame or less: every scroll
  bool scrolled(double nowMs);    // didScroll: true when a scroll event goes out
  std::vector<ScrollEventKind> willBeginDragging();
  std::vector<ScrollEventKind> willEndDragging();
  void didEndDragging(bool decelerate);
  std::vector<ScrollEventKind> willBeginDecelerating();
  std::vector<ScrollEventKind> didEndDecelerating();
  std::vector<ScrollEventKind> scrolledProgrammatically(bool animated);  // animated: nothing until it ends
  std::vector<ScrollEventKind> didEndScrollingAnimation();
  std::vector<ScrollEventKind> leftWindow();  // ends a fling or a programmatic scroll still running

 private:
  enum class Phase { Idle, Dragging, Decelerating, Animating };

  void force() { lastMs_ = 0; }

  Phase phase_ = Phase::Idle;
  double intervalMs_ = 0;
  double lastMs_ = 0;  // 0 before the first scroll and after a forced edge
};

// An inverted list counts its offset from the content start, its visual end; the mapping is its own inverse.
float contentStartOffset(float offset, float contentLength, float viewportLength, bool inverted);

}  // namespace flume
