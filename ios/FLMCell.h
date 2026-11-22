#import <UIKit/UIKit.h>

// A worklet cell: binds through the engine into its own view tree and sizes itself from the bound root.
@interface FLMCell : UICollectionViewCell

@property (nonatomic) int holderId;
// A grid cell: a root shorter than the cell is laid out again at the cell's length, as a FlatList row stretches it.
@property (nonatomic) BOOL stretches;
@property (nonatomic, copy) void (^onPress)(FLMCell *cell, NSString *pressId, BOOL longPress);

- (BOOL)bindStore:(int)storeId index:(NSInteger)index width:(CGFloat)width height:(CGFloat)height horizontal:(BOOL)horizontal;

@end
