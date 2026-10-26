#import "FLMView.h"

#import <react/renderer/components/FlumeSpec/ComponentDescriptors.h>
#import <react/renderer/components/FlumeSpec/Props.h>
#import <react/renderer/components/FlumeSpec/RCTComponentViewHelpers.h>

#import "FLMCell.h"
#import "FLMLayout.h"
#import "FLMStoreListener.h"

#include <cmath>
#include <memory>
#include <unordered_set>

#include "adapter/BatchUpdates.h"
#include "bindings/Engine.h"

using namespace facebook::react;

namespace {

// Every text measures empty until the text measurer lands.
class FLMEmptyMeasurer final : public flume::TextMeasurer {
 public:
  flume::TextMetrics measure(const flume::TextMeasureRequest &) override
  {
    return {0, 0};
  }
};

NSArray<NSIndexPath *> *FLMIndexPaths(const std::vector<int> &indices)
{
  NSMutableArray<NSIndexPath *> *paths = [NSMutableArray arrayWithCapacity:indices.size()];
  for (int index : indices) {
    [paths addObject:[NSIndexPath indexPathForItem:index inSection:0]];
  }
  return paths;
}

}  // namespace

@interface FLMView () <RCTFlumeViewViewProtocol, UICollectionViewDataSource, UICollectionViewDelegate>
@end

@implementation FLMView {
  UICollectionView *_collectionView;
  FLMLayout *_layout;
  UIView *_children;  // Fabric children, kept out of sight
  std::unique_ptr<FLMStoreListener> _listener;
  NSMutableSet<NSString *> *_registered;
  std::unordered_set<int> _holders;
  int _storeId;
  int _nextHolderId;
  NSInteger _count;
  BOOL _attached;
  BOOL _itemAnimations;
  BOOL _horizontal;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<FlumeViewComponentDescriptor>();
}

// A list owns a store and the cells bound to it, so it is never reused for another list.
+ (BOOL)shouldBeRecycled
{
  return NO;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const FlumeViewProps>();
    _props = defaultProps;
    _itemAnimations = YES;
    _registered = [NSMutableSet set];
    _layout = [FLMLayout new];
    [_layout configure:flume::ListLayoutConfig() fullSpan:std::vector<int>() sticky:std::vector<int>()];
    _collectionView = [[UICollectionView alloc] initWithFrame:self.bounds collectionViewLayout:_layout];
    _collectionView.backgroundColor = UIColor.clearColor;
    _collectionView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
    _collectionView.alwaysBounceVertical = YES;
    _collectionView.dataSource = self;
    _collectionView.delegate = self;
    self.contentView = _collectionView;
    _children = [UIView new];
    _children.hidden = YES;
    [self addSubview:_children];
  }
  return self;
}

- (void)dealloc
{
  [self detach];
}

- (void)updateProps:(const Props::Shared &)props oldProps:(const Props::Shared &)oldProps
{
  const auto &next = *std::static_pointer_cast<const FlumeViewProps>(props);
  if (next.storeId != _storeId) {
    [self detach];
    _storeId = next.storeId;
    [self attach];
  }
  _itemAnimations = next.itemAnimations;
  [super updateProps:props oldProps:oldProps];
}

- (void)didMoveToWindow
{
  [super didMoveToWindow];
  if (self.window != nil) {
    [self attach];
  } else {
    [self detach];
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_children insertSubview:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [childComponentView removeFromSuperview];
}

#pragma mark - The store

- (flume::ListLayout::TypeOf)typeOf
{
  const int storeId = _storeId;
  return [storeId](int index) { return flume::Engine::instance().viewType(storeId, index); };
}

// Reloads on every attach, so rows bound before a detach show the data and size they have now.
- (void)attach
{
  if (_attached || self.window == nil || _storeId == 0) {
    return;
  }
  _listener = std::make_unique<FLMStoreListener>(self);
  const float scale = self.traitCollection.displayScale > 0 ? self.traitCollection.displayScale : 1;
  _count = flume::Engine::instance().attach(_storeId, _listener.get(), std::make_unique<FLMEmptyMeasurer>(), scale);
  _attached = YES;
  [_layout resetWithCount:_count typeOf:[self typeOf]];
  [_collectionView reloadData];
}

- (void)detach
{
  if (!_attached) {
    return;
  }
  auto &engine = flume::Engine::instance();
  engine.detach(_storeId);
  for (int holder : _holders) {
    engine.releaseHolder(_storeId, holder);
  }
  _holders.clear();
  _listener.reset();
  _attached = NO;
}

- (void)applyListOps:(const flume::ListOps &)ops count:(int)count
{
  if (!_attached) {
    return;
  }
  flume::BatchUpdates batch = flume::toBatchUpdates(ops, static_cast<int>(_count));
  if (batch.reload) {
    _count = count;
    [_layout resetWithCount:count typeOf:[self typeOf]];
    [_collectionView reloadData];
    return;
  }
  void (^update)(void) = ^{
    [self->_collectionView performBatchUpdates:^{
      self->_count = count;
      [self->_layout applyOps:ops count:count typeOf:[self typeOf]];
      [self->_collectionView deleteItemsAtIndexPaths:FLMIndexPaths(batch.deletes)];
      [self->_collectionView insertItemsAtIndexPaths:FLMIndexPaths(batch.inserts)];
      for (const auto &[from, to] : batch.moves) {
        [self->_collectionView moveItemAtIndexPath:[NSIndexPath indexPathForItem:from inSection:0]
                                       toIndexPath:[NSIndexPath indexPathForItem:to inSection:0]];
      }
    }
                                    completion:nil];
    NSMutableArray<NSIndexPath *> *visible = [NSMutableArray array];
    NSSet<NSIndexPath *> *shown = [NSSet setWithArray:self->_collectionView.indexPathsForVisibleItems];
    for (NSIndexPath *path in FLMIndexPaths(batch.reconfigures)) {
      if ([shown containsObject:path]) {
        [visible addObject:path];
      }
    }
    if (visible.count > 0) {
      [self->_collectionView reconfigureItemsAtIndexPaths:visible];
    }
  };
  if (_itemAnimations) {
    update();
  } else {
    [UIView performWithoutAnimation:update];
  }
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
  return _attached ? _count : 0;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
  const int viewType = flume::Engine::instance().viewType(_storeId, static_cast<int>(indexPath.item));
  NSString *identifier = [NSString stringWithFormat:@"t%d", viewType];
  if (![_registered containsObject:identifier]) {
    [collectionView registerClass:FLMCell.class forCellWithReuseIdentifier:identifier];
    [_registered addObject:identifier];
  }
  FLMCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:identifier forIndexPath:indexPath];
  if (cell.holderId == 0) {
    cell.holderId = ++_nextHolderId;
  }
  // Odd view types are React rows, which get their slots in a later plan.
  if (viewType % 2 == 0) {
    const CGFloat cross = [_layout bindCrossLengthForItem:indexPath.item];
    if ([cell bindStore:_storeId
                  index:indexPath.item
                  width:_horizontal ? NAN : cross
                 height:_horizontal ? cross : NAN
             horizontal:_horizontal]) {
      _holders.insert(cell.holderId);
    }
  }
  return cell;
}

#pragma mark - Commands

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTFlumeViewHandleCommand(self, commandName, args);
}

// The scroll commands do nothing on iOS yet.
- (void)scrollToIndex:(NSInteger)index animated:(BOOL)animated viewPosition:(double)viewPosition viewOffset:(double)viewOffset
{
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
}

- (void)scrollToEnd:(BOOL)animated
{
}

@end
