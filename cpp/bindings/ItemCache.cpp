#include "bindings/ItemCache.h"

#include <string_view>
#include <unordered_set>

#include "bindings/JsiConverter.h"

namespace flume {

namespace jsi = facebook::jsi;

const jsi::Value& ItemCache::materialize(jsi::Runtime& rt, const std::string& key, uint64_t version,
                                         const Value& item) {
  auto it = entries_.find(key);
  if (it != entries_.end() && it->second.first == version) return it->second.second;
  conversions++;
  jsi::Value materialized = toJsi(rt, item);
  if (it == entries_.end()) {
    it = entries_.emplace(key, std::make_pair(version, std::move(materialized))).first;
  } else {
    it->second = std::make_pair(version, std::move(materialized));
  }
  return it->second.second;
}

void ItemCache::retain(const std::vector<std::string>& keys) {
  std::unordered_set<std::string_view> keep(keys.begin(), keys.end());
  for (auto it = entries_.begin(); it != entries_.end();) {
    it = keep.count(it->first) == 0 ? entries_.erase(it) : std::next(it);
  }
}

void ItemCache::clear() { entries_.clear(); }

}  // namespace flume
