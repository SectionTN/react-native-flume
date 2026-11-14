#import <UIKit/UIKit.h>

#include "adapter/KeyboardTaps.h"

NS_ASSUME_NONNULL_BEGIN

// Whether a touch's view is a text input or inside one, up to the container. React Native's inputs subclass UIKit's.
BOOL FLMIsTextInput(UIView *_Nullable view, UIView *_Nullable container);

// keyboardShouldPersistTaps on a scroll view: a swallowing tap for never, a pass-through tap for handled.
@interface FLMKeyboardTaps : NSObject

- (instancetype)initWithScrollView:(UIScrollView *)scrollView pressableAt:(BOOL (^)(UITouch *touch))pressableAt;

@property (nonatomic) flume::KeyboardTaps mode;
@property (nonatomic, readonly) BOOL keyboardUp;

@end

NS_ASSUME_NONNULL_END
