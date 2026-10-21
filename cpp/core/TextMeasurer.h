#pragma once

#include <string_view>

namespace flume {

struct TextMetrics {
  float width;
  float height;
};

// Everything the platform needs to build the same text layout it draws; sizes in dp.
struct TextMeasureRequest {
  std::string_view text;
  float fontSize;
  int fontWeight;
  float maxWidth;
  int numberOfLines;
  std::string_view fontFamily;
  bool italic;
  float lineHeight;  // 0 for the font's own
  float letterSpacing;
  int textAlign;   // TextAlign
  int decoration;  // 1 underline, 2 line-through
  bool allowFontScaling = true;  // false keeps the sizes in dp whatever the font scale
};

// Implemented per platform with the same text layout the platform draws, so measured and drawn text agree.
class TextMeasurer {
 public:
  virtual ~TextMeasurer() = default;
  virtual TextMetrics measure(const TextMeasureRequest& request) = 0;
};

}  // namespace flume
