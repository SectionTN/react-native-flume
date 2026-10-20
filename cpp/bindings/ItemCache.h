#pragma once

#include <jsi/jsi.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/Value.h"

namespace flume {

// Materialized items per key on the flume runtime; every method runs under that runtime's lock.
class ItemCache {
 public:
  // Converts on a miss or a version change; the reference is valid until the next retain or clear.
  const facebook::jsi::Value& materialize(facebook::jsi::Runtime& rt, const std::string& key, uint64_t version,
                                          const Value& item);
  void retain(const std::vector<std::string>& keys);  // drops every entry whose key is absent
  void clear();
  size_t size() const { return entries_.size(); }
  int conversions = 0;

 private:
  std::unordered_map<std::string, std::pair<uint64_t, facebook::jsi::Value>> entries_;
};

}  // namespace flume
