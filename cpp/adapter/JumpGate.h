#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>

#include "adapter/ScrollTargets.h"

namespace flume {

// Holds a jump until every slot it waits for has rendered, or until its wait runs out. One jump waits at a time.
template <class Jump>
class JumpGate {
 public:
  explicit JumpGate(int64_t waitMs = kJumpWaitMs) : waitMs_(waitMs) {}

  // Returns the jump this one replaced, whose slots the caller releases. A jump that waits for data runs on no render.
  std::optional<Jump> request(Jump jump, std::unordered_set<int> slots, int64_t nowMs, bool waitsForData = false) {
    std::optional<Jump> replaced = take();
    pending_ = std::move(jump);
    waiting_ = std::move(slots);
    waitsForData_ = waitsForData;
    deadlineMs_ = nowMs + waitMs_;
    return replaced;
  }

  // Swaps the waiting jump for one aimed at new rows, keeping its deadline. Returns the old one, or nothing when none waits.
  std::optional<Jump> retarget(Jump jump, std::unordered_set<int> slots, bool waitsForData = false) {
    if (!pending_) return std::nullopt;
    std::optional<Jump> replaced = std::exchange(pending_, std::move(jump));
    waiting_ = std::move(slots);
    waitsForData_ = waitsForData;
    return replaced;
  }

  std::optional<Jump> rendered(int slot) {
    if (!pending_ || waiting_.erase(slot) == 0 || !waiting_.empty() || waitsForData_) return std::nullopt;
    return take();
  }

  std::optional<Jump> cancel() { return take(); }

  std::optional<Jump> due(int64_t nowMs) { return pending_ && nowMs >= deadlineMs_ ? take() : std::nullopt; }

  const std::optional<Jump>& pending() const { return pending_; }

 private:
  std::optional<Jump> take() {
    std::optional<Jump> jump = std::move(pending_);
    pending_.reset();
    waiting_.clear();
    waitsForData_ = false;
    return jump;
  }

  int64_t waitMs_;
  std::optional<Jump> pending_;
  std::unordered_set<int> waiting_;
  bool waitsForData_ = false;
  int64_t deadlineMs_ = 0;
};

}  // namespace flume
