#include "core/MeasureCache.h"

#include <functional>
#include <string_view>

namespace flume {

constexpr size_t kMaxEntries = 4096;

MeasureCache::Key::Key(const TextMeasureRequest& request)
    : text(request.text), fontFamily(request.fontFamily), fields(request) {
  fields.text = {};
  fields.fontFamily = {};
}

TextMeasureRequest MeasureCache::Key::view() const {
  TextMeasureRequest request = fields;
  request.text = text;
  request.fontFamily = fontFamily;
  return request;
}

size_t MeasureCache::Hash::operator()(const TextMeasureRequest& r) const {
  size_t seed = std::hash<std::string_view>{}(r.text);
  auto mix = [&seed](size_t h) { seed ^= h + size_t{0x9e3779b9} + (seed << 6) + (seed >> 2); };
  mix(std::hash<std::string_view>{}(r.fontFamily));
  mix(std::hash<float>{}(r.fontSize));
  mix(std::hash<int>{}(r.fontWeight));
  mix(std::hash<float>{}(r.maxWidth));
  mix(std::hash<int>{}(r.numberOfLines));
  mix(std::hash<float>{}(r.lineHeight));
  mix(std::hash<float>{}(r.letterSpacing));
  mix(std::hash<int>{}(r.textAlign));
  mix(std::hash<int>{}(r.decoration));
  mix((r.italic ? 1u : 0u) | (r.allowFontScaling ? 2u : 0u));
  return seed;
}

bool MeasureCache::Equal::same(const TextMeasureRequest& a, const TextMeasureRequest& b) {
  return a.text == b.text && a.fontFamily == b.fontFamily && a.fontSize == b.fontSize &&
         a.fontWeight == b.fontWeight && a.maxWidth == b.maxWidth && a.numberOfLines == b.numberOfLines &&
         a.italic == b.italic && a.lineHeight == b.lineHeight && a.letterSpacing == b.letterSpacing &&
         a.textAlign == b.textAlign && a.decoration == b.decoration && a.allowFontScaling == b.allowFontScaling;
}

const TextMetrics* MeasureCache::find(const TextMeasureRequest& request) const {
  auto it = entries_.find(request);
  return it == entries_.end() ? nullptr : &it->second;
}

void MeasureCache::put(const TextMeasureRequest& request, TextMetrics metrics) {
  if (entries_.size() >= kMaxEntries) entries_.clear();
  entries_.insert_or_assign(Key(request), metrics);
}

void MeasureCache::clear() { entries_.clear(); }

}  // namespace flume
