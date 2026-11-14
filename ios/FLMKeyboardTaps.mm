#import "FLMKeyboardTaps.h"

// The keyboard's end frame in screen coordinates, kept for the whole app so a list mounted under the keyboard sees it.
static CGRect FLMKeyboardEnd = CGRectZero;

BOOL FLMIsTextInput(UIView *view, UIView *container)
{
  for (UIView *at = view; at != nil && at != container; at = at.superview) {
    if ([at isKindOfClass:UITextField.class] || [at isKindOfClass:UITextView.class]) {
      return YES;
    }
  }
  return NO;
}

@interface FLMKeyboardTaps () <UIGestureRecognizerDelegate>
@end

@implementation FLMKeyboardTaps {
  __weak UIScrollView *_scrollView;
  BOOL (^_pressableAt)(UITouch *);
  UITapGestureRecognizer *_swallow;
  UITapGestureRecognizer *_passThrough;
}

+ (void)load
{
  NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
  [center addObserverForName:UIKeyboardWillChangeFrameNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification *notification) {
                    FLMKeyboardEnd = [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
                  }];
  [center addObserverForName:UIKeyboardWillHideNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification *notification) {
                    FLMKeyboardEnd = CGRectZero;
                  }];
}

- (instancetype)initWithScrollView:(UIScrollView *)scrollView pressableAt:(BOOL (^)(UITouch *))pressableAt
{
  if (self = [super init]) {
    _scrollView = scrollView;
    _pressableAt = [pressableAt copy];
    _mode = flume::KeyboardTaps::Never;
    _swallow = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapped)];
    _swallow.cancelsTouchesInView = YES;
    _swallow.delaysTouchesBegan = YES;
    _swallow.delegate = self;
    _passThrough = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapped)];
    _passThrough.cancelsTouchesInView = NO;
    _passThrough.delaysTouchesBegan = NO;
    _passThrough.delegate = self;
    [scrollView addGestureRecognizer:_swallow];
    [scrollView addGestureRecognizer:_passThrough];
  }
  return self;
}

- (BOOL)keyboardUp
{
  UIWindow *window = _scrollView.window;
  UIScreen *screen = window.windowScene.screen;
  if (screen == nil || CGRectIsEmpty(FLMKeyboardEnd)) {
    return NO;
  }
  const CGRect frame = [window convertRect:FLMKeyboardEnd fromCoordinateSpace:screen.coordinateSpace];
  return CGRectIntersection(window.bounds, frame).size.height > 0;
}

- (void)tapped
{
  [_scrollView.window endEditing:YES];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer shouldReceiveTouch:(UITouch *)touch
{
  const flume::TapDecision decision =
      flume::decideTap(_mode, self.keyboardUp, _pressableAt(touch), FLMIsTextInput(touch.view, _scrollView));
  return recognizer == _swallow ? decision.swallow : decision.dismiss && !decision.swallow;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)other
{
  return recognizer == _passThrough;
}

@end
