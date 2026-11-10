#import <UIKit/UIKit.h>

#import <react/renderer/components/FlumeSpec/EventEmitters.h>

#import "FLMSlotCell.h"
#import "FLMSlotView.h"

#include "adapter/SlotPool.h"

NS_ASSUME_NONNULL_BEGIN

// The list's Fabric children in Fabric's order, the bay for the slot views no cell shows, and the slot pool.
@interface FLMSlots : NSObject

- (instancetype)initWithBay:(UIView *)bay;

- (flume::SlotPool &)pool;
@property (nonatomic, weak, nullable) id<FLMSlotViewHost> viewHost;  // every slot view reports its size changes here
@property (nonatomic, copy, nullable) void (^onRendered)(int slot);  // a slot mounted, parked or shown

- (void)insertChild:(UIView *)child atIndex:(NSInteger)index;  // Fabric's mount
- (void)removeChild:(UIView *)child atIndex:(NSInteger)index;  // Fabric's unmount, out of any cell first
- (nullable FLMSlotView *)viewForSlot:(int)slot;
- (BOOL)takeRegisteredPrerendered;  // YES once per transaction that registered a pre-rendered slot
// The whole table when it changed; Fabric replaces a queued unique event of the same type.
- (void)flushTo:(const facebook::react::FlumeViewEventEmitter &)emitter cellWidth:(CGFloat)cellWidth;

- (void)bindCell:(FLMSlotCell *)cell group:(flume::SlotGroup)group kind:(int)kind key:(NSString *)key index:(int)index;
- (void)releaseCell:(FLMSlotCell *)cell;  // idempotent, the view leaves only the cell that holds it
- (void)reindexCell:(FLMSlotCell *)cell index:(int)index;
// Every slot view back to the bay, its slot lent for the next binds of its key; the caller reclaims after them.
- (void)detachAll;
- (NSArray<FLMSlotCell *> *)cells;  // the cells bound to a slot

@end

NS_ASSUME_NONNULL_END
