#import "FLMView.h"

#import <react/renderer/components/FlumeSpec/ComponentDescriptors.h>
#import <react/renderer/components/FlumeSpec/Props.h>
#import <react/renderer/components/FlumeSpec/RCTComponentViewHelpers.h>
#import <React/RCTConversions.h>
#import <React/RCTUtils.h>

#import "FLMBindStats.h"
#import "FLMCell.h"
#import "FLMLayout.h"
#import "FLMCollectionView.h"
#import "FLMKeyboardTaps.h"
#import "FLMPressableView.h"
#import "FLMRefresh.h"
#import "FLMScrollEvents.h"
#import "FLMStoreListener.h"
#import "FLMText.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <time.h>
#include <unordered_set>

#include "adapter/BatchUpdates.h"
#include "adapter/EdgeTracker.h"
#include "adapter/ScrollTargets.h"
#include "adapter/Viewability.h"
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

// An animated command's landing, computed again when its animation ends.
struct FLMPendingTarget {
  enum class Kind { Index, Start, End } kind;  // Start and End are the content's edges, padding included
  int index;
  double viewPosition;
  double viewOffset;
};

flume::KeyboardTaps FLMKeyboardTapsMode(FlumeViewKeyboardShouldPersistTaps mode)
{
  switch (mode) {
    case FlumeViewKeyboardShouldPersistTaps::Always:
      return flume::KeyboardTaps::Always;
    case FlumeViewKeyboardShouldPersistTaps::Handled:
      return flume::KeyboardTaps::Handled;
    case FlumeViewKeyboardShouldPersistTaps::Never:
      return flume::KeyboardTaps::Never;
  }
  return flume::KeyboardTaps::Never;
}

}  // namespace

@interface FLMView () <RCTFlumeViewViewProtocol,
                       UICollectionViewDataSource,
                       UICollectionViewDelegate,
                       UIGestureRecognizerDelegate>
@end

@implementation FLMView {
  FLMCollectionView *_collectionView;
  FLMLayout *_layout;
  UIView *_children;  // Fabric children, kept out of sight
  std::unique_ptr<FLMStoreListener> _listener;
  NSMutableSet<NSString *> *_registered;
  std::unordered_set<int> _holders;
  int _storeId;
  int _nextHolderId;
  NSInteger _count;
  BOOL _attached;
  CGFloat _visibleEnd;  // the viewport's end in content coordinates as of the last scroll, NAN before one
  BOOL _itemAnimations;
  BOOL _horizontal;
  BOOL _rowsPressable;
  BOOL _inverted;
  BOOL _everAttached;
  BOOL _openingArmed;  // the opening position waits for rows
  BOOL _stackFromEnd;
  int _initialScrollIndex;
  CGFloat _lastCross;
  CGFloat _fontScale;  // the font size multiplier the store's text was measured at
  UILongPressGestureRecognizer *_rowLongPress;
  flume::ScrollEvents _scrollEvents;
  BOOL _emitScrollEvents;
  flume::ViewabilityTracker _viewability;
  BOOL _emitViewability;
  flume::EdgeTracker _edges;
  FLMRefresh *_refresh;
  FLMKeyboardTaps *_keyboardTaps;
  int _measureHolder;  // binds rows no cell shows, for their lengths; 0 until the first measure
  flume::OpBatch _measureBatch;
  std::optional<FLMPendingTarget> _pendingTarget;
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
    _initialScrollIndex = -1;
    _visibleEnd = NAN;
    _itemAnimations = YES;
    _registered = [NSMutableSet set];
    _layout = [FLMLayout new];
    [_layout configure:flume::ListLayoutConfig() fullSpan:std::vector<int>() sticky:std::vector<int>()];
    _collectionView = [[FLMCollectionView alloc] initWithFrame:self.bounds collectionViewLayout:_layout];
    __weak FLMView *weakSelf = self;
    _collectionView.didLayout = ^{
      [weakSelf listDidLayout];
    };
    _refresh = [[FLMRefresh alloc] initWithScrollView:_collectionView
                                            onRefresh:^{
                                              [weakSelf emitRefresh];
                                            }];
    _keyboardTaps = [[FLMKeyboardTaps alloc] initWithScrollView:_collectionView
                                                    pressableAt:^BOOL(UITouch *touch) {
                                                      return [weakSelf pressableAt:touch];
                                                    }];
    _layout.measureItem = ^CGFloat(NSInteger index) {
      FLMView *view = weakSelf;
      return view != nil ? [view measureItem:index] : -1;
    };
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
      next.separatorInsetStart != old.separatorInsetStart || next.separatorInsetEnd != old.separatorInsetEnd ||
      next.maintainVisibleContentPosition.startRenderingFromBottom !=
          old.maintainVisibleContentPosition.startRenderingFromBottom;
  if (next.separatorColor != old.separatorColor) {
    _layout.separatorColor = RCTUIColorFromSharedColor(next.separatorColor);
  }
  if (layout) {
    [self configureLayout:next];
  }
  // Turning inverted on or off keeps item 0 in view.
  if (next.inverted != old.inverted) {
    [_layout scheduleOffset:FLMPendingOffset{FLMPendingOffset::Kind::Edge, 0, 0,
                                             next.inverted ? flume::VisualEdge::Bottom : flume::VisualEdge::Top}];
  }
  // Read when rows first arrive, as Android opens on each empty-to-filled change; a later change moves nothing.
  if (next.initialScrollIndex != old.initialScrollIndex) {
    _initialScrollIndex = next.initialScrollIndex;
  }
  if (next.storeId != _storeId) {
    [self detach];
    _storeId = next.storeId;
    _everAttached = NO;
    [self attach];
  } else if (shape) {
    [self rebind];
  }
  _emitScrollEvents = next.emitScrollEvents;
  _emitViewability = next.emitViewabilityEvents;
  const auto &viewability = next.viewabilityConfig;
  _viewability.config = flume::ViewabilityConfig{viewability.itemVisiblePercentThreshold,
                                                 viewability.viewAreaCoveragePercentThreshold,
                                                 viewability.minimumViewTime, viewability.waitForInteraction};
  if (next.scrollEventThrottle != old.scrollEventThrottle) {
    _scrollEvents.setThrottleMs(next.scrollEventThrottle);
  }
  const auto &anchoring = next.maintainVisibleContentPosition;
  _layout.anchorConfig = flume::AnchorConfig{anchoring.disabled, anchoring.autoscrollToTopThreshold,
                                             anchoring.autoscrollToBottomThreshold,
                                             anchoring.animateAutoScrollToBottom};
  _itemAnimations = next.itemAnimations;
  _rowsPressable = next.rowsPressable;
  _collectionView.allowsSelection = _rowsPressable;
  _rowLongPress.enabled = _rowsPressable;
  [self updateScrollProps:next old:old];
  // React Native applies the control's look before it begins refreshing.
  if (next.refreshTintColor != old.refreshTintColor) {
    _refresh.tintColor = RCTUIColorFromSharedColor(next.refreshTintColor);
  }
  if (next.refreshProgressViewOffset != old.refreshProgressViewOffset) {
    _refresh.progressViewOffset = next.refreshProgressViewOffset;
  }
  _refresh.enabled = next.refreshEnabled && !next.horizontal;
  if (next.refreshing != old.refreshing) {
    [_refresh setRefreshing:next.refreshing];
  }
  [super updateProps:props oldProps:oldProps];
}

- (void)emitRefresh
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  if (emitter != nullptr) {
    emitter->onRefresh({});
  }
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
  if (next.keyboardShouldPersistTaps != old.keyboardShouldPersistTaps) {
    _keyboardTaps.mode = FLMKeyboardTapsMode(next.keyboardShouldPersistTaps);
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
  config.stackFromEnd =
      !props.inverted && config.lanes == 1 && props.maintainVisibleContentPosition.startRenderingFromBottom;
  std::vector<int> sticky = props.stickyHeaderIndices;
  std::sort(sticky.begin(), sticky.end());
  // A sticky header spans every column of a grid, as on Android.
  std::vector<int> fullSpan = config.lanes > 1 ? sticky : std::vector<int>();
  [_layout configure:config fullSpan:fullSpan sticky:sticky];
  _horizontal = props.horizontal;
  _inverted = props.inverted;
  _stackFromEnd = config.stackFromEnd;
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
  [_refresh layoutSubviews];
}

- (void)didMoveToWindow
{
  [super didMoveToWindow];
  if (self.window != nil) {
    [self attach];
  } else {
    _pendingTarget.reset();
    [self emitScrollEvents:_scrollEvents.leftWindow() velocity:CGPointZero];
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
  if (!_everAttached) {
    _openingArmed = YES;
  }
  // A return to the same rows keeps their lengths, so the offset still shows them; new data starts over.
  if (_everAttached && count == _count) {
    [_layout invalidateLengths];
  } else {
    [_layout resetWithCount:count typeOf:[self typeOf]];
  }
  _everAttached = YES;
  _count = count;
  _viewability.invalidate();
  [_collectionView reloadData];
  [self scheduleOpening];
}

// FlatList's initialScrollIndex, or the bottom of a list anchored there; an upright list opens at its top as it is.
- (void)scheduleOpening
{
  if (!_openingArmed || _count == 0) {
    return;
  }
  _openingArmed = NO;
  const std::optional<flume::Landing> landing = flume::openingLanding(_initialScrollIndex, static_cast<int>(_count));
  FLMPendingOffset pending{FLMPendingOffset::Kind::Edge};
  if (landing && !landing->contentEnd) {
    pending = FLMPendingOffset{FLMPendingOffset::Kind::Landing, landing->index, landing->viewPosition};
  } else if (landing) {
    pending.edge = _inverted ? flume::VisualEdge::Top : flume::VisualEdge::Bottom;
  } else if (_inverted || _stackFromEnd) {
    pending.edge = flume::VisualEdge::Bottom;
  } else {
    return;
  }
  [_layout scheduleOffset:pending];
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
  // Keys moved between indices, so the next viewable set counts as changed, as FlatList resets its cache.
  _viewability.invalidate();
  if (count == 0) {
    _openingArmed = YES;
  }
  flume::BatchUpdates batch = flume::toBatchUpdates(ops, static_cast<int>(_count));
  // The first rows reset and reload, so an armed opening lands before any of them binds.
  if (batch.reload || _count == 0) {
    _count = count;
    [_layout resetWithCount:count typeOf:[self typeOf]];
    [_collectionView reloadData];
    [self scheduleOpening];
    return;
  }
  const flume::AnchorDecision anchor = [_layout prepareForOps:ops count:count];
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
  // An animated batch while the list is dragged or decelerating draws its rows twice for a few frames.
  if (_itemAnimations && !_collectionView.isDragging && !_collectionView.isDecelerating) {
    update();
  } else {
    [UIView performWithoutAnimation:update];
  }
  if (anchor.outcome == flume::AnchorOutcome::ScrollToEdge) {
    _pendingTarget.reset();
    [self scrollTo:[_layout offsetForVisualEdge:anchor.edge] animated:anchor.animated];
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

// A row's length before any cell shows it, bound into a holder of its own; -1 for a React row.
- (CGFloat)measureItem:(NSInteger)index
{
  auto &engine = flume::Engine::instance();
  if (!_attached || engine.viewType(_storeId, static_cast<int>(index)) % 2 != 0) {
    return -1;
  }
  if (_measureHolder == 0) {
    _measureHolder = ++_nextHolderId;
  }
  const CGFloat cross = [_layout bindCrossLengthForItem:index];
  const uint64_t start = FLMBindStats.enabled ? clock_gettime_nsec_np(CLOCK_UPTIME_RAW) : 0;
  if (!engine.bind(_storeId, _measureHolder, static_cast<int>(index), _horizontal ? NAN : cross,
                   _horizontal ? cross : NAN, _measureBatch)) {
    return -1;
  }
  _holders.insert(_measureHolder);
  if (start != 0) {
    [FLMBindStats recordMeasure:static_cast<int64_t>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - start)];
  }
  return _horizontal ? _measureBatch.rootWidth : _measureBatch.rootHeight;
}

#pragma mark - UIScrollViewDelegate

// The insets bound the viewport the layout measures inside.
- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView *)scrollView
{
  [_layout keepVisibleEnd:_visibleEnd];
  [_layout invalidateLayout];
}

// The scroll event goes out only with a handler; drag and momentum events once per gesture.
- (void)emitScrollEvents:(const std::vector<flume::ScrollEventKind> &)kinds velocity:(CGPoint)velocity
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  if (emitter == nullptr) {
    return;
  }
  for (flume::ScrollEventKind kind : kinds) {
    if (kind == flume::ScrollEventKind::Scroll && !_emitScrollEvents) {
      continue;
    }
    const CGPoint released = kind == flume::ScrollEventKind::EndDrag ? velocity : CGPointZero;
    FLMEmitScrollEvent(*emitter, kind, _collectionView, _horizontal, _inverted, released);
  }
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
  const CGRect bounds = scrollView.bounds;
  const UIEdgeInsets inset = scrollView.adjustedContentInset;
  _visibleEnd = _horizontal ? CGRectGetMaxX(bounds) - inset.right : CGRectGetMaxY(bounds) - inset.bottom;
  if (_emitScrollEvents && _scrollEvents.scrolled(CACurrentMediaTime() * 1000)) {
    [self emitScrollEvents:std::vector<flume::ScrollEventKind>{flume::ScrollEventKind::Scroll} velocity:CGPointZero];
  }
}

// The status bar tap's own animation ends in scrollViewDidScrollToTop:, never in a command's re-land.
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView
{
  _pendingTarget.reset();
  return YES;
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  _pendingTarget.reset();
  [self emitScrollEvents:_scrollEvents.willBeginDragging() velocity:CGPointZero];
  _viewability.interacted = true;
  [self evaluateViewability];
}

#pragma mark - List events

// After every layout pass, scroll frames included.
- (void)listDidLayout
{
  [self evaluateViewability];
  [self evaluateEdges];
}

- (void)evaluateEdges
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  const CGSize size = _collectionView.bounds.size;
  if (!_attached || emitter == nullptr || size.width <= 0 || size.height <= 0) {
    return;
  }
  const UIEdgeInsets insets = _collectionView.adjustedContentInset;
  const CGPoint offset = _collectionView.contentOffset;
  const CGSize content = _collectionView.contentSize;
  const flume::EdgeDistances distances = _horizontal
      ? flume::edgeDistances(offset.x, content.width, size.width, insets.left, insets.right, _inverted)
      : flume::edgeDistances(offset.y, content.height, size.height, insets.top, insets.bottom, _inverted);
  const auto &props = static_cast<const FlumeViewProps &>(*_props);
  const FLMViewport viewport = [_layout viewportFor:_collectionView.bounds];
  const flume::EdgeHits hits = _edges.update(distances, static_cast<int>(_count), static_cast<float>(viewport.length),
                                             props.startReachedThreshold, props.endReachedThreshold);
  if (hits.end) {
    emitter->onEndReached({hits.distances.fromEnd});
  }
  if (hits.start) {
    emitter->onStartReached({hits.distances.fromStart});
  }
}

- (void)evaluateViewability
{
  if (!_emitViewability || !_attached) {
    return;
  }
  const FLMViewport viewport = [_layout viewportFor:_collectionView.bounds];
  const std::vector<int> viewable = _viewability.compute(static_cast<float>(viewport.length), [_layout spansIn:viewport]);
  const int storeId = _storeId;
  auto keyed =
      _viewability.keyed(viewable, [storeId](int index) { return flume::Engine::instance().keyAt(storeId, index); });
  if (!keyed) {
    return;
  }
  const double delayMs = _viewability.config.minimumViewTimeMs;
  if (delayMs <= 0) {
    [self emitViewability:_viewability.report(*keyed)];
    return;
  }
  __weak FLMView *weakSelf = self;
  const std::vector<flume::ViewToken> scheduled = *keyed;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(delayMs * NSEC_PER_MSEC)),
                 dispatch_get_main_queue(), ^{
                   FLMView *view = weakSelf;
                   if (view != nil && view->_attached) {
                     [view emitViewability:view->_viewability.reportDelayed(scheduled)];
                   }
                 });
}

- (void)emitViewability:(const std::optional<flume::ViewabilityReport> &)report
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  if (!report || emitter == nullptr) {
    return;
  }
  FlumeViewEventEmitter::OnViewableItemsChanged event;
  for (const flume::ViewToken &token : report->viewable) {
    event.viewable.push_back({token.index, token.key, true});
  }
  for (const flume::ViewableChange &change : report->changed) {
    event.changed.push_back({change.token.index, change.token.key, change.isViewable});
  }
  emitter->onViewableItemsChanged(event);
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  [self emitScrollEvents:_scrollEvents.willEndDragging() velocity:velocity];
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  _scrollEvents.didEndDragging(decelerate);
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView *)scrollView
{
  [self emitScrollEvents:_scrollEvents.willBeginDecelerating() velocity:CGPointZero];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  [self emitScrollEvents:_scrollEvents.didEndDecelerating() velocity:CGPointZero];
}

// Rows measured during the glide can move the target, so an animated command lands on it again.
- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  if (_pendingTarget && _count > 0) {
    FLMPendingTarget target = *_pendingTarget;
    target.index = *flume::clampIndex(target.index, static_cast<int>(_count));
    const CGFloat offset = [self offsetOf:target];
    if (std::fabs(offset - [self currentOffset]) > 0.5) {
      [_collectionView setContentOffset:[self pointAt:offset] animated:NO];
    }
  }
  _pendingTarget.reset();
  [self emitScrollEvents:_scrollEvents.didEndScrollingAnimation() velocity:CGPointZero];
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

// A pressable keeps the keyboard in handled mode; a row is one while the list has a press handler.
- (BOOL)pressableAt:(UITouch *)touch
{
  for (UIView *view = touch.view; view != nil && view != _collectionView; view = view.superview) {
    if ([view isKindOfClass:FLMPressableView.class] || (_rowsPressable && [view isKindOfClass:FLMCell.class])) {
      return YES;
    }
  }
  return NO;
}

// In never mode the first tap with the keyboard up only closes it.
- (BOOL)swallowsTaps
{
  return flume::decideTap(_keyboardTaps.mode, _keyboardTaps.keyboardUp, true).swallow;
}

- (BOOL)collectionView:(UICollectionView *)collectionView shouldHighlightItemAtIndexPath:(NSIndexPath *)indexPath
{
  return ![self swallowsTaps];
}

- (BOOL)collectionView:(UICollectionView *)collectionView shouldSelectItemAtIndexPath:(NSIndexPath *)indexPath
{
  return ![self swallowsTaps];
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

- (CGFloat)currentOffset
{
  return _horizontal ? _collectionView.contentOffset.x : _collectionView.contentOffset.y;
}

- (CGPoint)pointAt:(CGFloat)offset
{
  const CGPoint current = _collectionView.contentOffset;
  return _horizontal ? CGPointMake(offset, current.y) : CGPointMake(current.x, offset);
}

// React Native sends nothing for a move under half a point; an animated scroll's events come from its frames.
- (void)scrollTo:(CGFloat)offset animated:(BOOL)animated
{
  // No animation will end, so no command waits to land again.
  if (std::fabs(offset - [self currentOffset]) <= 0.5) {
    _pendingTarget.reset();
    return;
  }
  if (animated) {
    _scrollEvents.scrolledProgrammatically(true);
    [_collectionView setContentOffset:[self pointAt:offset] animated:YES];
    return;
  }
  [_collectionView setContentOffset:[self pointAt:offset] animated:NO];
  [self emitScrollEvents:_scrollEvents.scrolledProgrammatically(false) velocity:CGPointZero];
}

// The content start is the visual top, or the bottom when inverted.
- (CGFloat)offsetOf:(const FLMPendingTarget &)target
{
  switch (target.kind) {
    case FLMPendingTarget::Kind::Start:
      return [_layout offsetForVisualEdge:_inverted ? flume::VisualEdge::Bottom : flume::VisualEdge::Top];
    case FLMPendingTarget::Kind::End:
      return [_layout offsetForVisualEdge:_inverted ? flume::VisualEdge::Top : flume::VisualEdge::Bottom];
    case FLMPendingTarget::Kind::Index:
      break;
  }
  return [_layout targetOffsetForIndex:target.index viewPosition:target.viewPosition viewOffset:target.viewOffset];
}

// Past two viewports an animated scroll jumps first, so it glides over measured rows only.
- (void)scrollToTarget:(const FLMPendingTarget &)target animated:(BOOL)animated
{
  _viewability.interacted = true;
  _pendingTarget.reset();
  if (!animated) {
    [self scrollTo:[self offsetOf:target] animated:NO];
    return;
  }
  const CGFloat viewport = [_layout viewportFor:_collectionView.bounds].length;
  const std::optional<float> near = flume::preJumpOffset(static_cast<float>([self currentOffset]),
                                                         static_cast<float>([self offsetOf:target]),
                                                         static_cast<float>(viewport));
  if (near) {
    [_collectionView setContentOffset:[self pointAt:*near] animated:NO];
    [_collectionView layoutIfNeeded];
  }
  [_layout measureGlideToIndex:target.index viewPosition:target.viewPosition];
  const CGFloat offset = [self offsetOf:target];
  _pendingTarget = target;
  [self scrollTo:offset animated:YES];
}

- (void)scrollToIndex:(NSInteger)index animated:(BOOL)animated viewPosition:(double)viewPosition viewOffset:(double)viewOffset
{
  if (!_attached || _count == 0) {
    return;
  }
  const int clamped = *flume::clampIndex(static_cast<int>(index), static_cast<int>(_count));
  [self scrollToTarget:FLMPendingTarget{FLMPendingTarget::Kind::Index, clamped, viewPosition, viewOffset}
              animated:animated];
}

// The content start lands on measured rows; further offsets stay on the estimates.
- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!_attached || _count == 0) {
    return;
  }
  if (offset <= 0) {
    [self scrollToTarget:FLMPendingTarget{FLMPendingTarget::Kind::Start, 0, 0, 0} animated:animated];
    return;
  }
  _viewability.interacted = true;
  _pendingTarget.reset();
  const UIEdgeInsets insets = _collectionView.adjustedContentInset;
  const CGSize content = _collectionView.contentSize;
  const CGSize bounds = _collectionView.bounds.size;
  const float contentLength = static_cast<float>(_horizontal ? content.width : content.height);
  const float boundsLength = static_cast<float>(_horizontal ? bounds.width : bounds.height);
  const float start = flume::contentStartOffset(static_cast<float>(offset), contentLength, boundsLength, _inverted);
  [self scrollTo:flume::clampOffset(start, contentLength, boundsLength,
                                    static_cast<float>(_horizontal ? insets.left : insets.top),
                                    static_cast<float>(_horizontal ? insets.right : insets.bottom))
        animated:animated];
}

- (void)scrollToEnd:(BOOL)animated
{
  if (!_attached || _count == 0) {
    return;
  }
  [self scrollToTarget:FLMPendingTarget{FLMPendingTarget::Kind::End, static_cast<int>(_count) - 1, 1, 0}
              animated:animated];
}

- (void)setNativeRefreshing:(BOOL)refreshing
{
  [_refresh setRefreshing:refreshing];
}

@end
