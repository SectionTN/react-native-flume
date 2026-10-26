#import <UIKit/UIKit.h>

// A worklet cell: binds through the engine into its own view tree and sizes itself from the bound root.
@interface FLMCell : UICollectionViewCell

@property (nonatomic) int holderId;
@property (nonatomic, copy) void (^onPress)(FLMCell *cell, NSString *pressId, BOOL longPress);

- (BOOL)bindStore:(int)storeId index:(NSInteger)index width:(CGFloat)width height:(CGFloat)height horizontal:(BOOL)horizontal;

@end
