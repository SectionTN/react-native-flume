#import "FLMBoxView.h"

// A pressable cell view: dims while touched, presses on a lift inside, long presses at 500 ms.
@interface FLMPressableView : FLMBoxView

@property (nonatomic, copy) NSString *pressId;
@property (nonatomic) CGFloat pressedOpacity;
@property (nonatomic, copy) void (^onPress)(NSString *pressId, BOOL longPress);

@end
