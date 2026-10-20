#include "core/Color.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/ColorNames.h"

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

struct Part {
  double value;
  bool percent;
};

// The comma separated arguments inside the parentheses, each a number with an optional trailing %.
std::optional<std::vector<Part>> parseParts(std::string_view text) {
  if (text.empty() || text.back() != ')') return std::nullopt;
  text.remove_suffix(1);
  std::vector<Part> parts;
  while (true) {
    size_t comma = text.find(',');
    std::string_view piece = trim(comma == std::string_view::npos ? text : text.substr(0, comma));
    if (piece.empty() || parts.size() == 4) return std::nullopt;
    bool percent = piece.back() == '%';
    if (percent) piece.remove_suffix(1);
    std::string buffer(piece);
    char* end = nullptr;
    double value = std::strtod(buffer.c_str(), &end);
    if (buffer.empty() || end == nullptr || *end != '\0') return std::nullopt;
    parts.push_back({value, percent});
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return parts;
}

uint32_t channel(double v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 255.0))); }
uint32_t alphaByte(double a) { return static_cast<uint32_t>(std::lround(std::clamp(a, 0.0, 1.0) * 255.0)); }
uint32_t pack(uint32_t a, uint32_t rgb) { return (a << 24) | (rgb & 0xFFFFFFu); }
uint32_t unit(double fraction) { return static_cast<uint32_t>(std::lround(fraction * 255.0)); }

double hue2rgb(double p, double q, double t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0 / 6) return p + (q - p) * 6 * t;
  if (t < 1.0 / 2) return q;
  if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
  return p;
}

uint32_t hslToRgb(double h, double s, double l) {
  double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  double p = 2 * l - q;
  return (unit(hue2rgb(p, q, h + 1.0 / 3)) << 16) | (unit(hue2rgb(p, q, h)) << 8) | unit(hue2rgb(p, q, h - 1.0 / 3));
}

uint32_t hwbToRgb(double h, double w, double b) {
  if (w + b >= 1) {
    uint32_t gray = static_cast<uint32_t>(std::lround(w * 255 / (w + b)));
    return (gray << 16) | (gray << 8) | gray;
  }
  double red = hue2rgb(0, 1, h + 1.0 / 3) * (1 - w - b) + w;
  double green = hue2rgb(0, 1, h) * (1 - w - b) + w;
  double blue = hue2rgb(0, 1, h - 1.0 / 3) * (1 - w - b) + w;
  return (unit(red) << 16) | (unit(green) << 8) | unit(blue);
}

double hue(double degrees) { return (std::fmod(std::fmod(degrees, 360.0) + 360.0, 360.0)) / 360.0; }
double percentage(double v) { return std::clamp(v, 0.0, 100.0) / 100.0; }

std::optional<uint32_t> parseRgb(std::string_view text, bool withAlpha) {
  auto parts = parseParts(text);
  if (!parts || parts->size() != (withAlpha ? 4u : 3u)) return std::nullopt;
  for (const Part& part : *parts) {
    if (part.percent) return std::nullopt;
  }
  uint32_t rgb = (channel((*parts)[0].value) << 16) | (channel((*parts)[1].value) << 8) | channel((*parts)[2].value);
  return pack(alphaByte(withAlpha ? (*parts)[3].value : 1), rgb);
}

// hsl takes a hue, two percentages and an optional alpha number; hwb takes a hue and two percentages.
std::optional<uint32_t> parseHue(std::string_view text, bool hwb, bool withAlpha) {
  auto parts = parseParts(text);
  if (!parts || parts->size() != (withAlpha ? 4u : 3u)) return std::nullopt;
  const auto& p = *parts;
  if (p[0].percent || !p[1].percent || !p[2].percent || (withAlpha && p[3].percent)) return std::nullopt;
  uint32_t rgb = hwb ? hwbToRgb(hue(p[0].value), percentage(p[1].value), percentage(p[2].value))
                     : hslToRgb(hue(p[0].value), percentage(p[1].value), percentage(p[2].value));
  return pack(alphaByte(withAlpha ? p[3].value : 1), rgb);
}

std::optional<uint32_t> keyword(std::string_view name) {
  auto it = std::lower_bound(kColorNames.begin(), kColorNames.end(), name,
                             [](const auto& entry, std::string_view key) { return entry.first < key; });
  if (it == kColorNames.end() || it->first != name) return std::nullopt;
  return it->second;
}

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

}  // namespace

std::optional<uint32_t> parseColor(std::string_view text) {
  if (text.empty()) return std::nullopt;
  if (text.front() == '#') return parseHex(text.substr(1));
  if (startsWith(text, "rgba(")) return parseRgb(text.substr(5), true);
  if (startsWith(text, "rgb(")) return parseRgb(text.substr(4), false);
  if (startsWith(text, "hsla(")) return parseHue(text.substr(5), false, true);
  if (startsWith(text, "hsl(")) return parseHue(text.substr(4), false, false);
  if (startsWith(text, "hwb(")) return parseHue(text.substr(4), true, false);
  return keyword(text);
}

}  // namespace flume
