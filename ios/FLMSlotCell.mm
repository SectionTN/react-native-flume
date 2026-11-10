#import "FLMSlotCell.h"

#import "FLMSlotView.h"

@implementation FLMSlotCell

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    [self show:nil];
  }
  return self;
}

- (void)show:(nullable FLMSlotView *)view
{
  _view = view;
  if (view != nil && view.superview != self.contentView) {
    [self.contentView addSubview:view];
  }
  self.contentView.hidden = view == nil;
  self.contentView.userInteractionEnabled = view != nil;
}

- (void)prepareForReuse
{
  [super prepareForReuse];
  if (self.onPrepareForReuse != nil) {
    self.onPrepareForReuse(self);
  }
}

// The row's length comes from its slot through the layout, never from UIKit self-sizing.
- (UICollectionViewLayoutAttributes *)preferredLayoutAttributesFittingAttributes:
    (UICollectionViewLayoutAttributes *)layoutAttributes
{
  return layoutAttributes;
}

@end
