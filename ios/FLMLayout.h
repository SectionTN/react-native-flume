#import <UIKit/UIKit.h>

#include <vector>

#include "adapter/ContentAnchor.h"
#include "adapter/ListLayout.h"
#include "adapter/ScrollTargets.h"
#include "adapter/Viewability.h"

struct FLMViewport {
  CGFloat start;  // along the scroll axis, in content coordinates
  CGFloat length;
};

struct FLMPendingOffset {
  enum class Kind { Landing, Edge, Anchor } kind;
  int index = 0;           // Landing and Anchor
  float viewPosition = 0;  // Landing
  flume::VisualEdge edge = flume::VisualEdge::Top;  // Edge
  float anchorDelta = 0;  // Anchor: the anchor edge's distance from the viewport start
};

// The collection view layout over the shared ListLayout model: estimates, self-sizing with offset compensation, sticky headers.
@interface FLMLayout : UICollectionViewLayout

- (FLMViewport)viewportFor:(CGRect)bounds;  // the part of the bounds inside the adjusted content insets

@property (nonatomic, strong, nullable) UIColor *separatorColor;  // nil keeps the gap and draws nothing

- (std::vector<flume::ItemSpan>)spansIn:(FLMViewport)viewport;  // the items in it, from its start, unpinned
- (std::vector<int>)itemsFrom:(CGFloat)start to:(CGFloat)end;  // along the scroll axis, in content coordinates
- (CGFloat)emptyMinLength;  // the viewport the padding, the header and the footer leave
- (int)pinnedIndexAt:(CGFloat)viewportStart;  // the sticky row pinned with the viewport there, -1 for none
- (void)configure:(const flume::ListLayoutConfig &)config fullSpan:(std::vector<int>)fullSpan sticky:(std::vector<int>)sticky;
- (void)resetWithCount:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (std::vector<int>)applyOps:(const flume::ListOps &)ops count:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf;
- (void)invalidateLengths;
- (CGFloat)bindCrossLengthForItem:(NSInteger)index;
- (CGFloat)gridCellWidth;  // a column of a vertical grid, -1 for any other list

@property (nonatomic, copy, nullable) CGFloat (^measureItem)(NSInteger index);  // a worklet row's length, negative for a React row
// The clamped content offset that lands the item per viewPosition, after measuring the rows the landing shows.
- (CGFloat)targetOffsetForIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition viewOffset:(CGFloat)viewOffset;
// The content offset of a visual edge, after measuring the rows it shows; the content end includes the padding.
- (CGFloat)offsetForVisualEdge:(flume::VisualEdge)edge;
// Measures the rows between the viewport and the item's landing, capped at the rows nearest the landing.
- (void)measureGlideToIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition;
// The items a jump shows: its landing, and for a glide the rows from the viewport at glideStart, NAN for none.
- (flume::IndexRange)jumpItemsToIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition glideStart:(CGFloat)glideStart;
// Applied at the end of the next prepareLayout with rows and a size, so the rows at the wrong end never bind.
- (void)scheduleOffset:(const FLMPendingOffset &)pending;
// A list anchored at its end keeps the viewport's end where it was when the end inset moves, as a keyboard inset does.
- (void)keepVisibleEnd:(CGFloat)visibleEnd;
@property (nonatomic, readonly) BOOL hasPendingOffset;

@property (nonatomic) flume::AnchorConfig anchorConfig;
// Before performBatchUpdates, at list positions: records the rows in view and decides what the batch keeps.
- (flume::AnchorDecision)prepareForOps:(const flume::ListOps &)ops count:(NSInteger)count edges:(flume::EdgeInserts)edges;

// A React row's length from its slot: the anchor's move becomes the offset adjustment, or it waits for the batch.
- (void)setLength:(CGFloat)length forItem:(NSInteger)index;
@property (nonatomic, readonly) BOOL updating;  // between prepareForCollectionViewUpdates: and finalizeCollectionViewUpdates

@end
