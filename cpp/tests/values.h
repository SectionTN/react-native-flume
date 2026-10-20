#pragma once

#include <initializer_list>
#include <string>
#include <utility>

#include "core/Value.h"

inline flume::Value V(const char* s) { return flume::Value(std::string(s)); }
inline flume::Value V(double n) { return flume::Value(n); }
inline flume::Value V(int n) { return flume::Value(static_cast<double>(n)); }
inline flume::Value V(bool b) { return flume::Value(b); }

inline flume::Value obj(std::initializer_list<std::pair<std::string, flume::Value>> entries) {
  flume::Value::Object object;
  for (const auto& entry : entries) object.emplace_back(entry.first, entry.second);
  return flume::Value(std::move(object));
}

inline flume::Value arr(std::initializer_list<flume::Value> items) {
  return flume::Value(flume::Value::Array(items));
}
