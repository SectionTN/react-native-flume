#import "FLMText.h"

#import <React/RCTFont.h>
#import <React/RCTUtils.h>

#import "FLMColor.h"

FLMTextStyle FLMTextStyleOf(const flume::TextMeasureRequest &request)
{
  FLMTextStyle style;
  style.family = std::string(request.fontFamily);
  style.size = request.fontSize;
  style.weight = request.fontWeight;
  style.italic = request.italic;
  style.lineHeight = request.lineHeight;
  style.letterSpacing = request.letterSpacing;
  style.align = request.textAlign;
  style.decoration = request.decoration;
  style.lines = request.numberOfLines;
  style.scaling = request.allowFontScaling;
  return style;
}

FLMTextStyle FLMTextStyleOf(const flume::TextSpec &spec)
{
  FLMTextStyle style;
  style.family = spec.fontFamily;
  style.size = spec.fontSize;
  style.weight = spec.fontWeight;
  style.italic = spec.italic;
  style.lineHeight = spec.lineHeight;
  style.letterSpacing = spec.letterSpacing;
  style.align = spec.textAlign;
  style.decoration = spec.decoration;
  style.lines = spec.numberOfLines;
  style.scaling = spec.allowFontScaling;
  style.color = spec.color;
  return style;
}

static NSTextAlignment FLMAlignment(int align)
{
  switch (align) {
    case 1:
      return NSTextAlignmentLeft;
    case 2:
      return NSTextAlignmentRight;
    case 3:
      return NSTextAlignmentCenter;
    case 4:
      return NSTextAlignmentJustified;
    default:
      return NSTextAlignmentNatural;
  }
}

NSAttributedString *FLMAttributedString(std::string_view text, const FLMTextStyle &style)
{
  NSString *string = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding] ?: @"";
  const CGFloat multiplier = style.scaling ? RCTFontSizeMultiplier() : 1;
  UIFont *font = [RCTFont updateFont:nil
                          withFamily:style.family.empty() ? nil : @(style.family.c_str())
                                size:@(style.size)
                              weight:[NSString stringWithFormat:@"%d", style.weight]
                               style:style.italic ? @"italic" : @"normal"
                             variant:nil
                     scaleMultiplier:multiplier];
  NSMutableParagraphStyle *paragraph = [NSMutableParagraphStyle new];
  paragraph.alignment = FLMAlignment(style.align);
  NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary dictionary];
  attributes[NSFontAttributeName] = font;
  attributes[NSForegroundColorAttributeName] = FLMColor(style.color);
  if (style.lineHeight > 0) {
    const CGFloat lineHeight = style.lineHeight * multiplier;
    paragraph.minimumLineHeight = lineHeight;
    paragraph.maximumLineHeight = lineHeight;
    // React Native centers the glyphs in a line taller than the font's (RCTApplyBaselineOffsetForRange).
    if (lineHeight >= font.lineHeight) {
      attributes[NSBaselineOffsetAttributeName] = @((lineHeight - font.lineHeight) / 2);
    }
  }
  attributes[NSParagraphStyleAttributeName] = paragraph;
  if (style.letterSpacing != 0) {
    attributes[NSKernAttributeName] = @(style.letterSpacing);
  }
  if (style.decoration & 1) {
    attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
  }
  if (style.decoration & 2) {
    attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);
  }
  return [[NSAttributedString alloc] initWithString:string attributes:attributes];
}

NSTextStorage *FLMTextStorage(NSAttributedString *text, CGFloat maxWidth, int numberOfLines)
{
  NSTextContainer *container = [[NSTextContainer alloc] initWithSize:CGSizeMake(maxWidth, CGFLOAT_MAX)];
  container.lineFragmentPadding = 0;
  container.maximumNumberOfLines = numberOfLines > 0 ? numberOfLines : 0;
  container.lineBreakMode = numberOfLines > 0 ? NSLineBreakByTruncatingTail : NSLineBreakByClipping;
  NSLayoutManager *layoutManager = [NSLayoutManager new];
  layoutManager.usesFontLeading = NO;
  [layoutManager addTextContainer:container];
  NSTextStorage *storage = [[NSTextStorage alloc] initWithAttributedString:text];
  [storage addLayoutManager:layoutManager];
  return storage;
}

flume::TextMetrics FLMTextMeasurer::measure(const flume::TextMeasureRequest &request)
{
  FLMTextStyle style = FLMTextStyleOf(request);
  NSTextStorage *storage = FLMTextStorage(FLMAttributedString(request.text, style), request.maxWidth, request.numberOfLines);
  NSLayoutManager *layoutManager = storage.layoutManagers.firstObject;
  NSTextContainer *container = layoutManager.textContainers.firstObject;
  [layoutManager ensureLayoutForTextContainer:container];
  const CGSize size = [layoutManager usedRectForTextContainer:container].size;
  return {static_cast<float>(RCTCeilPixelValue(size.width)), static_cast<float>(RCTCeilPixelValue(size.height))};
}
