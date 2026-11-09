#import "FLMCell.h"

#import "FLMBindStats.h"
#import "FLMCellViews.h"
#import "FLMLayoutAttributes.h"

#include <memory>
#include <time.h>

#include "bindings/Engine.h"

@implementation FLMCell {
  std::unique_ptr<FLMCellViews> _views;
  std::unique_ptr<flume::CellTree> _tree;
  flume::OpBatch _batch;
  BOOL _bound;
  BOOL _horizontal;
  UIView *_separator;  // on the cell itself: the cell tree owns contentView's subviews
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    _views = std::make_unique<FLMCellViews>(self.contentView);
    __weak FLMCell *weakSelf = self;
    _views->onPress = ^(NSString *pressId, BOOL longPress) {
      FLMCell *cell = weakSelf;
      if (cell.onPress != nil) {
        cell.onPress(cell, pressId, longPress);
      }
    };
    _tree = std::make_unique<flume::CellTree>(*_views);
    _separator = [UIView new];
    _separator.userInteractionEnabled = NO;
    _separator.hidden = YES;
    [self addSubview:_separator];
  }
  return self;
}

- (void)applyLayoutAttributes:(UICollectionViewLayoutAttributes *)layoutAttributes
{
  [super applyLayoutAttributes:layoutAttributes];
  FLMLayoutAttributes *attributes =
      [layoutAttributes isKindOfClass:FLMLayoutAttributes.class] ? (FLMLayoutAttributes *)layoutAttributes : nil;
  const CGRect frame = attributes != nil ? attributes.separatorFrame : CGRectZero;
  _separator.hidden = CGRectIsEmpty(frame) || attributes.separatorColor == nil;
  _separator.frame = frame;
  _separator.backgroundColor = attributes.separatorColor;
}

- (BOOL)bindStore:(int)storeId index:(NSInteger)index width:(CGFloat)width height:(CGFloat)height horizontal:(BOOL)horizontal
{
  _horizontal = horizontal;
  uint64_t start = FLMBindStats.enabled ? clock_gettime_nsec_np(CLOCK_UPTIME_RAW) : 0;
  if (!flume::Engine::instance().bind(storeId, _holderId, static_cast<int>(index), width, height, _batch)) {
    return NO;
  }
  _tree->apply(_batch);
  if (_batch.timed) {
    [FLMBindStats record:static_cast<int64_t>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - start) stats:_batch.stats];
  }
  _bound = YES;
  return YES;
}

- (UICollectionViewLayoutAttributes *)preferredLayoutAttributesFittingAttributes:
    (UICollectionViewLayoutAttributes *)layoutAttributes
{
  if (!_bound) {
    return layoutAttributes;
  }
  UICollectionViewLayoutAttributes *attributes = [layoutAttributes copy];
  CGSize size = attributes.size;
  if (_horizontal) {
    size.width = _tree->rootWidth();
  } else {
    size.height = _tree->rootHeight();
  }
  attributes.size = size;
  return attributes;
}

@end
