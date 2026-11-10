#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// Ends the JS touches of the surface that holds fromView, the way gesture handler does when a native gesture takes over.
void FLMCancelJSTouches(UIView *fromView);

// Keeps the touch being hit-tested from the surface's JS: its touch handler is off until the next main-queue turn.
void FLMWithholdJSTouch(UIView *fromView);

NS_ASSUME_NONNULL_END
