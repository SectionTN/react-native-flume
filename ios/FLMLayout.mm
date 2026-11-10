#import "FLMLayout.h"

#import "FLMLayoutAttributes.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace {

// A batch update's anchor: the offset before it and the rows in view with their edges, the anchored row first.
struct FLMAnchorUpdate {
  flume::ListOps ops;
  flume::AnchorOutcome outcome;
  CGFloat offset;
  std::vector<std::pair<int, float>> items;
};

}  // namespace

@implementation FLMLayout {
  flume::ListLayout _model;
  flume::ListLayoutConfig _config;
  std::vector<int> _sticky;
  CGSize _bounds;
  CGFloat _viewportLength;  // inside the adjusted insets, as of the last prepareLayout
  int _pinnedIndex;  // the header pinned by the last layout pass, -1 for none
  BOOL _preparing;  // inside prepareLayout, where measuring leaves the offset to the caller
  float _measureViewport;  // the measuring session's viewport length
  int _measureAnchor;
  float _measureMoved;
  BOOL _measuredAny;
  std::optional<FLMPendingOffset> _pendingOffset;
  std::optional<FLMAnchorUpdate> _update;
  std::vector<std::pair<NSInteger, CGFloat>> _lateLengths;  // slot lengths that arrived during a batch
  CGFloat _appearShift;  // the anchored offset minus the one UIKit proposed, during an update
}

- (instancetype)init
{
  if (self = [super init]) {
    _pinnedIndex = -1;
  }
  return self;
}

// A new column count or axis keeps the row in view where it was.
- (void)configure:(const flume::ListLayoutConfig &)config fullSpan:(std::vector<int>)fullSpan sticky:(std::vector<int>)sticky
{
  const bool reshaped = config.lanes != _config.lanes || config.horizontal != _config.horizontal;
  const std::optional<FLMPendingOffset> kept = reshaped ? [self anchorOffset] : std::nullopt;
  _config = config;
  _sticky = std::move(sticky);
  _model.configure(config, std::move(fullSpan));
  if (kept) {
    [self scheduleOffset:*kept];
  } else {
    [self invalidateLayout];
  }
}

// New rows keep the position in view, as RecyclerView keeps its anchor across notifyDataSetChanged.
- (void)resetWithCount:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf
{
  // A position still waiting for layout wins: the offset it would anchor on is the one before it lands.
  const std::optional<FLMPendingOffset> kept = _pendingOffset ? _pendingOffset : [self anchorOffset];
  _model.reset(static_cast<int>(count), typeOf);
  if (kept && count > 0) {
    [self scheduleOffset:*kept];
  } else {
    [self invalidateLayout];
  }
}

// The anchored row and its edge's distance from the viewport start; none without rows or a viewport.
- (std::optional<FLMPendingOffset>)anchorOffset
{
  const FLMViewport viewport = [self viewportFor:self.collectionView.bounds];
  const float start = static_cast<float>(viewport.start);
  const float length = static_cast<float>(viewport.length);
  const int anchor = length > 0 ? _model.anchorAt(start, length) : -1;
  if (anchor < 0) {
    return std::nullopt;
  }
  FLMPendingOffset pending{FLMPendingOffset::Kind::Anchor, anchor};
  pending.anchorDelta = _model.anchorEdgeOf(anchor, length) - start;
  return pending;
}

- (std::vector<int>)applyOps:(const flume::ListOps &)ops count:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf
{
  // Immediate data lands before its props, so until they catch up the sticky indices follow their rows.
  if (!ops.deferred) {
    _sticky = flume::mapIndices(ops, _sticky);
  }
  return _model.apply(ops, static_cast<int>(count), typeOf);
}

- (void)invalidateLengths
{
  _model.invalidateLengths();
  [self invalidateLayout];
}

- (CGFloat)bindCrossLengthForItem:(NSInteger)index
{
  return _model.crossLength(static_cast<int>(index), [self crossLength:self.collectionView.bounds.size]);
}

- (CGFloat)gridCellWidth
{
  const CGFloat width = self.collectionView.bounds.size.width;
  return _config.lanes > 1 && !_config.horizontal && width > 0 ? _model.laneLength(static_cast<float>(width)) : -1;
}

#pragma mark - Geometry

- (CGFloat)mainLength:(CGSize)size
{
  return _config.horizontal ? size.width : size.height;
}

- (CGFloat)crossLength:(CGSize)size
{
  return _config.horizontal ? size.height : size.width;
}

- (FLMViewport)viewportFor:(CGRect)bounds
{
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  const CGFloat start = _config.horizontal ? insets.left : insets.top;
  const CGFloat end = _config.horizontal ? insets.right : insets.bottom;
  const CGFloat origin = _config.horizontal ? bounds.origin.x : bounds.origin.y;
  return FLMViewport{origin + start, std::max<CGFloat>(0, [self mainLength:bounds.size] - start - end)};
}

- (std::vector<int>)itemsFrom:(CGFloat)start to:(CGFloat)end
{
  return _model.itemsIn(static_cast<float>(start), static_cast<float>(end), _viewportLength);
}

- (int)pinnedIndexAt:(CGFloat)viewportStart
{
  const auto pinned = _model.pinned(_sticky, static_cast<float>(viewportStart), _viewportLength);
  return pinned ? pinned->index : -1;
}

- (CGFloat)emptyMinLength
{
  return _model.emptyMinLength(static_cast<float>([self viewportFor:self.collectionView.bounds].length));
}

- (std::vector<flume::ItemSpan>)spansIn:(FLMViewport)viewport
{
  std::vector<flume::ItemSpan> spans;
  const float start = static_cast<float>(viewport.start);
  for (int index : _model.itemsIn(start, start + static_cast<float>(viewport.length), _viewportLength)) {
    const CGRect frame = [self frameOf:index];
    const CGFloat from = _config.horizontal ? CGRectGetMinX(frame) : CGRectGetMinY(frame);
    const CGFloat to = _config.horizontal ? CGRectGetMaxX(frame) : CGRectGetMaxY(frame);
    spans.push_back(flume::ItemSpan{index, static_cast<float>(from) - start, static_cast<float>(to) - start});
  }
  return spans;
}

- (CGRect)frameOf:(int)index
{
  flume::ItemRect rect = _model.rectOf(index, [self crossLength:_bounds], _viewportLength);
  return CGRectMake(rect.x, rect.y, rect.width, rect.height);
}

- (UICollectionViewLayoutAttributes *)attributesOf:(int)index
{
  FLMLayoutAttributes *attributes =
      [FLMLayoutAttributes layoutAttributesForCellWithIndexPath:[NSIndexPath indexPathForItem:index inSection:0]];
  attributes.frame = [self frameOf:index];
  auto separator = _model.separatorRect(index, [self crossLength:_bounds]);
  if (separator) {
    attributes.separatorFrame = CGRectMake(separator->x, separator->y, separator->width, separator->height);
    attributes.separatorColor = _separatorColor;
  }
  return attributes;
}

- (void)setSeparatorColor:(UIColor *)separatorColor
{
  if (separatorColor == _separatorColor || [separatorColor isEqual:_separatorColor]) {
    return;
  }
  _separatorColor = separatorColor;
  [self invalidateLayout];
}

- (std::optional<flume::Pinned>)pinned
{
  if (_sticky.empty()) {
    return std::nullopt;
  }
  return _model.pinned(_sticky, [self viewportFor:self.collectionView.bounds].start, _viewportLength);
}

- (void)pin:(UICollectionViewLayoutAttributes *)attributes at:(CGFloat)start
{
  CGRect frame = attributes.frame;
  if (_config.horizontal) {
    frame.origin.x = start;
  } else {
    frame.origin.y = start;
  }
  attributes.frame = frame;
  attributes.zIndex = 1;
}

#pragma mark - Measuring

// A measuring session keeps what is on screen in place: the anchor's summed move shifts the offset once at its end.
- (void)beginMeasuring
{
  const FLMViewport viewport = [self viewportFor:self.collectionView.bounds];
  _measureViewport = static_cast<float>(viewport.length);
  _measureAnchor = _model.anchorAt(static_cast<float>(viewport.start), _measureViewport);
  _measureMoved = 0;
  _measuredAny = NO;
}

- (void)measureRows:(flume::IndexRange)rows
{
  [self measureRows:rows through:nullptr];
}

// Before a batch the store already holds the data after its ops, so each item measures the data it will show.
- (void)measureRows:(flume::IndexRange)rows through:(const flume::ListOps *)ops
{
  if (self.measureItem == nil) {
    return;
  }
  for (int row = rows.first; row <= rows.last; row++) {
    const flume::IndexRange items = _model.itemsOfRows(row, row);
    for (int index = items.first; index <= items.last; index++) {
      if (_model.measured(index)) {
        continue;
      }
      const std::optional<int> data = ops ? flume::mapIndex(*ops, index) : std::optional<int>(index);
      if (!data) {
        continue;
      }
      const CGFloat length = self.measureItem(*data);
      if (length < 0) {
        continue;
      }
      _measureMoved += _model.setLength(index, static_cast<float>(length), _measureAnchor, _measureViewport).anchorMoved;
      _measuredAny = YES;
    }
  }
}

- (void)endMeasuring
{
  if (!_measuredAny || _preparing) {
    return;
  }
  if (_measureMoved != 0) {
    CGPoint offset = self.collectionView.contentOffset;
    if (_config.horizontal) {
      offset.x += _measureMoved;
    } else {
      offset.y += _measureMoved;
    }
    self.collectionView.contentOffset = offset;
  }
  [self invalidateLayout];
}

// The walk counts each row with the separator after it.
- (flume::IndexRange)measureLandingOfRow:(int)row viewPosition:(CGFloat)viewPosition
{
  const float separator = _config.separator;
  const flume::IndexRange rows =
      flume::landingRows(row, static_cast<float>(viewPosition), _measureViewport, _model.rowCount(), [&](int at) {
        [self measureRows:flume::IndexRange{at, at}];
        return _model.rowLength(at) + separator;
      });
  [self measureRows:rows];
  return rows;
}

// UIScrollView's range on the scroll axis, with the adjusted insets.
- (CGFloat)clampedOffset:(float)offset
{
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  const CGSize size = self.collectionView.bounds.size;
  const float content = _model.contentLength(static_cast<float>([self viewportFor:self.collectionView.bounds].length));
  return flume::clampOffset(offset, content, static_cast<float>([self mainLength:size]),
                            static_cast<float>(_config.horizontal ? insets.left : insets.top),
                            static_cast<float>(_config.horizontal ? insets.right : insets.bottom));
}

- (CGFloat)targetOffsetForIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition viewOffset:(CGFloat)viewOffset
{
  const std::optional<int> target = flume::clampIndex(static_cast<int>(index), _model.count());
  if (!target) {
    return NAN;
  }
  [self beginMeasuring];
  [self measureLandingOfRow:_model.rowOf(*target) viewPosition:viewPosition];
  [self endMeasuring];
  const flume::ItemRect rect = _model.rectOf(*target, [self crossLength:self.collectionView.bounds.size], _measureViewport);
  const float viewportStart = flume::alignedViewportStart(
      _config.horizontal ? rect.x : rect.y, _config.horizontal ? rect.width : rect.height, _measureViewport,
      static_cast<float>(viewPosition), static_cast<float>(viewOffset), _config.inverted);
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  return [self clampedOffset:viewportStart - static_cast<float>(_config.horizontal ? insets.left : insets.top)];
}

- (CGFloat)offsetForVisualEdge:(flume::VisualEdge)edge
{
  const bool top = edge == flume::VisualEdge::Top;
  if (_model.count() > 0) {
    // Upright, the top shows item 0 at the content start; inverted, the last item at the content end.
    const bool contentStart = top != _config.inverted;
    [self beginMeasuring];
    [self measureLandingOfRow:contentStart ? 0 : _model.rowCount() - 1 viewPosition:contentStart ? 0 : 1];
    [self endMeasuring];
  }
  return [self clampedOffset:top ? -INFINITY : INFINITY];
}

- (void)measureGlideToIndex:(NSInteger)index viewPosition:(CGFloat)viewPosition
{
  const std::optional<int> target = flume::clampIndex(static_cast<int>(index), _model.count());
  if (!target) {
    return;
  }
  [self beginMeasuring];
  const FLMViewport viewport = [self viewportFor:self.collectionView.bounds];
  const float start = static_cast<float>(viewport.start);
  const std::vector<int> visible = _model.itemsIn(start, start + _measureViewport, _measureViewport);
  const flume::IndexRange landing = [self measureLandingOfRow:_model.rowOf(*target) viewPosition:viewPosition];
  if (!visible.empty()) {
    const auto [low, high] = std::minmax_element(visible.begin(), visible.end());
    [self measureRows:flume::glideWindow({_model.rowOf(*low), _model.rowOf(*high)}, landing)];
  }
  [self endMeasuring];
}

#pragma mark - Anchoring

// A row that self-sizes inside performBatchUpdates loses its offset adjustment, so the rows a batch can show measure first.
- (void)measureBatchRowsThrough:(const flume::ListOps &)ops
{
  [self beginMeasuring];
  const FLMViewport viewport = [self viewportFor:self.collectionView.bounds];
  const float start = static_cast<float>(viewport.start);
  const std::vector<int> visible = _model.itemsIn(start, start + _measureViewport, _measureViewport);
  if (!visible.empty()) {
    const auto [low, high] = std::minmax_element(visible.begin(), visible.end());
    const flume::IndexRange rows{_model.rowOf(*low), _model.rowOf(*high)};
    [self measureRows:rows through:&ops];
    const float separator = _config.separator;
    flume::batchRows(rows, _measureViewport, _model.rowCount(), [&](int row) {
      [self measureRows:flume::IndexRange{row, row} through:&ops];
      return _model.rowLength(row) + separator;
    });
  }
  [self endMeasuring];
}

- (flume::AnchorDecision)prepareForOps:(const flume::ListOps &)ops count:(NSInteger)count edges:(flume::EdgeInserts)edges
{
  [self measureBatchRowsThrough:ops];
  const FLMViewport viewport = [self viewportFor:self.collectionView.bounds];
  const float start = static_cast<float>(viewport.start);
  const float length = static_cast<float>(viewport.length);
  const CGPoint offset = self.collectionView.contentOffset;
  FLMAnchorUpdate update{ops, flume::AnchorOutcome::KeepAnchor, _config.horizontal ? offset.x : offset.y, {}};
  const int anchor = _model.anchorAt(start, length);
  if (anchor >= 0) {
    std::vector<int> visible = _model.itemsIn(start, start + length, length);
    // From the anchored edge: item order for upright and inverted lists, reversed for one stacked from its end.
    std::sort(visible.begin(), visible.end());
    if (_config.stackFromEnd) {
      std::reverse(visible.begin(), visible.end());
    }
    update.items.emplace_back(anchor, _model.anchorEdgeOf(anchor, length));
    for (int index : visible) {
      if (index != anchor) {
        update.items.emplace_back(index, _model.anchorEdgeOf(index, length));
      }
    }
  }
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  const float low = -static_cast<float>(_config.horizontal ? insets.left : insets.top);
  const float high = [self clampedOffset:INFINITY];
  const float current = static_cast<float>(update.offset);
  const flume::AnchorState state{std::max(0.f, current - low), std::max(0.f, high - current), length,
                                 _pendingOffset.has_value()};
  // A kept position follows its row through the ops.
  if (_pendingOffset && _pendingOffset->kind == FLMPendingOffset::Kind::Anchor) {
    _pendingOffset->index = flume::mapIndex(ops, _pendingOffset->index).value_or(_pendingOffset->index);
  }
  const flume::AnchorDecision decision = flume::decideAnchor(_anchorConfig, state, edges.visualTop, edges.visualBottom);
  update.outcome = decision.outcome;
  _update = std::move(update);
  _appearShift = 0;
  return decision;
}

#pragma mark - UICollectionViewLayout

+ (Class)layoutAttributesClass
{
  return FLMLayoutAttributes.class;
}

- (void)scheduleOffset:(const FLMPendingOffset &)pending
{
  _pendingOffset = pending;
  [self invalidateLayout];
}

- (BOOL)hasPendingOffset
{
  return _pendingOffset.has_value();
}

- (void)prepareLayout
{
  [super prepareLayout];
  _preparing = YES;
  _bounds = self.collectionView.bounds.size;
  _viewportLength = [self viewportFor:self.collectionView.bounds].length;
  if (_pendingOffset && _model.count() > 0 && _viewportLength > 0) {
    const FLMPendingOffset pending = *_pendingOffset;
    _pendingOffset.reset();
    CGFloat offset = 0;
    switch (pending.kind) {
      case FLMPendingOffset::Kind::Landing:
        offset = [self targetOffsetForIndex:pending.index viewPosition:pending.viewPosition viewOffset:0];
        break;
      case FLMPendingOffset::Kind::Edge:
        offset = [self offsetForVisualEdge:pending.edge];
        break;
      case FLMPendingOffset::Kind::Anchor: {
        const int index = std::min(pending.index, _model.count() - 1);
        const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
        const float edge = _model.anchorEdgeOf(index, static_cast<float>(_viewportLength));
        offset = [self clampedOffset:edge - pending.anchorDelta -
                                     static_cast<float>(_config.horizontal ? insets.left : insets.top)];
        break;
      }
    }
    CGPoint point = self.collectionView.contentOffset;
    if (_config.horizontal) {
      point.x = offset;
    } else {
      point.y = offset;
    }
    self.collectionView.contentOffset = point;
  }
  _preparing = NO;
}

- (CGSize)collectionViewContentSize
{
  CGFloat content = _model.contentLength(_viewportLength);
  return _config.horizontal ? CGSizeMake(content, _bounds.height) : CGSizeMake(_bounds.width, content);
}

- (NSArray<UICollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(CGRect)rect
{
  const float start = _config.horizontal ? CGRectGetMinX(rect) : CGRectGetMinY(rect);
  const float end = _config.horizontal ? CGRectGetMaxX(rect) : CGRectGetMaxY(rect);
  std::vector<int> items = _model.itemsIn(start, end, _viewportLength);
  auto pinned = [self pinned];
  NSMutableArray<UICollectionViewLayoutAttributes *> *out = [NSMutableArray arrayWithCapacity:items.size() + 1];
  bool pinnedListed = false;
  for (int index : items) {
    UICollectionViewLayoutAttributes *attributes = [self attributesOf:index];
    if (pinned && pinned->index == index) {
      [self pin:attributes at:pinned->start];
      pinnedListed = true;
    }
    [out addObject:attributes];
  }
  if (pinned && !pinnedListed) {
    UICollectionViewLayoutAttributes *attributes = [self attributesOf:pinned->index];
    [self pin:attributes at:pinned->start];
    [out addObject:attributes];
  }
  _pinnedIndex = pinned ? pinned->index : -1;
  return out;
}

- (UICollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath
{
  const int index = static_cast<int>(indexPath.item);
  if (index < 0 || index >= _model.count()) {
    return nil;
  }
  UICollectionViewLayoutAttributes *attributes = [self attributesOf:index];
  auto pinned = [self pinned];
  if (pinned && pinned->index == index) {
    [self pin:attributes at:pinned->start];
  }
  return attributes;
}

// Any new size lays out again: an inverted list shorter than its viewport sits at the viewport's end.
// How far a list anchored at its end moves to keep that end in view, inside the scroll range at these bounds.
- (CGFloat)endKeepingShiftFrom:(CGFloat)oldEnd to:(CGFloat)newEnd bounds:(CGRect)bounds
{
  if (!_model.bottomAnchored() || _pendingOffset || _model.count() == 0) {
    return 0;
  }
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  const float start = static_cast<float>(_config.horizontal ? insets.left : insets.top);
  const float end = static_cast<float>(_config.horizontal ? insets.right : insets.bottom);
  const float length = static_cast<float>([self mainLength:bounds.size]);
  const float content = _model.contentLength(static_cast<float>([self viewportFor:bounds].length));
  const float offset = static_cast<float>(_config.horizontal ? bounds.origin.x : bounds.origin.y);
  const float kept = flume::offsetAfterViewportChange(true, offset, static_cast<float>(oldEnd), static_cast<float>(newEnd),
                                                      flume::clampOffset(-INFINITY, content, length, start, end),
                                                      flume::clampOffset(INFINITY, content, length, start, end));
  return kept - offset;
}

// UIKit may already have clamped the offset into a shrunk range, which keeps the end, so this restores the end itself.
- (void)keepVisibleEnd:(CGFloat)visibleEnd
{
  if (std::isnan(visibleEnd)) {
    return;
  }
  const CGRect bounds = self.collectionView.bounds;
  const UIEdgeInsets insets = self.collectionView.adjustedContentInset;
  const CGFloat offset = _config.horizontal ? bounds.origin.x : bounds.origin.y;
  const CGFloat end = [self mainLength:bounds.size] - (_config.horizontal ? insets.right : insets.bottom);
  const CGFloat shift = [self endKeepingShiftFrom:visibleEnd - offset to:end bounds:bounds];
  if (shift == 0) {
    return;
  }
  CGPoint point = self.collectionView.contentOffset;
  if (_config.horizontal) {
    point.x += shift;
  } else {
    point.y += shift;
  }
  self.collectionView.contentOffset = point;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds
{
  return !CGSizeEqualToSize(newBounds.size, _bounds) || !_sticky.empty();
}

// A scroll with sticky headers moves only the pinned header and the one taking its place.
- (UICollectionViewLayoutInvalidationContext *)invalidationContextForBoundsChange:(CGRect)newBounds
{
  UICollectionViewLayoutInvalidationContext *context = [super invalidationContextForBoundsChange:newBounds];
  if (!CGSizeEqualToSize(newBounds.size, _bounds)) {
    // A list anchored at its end keeps the end in view, as a reversed RecyclerView lays out from its end.
    const CGFloat old = [self mainLength:_bounds];
    if (old > 0) {
      const CGFloat shift = [self endKeepingShiftFrom:old to:[self mainLength:newBounds.size] bounds:newBounds];
      context.contentOffsetAdjustment = _config.horizontal ? CGPointMake(shift, 0) : CGPointMake(0, shift);
    }
    return context;
  }
  NSMutableArray<NSIndexPath *> *paths = [NSMutableArray array];
  if (_pinnedIndex >= 0 && _pinnedIndex < _model.count()) {
    [paths addObject:[NSIndexPath indexPathForItem:_pinnedIndex inSection:0]];
  }
  const FLMViewport viewport = [self viewportFor:newBounds];
  auto after = _model.pinned(_sticky, viewport.start, viewport.length);
  if (after && after->index != _pinnedIndex) {
    [paths addObject:[NSIndexPath indexPathForItem:after->index inSection:0]];
  }
  if (paths.count > 0) {
    [context invalidateItemsAtIndexPaths:paths];
  }
  return context;
}

- (BOOL)shouldInvalidateLayoutForPreferredLayoutAttributes:(UICollectionViewLayoutAttributes *)preferred
                                    withOriginalAttributes:(UICollectionViewLayoutAttributes *)original
{
  const int index = static_cast<int>(original.indexPath.item);
  if (index < 0 || index >= _model.count()) {
    return NO;
  }
  const CGFloat scale = self.collectionView.traitCollection.displayScale;
  const CGFloat pixel = scale > 0 ? 1 / scale : 1;
  const CGFloat length = [self mainLength:preferred.size];
  return std::fabs(length - [self mainLength:[self frameOf:index].size]) > pixel;
}

- (UICollectionViewLayoutInvalidationContext *)
    invalidationContextForPreferredLayoutAttributes:(UICollectionViewLayoutAttributes *)preferred
                             withOriginalAttributes:(UICollectionViewLayoutAttributes *)original
{
  UICollectionViewLayoutInvalidationContext *context =
      [super invalidationContextForPreferredLayoutAttributes:preferred withOriginalAttributes:original];
  const int index = static_cast<int>(original.indexPath.item);
  const int anchor = _model.anchorAt([self viewportFor:self.collectionView.bounds].start, _viewportLength);
  const flume::LengthChange change =
      _model.setLength(index, [self mainLength:preferred.size], anchor, _viewportLength);
  const CGFloat moved = change.anchorMoved;
  const CGFloat grown = change.contentMoved;
  context.contentOffsetAdjustment = _config.horizontal ? CGPointMake(moved, 0) : CGPointMake(0, moved);
  context.contentSizeAdjustment = _config.horizontal ? CGSizeMake(grown, 0) : CGSizeMake(0, grown);
  return context;
}

// A batch keeps the first recorded row that survives it where it was.
- (CGPoint)targetContentOffsetForProposedContentOffset:(CGPoint)proposed
{
  if (!_updating || !_update || _update->outcome == flume::AnchorOutcome::KeepOffset) {
    return proposed;
  }
  const float length = static_cast<float>([self viewportFor:self.collectionView.bounds].length);
  for (const auto &[index, edge] : _update->items) {
    const std::optional<int> now = flume::mapIndex(_update->ops, index);
    if (!now || *now >= _model.count()) {
      continue;
    }
    const CGFloat target =
        [self clampedOffset:static_cast<float>(_update->offset) + _model.anchorEdgeOf(*now, length) - edge];
    _appearShift = target - (_config.horizontal ? proposed.x : proposed.y);
    return _config.horizontal ? CGPointMake(target, proposed.y) : CGPointMake(proposed.x, target);
  }
  return proposed;
}

// Rows that come into view because the offset moved start where the old offset showed them.
- (UICollectionViewLayoutAttributes *)initialLayoutAttributesForAppearingItemAtIndexPath:(NSIndexPath *)indexPath
{
  if (_appearShift == 0) {
    return nil;
  }
  UICollectionViewLayoutAttributes *attributes = [[self layoutAttributesForItemAtIndexPath:indexPath] copy];
  CGRect frame = attributes.frame;
  if (_config.horizontal) {
    frame.origin.x -= _appearShift;
  } else {
    frame.origin.y -= _appearShift;
  }
  attributes.frame = frame;
  return attributes;
}

- (void)prepareForCollectionViewUpdates:(NSArray<UICollectionViewUpdateItem *> *)updateItems
{
  [super prepareForCollectionViewUpdates:updateItems];
  _updating = YES;
}

- (void)finalizeCollectionViewUpdates
{
  [super finalizeCollectionViewUpdates];
  _updating = NO;
  _update.reset();
  _appearShift = 0;
  const std::vector<std::pair<NSInteger, CGFloat>> late = std::move(_lateLengths);
  _lateLengths.clear();
  for (const auto &[index, length] : late) {
    [self setLength:length forItem:index];
  }
}

#pragma mark - React rows

// UIKit drops an offset adjustment inside performBatchUpdates, so a length that arrives then waits for its end.
- (void)setLength:(CGFloat)length forItem:(NSInteger)index
{
  const int item = static_cast<int>(index);
  if (item < 0 || item >= _model.count() || length < 0) {
    return;
  }
  if (_updating) {
    NSLog(@"FLMSlot late size deferred item=%d", item);
    _lateLengths.emplace_back(index, length);
    return;
  }
  const CGFloat scale = self.collectionView.traitCollection.displayScale;
  const CGFloat pixel = scale > 0 ? 1 / scale : 1;
  if (_model.measured(item) && std::fabs(length - [self mainLength:[self frameOf:item].size]) <= pixel) {
    return;
  }
  const int anchor = _model.anchorAt([self viewportFor:self.collectionView.bounds].start, _viewportLength);
  const flume::LengthChange change = _model.setLength(item, static_cast<float>(length), anchor, _viewportLength);
  UICollectionViewLayoutInvalidationContext *context = [UICollectionViewLayoutInvalidationContext new];
  const CGFloat moved = change.anchorMoved;
  const CGFloat grown = change.contentMoved;
  context.contentOffsetAdjustment = _config.horizontal ? CGPointMake(moved, 0) : CGPointMake(0, moved);
  context.contentSizeAdjustment = _config.horizontal ? CGSizeMake(grown, 0) : CGSizeMake(0, grown);
  [self invalidateLayoutWithContext:context];
}

@end
