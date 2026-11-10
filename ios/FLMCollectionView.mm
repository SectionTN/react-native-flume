#import "FLMCollectionView.h"

@implementation FLMCollectionView

- (void)layoutSubviews
{
  [super layoutSubviews];
  if (_didLayout != nil) {
    _didLayout();
  }
}

// The hit test runs before a touch stops a glide or a deceleration, so the list's motion still shows here.
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
  if (_touchDown != nil && event != nil && event.type == UIEventTypeTouches && [self pointInside:point withEvent:event]) {
    _touchDown();
  }
  return [super hitTest:point withEvent:event];
}

@end
