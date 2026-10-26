#import <UIKit/UIKit.h>

#include <vector>

#include "adapter/ListLayout.h"

// The collection view layout over the shared ListLayout model: estimates, self-sizing with offset compensation, sticky headers.
@interface FLMLayout : UICollectionViewLayout

- (void)configure:(const flume::ListLayoutConfig &)config fullSpan:(std::vector<int>)fullSpan sticky:(std::vector<int>)sticky;
- (void)resetWithCount:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (void)applyOps:(const flume::ListOps &)ops count:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (void)invalidateLengths;
- (CGFloat)bindCrossLengthForItem:(NSInteger)index;

@end
