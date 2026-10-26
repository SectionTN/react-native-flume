#import "FLMTextView.h"

#import "FLMText.h"

@implementation FLMTextView {
  NSTextStorage *_storage;
  CGPoint _inset;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.opaque = NO;
    self.contentMode = UIViewContentModeRedraw;
  }
  return self;
}

- (void)setText:(NSAttributedString *)text lines:(int)lines maxWidth:(CGFloat)maxWidth inset:(CGPoint)inset
{
  _storage = FLMTextStorage(text, maxWidth, lines);
  _inset = inset;
  [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect
{
  NSLayoutManager *layoutManager = _storage.layoutManagers.firstObject;
  NSTextContainer *container = layoutManager.textContainers.firstObject;
  if (container == nil) {
    return;
  }
  NSRange glyphs = [layoutManager glyphRangeForTextContainer:container];
  [layoutManager drawBackgroundForGlyphRange:glyphs atPoint:_inset];
  [layoutManager drawGlyphsForGlyphRange:glyphs atPoint:_inset];
}

@end
