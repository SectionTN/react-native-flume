#import "FLMView.h"

#import <react/renderer/components/FlumeSpec/ComponentDescriptors.h>
#import <react/renderer/components/FlumeSpec/Props.h>
#import <react/renderer/components/FlumeSpec/RCTComponentViewHelpers.h>
#import <React/RCTConversions.h>
#import <React/RCTUtils.h>

#import "FLMCell.h"
#import "FLMLayout.h"
#import "FLMPressableView.h"
#import "FLMStoreListener.h"
#import "FLMText.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_set>

#include "adapter/BatchUpdates.h"
#include "bindings/Engine.h"

using namespace facebook::react;

namespace {

NSArray<NSIndexPath *> *FLMIndexPaths(const std::vector<int> &indices)
{
  NSMutableArray<NSIndexPath *> *paths = [NSMutableArray arrayWithCapacity:indices.size()];
  for (int index : indices) {
    [paths addObject:[NSIndexPath indexPathForItem:index inSection:0]];
  }
  return paths;
}

// The two inset props are distinct codegen structs with the same fields.
template <typename Insets>
UIEdgeInsets FLMEdgeInsets(const Insets &insets)
{
  return UIEdgeInsetsMake(insets.top, insets.left, insets.bottom, insets.right);
}

UIScrollViewContentInsetAdjustmentBehavior FLMInsetAdjustment(FlumeViewContentInsetAdjustmentBehavior behavior)
{
  switch (behavior) {
    case FlumeViewContentInsetAdjustmentBehavior::Automatic:
      return UIScrollViewContentInsetAdjustmentAutomatic;
    case FlumeViewContentInsetAdjustmentBehavior::ScrollableAxes:
      return UIScrollViewContentInsetAdjustmentScrollableAxes;
    case FlumeViewContentInsetAdjustmentBehavior::Always:
      return UIScrollViewContentInsetAdjustmentAlways;
    case FlumeViewContentInsetAdjustmentBehavior::Never:
      return UIScrollViewContentInsetAdjustmentNever;
  }
  return UIScrollViewContentInsetAdjustmentNever;
}

UIScrollViewKeyboardDismissMode FLMKeyboardDismissMode(FlumeViewKeyboardDismissMode mode)
{
  switch (mode) {
    case FlumeViewKeyboardDismissMode::OnDrag:
      return UIScrollViewKeyboardDismissModeOnDrag;
    case FlumeViewKeyboardDismissMode::Interactive:
      return UIScrollViewKeyboardDismissModeInteractive;
    case FlumeViewKeyboardDismissMode::None:
      return UIScrollViewKeyboardDismissModeNone;
  }
  return UIScrollViewKeyboardDismissModeNone;
}

}  // namespace

@interface FLMView () <RCTFlumeViewViewProtocol,
                       UICollectionViewDataSource,
                       UICollectionViewDelegate,
                       UIGestureRecognizerDelegate>
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
  BOOL _rowsPressable;
  BOOL _inverted;
  BOOL _everAttached;
  BOOL _pinToEnd;  // an inverted list opens at its newest row once it has a size
  CGFloat _lastCross;
  CGFloat _fontScale;  // the font size multiplier the store's text was measured at
  UILongPressGestureRecognizer *_rowLongPress;
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
    _collectionView.allowsSelection = NO;
    _rowLongPress = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(rowLongPressed:)];
    _rowLongPress.minimumPressDuration = 0.5;
    _rowLongPress.delegate = self;
    _rowLongPress.enabled = NO;
    [_collectionView addGestureRecognizer:_rowLongPress];
    self.contentView = _collectionView;
    _children = [UIView new];
    _children.hidden = YES;
    [self addSubview:_children];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(contentSizeCategoryChanged:)
                                               name:UIContentSizeCategoryDidChangeNotification
                                             object:nil];
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
  const auto &old = static_cast<const FlumeViewProps &>(*_props);
  // Fabric can send a prop again with the value the view already has, so only a changed value acts (bug-089).
  const BOOL shape = next.horizontal != old.horizontal || next.numColumns != old.numColumns ||
      [self crossPadding:next] != [self crossPadding:old];
  const BOOL layout = shape || next.inverted != old.inverted || next.contentPaddingTop != old.contentPaddingTop ||
      next.contentPaddingLeft != old.contentPaddingLeft || next.contentPaddingBottom != old.contentPaddingBottom ||
      next.contentPaddingRight != old.contentPaddingRight || next.estimatedItemSize != old.estimatedItemSize ||
      next.stickyHeaderIndices != old.stickyHeaderIndices || next.separatorSize != old.separatorSize ||
      next.separatorInsetStart != old.separatorInsetStart || next.separatorInsetEnd != old.separatorInsetEnd;
  if (next.separatorColor != old.separatorColor) {
    _layout.separatorColor = RCTUIColorFromSharedColor(next.separatorColor);
  }
  if (layout) {
    [self configureLayout:next];
  }
  if (next.inverted && !old.inverted) {
    _pinToEnd = YES;
    [self setNeedsLayout];
  }
  if (next.storeId != _storeId) {
    [self detach];
    _storeId = next.storeId;
    _everAttached = NO;
    [self attach];
  } else if (shape) {
    [self rebind];
  }
  _itemAnimations = next.itemAnimations;
  _rowsPressable = next.rowsPressable;
  _collectionView.allowsSelection = _rowsPressable;
  _rowLongPress.enabled = _rowsPressable;
  [self updateScrollProps:next old:old];
  [super updateProps:props oldProps:oldProps];
}

// React Native's ScrollView props, straight onto the scroll view.
- (void)updateScrollProps:(const FlumeViewProps &)next old:(const FlumeViewProps &)old
{
  UICollectionView *view = _collectionView;
  if (next.scrollEnabled != old.scrollEnabled) {
    view.scrollEnabled = next.scrollEnabled;
  }
  if (next.decelerationRate != old.decelerationRate) {
    view.decelerationRate = static_cast<UIScrollViewDecelerationRate>(next.decelerationRate);
  }
  if (next.showsVerticalScrollIndicator != old.showsVerticalScrollIndicator) {
    view.showsVerticalScrollIndicator = next.showsVerticalScrollIndicator;
  }
  if (next.showsHorizontalScrollIndicator != old.showsHorizontalScrollIndicator) {
    view.showsHorizontalScrollIndicator = next.showsHorizontalScrollIndicator;
  }
  if (next.bounces != old.bounces) {
    view.bounces = next.bounces;
  }
  if (next.alwaysBounceVertical != old.alwaysBounceVertical) {
    view.alwaysBounceVertical = next.alwaysBounceVertical;
  }
  if (next.contentInsetAdjustmentBehavior != old.contentInsetAdjustmentBehavior) {
    view.contentInsetAdjustmentBehavior = FLMInsetAdjustment(next.contentInsetAdjustmentBehavior);
  }
  if (next.keyboardDismissMode != old.keyboardDismissMode) {
    view.keyboardDismissMode = FLMKeyboardDismissMode(next.keyboardDismissMode);
  }
  const UIEdgeInsets inset = FLMEdgeInsets(next.contentInset);
  const UIEdgeInsets oldInset = FLMEdgeInsets(old.contentInset);
  if (!UIEdgeInsetsEqualToEdgeInsets(inset, oldInset)) {
    // UIRefreshControl adds its height to the top inset while refreshing, so the prop moves the inset by its change.
    const UIEdgeInsets current = view.contentInset;
    view.contentInset = UIEdgeInsetsMake(current.top + inset.top - oldInset.top,
                                         current.left + inset.left - oldInset.left,
                                         current.bottom + inset.bottom - oldInset.bottom,
                                         current.right + inset.right - oldInset.right);
  }
  const UIEdgeInsets indicator = FLMEdgeInsets(next.scrollIndicatorInsets);
  if (!UIEdgeInsetsEqualToEdgeInsets(indicator, FLMEdgeInsets(old.scrollIndicatorInsets))) {
    view.verticalScrollIndicatorInsets = indicator;
    view.horizontalScrollIndicatorInsets = indicator;
  }
}

- (std::pair<double, double>)crossPadding:(const FlumeViewProps &)props
{
  return props.horizontal ? std::make_pair(props.contentPaddingTop, props.contentPaddingBottom)
                          : std::make_pair(props.contentPaddingLeft, props.contentPaddingRight);
}

- (void)configureLayout:(const FlumeViewProps &)props
{
  flume::ListLayoutConfig config;
  config.horizontal = props.horizontal;
  config.inverted = props.inverted;
  config.lanes = std::max(1, props.numColumns);
  config.paddingStart = props.horizontal ? props.contentPaddingLeft : props.contentPaddingTop;
  config.paddingEnd = props.horizontal ? props.contentPaddingRight : props.contentPaddingBottom;
  const auto cross = [self crossPadding:props];
  config.paddingCrossStart = cross.first;
  config.paddingCrossEnd = cross.second;
  config.estimatedItemLength = props.estimatedItemSize > 0 ? props.estimatedItemSize : -1;
  config.separator = std::max(0.0, props.separatorSize);
  config.separatorInsetStart = props.separatorInsetStart;
  config.separatorInsetEnd = props.separatorInsetEnd;
  std::vector<int> sticky = props.stickyHeaderIndices;
  std::sort(sticky.begin(), sticky.end());
  // A sticky header spans every column of a grid, as on Android.
  std::vector<int> fullSpan = config.lanes > 1 ? sticky : std::vector<int>();
  [_layout configure:config fullSpan:fullSpan sticky:sticky];
  _horizontal = props.horizontal;
  _inverted = props.inverted;
  _collectionView.alwaysBounceHorizontal = _horizontal;
  // A horizontal strip never takes the status bar tap from the vertical list beside it.
  _collectionView.scrollsToTop = !_horizontal;
}

// A new cell size across the scroll axis or a new text size: every row binds and measures again.
- (void)rebind
{
  if (!_attached) {
    return;
  }
  flume::Engine::instance().invalidateMeasures(_storeId);
  _fontScale = RCTFontSizeMultiplier();
  [_layout invalidateLengths];
  [_collectionView reloadData];
}

- (void)contentSizeCategoryChanged:(NSNotification *)notification
{
  [self rebind];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  const CGSize size = _collectionView.bounds.size;
  const CGFloat cross = _horizontal ? size.height : size.width;
  if (cross != _lastCross) {
    const BOOL resized = _lastCross > 0;
    _lastCross = cross;
    if (resized) {
      [self rebind];
    }
  }
  if (_pinToEnd && _attached && size.width > 0 && size.height > 0) {
    _pinToEnd = NO;
    [_collectionView layoutIfNeeded];
    const CGSize content = _collectionView.collectionViewLayout.collectionViewContentSize;
    _collectionView.contentOffset = _horizontal ? CGPointMake(std::max<CGFloat>(0, content.width - size.width), 0)
                                                : CGPointMake(0, std::max<CGFloat>(0, content.height - size.height));
  }
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
  const int count = flume::Engine::instance().attach(_storeId, _listener.get(), std::make_unique<FLMTextMeasurer>(), scale);
  // A text size change while the list was off screen left its text measured at the old size.
  const CGFloat fontScale = RCTFontSizeMultiplier();
  if (_everAttached && fontScale != _fontScale) {
    flume::Engine::instance().invalidateMeasures(_storeId);
  }
  _fontScale = fontScale;
  _attached = YES;
  if (_inverted && !_everAttached) {
    _pinToEnd = YES;
    [self setNeedsLayout];
  }
  // A return to the same rows keeps their lengths, so the offset still shows them; new data starts over.
  if (_everAttached && count == _count) {
    [_layout invalidateLengths];
  } else {
    [_layout resetWithCount:count typeOf:[self typeOf]];
  }
  _everAttached = YES;
  _count = count;
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
  __block std::vector<int> retyped;
  void (^update)(void) = ^{
    [self->_collectionView performBatchUpdates:^{
      self->_count = count;
      retyped = [self->_layout applyOps:ops count:count typeOf:[self typeOf]];
      [self->_collectionView deleteItemsAtIndexPaths:FLMIndexPaths(batch.deletes)];
      [self->_collectionView insertItemsAtIndexPaths:FLMIndexPaths(batch.inserts)];
      for (const auto &[from, to] : batch.moves) {
        [self->_collectionView moveItemAtIndexPath:[NSIndexPath indexPathForItem:from inSection:0]
                                       toIndexPath:[NSIndexPath indexPathForItem:to inSection:0]];
      }
    }
                                    completion:nil];
    // UIKit reconfigures within one cell type only, so a row whose type changed is reloaded instead.
    std::vector<int> reconfigures;
    for (int index : batch.reconfigures) {
      if (std::find(retyped.begin(), retyped.end(), index) == retyped.end()) {
        reconfigures.push_back(index);
      }
    }
    if (!retyped.empty()) {
      [self->_collectionView reloadItemsAtIndexPaths:FLMIndexPaths(retyped)];
    }
    // Every changed row, so the cells UIKit prepared ahead of the viewport update too.
    if (!reconfigures.empty()) {
      [self->_collectionView reconfigureItemsAtIndexPaths:FLMIndexPaths(reconfigures)];
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
    // The index is read at press time, so a reused cell reports the row it shows now.
    __weak FLMView *weakSelf = self;
    cell.onPress = ^(FLMCell *pressed, NSString *pressId, BOOL longPress) {
      FLMView *view = weakSelf;
      if (view == nil) {
        return;
      }
      NSIndexPath *path = [view->_collectionView indexPathForCell:pressed];
      if (path != nil) {
        [view emitPress:path.item pressId:pressId longPress:longPress];
      }
    };
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

#pragma mark - UIScrollViewDelegate

// The insets bound the viewport the layout measures inside.
- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView *)scrollView
{
  [_layout invalidateLayout];
}

#pragma mark - Presses

- (void)emitPress:(NSInteger)index pressId:(NSString *)pressId longPress:(BOOL)longPress
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  if (emitter == nullptr) {
    return;
  }
  const int row = static_cast<int>(index);
  std::string key = flume::Engine::instance().keyAt(_storeId, row);
  std::string id = pressId.length > 0 ? std::string(pressId.UTF8String) : std::string();
  if (longPress) {
    emitter->onCellLongPress({row, key, id});
  } else {
    emitter->onCellPress({row, key, id});
  }
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath
{
  [collectionView deselectItemAtIndexPath:indexPath animated:NO];
  [self emitPress:indexPath.item pressId:@"" longPress:NO];
}

- (void)rowLongPressed:(UILongPressGestureRecognizer *)recognizer
{
  if (recognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  NSIndexPath *path = [_collectionView indexPathForItemAtPoint:[recognizer locationInView:_collectionView]];
  if (path != nil) {
    [self emitPress:path.item pressId:@"" longPress:YES];
  }
}

// A pressable inside the row handles its own long press.
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer shouldReceiveTouch:(UITouch *)touch
{
  for (UIView *view = touch.view; view != nil && view != _collectionView; view = view.superview) {
    if ([view isKindOfClass:FLMPressableView.class]) {
      return NO;
    }
  }
  return YES;
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
