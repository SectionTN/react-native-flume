#include "core/Color.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

namespace flume {
namespace {

std::optional<uint32_t> hexDigit(char c) {
  if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
  return std::nullopt;
}

std::optional<uint32_t> parseHex(std::string_view digits) {
  uint32_t channels[4] = {0, 0, 0, 255};
  if (digits.size() == 3 || digits.size() == 4) {
    for (size_t i = 0; i < digits.size(); i++) {
      auto d = hexDigit(digits[i]);
      if (!d) return std::nullopt;
      channels[i] = *d * 17;
    }
  } else if (digits.size() == 6 || digits.size() == 8) {
    for (size_t i = 0; i < digits.size() / 2; i++) {
      auto hi = hexDigit(digits[i * 2]);
      auto lo = hexDigit(digits[i * 2 + 1]);
      if (!hi || !lo) return std::nullopt;
      channels[i] = *hi * 16 + *lo;
    }
  } else {
    return std::nullopt;
  }
  return (channels[3] << 24) | (channels[0] << 16) | (channels[1] << 8) | channels[2];
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

std::optional<uint32_t> parseFunctional(std::string_view text, bool withAlpha) {
  if (text.empty() || text.back() != ')') return std::nullopt;
  text.remove_suffix(1);
  double parts[4] = {0, 0, 0, 1};
  size_t count = 0;
  while (true) {
    size_t comma = text.find(',');
    std::string_view piece = trim(comma == std::string_view::npos ? text : text.substr(0, comma));
    if (piece.empty() || count == 4) return std::nullopt;
    std::string buffer(piece);
    char* end = nullptr;
    parts[count] = std::strtod(buffer.c_str(), &end);
    if (end == nullptr || *end != '\0') return std::nullopt;
    count++;
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  if (count != (withAlpha ? 4u : 3u)) return std::nullopt;
  auto channel = [](double v) { return static_cast<uint32_t>(std::lround(std::fmin(std::fmax(v, 0.0), 255.0))); };
  auto alpha = static_cast<uint32_t>(std::lround(std::fmin(std::fmax(parts[3], 0.0), 1.0) * 255.0));
  return (alpha << 24) | (channel(parts[0]) << 16) | (channel(parts[1]) << 8) | channel(parts[2]);
}

}  // namespace

std::optional<uint32_t> parseColor(std::string_view text) {
  if (text.empty()) return std::nullopt;
  if (text == "transparent") return 0u;
  if (text.front() == '#') return parseHex(text.substr(1));
  if (text.rfind("rgba(", 0) == 0) return parseFunctional(text.substr(5), true);
  if (text.rfind("rgb(", 0) == 0) return parseFunctional(text.substr(4), false);
  return std::nullopt;
}

}  // namespace flume
