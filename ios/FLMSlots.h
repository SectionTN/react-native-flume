#import <UIKit/UIKit.h>

#import <react/renderer/components/FlumeSpec/EventEmitters.h>

#import "FLMSlotView.h"

#include "adapter/SlotPool.h"

NS_ASSUME_NONNULL_BEGIN

// The list's Fabric children in Fabric's order, the bay for the slot views no cell shows, and the slot pool.
@interface FLMSlots : NSObject

- (instancetype)initWithBay:(UIView *)bay;

- (flume::SlotPool &)pool;
@property (nonatomic, weak, nullable) id<FLMSlotViewHost> viewHost;  // every slot view reports its size changes here

- (void)insertChild:(UIView *)child atIndex:(NSInteger)index;  // Fabric's mount
- (void)removeChild:(UIView *)child atIndex:(NSInteger)index;  // Fabric's unmount, out of any cell first
- (nullable FLMSlotView *)viewForSlot:(int)slot;
- (BOOL)takeRegisteredPrerendered;  // YES once per transaction that registered a pre-rendered slot
// The whole table when it changed; Fabric replaces a queued unique event of the same type.
- (void)flushTo:(const facebook::react::FlumeViewEventEmitter &)emitter cellWidth:(CGFloat)cellWidth;

@end

NS_ASSUME_NONNULL_END
