#import <React/RCTViewComponentView.h>

NS_ASSUME_NONNULL_BEGIN

@class FLMSlotView;

@protocol FLMSlotViewHost <NSObject>
- (void)slotViewDidResize:(FLMSlotView *)view;
@end

// The FlumeSlotView container of one React cell: never recycled, at zero inside the cell that shows it.
@interface FLMSlotView : RCTViewComponentView

@property (nonatomic, readonly) int slot;
@property (nonatomic, readonly) int kind;
@property (nonatomic, readonly, copy) NSString *itemKey;
@property (nonatomic, readonly) CGSize layoutSize;  // Fabric's size, CGSizeZero before the first layout
@property (nonatomic, weak, nullable) id<FLMSlotViewHost> host;

- (void)writeOrigin:(CGPoint)origin;  // the shadow state, deduped

@end

NS_ASSUME_NONNULL_END
