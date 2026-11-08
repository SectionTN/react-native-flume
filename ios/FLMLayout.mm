#import "FLMLayout.h"

#include <cmath>

@implementation FLMLayout {
  flume::ListLayout _model;
  flume::ListLayoutConfig _config;
  std::vector<int> _sticky;
  CGSize _bounds;
  int _pinnedIndex;  // the header pinned by the last layout pass, -1 for none
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

- (CGFloat)viewportStart
{
  UICollectionView *view = self.collectionView;
  return _config.horizontal ? view.contentOffset.x + view.adjustedContentInset.left
                            : view.contentOffset.y + view.adjustedContentInset.top;
}

- (CGRect)frameOf:(int)index
{
  flume::ItemRect rect = _model.rectOf(index, [self crossLength:_bounds], [self mainLength:_bounds]);
  return CGRectMake(rect.x, rect.y, rect.width, rect.height);
}

- (UICollectionViewLayoutAttributes *)attributesOf:(int)index
{
  UICollectionViewLayoutAttributes *attributes =
      [UICollectionViewLayoutAttributes layoutAttributesForCellWithIndexPath:[NSIndexPath indexPathForItem:index inSection:0]];
  attributes.frame = [self frameOf:index];
  return attributes;
}

- (std::optional<flume::Pinned>)pinned
{
  if (_sticky.empty()) {
    return std::nullopt;
  }
  return _model.pinned(_sticky, [self viewportStart], [self mainLength:_bounds]);
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

#pragma mark - UICollectionViewLayout

- (void)prepareLayout
{
  [super prepareLayout];
  _bounds = self.collectionView.bounds.size;
}

- (CGSize)collectionViewContentSize
{
  CGFloat content = _model.contentLength([self mainLength:_bounds]);
  return _config.horizontal ? CGSizeMake(content, _bounds.height) : CGSizeMake(_bounds.width, content);
}

- (NSArray<UICollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(CGRect)rect
{
  const float start = _config.horizontal ? CGRectGetMinX(rect) : CGRectGetMinY(rect);
  const float end = _config.horizontal ? CGRectGetMaxX(rect) : CGRectGetMaxY(rect);
  std::vector<int> items = _model.itemsIn(start, end, [self mainLength:_bounds]);
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

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds
{
  return [self crossLength:newBounds.size] != [self crossLength:_bounds] || !_sticky.empty();
}

// A scroll with sticky headers moves only the pinned header and the one taking its place.
- (UICollectionViewLayoutInvalidationContext *)invalidationContextForBoundsChange:(CGRect)newBounds
{
  UICollectionViewLayoutInvalidationContext *context = [super invalidationContextForBoundsChange:newBounds];
  if ([self crossLength:newBounds.size] != [self crossLength:_bounds]) {
    return context;
  }
  NSMutableArray<NSIndexPath *> *paths = [NSMutableArray array];
  if (_pinnedIndex >= 0 && _pinnedIndex < _model.count()) {
    [paths addObject:[NSIndexPath indexPathForItem:_pinnedIndex inSection:0]];
  }
  const CGFloat start = _config.horizontal ? newBounds.origin.x : newBounds.origin.y;
  auto after = _model.pinned(_sticky, start, [self mainLength:newBounds.size]);
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
  const CGFloat viewport = [self mainLength:_bounds];
  const int anchor = _model.anchorAt([self viewportStart], viewport);
  const CGFloat before = [self mainLength:[self frameOf:index].size];
  const CGFloat length = [self mainLength:preferred.size];
  const CGFloat moved = _model.setLength(index, length, anchor, viewport);
  context.contentOffsetAdjustment = _config.horizontal ? CGPointMake(moved, 0) : CGPointMake(0, moved);
  context.contentSizeAdjustment = _config.horizontal ? CGSizeMake(length - before, 0) : CGSizeMake(0, length - before);
  return context;
}

@end
