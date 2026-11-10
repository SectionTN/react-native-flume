#import "FLMTouchCancel.h"

#import <React/RCTSurfaceTouchHandler.h>

static UIGestureRecognizer *_Nullable FLMSurfaceTouchHandler(UIView *fromView)
{
  for (UIView *view = fromView; view != nil; view = view.superview) {
    for (UIGestureRecognizer *recognizer in view.gestureRecognizers) {
      if ([recognizer isKindOfClass:RCTSurfaceTouchHandler.class]) {
        return recognizer;
      }
    }
  }
  return nil;
}

void FLMCancelJSTouches(UIView *fromView)
{
  UIGestureRecognizer *handler = FLMSurfaceTouchHandler(fromView);
  // Disabling resets the handler, and its reset sends JS a touch cancel.
  if (handler.enabled) {
    handler.enabled = NO;
    handler.enabled = YES;
  }
}

void FLMWithholdJSTouch(UIView *fromView)
{
  UIGestureRecognizer *handler = FLMSurfaceTouchHandler(fromView);
  if (!handler.enabled) {
    return;
  }
  // UIKit hands a new touch only to the recognizers enabled after its hit test.
  handler.enabled = NO;
  dispatch_async(dispatch_get_main_queue(), ^{
    handler.enabled = YES;
  });
}
