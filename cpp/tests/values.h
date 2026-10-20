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

// The spec's chat cell without the pressable: row, avatar, a column with two texts.
inline flume::Value chatCell() {
  return obj(
      {{"type", V("view")},
       {"style",
        obj({{"flexDirection", V("row")}, {"padding", V(12)}, {"gap", V(12)}, {"alignItems", V("center")}})},
       {"children",
        arr({obj({{"type", V("image")},
                  {"source", V("https://x/a.png")},
                  {"style", obj({{"width", V(40)}, {"height", V(40)}, {"borderRadius", V(20)}})}}),
             obj({{"type", V("view")},
                  {"style", obj({{"flex", V(1)}})},
                  {"children",
                   arr({obj({{"type", V("text")},
                             {"text", V("hello")},
                             {"style", obj({{"fontSize", V(16)}, {"color", V("#111")}})}}),
                        obj({{"type", V("text")},
                             {"text", V("9:41")},
                             {"style", obj({{"fontSize", V(12)}, {"color", V("#666")}})}})})}})})}});
}
