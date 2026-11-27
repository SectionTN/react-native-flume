import type * as React from 'react';
import type { CodegenTypes, ColorValue, HostComponent, ViewProps } from 'react-native';
import { codegenNativeCommands, codegenNativeComponent } from 'react-native';

// A press on a row: the pressable id is '' for a press on the cell outside any pressable.
export type PressEvent = Readonly<{
  index: CodegenTypes.Int32;
  key: string;
  id: string;
}>;

// The ScrollView shape in dp, so Reanimated's useAnimatedScrollHandler reads it unchanged.
export type ScrollEvent = Readonly<{
  contentOffset: Readonly<{ x: CodegenTypes.Double; y: CodegenTypes.Double }>;
  contentSize: Readonly<{ width: CodegenTypes.Double; height: CodegenTypes.Double }>;
  layoutMeasurement: Readonly<{ width: CodegenTypes.Double; height: CodegenTypes.Double }>;
  contentInset: Readonly<{
    top: CodegenTypes.Double;
    left: CodegenTypes.Double;
    bottom: CodegenTypes.Double;
    right: CodegenTypes.Double;
  }>;
  velocity: Readonly<{ x: CodegenTypes.Double; y: CodegenTypes.Double }>;
}>;

export type ViewabilityEvent = Readonly<{
  viewable: { index: CodegenTypes.Int32; key: string; isViewable: boolean }[];
  changed: { index: CodegenTypes.Int32; key: string; isViewable: boolean }[];
}>;

// The whole slot table, since Fabric replaces a queued event of the same type. cellWidth is -1 off a grid, width
// (the list's) -1 before layout.
export type SlotsEvent = Readonly<{
  slots: {
    slot: CodegenTypes.Int32;
    kind: CodegenTypes.Int32;
    key: string;
    index: CodegenTypes.Int32;
    bound: boolean;
    minMain: CodegenTypes.Double;
  }[];
  jsSeen: CodegenTypes.Int32;
  cellWidth: CodegenTypes.Double;
  width: CodegenTypes.Double;
}>;

export interface NativeProps extends ViewProps {
  storeId?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
  itemAnimations?: CodegenTypes.WithDefault<boolean, true>;
  horizontal?: CodegenTypes.WithDefault<boolean, false>;
  numColumns?: CodegenTypes.WithDefault<CodegenTypes.Int32, 1>;
  inverted?: CodegenTypes.WithDefault<boolean, false>;
  contentPaddingTop?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  contentPaddingLeft?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  contentPaddingBottom?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  contentPaddingRight?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  // Fractions of the list width, added to the padding in dp above.
  paddingPercentTop?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  paddingPercentLeft?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  paddingPercentBottom?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  paddingPercentRight?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  // With contentContainerStyle flexGrow: where short content sits, and whether the footer takes the slack.
  contentJustify?: CodegenTypes.WithDefault<'start' | 'center' | 'end', 'start'>;
  growFooter?: CodegenTypes.WithDefault<boolean, false>;
  // Paints the content and its padding, and scrolls with it.
  contentBackgroundColor?: ColorValue;
  // In dp: between every pair of rows, then between grid rows, then between columns.
  mainGap?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  gridRowGap?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  crossGap?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  // A grid's data rows, inside the content padding across the scroll axis.
  laneInsetStart?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  laneInsetEnd?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  estimatedItemSize?: CodegenTypes.WithDefault<CodegenTypes.Double, -1>;
  listHeader?: CodegenTypes.WithDefault<boolean, false>;
  listFooter?: CodegenTypes.WithDefault<boolean, false>;
  listEmpty?: CodegenTypes.WithDefault<boolean, false>;
  // In dp; -1 means one viewport.
  drawDistance?: CodegenTypes.WithDefault<CodegenTypes.Double, -1>;
  initialScrollIndex?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  separatorSize?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  separatorColor?: ColorValue;
  separatorInsetStart?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  separatorInsetEnd?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  scrollEventThrottle?: CodegenTypes.WithDefault<CodegenTypes.Double, 16>;
  // Scroll events reach JS only when a handler is set; drag and momentum events are per gesture.
  emitScrollEvents?: CodegenTypes.WithDefault<boolean, false>;
  emitViewabilityEvents?: CodegenTypes.WithDefault<boolean, false>;
  // In viewports, as FlatList counts them.
  endReachedThreshold?: CodegenTypes.WithDefault<CodegenTypes.Double, 2>;
  startReachedThreshold?: CodegenTypes.WithDefault<CodegenTypes.Double, 2>;
  refreshEnabled?: CodegenTypes.WithDefault<boolean, false>;
  refreshing?: CodegenTypes.WithDefault<boolean, false>;
  refreshColors?: ReadonlyArray<ColorValue>;
  refreshProgressViewOffset?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  scrollEnabled?: CodegenTypes.WithDefault<boolean, true>;
  nestedScrollEnabled?: CodegenTypes.WithDefault<boolean, false>;
  overScrollMode?: CodegenTypes.WithDefault<'auto' | 'always' | 'never', 'auto'>;
  // React Native's numbers: normal is 0.998, fast 0.99; Android scales the fling velocity from it.
  decelerationRate?: CodegenTypes.WithDefault<CodegenTypes.Double, 0.998>;
  showsVerticalScrollIndicator?: CodegenTypes.WithDefault<boolean, true>;
  showsHorizontalScrollIndicator?: CodegenTypes.WithDefault<boolean, true>;
  // iOS only: the UIScrollView props of React Native's ScrollView, with its defaults.
  contentInset?: Readonly<{
    top?: CodegenTypes.Double;
    left?: CodegenTypes.Double;
    bottom?: CodegenTypes.Double;
    right?: CodegenTypes.Double;
  }>;
  scrollIndicatorInsets?: Readonly<{
    top?: CodegenTypes.Double;
    left?: CodegenTypes.Double;
    bottom?: CodegenTypes.Double;
    right?: CodegenTypes.Double;
  }>;
  contentInsetAdjustmentBehavior?: CodegenTypes.WithDefault<
    'automatic' | 'scrollableAxes' | 'never' | 'always',
    'never'
  >;
  bounces?: CodegenTypes.WithDefault<boolean, true>;
  alwaysBounceVertical?: CodegenTypes.WithDefault<boolean, true>;
  refreshTintColor?: ColorValue;
  keyboardDismissMode?: CodegenTypes.WithDefault<'none' | 'on-drag' | 'interactive', 'none'>;
  keyboardShouldPersistTaps?: CodegenTypes.WithDefault<'always' | 'never' | 'handled', 'never'>;
  // Rows are clickable only when the list has a press handler.
  rowsPressable?: CodegenTypes.WithDefault<boolean, false>;
  maintainVisibleContentPosition?: Readonly<{
    disabled?: boolean;
    autoscrollToTopThreshold?: CodegenTypes.Double;
    autoscrollToBottomThreshold?: CodegenTypes.Double;
    animateAutoScrollToBottom?: boolean;
    startRenderingFromBottom?: boolean;
  }>;
  viewabilityConfig?: Readonly<{
    itemVisiblePercentThreshold?: CodegenTypes.Double;
    viewAreaCoveragePercentThreshold?: CodegenTypes.Double;
    minimumViewTime?: CodegenTypes.Double;
    waitForInteraction?: boolean;
  }>;
  onCellPress?: CodegenTypes.DirectEventHandler<PressEvent>;
  onCellLongPress?: CodegenTypes.DirectEventHandler<PressEvent>;
  onScroll?: CodegenTypes.DirectEventHandler<ScrollEvent>;
  onScrollBeginDrag?: CodegenTypes.DirectEventHandler<ScrollEvent>;
  onScrollEndDrag?: CodegenTypes.DirectEventHandler<ScrollEvent>;
  onMomentumScrollBegin?: CodegenTypes.DirectEventHandler<ScrollEvent>;
  onMomentumScrollEnd?: CodegenTypes.DirectEventHandler<ScrollEvent>;
  onViewableItemsChanged?: CodegenTypes.DirectEventHandler<ViewabilityEvent>;
  onEndReached?: CodegenTypes.DirectEventHandler<
    Readonly<{ distanceFromEnd: CodegenTypes.Double }>
  >;
  onStartReached?: CodegenTypes.DirectEventHandler<
    Readonly<{ distanceFromStart: CodegenTypes.Double }>
  >;
  onRefresh?: CodegenTypes.DirectEventHandler<null>;
  onSlotsChanged?: CodegenTypes.DirectEventHandler<SlotsEvent>;
  // Every scroll command up to id has ended.
  onScrollCommandEnd?: CodegenTypes.DirectEventHandler<Readonly<{ id: CodegenTypes.Int32 }>>;
}

type FlumeViewType = HostComponent<NativeProps>;

// viewPosition is in content order and offsets are in dp. requestId grows per command and comes back in onScrollCommandEnd.
interface NativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<FlumeViewType>,
    requestId: CodegenTypes.Int32,
    index: CodegenTypes.Int32,
    animated: boolean,
    viewPosition: CodegenTypes.Double,
    viewOffset: CodegenTypes.Double,
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<FlumeViewType>,
    requestId: CodegenTypes.Int32,
    offset: CodegenTypes.Double,
    animated: boolean,
  ) => void;
  scrollToEnd: (
    viewRef: React.ElementRef<FlumeViewType>,
    requestId: CodegenTypes.Int32,
    animated: boolean,
  ) => void;
  // Scrolls only when the row is not fully in view, to the edge it is past.
  scrollIndexIntoView: (
    viewRef: React.ElementRef<FlumeViewType>,
    requestId: CodegenTypes.Int32,
    index: CodegenTypes.Int32,
    animated: boolean,
    viewOffset: CodegenTypes.Double,
  ) => void;
  // ScrollView's names and arguments, so Reanimated's scrollTo reaches the list. The offset is along the list's axis.
  scrollTo: (
    viewRef: React.ElementRef<FlumeViewType>,
    x: CodegenTypes.Double,
    y: CodegenTypes.Double,
    animated: boolean,
  ) => void;
  flashScrollIndicators: (viewRef: React.ElementRef<FlumeViewType>) => void;
  // A pull starts refreshing natively; the wrapper sends the controlled prop back when it did not change.
  setNativeRefreshing: (viewRef: React.ElementRef<FlumeViewType>, refreshing: boolean) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'scrollToIndex',
    'scrollToOffset',
    'scrollToEnd',
    'scrollIndexIntoView',
    'scrollTo',
    'flashScrollIndicators',
    'setNativeRefreshing',
  ],
});

export default codegenNativeComponent<NativeProps>('FlumeView', {
  interfaceOnly: true,
}) as FlumeViewType;
