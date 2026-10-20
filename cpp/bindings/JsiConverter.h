#pragma once

#include <jsi/jsi.h>

#include "core/Value.h"

namespace flume {

// Plain data only: functions, symbols, bigints and host objects become Null.
Value toValue(facebook::jsi::Runtime& rt, const facebook::jsi::Value& value);
facebook::jsi::Value toJsi(facebook::jsi::Runtime& rt, const Value& value);

}  // namespace flume
