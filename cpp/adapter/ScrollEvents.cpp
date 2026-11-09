#include "adapter/ScrollEvents.h"

#include <limits>

namespace flume {

namespace {

// React Native sends every scroll at a throttle of one frame or less.
constexpr double kFrameMs = 1000.0 / 60.0;

}  // namespace

void ScrollEvents::setThrottleMs(double ms) {
  if (ms < 0) {
    intervalMs_ = std::numeric_limits<double>::infinity();
  } else {
    intervalMs_ = ms <= kFrameMs ? 0 : ms;
  }
}

bool ScrollEvents::scrolled(double nowMs) {
  if (lastMs_ != 0 && nowMs - lastMs_ <= intervalMs_) return false;
  lastMs_ = nowMs;
  return true;
}

std::vector<ScrollEventKind> ScrollEvents::willBeginDragging() {
  phase_ = Phase::Dragging;
  force();
  return {ScrollEventKind::BeginDrag};
}

std::vector<ScrollEventKind> ScrollEvents::willEndDragging() { return {ScrollEventKind::EndDrag}; }

void ScrollEvents::didEndDragging(bool decelerate) {
  if (!decelerate) phase_ = Phase::Idle;
  force();
}

std::vector<ScrollEventKind> ScrollEvents::willBeginDecelerating() {
  phase_ = Phase::Decelerating;
  force();
  return {ScrollEventKind::MomentumBegin};
}

std::vector<ScrollEventKind> ScrollEvents::didEndDecelerating() {
  phase_ = Phase::Idle;
  force();
  return {ScrollEventKind::MomentumEnd};
}

std::vector<ScrollEventKind> ScrollEvents::scrolledProgrammatically(bool animated) {
  force();
  if (animated) {
    phase_ = Phase::Animating;
    return {};
  }
  phase_ = Phase::Idle;
  return {ScrollEventKind::Scroll, ScrollEventKind::MomentumEnd};
}

std::vector<ScrollEventKind> ScrollEvents::didEndScrollingAnimation() {
  phase_ = Phase::Idle;
  force();
  return {ScrollEventKind::Scroll, ScrollEventKind::MomentumEnd};
}

std::vector<ScrollEventKind> ScrollEvents::leftWindow() {
  const bool moving = phase_ == Phase::Decelerating || phase_ == Phase::Animating;
  phase_ = Phase::Idle;
  if (!moving) return {};
  return {ScrollEventKind::MomentumEnd};
}

float contentStartOffset(float offset, float contentLength, float viewportLength, bool inverted) {
  return inverted ? contentLength - viewportLength - offset : offset;
}

}  // namespace flume
