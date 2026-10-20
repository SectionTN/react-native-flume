#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace flume {

// React Native color string to ARGB. Hex forms, rgb(), rgba() and transparent; anything else is nullopt.
std::optional<uint32_t> parseColor(std::string_view text);

}  // namespace flume
