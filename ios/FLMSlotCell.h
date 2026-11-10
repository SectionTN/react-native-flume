#import <UIKit/UIKit.h>

@class FLMSlotView;

NS_ASSUME_NONNULL_BEGIN

// The cell a React row binds: shows its slot's view, or nothing and takes no touch until the slot has mounted.
@interface FLMSlotCell : UICollectionViewCell

@property (nonatomic) int owner;  // holder id, from FLMView's counter
@property (nonatomic) int slot;   // 0 when it shows nothing
@property (nonatomic, readonly, nullable) FLMSlotView *view;
@property (nonatomic, copy, nullable) void (^onPrepareForReuse)(FLMSlotCell *cell);

- (void)show:(nullable FLMSlotView *)view;

@end

NS_ASSUME_NONNULL_END
