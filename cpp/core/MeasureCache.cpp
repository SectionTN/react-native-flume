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
  key += '\0';
  key += request.fontFamily;
  key += '\0';
  key += request.italic ? '1' : '0';
  key += std::to_string(request.lineHeight);
  key += '\0';
  key += std::to_string(request.letterSpacing);
  key += '\0';
  key += std::to_string(request.textAlign);
  key += std::to_string(request.decoration);
  key += request.allowFontScaling ? '1' : '0';
  return key;
}

}  // namespace flume
