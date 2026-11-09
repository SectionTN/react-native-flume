#import <UIKit/UIKit.h>

#include <vector>

#include "adapter/ListLayout.h"

struct FLMViewport {
  CGFloat start;  // along the scroll axis, in content coordinates
  CGFloat length;
};

// The collection view layout over the shared ListLayout model: estimates, self-sizing with offset compensation, sticky headers.
@interface FLMLayout : UICollectionViewLayout

- (FLMViewport)viewportFor:(CGRect)bounds;  // the part of the bounds inside the adjusted content insets
- (void)configure:(const flume::ListLayoutConfig &)config fullSpan:(std::vector<int>)fullSpan sticky:(std::vector<int>)sticky;
- (void)resetWithCount:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (std::vector<int>)applyOps:(const flume::ListOps &)ops count:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (void)invalidateLengths;
- (CGFloat)bindCrossLengthForItem:(NSInteger)index;

@end
