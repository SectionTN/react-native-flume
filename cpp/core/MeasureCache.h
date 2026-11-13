#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "core/TextMeasurer.h"

namespace flume {

// Keyed by every field of the request and looked up without a copy; cleared when it reaches 4096 entries.
class MeasureCache {
 public:
  const TextMetrics* find(const TextMeasureRequest& request) const;
  void put(const TextMeasureRequest& request, TextMetrics metrics);
  void clear();

 private:
  // A request that owns its two strings; the views in fields stay empty.
  struct Key {
    explicit Key(const TextMeasureRequest& request);
    TextMeasureRequest view() const;
    std::string text;
    std::string fontFamily;
    TextMeasureRequest fields;
  };

  struct Hash {
    using is_transparent = void;
    size_t operator()(const TextMeasureRequest& request) const;
    size_t operator()(const Key& key) const { return (*this)(key.view()); }
  };

  struct Equal {
    using is_transparent = void;
    static bool same(const TextMeasureRequest& a, const TextMeasureRequest& b);
    bool operator()(const Key& a, const Key& b) const { return same(a.view(), b.view()); }
    bool operator()(const Key& a, const TextMeasureRequest& b) const { return same(a.view(), b); }
    bool operator()(const TextMeasureRequest& a, const Key& b) const { return same(a, b.view()); }
  };

  std::unordered_map<Key, TextMetrics, Hash, Equal> entries_;
};

}  // namespace flume
