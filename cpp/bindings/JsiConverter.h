#pragma once

#include <jsi/jsi.h>

#include <string>
#include <vector>

#include "core/Value.h"

namespace flume {

// Plain data only: functions, symbols, bigints and host objects become Null, as does anything past 64 levels;
// an undefined object property is left out.
Value toValue(facebook::jsi::Runtime& rt, const facebook::jsi::Value& value, bool* truncated = nullptr);
facebook::jsi::Value toJsi(facebook::jsi::Runtime& rt, const Value& value);

// Expands an Int32Array of indices into a table of type names; an index outside the table throws.
std::vector<std::string> typesOf(facebook::jsi::Runtime& rt, const facebook::jsi::Value& table,
                                 const facebook::jsi::Value& ids);

}  // namespace flume
