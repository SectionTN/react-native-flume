#import "FLMView.h"

#include "fabric/FlumeComponentDescriptors.h"
#import <react/renderer/components/FlumeSpec/Props.h>
#import <react/renderer/components/FlumeSpec/RCTComponentViewHelpers.h>
#import <React/RCTConversions.h>
#import <React/RCTMountingTransactionObserving.h>
#import <React/RCTUtils.h>

#import "FLMBindStats.h"
#import "FLMCell.h"
#import "FLMLayout.h"
#import "FLMCollectionView.h"
#import "FLMKeyboardTaps.h"
#import "FLMPressableView.h"
#import "FLMRefresh.h"
#import "FLMScrollEvents.h"
#import "FLMSlots.h"
#import "FLMStoreListener.h"
#import "FLMText.h"
#import "FLMTouchCancel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <time.h>
#include <unordered_set>

#include "adapter/BatchUpdates.h"
#include "adapter/EdgeTracker.h"
#include "adapter/JumpGate.h"
#include "adapter/ListRows.h"
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

constexpr CGFloat kFLMSlotCrossSlop = 0.5;  // Yoga rounds a slot to the pixel grid

// An animated command's landing, computed again when its animation ends.
struct FLMPendingTarget {
  enum class Kind { Index, Start, End } kind;  // Start and End are the content's edges, padding included
  int index;
  double viewPosition;
  double viewOffset;
};

// A command held until the React rows it shows render: their slots belong to owner until it runs.
struct FLMJump {
  FLMPendingTarget target;
  bool animated;
  int owner;
  bool timedOut = false;
};

constexpr int kFLMMaxRealigns = 4;

static double FLMNowMs()
{
  return CACurrentMediaTime() * 1000;
}

// Ops that move, add or change no row: a sticky-only change.
static BOOL FLMMovesNoRow(const flume::ListOps &ops)
{
  return !ops.reset && ops.removed.empty() && ops.inserted.empty() && ops.moved.empty() && ops.changed.empty();
}

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

// The store's type for a data row, the list component's own for the rest.
int FLMViewType(int storeId, const flume::ListRows &rows, int position)
{
  switch (rows.kind(position)) {
    case flume::RowKind::Header:
      return flume::kHeaderViewType;
    case flume::RowKind::Data:
      return flume::Engine::instance().viewType(storeId, rows.dataIndex(position));
    case flume::RowKind::Empty:
      return flume::kEmptyViewType;
    case flume::RowKind::Footer:
      return flume::kFooterViewType;
  }
  return flume::kFooterViewType;
}

}  // namespace

@interface FLMView () <RCTFlumeViewViewProtocol,
                       UICollectionViewDataSource,
                       UICollectionViewDelegate,
                       UIGestureRecognizerDelegate,
                       RCTMountingTransactionObserving,
                       FLMSlotViewHost>
@end

@implementation FLMView {
  FLMCollectionView *_collectionView;
  FLMLayout *_layout;
  UIView *_bay;  // the Fabric children no cell shows, out of sight
  FLMSlots *_slots;
  BOOL _slotsFlushPending;
  BOOL _reclaimSlots;  // slots lent around a reload go back to the pool after the next layout pass
  std::optional<std::pair<int, int>> _aheadRange;  // the first and last row of the last look-ahead, reset when rows change
  flume::ListLayoutConfig _layoutConfig;  // from the props, plus what the list components change in configureLayoutRows
  std::vector<int> _stickyIndices;  // data indices
  BOOL _componentsMoved;  // a list component came or went while detached, so the next attach resets the rows
  FlumeViewShadowNode::ConcreteState::Shared _state;
  CGPoint _writtenOffset;
  BOOL _offsetWritten;
  std::unique_ptr<FLMStoreListener> _listener;
  NSMutableSet<NSString *> *_registered;
  std::unordered_set<int> _holders;
  int _storeId;
  int _nextHolderId;
  flume::ListRows _rows;  // the rows the store holds, with the store's data count
  NSInteger _itemCount;  // the rows UIKit was told about
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
  CGFloat _paddingWidth;  // the width percent padding last resolved against
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
  BOOL _gliding;  // an animated scroll runs, isDecelerating reads NO through it
  flume::JumpGate<FLMJump> _jumps;
  std::optional<FLMJump> _readyJump;  // its rows rendered inside a Fabric mount, so it runs a turn later
  BOOL _reclaimAfterGlide;  // a glide's lent slots go back to the pool when it ends
  std::optional<FLMPendingTarget> _realign;
  int _realigns;
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
    _collectionView.touchDown = ^(UIView *hit) {
      [weakSelf listTouchedDown:hit];
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
    _bay = [UIView new];
    _bay.hidden = YES;
    [self addSubview:_bay];
    _slots = [[FLMSlots alloc] initWithBay:_bay];
    _slots.viewHost = self;
    _slots.onRendered = ^(int slot) {
      [weakSelf slotRendered:slot];
    };
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
      [self crossPadding:next] != [self crossPadding:old] || next.crossGap != old.crossGap ||
      next.laneInsetStart != old.laneInsetStart || next.laneInsetEnd != old.laneInsetEnd;
  const BOOL layout = shape || next.inverted != old.inverted || next.mainGap != old.mainGap ||
      next.gridRowGap != old.gridRowGap || next.contentPaddingTop != old.contentPaddingTop ||
      next.contentJustify != old.contentJustify || next.growFooter != old.growFooter ||
      next.paddingPercentTop != old.paddingPercentTop || next.paddingPercentLeft != old.paddingPercentLeft ||
      next.paddingPercentBottom != old.paddingPercentBottom || next.paddingPercentRight != old.paddingPercentRight ||
      next.contentPaddingLeft != old.contentPaddingLeft || next.contentPaddingBottom != old.contentPaddingBottom ||
      next.contentPaddingRight != old.contentPaddingRight || next.estimatedItemSize != old.estimatedItemSize ||
      next.separatorSize != old.separatorSize ||
      next.separatorInsetStart != old.separatorInsetStart || next.separatorInsetEnd != old.separatorInsetEnd ||
      next.maintainVisibleContentPosition.startRenderingFromBottom !=
          old.maintainVisibleContentPosition.startRenderingFromBottom;
  if (next.contentBackgroundColor != old.contentBackgroundColor) {
    _layout.contentBackgroundColor = RCTUIColorFromSharedColor(next.contentBackgroundColor);
  }
  if (next.separatorColor != old.separatorColor) {
    _layout.separatorColor = RCTUIColorFromSharedColor(next.separatorColor);
  }
  const BOOL components = next.listHeader != old.listHeader || next.listFooter != old.listFooter ||
      next.listEmpty != old.listEmpty;
  if (components) {
    _rows.header = next.listHeader;
    _rows.footer = next.listFooter;
    _rows.emptyComponent = next.listEmpty;
  }
  if (layout || components) {
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
  } else {
    if (components) {
      [self reloadRows];
    }
    // Inverted too: a separator worklet sits after its row, or before it when inverted.
    if (shape || next.inverted != old.inverted) {
      [self rebind];
    }
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

// Points plus a fraction of the list width, Yoga's base for a percent on every edge.
- (UIEdgeInsets)paddingOf:(const FlumeViewProps &)props
{
  const float width = static_cast<float>(self.bounds.size.width);
  return UIEdgeInsetsMake(flume::paddingOf(props.contentPaddingTop, props.paddingPercentTop, width),
                          flume::paddingOf(props.contentPaddingLeft, props.paddingPercentLeft, width),
                          flume::paddingOf(props.contentPaddingBottom, props.paddingPercentBottom, width),
                          flume::paddingOf(props.contentPaddingRight, props.paddingPercentRight, width));
}

- (std::pair<double, double>)crossPadding:(const FlumeViewProps &)props
{
  const UIEdgeInsets padding = [self paddingOf:props];
  return props.horizontal ? std::make_pair(padding.top, padding.bottom) : std::make_pair(padding.left, padding.right);
}

- (void)configureLayout:(const FlumeViewProps &)props
{
  flume::ListLayoutConfig config;
  config.horizontal = props.horizontal;
  config.inverted = props.inverted;
  config.lanes = std::max(1, props.numColumns);
  const UIEdgeInsets padding = [self paddingOf:props];
  config.paddingStart = props.horizontal ? padding.left : padding.top;
  config.paddingEnd = props.horizontal ? padding.right : padding.bottom;
  const auto cross = [self crossPadding:props];
  config.paddingCrossStart = cross.first;
  config.paddingCrossEnd = cross.second;
  config.estimatedItemLength = props.estimatedItemSize > 0 ? props.estimatedItemSize : -1;
  config.separator = std::max(0.0, props.separatorSize);
  config.separatorInsetStart = props.separatorInsetStart;
  config.separatorInsetEnd = props.separatorInsetEnd;
  config.mainGap = std::max(0.0, props.mainGap);
  config.gridRowGap = std::max(0.0, props.gridRowGap);
  config.crossGap = std::max(0.0, props.crossGap);
  config.laneInsetStart = props.laneInsetStart;
  config.laneInsetEnd = props.laneInsetEnd;
  config.justify = props.contentJustify == FlumeViewContentJustify::Center ? flume::ListLayoutConfig::Justify::Center
                   : props.contentJustify == FlumeViewContentJustify::End  ? flume::ListLayoutConfig::Justify::End
                                                                          : flume::ListLayoutConfig::Justify::Start;
  config.growFooter = props.growFooter;
  config.stackFromEnd =
      !props.inverted && config.lanes == 1 && props.maintainVisibleContentPosition.startRenderingFromBottom;
  _layoutConfig = config;
  [self sendRowShape];
  [self configureLayoutRows];
  _horizontal = props.horizontal;
  _inverted = props.inverted;
  _stackFromEnd = config.stackFromEnd;
  _collectionView.alwaysBounceHorizontal = _horizontal;
  // A horizontal strip never takes the status bar tap from the vertical list beside it.
  _collectionView.scrollsToTop = !_horizontal;
  [self requestSlotsFlush];
}

// The engine composes worklet separators with the shape the rows are laid out with.
- (void)sendRowShape
{
  if (_storeId == 0) {
    return;
  }
  const flume::ListLayoutConfig &config = _layoutConfig;
  flume::Engine::instance().setRowShape(_storeId, {config.horizontal, config.inverted, config.horizontal ? 1 : config.lanes});
}

// The header, the footer and the empty row move the data's positions, its separators and the rows that span a grid.
- (void)configureLayoutRows
{
  flume::ListLayoutConfig config = _layoutConfig;
  config.dataStart = _rows.headerCount();
  config.afterData = _rows.footer ? 1 : 0;
  config.emptyIndex = _rows.hasEmptyRow() ? _rows.headerCount() : -1;
  std::vector<int> sticky = [self stickyPositions];
  [_layout configure:config fullSpan:[self fullSpanPositions:sticky] sticky:sticky];
}

- (std::vector<int>)stickyPositions
{
  std::vector<int> sticky = _stickyIndices;
  for (int &index : sticky) {
    index = _rows.position(index);
  }
  std::sort(sticky.begin(), sticky.end());
  return sticky;
}

// Sticky rows and the list components span every column of a grid, as on Android.
- (std::vector<int>)fullSpanPositions:(const std::vector<int> &)sticky
{
  std::vector<int> fullSpan;
  if (_layoutConfig.lanes > 1) {
    fullSpan = _rows.fullSpanPositions({});
    fullSpan.insert(fullSpan.end(), sticky.begin(), sticky.end());
    std::sort(fullSpan.begin(), fullSpan.end());
    fullSpan.erase(std::unique(fullSpan.begin(), fullSpan.end()), fullSpan.end());
  }
  return fullSpan;
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
  [self lendSlotCells];
  [self reloadCollection];
}

// UIKit performs a reloadData at its next layout, so until then a batch would land on rows reloaded from the new data.
- (void)reloadCollection
{
  [_collectionView reloadData];
  _layout.reloadPending = YES;
}

// A list component came or went, so every row after it moved.
- (void)reloadRows
{
  if (!_attached) {
    _componentsMoved = YES;
    return;
  }
  [self dropJump];
  _itemCount = _rows.itemCount();
  [_layout resetWithCount:_itemCount typeOf:[self typeOf]];
  [self lendSlotCells];
  [self reloadCollection];
}

- (void)contentSizeCategoryChanged:(NSNotification *)notification
{
  [self rebind];
}

- (void)layoutSubviews
{
  // Percent padding follows the list width, resolved before the rows lay out.
  const auto &props = static_cast<const FlumeViewProps &>(*_props);
  if (self.bounds.size.width != _paddingWidth) {
    _paddingWidth = self.bounds.size.width;
    if (props.paddingPercentTop != 0 || props.paddingPercentLeft != 0 || props.paddingPercentBottom != 0 ||
        props.paddingPercentRight != 0) {
      [self configureLayout:props];
    }
  }
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
  [self requestSlotsFlush];
}

- (void)didMoveToWindow
{
  [super didMoveToWindow];
  if (self.window != nil) {
    [self attach];
  } else {
    _pendingTarget.reset();
    _gliding = NO;
    [self dropJump];
    _reclaimAfterGlide = NO;
    [self emitScrollEvents:_scrollEvents.leftWindow() velocity:CGPointZero];
    [self detach];
  }
}

- (void)mountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_slots insertChild:childComponentView atIndex:index];
}

- (void)unmountChildComponentView:(UIView<RCTComponentViewProtocol> *)childComponentView index:(NSInteger)index
{
  [_slots removeChild:childComponentView atIndex:index];
}

// Pre-rendered slots arrive with the data change they render ahead for, so the deferred data applies once they mount.
- (void)mountingTransactionDidMount:(const MountingTransaction &)transaction
               withSurfaceTelemetry:(const SurfaceTelemetry &)surfaceTelemetry
{
  [_slots pool].endBatch();
  if ([_slots takeRegisteredPrerendered] && _storeId != 0) {
    flume::Engine::instance().refresh(_storeId);
  }
  [self requestSlotsFlush];
}

// One slot table per main run loop turn, and only when it, the grid cell width or the list width changed.
- (void)requestSlotsFlush
{
  if (_slotsFlushPending) {
    return;
  }
  _slotsFlushPending = YES;
  __weak FLMView *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    FLMView *view = weakSelf;
    if (view == nil) {
      return;
    }
    view->_slotsFlushPending = NO;
    auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(view->_eventEmitter);
    if (emitter != nullptr) {
      const CGFloat width = view->_collectionView.bounds.size.width;
      [view->_slots flushTo:*emitter cellWidth:[view->_layout gridCellWidth] width:width > 0 ? width : -1];
    }
  });
}

#pragma mark - The store

- (flume::ListLayout::TypeOf)typeOf
{
  const int storeId = _storeId;
  const flume::ListRows rows = _rows;
  return [storeId, rows](int position) { return FLMViewType(storeId, rows, position); };
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
  [self sendRowShape];
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
  const BOOL sameRows = _everAttached && !_componentsMoved && count == _rows.dataCount;
  _componentsMoved = NO;
  _rows.dataCount = count;
  _itemCount = _rows.itemCount();
  _stickyIndices = flume::Engine::instance().stickyIndices(_storeId);
  [self configureLayoutRows];
  if (sameRows) {
    [_layout invalidateLengths];
  } else {
    [_layout resetWithCount:_itemCount typeOf:[self typeOf]];
  }
  _everAttached = YES;
  _viewability.invalidate();
  [self lendSlotCells];
  [self reloadCollection];
  [self scheduleOpening];
}

// FlatList's initialScrollIndex, or the bottom of a list anchored there; an upright list opens at its top as it is.
- (void)scheduleOpening
{
  if (!_openingArmed || _rows.dataCount == 0) {
    return;
  }
  _openingArmed = NO;
  const std::optional<flume::Landing> landing = flume::openingLanding(_initialScrollIndex, _rows.dataCount);
  FLMPendingOffset pending{FLMPendingOffset::Kind::Edge};
  if (landing && !landing->contentEnd) {
    pending = FLMPendingOffset{FLMPendingOffset::Kind::Landing, _rows.position(landing->index), landing->viewPosition};
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
  // A waiting jump and a realign belong to the store being left.
  [self dropJump];
  auto &engine = flume::Engine::instance();
  engine.detach(_storeId);
  for (int holder : _holders) {
    engine.releaseHolder(_storeId, holder);
  }
  _holders.clear();
  _listener.reset();
  [self lendSlotCells];
  _attached = NO;
}

- (void)applyListOps:(const flume::ListOps &)ops count:(int)count
{
  if (!_attached) {
    return;
  }
  // The indices belong to this data, so every path below lays out with them.
  _stickyIndices = ops.sticky;
  if (FLMMovesNoRow(ops) && count == _rows.dataCount) {
    [self configureLayoutRows];
    return;
  }
  // Keys moved between indices, so the next viewable set counts as changed, as FlatList resets its cache.
  _viewability.invalidate();
  if (count == 0) {
    _openingArmed = YES;
  }
  const flume::ListOps positions = _rows.positions(ops);
  flume::BatchUpdates batch = flume::toBatchUpdates(positions, static_cast<int>(_itemCount), _layout.reloadPending);
  const BOOL first = _rows.dataCount == 0;
  const BOOL hadEmptyRow = _rows.hasEmptyRow();
  // The store holds the new data from here on, and the layout measures the rows a batch can show from it.
  _rows.dataCount = count;
  const NSInteger itemCount = _rows.itemCount();
  // The first rows and an empty row change reset and reload, so an armed opening lands before any of them binds.
  if (batch.reload || first || _rows.hasEmptyRow() != hadEmptyRow) {
    _realign.reset();
    _itemCount = itemCount;
    [self configureLayoutRows];
    [_layout resetWithCount:itemCount typeOf:[self typeOf]];
    [self lendSlotCells];
    [self reloadCollection];
    [self scheduleOpening];
    [self retargetJump];
    return;
  }
  const flume::AnchorDecision anchor =
      [_layout prepareForOps:positions count:itemCount edges:flume::insertedAtEdges(ops, count, _inverted)];
  const std::vector<int> sticky = [self stickyPositions];
  const std::vector<int> fullSpan = [self fullSpanPositions:sticky];
  __block std::vector<int> retyped;
  void (^update)(void) = ^{
    [self->_collectionView performBatchUpdates:^{
      self->_itemCount = itemCount;
      retyped = [self->_layout applyOps:positions
                                  count:itemCount
                                 typeOf:[self typeOf]
                               fullSpan:fullSpan
                                 sticky:sticky];
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
  // While the list moves on its own an animated batch draws its rows twice, or slides them out of view under a glide.
  if (_itemAnimations && !_collectionView.isDragging && !_collectionView.isDecelerating && !_pendingTarget) {
    update();
  } else {
    [UIView performWithoutAnimation:update];
  }
  _realign.reset();
  if (anchor.outcome == flume::AnchorOutcome::ScrollToEdge) {
    _pendingTarget.reset();
    [self dropJump];
    [self scrollTo:[_layout offsetForVisualEdge:anchor.edge] animated:anchor.animated];
  }
  [self reindexSlotCells];
  [self retargetJump];
}

// Inserts and removes move rows without a rebind, so each slot's index follows its cell.
- (void)reindexSlotCells
{
  for (FLMSlotCell *cell in [_slots cells]) {
    NSIndexPath *path = [_collectionView indexPathForCell:cell];
    if (path != nil) {
      [_slots reindexCell:cell index:_rows.dataIndex(static_cast<int>(path.item))];
    }
  }
  // The look-ahead's positions now hold other keys, or the same keys at other indices.
  _aheadRange.reset();
  [self requestSlotsFlush];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
  return _attached ? _itemCount : 0;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
  const int position = static_cast<int>(indexPath.item);
  const int viewType = FLMViewType(_storeId, _rows, position);
  // Odd view types are React rows, shown by their slots.
  const BOOL react = viewType % 2 != 0;
  NSString *identifier = [NSString stringWithFormat:@"t%d", viewType];
  if (![_registered containsObject:identifier]) {
    [collectionView registerClass:react ? FLMSlotCell.class : FLMCell.class forCellWithReuseIdentifier:identifier];
    [_registered addObject:identifier];
  }
  if (react) {
    FLMSlotCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:identifier forIndexPath:indexPath];
    if (cell.owner == 0) {
      cell.owner = ++_nextHolderId;
      __weak FLMView *weakSelf = self;
      cell.onPrepareForReuse = ^(FLMSlotCell *reused) {
        [weakSelf releaseSlotCell:reused];
      };
    }
    [self bindSlotCell:cell position:position viewType:viewType];
    return cell;
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
  const CGFloat cross = [_layout bindCrossLengthForItem:indexPath.item];
  cell.stretches = !_horizontal && _layoutConfig.lanes > 1;
  if ([cell bindStore:_storeId
                index:_rows.dataIndex(position)
                width:_horizontal ? NAN : cross
               height:_horizontal ? cross : NAN
           horizontal:_horizontal]) {
    _holders.insert(cell.holderId);
  }
  return cell;
}

// A React row's cell takes its key's slot, and shows its view at once when the slot has mounted.
- (void)bindSlotCell:(FLMSlotCell *)cell position:(int)position viewType:(int)viewType
{
  if (position < 0 || position >= _rows.itemCount()) {
    return;
  }
  const flume::SlotRequest request = [self slotRequestAt:position viewType:viewType];
  // UIKit can drop a cell it prepared for this row and keep it unreused, so a new cell takes its slot over.
  for (int slot : [_slots pool].slotsFor(request.kind, request.key)) {
    FLMSlotCell *holder = [_slots cellForSlot:slot];
    if (holder != nil && holder != cell && [_slots pool].groupOf(slot) == request.group &&
        [_collectionView indexPathForCell:holder] == nil) {
      [_slots releaseCell:holder];
    }
  }
  [_slots bindCell:cell
             group:request.group
              kind:request.kind
               key:[NSString stringWithUTF8String:request.key.c_str()]
             index:request.index];
  [self applySlotSize:[_slots viewForSlot:cell.slot] position:position];
  [self requestSlotsFlush];
}

// A list component has one slot key of its own, a data row its item's.
- (flume::SlotRequest)slotRequestAt:(int)position viewType:(int)viewType
{
  const flume::SlotGroup group{viewType};
  switch (_rows.kind(position)) {
    case flume::RowKind::Header:
      return flume::SlotRequest{group, flume::kSlotHeader, "header", -1};
    case flume::RowKind::Empty:
      return flume::SlotRequest{group, flume::kSlotEmpty, "empty", -1};
    case flume::RowKind::Footer:
      return flume::SlotRequest{group, flume::kSlotFooter, "footer", -1};
    case flume::RowKind::Data:
      break;
  }
  const int index = _rows.dataIndex(position);
  return flume::SlotRequest{group, flume::kSlotRow, flume::Engine::instance().keyAt(_storeId, index), index};
}

// The length of a mounted slot of the row laid out at the row's cross length, -1 when none is.
- (CGFloat)slotLengthAt:(int)position
{
  const flume::SlotRequest request = [self slotRequestAt:position viewType:FLMViewType(_storeId, _rows, position)];
  const CGFloat cross = [_layout bindCrossLengthForItem:position];
  for (int slot : [_slots pool].slotsFor(request.kind, request.key)) {
    const CGSize size = [_slots viewForSlot:slot].layoutSize;
    if (size.width > 0 && std::fabs((_horizontal ? size.height : size.width) - cross) <= kFLMSlotCrossSlop) {
      return _horizontal ? size.width : size.height;
    }
  }
  return -1;
}

// A slot's size reaches the row that shows it, or the row its key sits at when no cell shows it.
- (void)applySlotSize:(FLMSlotView *)view position:(int)position
{
  if (view == nil || !_attached || position < 0 || position >= std::min<NSInteger>(_itemCount, _rows.itemCount())) {
    return;
  }
  const CGFloat length = [self slotLengthAt:position];
  if (length >= 0) {
    [_layout setLength:length forItem:position];
  }
}

- (int)positionOfSlot:(int)slot
{
  NSIndexPath *path = [_collectionView indexPathForCell:[_slots cellForSlot:slot]];
  if (path != nil) {
    return static_cast<int>(path.item);
  }
  const std::optional<flume::SlotEntry> entry = [_slots pool].entry(slot);
  if (entry && entry->kind == flume::kSlotHeader) {
    return _rows.header ? 0 : -1;
  }
  if (entry && entry->kind == flume::kSlotEmpty) {
    return _rows.hasEmptyRow() ? _rows.headerCount() : -1;
  }
  if (entry && entry->kind == flume::kSlotFooter) {
    return _rows.footer ? _rows.itemCount() - 1 : -1;
  }
  if (!entry || entry->index < 0 || entry->index >= _rows.dataCount ||
      flume::Engine::instance().keyAt(_storeId, entry->index) != entry->key) {
    return -1;
  }
  return _rows.position(entry->index);
}

- (void)slotViewDidResize:(FLMSlotView *)view
{
  [self applySlotSize:view position:[self positionOfSlot:view.slot]];
}

// A slot mounts complete and laid out, so its row can take its size.
- (void)slotRendered:(int)slot
{
  FLMSlotView *view = [_slots viewForSlot:slot];
  [self applySlotSize:view position:[self positionOfSlot:slot]];
  [self jumpSlotRendered:slot];
}

- (void)releaseSlotCell:(FLMSlotCell *)cell
{
  if (cell.slot != 0) {
    [_slots releaseCell:cell];
    [self requestSlotsFlush];
  }
}

// UIKit configures a reload's new cells before it lets the old ones go, so the slots wait, lent, for the new cells.
- (void)lendSlotCells
{
  [_slots detachAll];
  _reclaimSlots = YES;
  _aheadRange.reset();
  [self requestSlotsFlush];
}

// A cell UIKit kept after it left the screen can come back without a new cellForItem.
- (void)collectionView:(UICollectionView *)collectionView
       willDisplayCell:(UICollectionViewCell *)cell
    forItemAtIndexPath:(NSIndexPath *)indexPath
{
  if ([cell isKindOfClass:FLMSlotCell.class]) {
    const int position = static_cast<int>(indexPath.item);
    [self bindSlotCell:(FLMSlotCell *)cell position:position viewType:FLMViewType(_storeId, _rows, position)];
  }
}

- (void)collectionView:(UICollectionView *)collectionView
    didEndDisplayingCell:(UICollectionViewCell *)cell
      forItemAtIndexPath:(NSIndexPath *)indexPath
{
  if ([cell isKindOfClass:FLMSlotCell.class]) {
    [self releaseSlotCell:(FLMSlotCell *)cell];
  }
}

// A row's length before any cell shows it: a worklet row binds into a holder of its own, a React row reads its slot.
- (CGFloat)measureItem:(NSInteger)position
{
  auto &engine = flume::Engine::instance();
  if (!_attached || position < 0 || position >= _rows.itemCount()) {
    return -1;
  }
  const int index = _rows.dataIndex(static_cast<int>(position));
  if (index < 0 || engine.viewType(_storeId, index) % 2 != 0) {
    return [self slotLengthAt:static_cast<int>(position)];
  }
  if (_measureHolder == 0) {
    _measureHolder = ++_nextHolderId;
  }
  const CGFloat cross = [_layout bindCrossLengthForItem:position];
  const uint64_t start = FLMBindStats.enabled ? clock_gettime_nsec_np(CLOCK_UPTIME_RAW) : 0;
  if (!engine.bind(_storeId, _measureHolder, index, _horizontal ? NAN : cross,
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

// React rows show bare under UIKit's own glide, so a list that can show them scrolls to its top through the jump gate.
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView
{
  if (_attached && _rows.dataCount > 0 && [self showsReactRows]) {
    // The visual top, which is the content end when inverted.
    const BOOL end = _inverted;
    // A glide still running must not re-land on its old target while this jump waits for its rows.
    _pendingTarget.reset();
    [self scrollToTarget:FLMPendingTarget{end ? FLMPendingTarget::Kind::End : FLMPendingTarget::Kind::Start,
                                          end ? _rows.dataCount - 1 : 0, end ? 1.0 : 0.0, 0}
                animated:YES];
    return NO;
  }
  // The status bar tap's own animation ends in scrollViewDidScrollToTop:, never in a command's re-land.
  _pendingTarget.reset();
  _gliding = NO;
  [self dropJump];
  [self reclaimGlideSlots];
  return YES;
}

- (BOOL)showsReactRows
{
  return _rows.header || _rows.footer || flume::Engine::instance().hasReactKinds(_storeId);
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
  _pendingTarget.reset();
  _gliding = NO;
  [self dropJump];
  [self reclaimGlideSlots];
  // The list takes the touch, so no React handler under it may still press, as on Android.
  FLMCancelJSTouches(self);
  [self emitScrollEvents:_scrollEvents.willBeginDragging() velocity:CGPointZero];
  _viewability.interacted = true;
  [self evaluateViewability];
}

// A touch that stops a fling or a glide, or that never mode spends on the keyboard, never reaches JS, as ScrollView claims it.
- (void)listTouchedDown:(UIView *)hit
{
  BOOL stops = NO;
  if (_collectionView.scrollEnabled) {
    stops = _collectionView.isDecelerating || _gliding;
    // The touch stops a glide in place, so neither the re-land nor the realign moves the list to the target.
    if (_gliding) {
      _pendingTarget.reset();
      _realign.reset();
    }
    _gliding = NO;
  }
  if (stops || [self swallowsTapsOn:hit]) {
    FLMWithholdJSTouch(self);
  }
}

#pragma mark - List events

// After every layout pass, scroll frames included.
- (void)listDidLayout
{
  // The reload's cells have bound by now, so the lent slots none took become released ones.
  if (_reclaimSlots && _attached) {
    _reclaimSlots = NO;
    [_slots pool].reclaim();
    [self requestSlotsFlush];
  }
  [self renderAhead];
  [self sizeEmptyRow];
  [self evaluateViewability];
  [self evaluateEdges];
  if (_realign) {
    __weak FLMView *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf realignIfIdle];
    });
  }
  // A press starts only at rest, so the shadow tree learns where React rows show only then.
  const BOOL moving = _collectionView.isDragging || _collectionView.isDecelerating || _pendingTarget.has_value() || _gliding;
  if (!moving || !_collectionView.scrollEnabled) {
    [self writeShadowOrigins];
  }
}

// React rows within drawDistance past each visible edge render ahead, so a fling never reaches one still rendering.
- (void)renderAhead
{
  const FLMViewport viewport = [_layout viewportFor:_collectionView.bounds];
  if (!_attached || viewport.length <= 0) {
    return;
  }
  // A waiting jump owns the rows it will show, and its landing is not where the list rests.
  if (_jumps.pending() || _readyJump) {
    return;
  }
  const double drawDistance = static_cast<const FlumeViewProps &>(*_props).drawDistance;
  const CGFloat distance = drawDistance < 0 ? viewport.length : drawDistance;
  const CGFloat end = viewport.start + viewport.length;
  const std::vector<int> window = [_layout itemsFrom:viewport.start - distance to:end + distance];
  const auto bounds = std::minmax_element(window.begin(), window.end());
  const std::pair<int, int> range =
      window.empty() ? std::make_pair(-1, -1) : std::make_pair(*bounds.first, *bounds.second);
  if (_aheadRange == range) {
    return;
  }
  _aheadRange = range;
  const std::vector<int> visible = [_layout itemsFrom:viewport.start to:end];
  const auto shown = std::minmax_element(visible.begin(), visible.end());
  std::vector<flume::SlotRequest> rows;
  const int count = static_cast<int>(std::min<NSInteger>(_itemCount, _rows.itemCount()));
  for (int position : window) {
    const BOOL onScreen = !visible.empty() && position >= *shown.first && position <= *shown.second;
    const int viewType = position < count && !onScreen ? FLMViewType(_storeId, _rows, position) : 0;
    if (viewType % 2 != 0) {
      rows.push_back([self slotRequestAt:position viewType:viewType]);
    }
  }
  // A sticky row pins from far outside the window, so the rows that pin a drawDistance either way render ahead too.
  for (const CGFloat edge : {viewport.start - distance, viewport.start + distance}) {
    const int pinned = [_layout pinnedIndexAt:edge];
    const int viewType = pinned >= 0 && pinned < count ? FLMViewType(_storeId, _rows, pinned) : 0;
    if (viewType % 2 != 0) {
      rows.push_back([self slotRequestAt:pinned viewType:viewType]);
    }
  }
  [_slots pool].keepAhead(rows);
  if ([_slots pool].dirty()) {
    [self requestSlotsFlush];
  }
}

// The empty row fills what the header and footer leave of the viewport, so its content centers in the visible area.
- (void)sizeEmptyRow
{
  if (!_attached) {
    return;
  }
  if (_rows.hasEmptyRow()) {
    const float length = static_cast<float>([_layout emptyMinLength]);
    for (int slot : [_slots pool].slotsFor(flume::kSlotEmpty, "empty")) {
      [_slots pool].setMinMain(slot, length);
    }
  }
  // A growing footer takes what the other rows leave, as a FlatList footer with flex: 1 under flexGrow content.
  if (_rows.footer) {
    const float length = static_cast<float>([_layout growMinLength]);
    for (int slot : [_slots pool].slotsFor(flume::kSlotFooter, "footer")) {
      [_slots pool].setMinMain(slot, length);
    }
  }
  if ([_slots pool].dirty()) {
    [self requestSlotsFlush];
  }
}

#pragma mark - Shadow tree origins

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<const FlumeViewShadowNode::ConcreteState>(state);
  if (oldState == nullptr) {
    _offsetWritten = NO;
    [self writeShadowOrigins];
  }
}

// measure() inside a React cell reads the scroll and the cell's origin from the shadow tree, as ScrollView's content does.
- (void)writeShadowOrigins
{
  NSArray<FLMSlotCell *> *cells = [_slots cells];
  if (_state == nullptr || cells.count == 0) {
    return;
  }
  const CGPoint offset = _collectionView.contentOffset;
  if (!_offsetWritten || !CGPointEqualToPoint(offset, _writtenOffset)) {
    _offsetWritten = YES;
    _writtenOffset = offset;
    const facebook::react::Point point{static_cast<Float>(offset.x), static_cast<Float>(offset.y)};
    _state->updateState([point](const FlumeViewState &old) -> StateData::Shared {
      if (old.contentOffset == point) {
        return nullptr;
      }
      auto next = std::make_shared<FlumeViewState>(old);
      next->contentOffset = point;
      return next;
    });
  }
  const CGPoint list = _collectionView.frame.origin;
  for (FLMSlotCell *cell in cells) {
    FLMSlotView *view = cell.view;
    if (view != nil && view.superview == cell.contentView) {
      [view writeOrigin:CGPointMake(list.x + cell.frame.origin.x, list.y + cell.frame.origin.y)];
    }
  }
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
  const flume::EdgeHits hits = _edges.update(distances, _rows.dataCount, static_cast<float>(viewport.length),
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
  // Only data rows are viewable items, reported at their data indices.
  std::vector<flume::ItemSpan> spans;
  for (const flume::ItemSpan &span : [_layout spansIn:viewport]) {
    const int index = _rows.dataIndex(span.index);
    if (index >= 0) {
      spans.push_back(flume::ItemSpan{index, span.start, span.end});
    }
  }
  const std::vector<int> viewable = _viewability.compute(static_cast<float>(viewport.length), spans);
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
  if (!decelerate) {
    [self writeShadowOrigins];
  }
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView *)scrollView
{
  [self emitScrollEvents:_scrollEvents.willBeginDecelerating() velocity:CGPointZero];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
  [self emitScrollEvents:_scrollEvents.didEndDecelerating() velocity:CGPointZero];
  [self writeShadowOrigins];
}

// Rows measured during the glide can move the target, so an animated command lands on it again.
- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  _gliding = NO;
  if (_pendingTarget && _rows.dataCount > 0) {
    FLMPendingTarget target = *_pendingTarget;
    target.index = *flume::clampIndex(target.index, _rows.dataCount);
    const CGFloat offset = [self offsetOf:target];
    if (std::fabs(offset - [self currentOffset]) > 0.5) {
      [_collectionView setContentOffset:[self pointAt:offset] animated:NO];
    }
  }
  _pendingTarget.reset();
  [self reclaimGlideSlots];
  [self emitScrollEvents:_scrollEvents.didEndScrollingAnimation() velocity:CGPointZero];
  [self writeShadowOrigins];
}

#pragma mark - Presses

- (void)emitPress:(NSInteger)position pressId:(NSString *)pressId longPress:(BOOL)longPress
{
  auto emitter = std::static_pointer_cast<const FlumeViewEventEmitter>(_eventEmitter);
  const int row = _rows.dataIndex(static_cast<int>(position));
  if (emitter == nullptr || row < 0) {
    return;
  }
  std::string key = flume::Engine::instance().keyAt(_storeId, row);
  std::string id = pressId.length > 0 ? std::string(pressId.UTF8String) : std::string();
  if (longPress) {
    emitter->onCellLongPress({row, key, id});
  } else {
    emitter->onCellPress({row, key, id});
  }
}

// A pressable keeps the keyboard in handled mode, a worklet row while the list has a press handler, a React row always (its slot decides in JS).
- (BOOL)pressableAt:(UITouch *)touch
{
  for (UIView *view = touch.view; view != nil && view != _collectionView; view = view.superview) {
    if ([view isKindOfClass:FLMPressableView.class] || [view isKindOfClass:FLMSlotCell.class] ||
        (_rowsPressable && [view isKindOfClass:FLMCell.class])) {
      return YES;
    }
  }
  return NO;
}

// In never mode the first tap with the keyboard up only closes it, unless it lands on a text input.
- (BOOL)swallowsTapsOn:(UIView *)hit
{
  return flume::decideTap(_keyboardTaps.mode, _keyboardTaps.keyboardUp, true, FLMIsTextInput(hit, _collectionView))
      .swallow;
}

// Worklet cells hold no inputs.
- (BOOL)collectionView:(UICollectionView *)collectionView shouldHighlightItemAtIndexPath:(NSIndexPath *)indexPath
{
  return ![self swallowsTapsOn:nil];
}

- (BOOL)collectionView:(UICollectionView *)collectionView shouldSelectItemAtIndexPath:(NSIndexPath *)indexPath
{
  return ![self swallowsTapsOn:nil];
}

// A React row presses through its own React handlers, so it never emits a row press, as on Android.
- (BOOL)reactRowAt:(NSIndexPath *)path
{
  return FLMViewType(_storeId, _rows, static_cast<int>(path.item)) % 2 != 0;
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath
{
  [collectionView deselectItemAtIndexPath:indexPath animated:NO];
  if (![self reactRowAt:indexPath]) {
    [self emitPress:indexPath.item pressId:@"" longPress:NO];
  }
}

- (void)rowLongPressed:(UILongPressGestureRecognizer *)recognizer
{
  if (recognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  NSIndexPath *path = [_collectionView indexPathForItemAtPoint:[recognizer locationInView:_collectionView]];
  if (path != nil && ![self reactRowAt:path]) {
    [self emitPress:path.item pressId:@"" longPress:YES];
  }
}

// A pressable inside the row handles its own long press.
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer shouldReceiveTouch:(UITouch *)touch
{
  for (UIView *view = touch.view; view != nil && view != _collectionView; view = view.superview) {
    // A React row emits no row press, and a recognized long press would cancel its own controls' touches.
    if ([view isKindOfClass:FLMPressableView.class] || [view isKindOfClass:FLMSlotCell.class]) {
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
    [self reclaimGlideSlots];
    return;
  }
  if (animated) {
    _scrollEvents.scrolledProgrammatically(true);
    [_collectionView setContentOffset:[self pointAt:offset] animated:YES];
    _gliding = YES;
    return;
  }
  _gliding = NO;
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
  return [_layout targetOffsetForIndex:_rows.position(target.index)
                          viewPosition:target.viewPosition
                            viewOffset:target.viewOffset];
}

// React rows paint a frame or more after their bind, so a jump first has the rows it will show rendered, as on Android.
- (void)scrollToTarget:(const FLMPendingTarget &)target animated:(BOOL)animated
{
  [self dropJump];
  const int owner = ++_nextHolderId;
  const std::unordered_set<int> waiting = [self reserveJumpTo:[self landingFor:target] animated:animated owner:owner];
  const FLMJump jump{target, static_cast<bool>(animated), owner};
  if (waiting.empty() && ![self waitsForData:target]) {
    [self runJump:jump];
    return;
  }
  _jumps.request(jump, waiting, static_cast<int64_t>(FLMNowMs()), [self waitsForData:target]);
  __weak FLMView *weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (flume::kJumpWaitMs + 1) * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
    [weakSelf jumpDue];
  });
}

// The jump owns a slot for each React row it shows that no cell holds, rendered or not, so the free cap cannot drop one.
- (std::unordered_set<int>)reserveJumpTo:(const FLMPendingTarget &)target animated:(BOOL)animated owner:(int)owner
{
  std::unordered_set<int> waiting;
  const int count = static_cast<int>(std::min<NSInteger>(_itemCount, _rows.itemCount()));
  if (!_attached || count == 0) {
    return waiting;
  }
  const bool index = target.kind == FLMPendingTarget::Kind::Index;
  const int position = index ? _rows.position(target.index) : target.kind == FLMPendingTarget::Kind::End ? count - 1 : 0;
  const double viewPosition = index ? target.viewPosition : target.kind == FLMPendingTarget::Kind::End ? 1 : 0;
  const FLMViewport viewport = [_layout viewportFor:_collectionView.bounds];
  const CGFloat current = [self currentOffset];
  const CGFloat landing = [self offsetOf:target];
  CGFloat glideStart = NAN;
  if (animated) {
    const std::optional<float> near = flume::preJumpOffset(static_cast<float>(current), static_cast<float>(landing),
                                                           static_cast<float>(viewport.length));
    glideStart = viewport.start + (near ? *near - current : 0);
  }
  const flume::IndexRange items = [_layout jumpItemsToIndex:position viewPosition:viewPosition glideStart:glideStart];
  std::vector<int> positions;
  for (int at = items.first; at <= items.last; at++) {
    positions.push_back(at);
  }
  // The sticky row that pins at the landing comes from outside the window.
  positions.push_back([_layout pinnedIndexAt:viewport.start + landing - current]);
  flume::SlotPool &pool = [_slots pool];
  for (const int at : positions) {
    const int viewType = at >= 0 && at < count ? FLMViewType(_storeId, _rows, at) : 0;
    if (viewType % 2 == 0) {
      continue;
    }
    const flume::SlotRequest request = [self slotRequestAt:at viewType:viewType];
    if ([self slotShownFor:request]) {
      continue;
    }
    const int slot = pool.acquire(owner, request.group, request.kind, request.key, request.index);
    if ([_slots viewForSlot:slot] == nil) {
      waiting.insert(slot);
    }
  }
  if (pool.dirty()) {
    [self requestSlotsFlush];
  }
  return waiting;
}

- (BOOL)slotShownFor:(const flume::SlotRequest &)request
{
  flume::SlotPool &pool = [_slots pool];
  for (const int slot : pool.slotsFor(request.kind, request.key)) {
    const std::optional<flume::SlotGroup> group = pool.groupOf(slot);
    if ((!group || *group == request.group) && [_slots cellForSlot:slot] != nil && [_slots viewForSlot:slot] != nil) {
      return YES;
    }
  }
  return NO;
}

// A slot mounts inside a Fabric mount, so the jump it opens runs a main-queue turn later, outside it.
- (void)jumpSlotRendered:(int)slot
{
  std::optional<FLMJump> jump = _jumps.rendered(slot);
  if (!jump) {
    return;
  }
  _readyJump = jump;
  __weak FLMView *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf runReadyJump];
  });
}

- (void)runReadyJump
{
  std::optional<FLMJump> jump = std::exchange(_readyJump, std::nullopt);
  if (jump) {
    [self runJump:*jump];
  }
}

- (void)jumpDue
{
  std::optional<FLMJump> jump = _jumps.due(static_cast<int64_t>(FLMNowMs()));
  if (jump) {
    jump->timedOut = true;
    [self runJump:*jump];
  }
}

- (void)runJump:(const FLMJump &)jump
{
  flume::SlotPool &pool = [_slots pool];
  pool.lend(jump.owner);
  [self requestSlotsFlush];
  if (!_attached || _rows.dataCount == 0) {
    pool.reclaim();
    return;
  }
  if (jump.animated) {
    _reclaimAfterGlide = YES;
  } else {
    _reclaimSlots = YES;
  }
  const FLMPendingTarget landing = [self landingFor:jump.target];
  _realign.reset();
  if (jump.timedOut) {
    _realign = landing;
    _realigns = 0;
  }
  [self startScrollToTarget:landing animated:jump.animated];
}

// The target on the rows there are now: an index past the data lands on the last item, the end on the new end.
- (FLMPendingTarget)landingFor:(const FLMPendingTarget &)target
{
  FLMPendingTarget landing = target;
  if (target.kind == FLMPendingTarget::Kind::End) {
    landing.index = std::max(0, _rows.dataCount - 1);
  } else if (target.kind == FLMPendingTarget::Kind::Index) {
    landing.index = flume::clampIndex(target.index, _rows.dataCount).value_or(0);
  }
  return landing;
}

// An index past the data waits for the data to grow, as when new data and the scroll come from one handler.
- (BOOL)waitsForData:(const FLMPendingTarget &)target
{
  return target.kind == FLMPendingTarget::Kind::Index && target.index >= _rows.dataCount;
}

// New data moves a waiting jump's target, so it waits for the new rows it shows, before the same deadline.
- (void)retargetJump
{
  if (!_jumps.pending()) {
    return;
  }
  FLMJump jump = *_jumps.pending();
  flume::SlotPool &pool = [_slots pool];
  pool.lend(jump.owner);
  pool.reclaim();
  jump.owner = ++_nextHolderId;
  std::unordered_set<int> waiting = [self reserveJumpTo:[self landingFor:jump.target]
                                               animated:jump.animated
                                                  owner:jump.owner];
  const BOOL waitsForData = [self waitsForData:jump.target];
  const BOOL ready = waiting.empty() && !waitsForData;
  _jumps.retarget(jump, std::move(waiting), waitsForData);
  [self requestSlotsFlush];
  if (ready) {
    _readyJump = _jumps.cancel();
    __weak FLMView *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf runReadyJump];
    });
  }
}

// A drag, another command, a reload or leaving the window ends a waiting jump and gives its slots back.
- (void)dropJump
{
  _realign.reset();
  std::optional<FLMJump> jump = _jumps.cancel();
  if (!jump) {
    jump = std::exchange(_readyJump, std::nullopt);
  }
  if (jump) {
    [_slots pool].lend(jump->owner);
    [_slots pool].reclaim();
    [self requestSlotsFlush];
  }
}

// After the next layout pass, so the rows the glide ends on bind their lent slots first.
- (void)reclaimGlideSlots
{
  if (_reclaimAfterGlide) {
    _reclaimAfterGlide = NO;
    _reclaimSlots = YES;
    [_collectionView setNeedsLayout];
  }
}

// While the list rests, a jump that ran before its rows rendered goes back to its target until the rows in view render.
- (void)realignIfIdle
{
  if (!_realign || _collectionView.isDragging || _collectionView.isDecelerating || _gliding || _pendingTarget) {
    return;
  }
  const CGFloat offset = [self offsetOf:*_realign];
  if (std::fabs(offset - [self currentOffset]) > 0.5) {
    if (_realigns >= kFLMMaxRealigns) {
      _realign.reset();
    } else {
      _realigns++;
      [_collectionView setContentOffset:[self pointAt:offset] animated:NO];
    }
    return;
  }
  if ([self visibleSlotCellsShow]) {
    _realign.reset();
  }
}

- (BOOL)visibleSlotCellsShow
{
  for (UICollectionViewCell *cell in _collectionView.visibleCells) {
    if ([cell isKindOfClass:[FLMSlotCell class]] && ((FLMSlotCell *)cell).view == nil) {
      return NO;
    }
  }
  return YES;
}

// Past two viewports an animated scroll jumps first, so it glides over measured rows only.
- (void)startScrollToTarget:(const FLMPendingTarget &)target animated:(BOOL)animated
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
  [_layout measureGlideToIndex:_rows.position(target.index) viewPosition:target.viewPosition];
  const CGFloat offset = [self offsetOf:target];
  _pendingTarget = target;
  [self scrollTo:offset animated:YES];
}

- (void)scrollToIndex:(NSInteger)index animated:(BOOL)animated viewPosition:(double)viewPosition viewOffset:(double)viewOffset
{
  if (!_attached) {
    return;
  }
  [self scrollToTarget:FLMPendingTarget{FLMPendingTarget::Kind::Index, static_cast<int>(index), viewPosition, viewOffset}
              animated:animated];
}

// The content start lands on measured rows; further offsets stay on the estimates.
- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
  if (!_attached || _rows.dataCount == 0) {
    return;
  }
  [self dropJump];
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
  if (!_attached || _rows.dataCount == 0) {
    return;
  }
  [self scrollToTarget:FLMPendingTarget{FLMPendingTarget::Kind::End, _rows.dataCount - 1, 1, 0}
              animated:animated];
}

- (void)setNativeRefreshing:(BOOL)refreshing
{
  [_refresh setRefreshing:refreshing];
}

@end
