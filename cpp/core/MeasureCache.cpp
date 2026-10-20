#include "core/MeasureCache.h"

namespace flume {

constexpr size_t kMaxEntries = 4096;

const TextMetrics* MeasureCache::find(const TextMeasureRequest& request) const {
  auto it = entries_.find(keyOf(request));
  return it == entries_.end() ? nullptr : &it->second;
}

void MeasureCache::put(const TextMeasureRequest& request, TextMetrics metrics) {
  if (entries_.size() >= kMaxEntries) entries_.clear();
  entries_[keyOf(request)] = metrics;
}

void MeasureCache::clear() { entries_.clear(); }

std::string MeasureCache::keyOf(const TextMeasureRequest& request) {
  std::string key(request.text);
  key += '\0';
  key += std::to_string(request.fontSize);
  key += '\0';
  key += std::to_string(request.fontWeight);
  key += '\0';
  key += std::to_string(request.maxWidth);
  key += '\0';
  key += std::to_string(request.numberOfLines);
  return key;
}

}  // namespace flume
