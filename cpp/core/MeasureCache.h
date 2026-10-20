#pragma once

#include <string>
#include <unordered_map>

#include "core/TextMeasurer.h"

namespace flume {

// Keyed by text, font size, weight, width constraint and line limit; cleared when it reaches 4096 entries.
class MeasureCache {
 public:
  const TextMetrics* find(const TextMeasureRequest& request) const;
  void put(const TextMeasureRequest& request, TextMetrics metrics);
  void clear();

 private:
  static std::string keyOf(const TextMeasureRequest& request);
  std::unordered_map<std::string, TextMetrics> entries_;
};

}  // namespace flume
