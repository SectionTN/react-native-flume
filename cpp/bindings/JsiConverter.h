#pragma once

#include <jsi/jsi.h>

#include "core/Value.h"

namespace flume {

// Plain data only: functions, symbols, bigints and host objects become Null, as does anything past 64 levels.
Value toValue(facebook::jsi::Runtime& rt, const facebook::jsi::Value& value, bool* truncated = nullptr);
facebook::jsi::Value toJsi(facebook::jsi::Runtime& rt, const Value& value);

}  // namespace flume
