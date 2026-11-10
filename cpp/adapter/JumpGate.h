#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>

#include "adapter/ScrollTargets.h"

namespace flume {

// Holds a jump until every slot it waits for has rendered, or until its wait runs out; one jump waits at a time.
template <class Jump>
class JumpGate {
 public:
  explicit JumpGate(int64_t waitMs = kJumpWaitMs) : waitMs_(waitMs) {}

  // Returns the jump this one replaced, whose slots the caller releases.
  std::optional<Jump> request(Jump jump, std::unordered_set<int> slots, int64_t nowMs) {
    std::optional<Jump> replaced = take();
    pending_ = std::move(jump);
    waiting_ = std::move(slots);
    deadlineMs_ = nowMs + waitMs_;
    return replaced;
  }

  std::optional<Jump> rendered(int slot) {
    if (!pending_ || waiting_.erase(slot) == 0 || !waiting_.empty()) return std::nullopt;
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
    return jump;
  }

  int64_t waitMs_;
  std::optional<Jump> pending_;
  std::unordered_set<int> waiting_;
  int64_t deadlineMs_ = 0;
};

}  // namespace flume
