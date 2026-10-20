#pragma once

#include <string_view>

namespace flume {

struct TextMetrics {
  float width;
  float height;
};

struct TextMeasureRequest {
  std::string_view text;
  float fontSize;
  int fontWeight;
  float maxWidth;
  int numberOfLines;
};

// Implemented per platform with the same text layout the platform draws, so measured and drawn text agree.
class TextMeasurer {
 public:
  virtual ~TextMeasurer() = default;
  virtual TextMetrics measure(const TextMeasureRequest& request) = 0;
};

}  // namespace flume
