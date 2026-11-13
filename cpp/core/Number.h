#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

namespace flume {

// normalize-colors' [-+]?\d*\.?\d+, or with css Fabric's CSS number (a trailing dot and an exponent too); no strtod.
inline std::optional<double> parseNumber(std::string_view s, bool css) {
  size_t i = 0;
  auto digit = [&] { return i < s.size() && s[i] >= '0' && s[i] <= '9'; };
  bool negative = false;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
  double mantissa = 0;
  int whole = 0;
  int fraction = 0;
  for (; digit(); i++, whole++) mantissa = mantissa * 10 + (s[i] - '0');
  if (i < s.size() && s[i] == '.') {
    for (i++; digit(); i++, fraction++) mantissa = mantissa * 10 + (s[i] - '0');
    if (fraction == 0 && (!css || whole == 0)) return std::nullopt;
  }
  if (whole + fraction == 0) return std::nullopt;
  int exponent = 0;
  if (css && i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    i++;
    bool down = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) down = s[i++] == '-';
    int digits = 0;
    for (; digit(); i++, digits++) exponent = std::min(exponent * 10 + (s[i] - '0'), 400);
    if (digits == 0) return std::nullopt;
    if (down) exponent = -exponent;
  }
  if (i != s.size()) return std::nullopt;
  int scale = exponent - fraction;
  double value = scale >= 0 ? mantissa * std::pow(10.0, scale) : mantissa / std::pow(10.0, -scale);
  return negative ? -value : value;
}

}  // namespace flume
