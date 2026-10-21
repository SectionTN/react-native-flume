#pragma once

#include <mutex>
#include <string>
#include <unordered_set>

namespace flume {

// Messages seen so far, shared by the JS and UI threads; first() is true once per message.
class OnceSet {
 public:
  bool first(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return seen_.insert(message).second;
  }

 private:
  std::mutex mutex_;
  std::unordered_set<std::string> seen_;
};

}  // namespace flume
