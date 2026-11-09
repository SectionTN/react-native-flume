#import <UIKit/UIKit.h>

#include "adapter/KeyboardTaps.h"

NS_ASSUME_NONNULL_BEGIN

// keyboardShouldPersistTaps on a scroll view: a swallowing tap for never, a pass-through tap for handled.
@interface FLMKeyboardTaps : NSObject

- (instancetype)initWithScrollView:(UIScrollView *)scrollView pressableAt:(BOOL (^)(UITouch *touch))pressableAt;

@property (nonatomic) flume::KeyboardTaps mode;
@property (nonatomic, readonly) BOOL keyboardUp;

@end

NS_ASSUME_NONNULL_END
