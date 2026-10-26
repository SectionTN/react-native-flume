#import "FLMPressableView.h"

#import "FLMTextView.h"

@implementation FLMPressableView {
  CGFloat _restingAlpha;
  BOOL _pressed;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    _pressedOpacity = 0.7;
    _restingAlpha = 1;
    UILongPressGestureRecognizer *longPress = [[UILongPressGestureRecognizer alloc] initWithTarget:self
                                                                                             action:@selector(longPressed:)];
    longPress.minimumPressDuration = 0.5;
    [self addGestureRecognizer:longPress];
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
  }
  return self;
}

// Without a label of its own a pressable reads as the texts inside it, as React Native's iOS views do.
- (NSString *)accessibilityLabel
{
  NSString *label = [super accessibilityLabel];
  if (label != nil) {
    return label;
  }
  NSMutableArray<NSString *> *texts = [NSMutableArray array];
  [self collectTexts:self into:texts];
  return texts.count > 0 ? [texts componentsJoinedByString:@" "] : nil;
}

- (void)collectTexts:(UIView *)view into:(NSMutableArray<NSString *> *)texts
{
  for (UIView *child in view.subviews) {
    if ([child isKindOfClass:FLMTextView.class]) {
      NSString *text = child.accessibilityLabel;
      if (text.length > 0) {
        [texts addObject:text];
      }
    } else {
      [self collectTexts:child into:texts];
    }
  }
}

// A view style sets the resting alpha; the press dims from it and restores it.
- (void)setAlpha:(CGFloat)alpha
{
  if (!_pressed) {
    _restingAlpha = alpha;
  }
  [super setAlpha:alpha];
}

- (void)dim:(BOOL)pressed
{
  if (pressed) {
    [super setAlpha:_restingAlpha * _pressedOpacity];
    _pressed = YES;
  } else {
    _pressed = NO;
    [super setAlpha:_restingAlpha];
  }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [self dim:YES];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [self dim:[self pointInside:[touches.anyObject locationInView:self] withEvent:event]];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  const BOOL inside = [self pointInside:[touches.anyObject locationInView:self] withEvent:event];
  [self dim:NO];
  if (inside && _onPress != nil) {
    _onPress(_pressId, NO);
  }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  [self dim:NO];
}

- (void)longPressed:(UILongPressGestureRecognizer *)recognizer
{
  if (recognizer.state == UIGestureRecognizerStateBegan) {
    [self dim:NO];
    if (_onPress != nil) {
      _onPress(_pressId, YES);
    }
  }
}

@end
