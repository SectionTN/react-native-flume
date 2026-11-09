#import "FLMLayout.h"

#import "FLMLayoutAttributes.h"

#include <algorithm>
#include <cmath>

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
}

- (instancetype)init
{
  if (self = [super init]) {
    _pinnedIndex = -1;
  }
  return self;
}

- (void)configure:(const flume::ListLayoutConfig &)config fullSpan:(std::vector<int>)fullSpan sticky:(std::vector<int>)sticky
{
  _config = config;
  _sticky = std::move(sticky);
  _model.configure(config, std::move(fullSpan));
  [self invalidateLayout];
}

- (void)resetWithCount:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf
{
  _model.reset(static_cast<int>(count), typeOf);
  [self invalidateLayout];
}

- (std::vector<int>)applyOps:(const flume::ListOps &)ops count:(NSInteger)count typeOf:(flume::ListLayout::TypeOf)typeOf
{
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
  if (self.measureItem == nil) {
    return;
  }
  for (int row = rows.first; row <= rows.last; row++) {
    const flume::IndexRange items = _model.itemsOfRows(row, row);
    for (int index = items.first; index <= items.last; index++) {
      if (_model.measured(index)) {
        continue;
      }
      const CGFloat length = self.measureItem(index);
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

#pragma mark - UICollectionViewLayout

+ (Class)layoutAttributesClass
{
  return FLMLayoutAttributes.class;
}

- (void)prepareLayout
{
  [super prepareLayout];
  _preparing = YES;
  _bounds = self.collectionView.bounds.size;
  _viewportLength = [self viewportFor:self.collectionView.bounds].length;
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
- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds
{
  return !CGSizeEqualToSize(newBounds.size, _bounds) || !_sticky.empty();
}

// A scroll with sticky headers moves only the pinned header and the one taking its place.
- (UICollectionViewLayoutInvalidationContext *)invalidationContextForBoundsChange:(CGRect)newBounds
{
  UICollectionViewLayoutInvalidationContext *context = [super invalidationContextForBoundsChange:newBounds];
  if (!CGSizeEqualToSize(newBounds.size, _bounds)) {
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

@end
