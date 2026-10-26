#import <UIKit/UIKit.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "adapter/CellTree.h"
#include "core/TextMeasurer.h"

struct FLMTextStyle {
  std::string family;  // empty for the system font
  float size = 14;
  int weight = 400;
  bool italic = false;
  float lineHeight = 0;  // 0 for the font's own
  float letterSpacing = 0;
  int align = 0;       // TextAlign
  int decoration = 0;  // 1 underline, 2 line-through
  int lines = 0;
  bool scaling = true;
  uint32_t color = 0xFF000000;
};

FLMTextStyle FLMTextStyleOf(const flume::TextMeasureRequest &request);
FLMTextStyle FLMTextStyleOf(const flume::TextSpec &spec);

NSAttributedString *FLMAttributedString(std::string_view text, const FLMTextStyle &style);

// The one TextKit stack that both measuring and drawing use, so their line breaks agree.
NSTextStorage *FLMTextStorage(NSAttributedString *text, CGFloat maxWidth, int numberOfLines);

class FLMTextMeasurer final : public flume::TextMeasurer {
 public:
  flume::TextMetrics measure(const flume::TextMeasureRequest &request) override;
};
