import type { ComponentType, ReactElement, ReactNode } from 'react';
import type {
  ColorValue,
  Insets,
  LayoutChangeEvent,
  NativeScrollEvent,
  NativeSyntheticEvent,
  RefreshControlProps,
  StyleProp,
  ViewProps,
  ViewStyle,
} from 'react-native';
import type { IgnoredProps } from './foreign';

export type CellDimension = number | `${number}%`;
// Yoga takes auto on these keys only, as React Native does.
export type CellAutoDimension = CellDimension | 'auto';

export type CellFontWeight =
  | 'normal'
  | 'bold'
  | '100'
  | '200'
  | '300'
  | '400'
  | '500'
  | '600'
  | '700'
  | '800'
  | '900'
  | 100
  | 200
  | 300
  | 400
  | 500
  | 600
  | 700
  | 800
  | 900;

// Every key is the React Native name with the React Native meaning, in dp.
export type CellStyle = {
  flex?: number;
  flexDirection?: 'row' | 'column' | 'row-reverse' | 'column-reverse';
  flexWrap?: 'wrap' | 'nowrap' | 'wrap-reverse';
  justifyContent?:
    | 'flex-start'
    | 'flex-end'
    | 'center'
    | 'space-between'
    | 'space-around'
    | 'space-evenly';
  alignItems?: 'flex-start' | 'flex-end' | 'center' | 'stretch' | 'baseline';
  alignSelf?: 'auto' | 'flex-start' | 'flex-end' | 'center' | 'stretch' | 'baseline';
  alignContent?:
    | 'flex-start'
    | 'flex-end'
    | 'center'
    | 'stretch'
    | 'space-between'
    | 'space-around'
    | 'space-evenly';
  flexGrow?: number;
  flexShrink?: number;
  flexBasis?: CellAutoDimension;
  width?: CellAutoDimension;
  height?: CellAutoDimension;
  minWidth?: CellDimension;
  minHeight?: CellDimension;
  maxWidth?: CellDimension;
  maxHeight?: CellDimension;
  aspectRatio?: number;
  margin?: CellAutoDimension;
  marginTop?: CellAutoDimension;
  marginBottom?: CellAutoDimension;
  marginLeft?: CellAutoDimension;
  marginRight?: CellAutoDimension;
  marginStart?: CellAutoDimension;
  marginEnd?: CellAutoDimension;
  marginHorizontal?: CellAutoDimension;
  marginVertical?: CellAutoDimension;
  padding?: CellDimension;
  paddingTop?: CellDimension;
  paddingBottom?: CellDimension;
  paddingLeft?: CellDimension;
  paddingRight?: CellDimension;
  paddingStart?: CellDimension;
  paddingEnd?: CellDimension;
  paddingHorizontal?: CellDimension;
  paddingVertical?: CellDimension;
  gap?: number;
  rowGap?: number;
  columnGap?: number;
  position?: 'absolute' | 'relative';
  top?: CellDimension;
  left?: CellDimension;
  right?: CellDimension;
  bottom?: CellDimension;
  start?: CellDimension;
  end?: CellDimension;
  display?: 'flex' | 'none';
  overflow?: 'visible' | 'hidden';
  direction?: 'inherit' | 'ltr' | 'rtl';
  backgroundColor?: string;
  opacity?: number;
  borderRadius?: number;
  borderTopLeftRadius?: number;
  borderTopRightRadius?: number;
  borderBottomLeftRadius?: number;
  borderBottomRightRadius?: number;
  borderWidth?: number;
  borderColor?: string;
  shadowColor?: string;
  shadowOpacity?: number;
  shadowRadius?: number;
  shadowOffset?: { width: number; height: number };
  elevation?: number;
  color?: string;
  fontSize?: number;
  fontWeight?: CellFontWeight;
  fontFamily?: string;
  fontStyle?: 'normal' | 'italic';
  lineHeight?: number;
  letterSpacing?: number;
  textAlign?: 'auto' | 'left' | 'right' | 'center' | 'justify';
  textDecorationLine?: 'none' | 'underline' | 'line-through' | 'underline line-through';
  numberOfLines?: number;
  ellipsizeMode?: 'tail';
};

export type CellImageSource = string | { uri: string; headers?: Record<string, string> };

export type CellNode = {
  type: 'view' | 'text' | 'image' | 'pressable';
  style?: CellStyle;
  children?: CellNode[];
  text?: string;
  allowFontScaling?: boolean;
  source?: CellImageSource;
  resizeMode?: 'cover' | 'contain' | 'stretch' | 'center';
  tintColor?: string;
  id?: string;
  pressedOpacity?: number;
  key?: string;
  testID?: string;
  accessibilityLabel?: string;
  accessible?: boolean;
};

export type CellTextOptions = { allowFontScaling?: boolean };

export type CellImageProps = CellStyle & {
  source: CellImageSource;
  resizeMode?: CellNode['resizeMode'];
  tintColor?: string;
};

export type CellPressableProps = CellStyle & { id: string; pressedOpacity?: number };

export type PressInfo<T> = { item: T; index: number; id?: string };

export type ViewToken<T> = { item: T; key: string; index: number; isViewable: boolean };

export type ViewabilityConfig = {
  itemVisiblePercentThreshold?: number;
  viewAreaCoveragePercentThreshold?: number;
  minimumViewTime?: number;
  waitForInteraction?: boolean;
};

// scrollToIndex is exact for worklet rows; the last two members serve Animated and Reanimated, which read them by name.
export type FlumeRef = {
  scrollToIndex(params: {
    index: number;
    animated?: boolean;
    viewPosition?: number;
    viewOffset?: number;
  }): void;
  scrollToOffset(params: { offset: number; animated?: boolean }): void;
  scrollToEnd(params?: { animated?: boolean }): void;
  getScrollableNode(): unknown;
  getAnimatableRef(): unknown;
};

// FlashList v2 shape, on by default. The top threshold is in points, the bottom one in viewports.
export type MaintainVisibleContentPosition = {
  disabled?: boolean;
  autoscrollToTopThreshold?: number;
  autoscrollToBottomThreshold?: number;
  animateAutoScrollToBottom?: boolean;
  startRenderingFromBottom?: boolean;
};

// FlatList's shape. Flume anchors on the first visible row whatever minIndexForVisible says.
export type FlatListAnchoring = {
  minIndexForVisible: number;
  autoscrollToTopThreshold?: number | null;
};

// LegendList's shape. Flume has one switch for both, and does not call shouldRestorePosition.
export type LegendListAnchoring<T> = {
  data?: boolean;
  size?: boolean;
  shouldRestorePosition?: (item: T, index: number, data: readonly T[]) => boolean;
};

export type ScrollHandler = (event: NativeSyntheticEvent<NativeScrollEvent>) => void;

// FlashList's render info; a pinned copy of a sticky row renders with the StickyHeader target.
export type CellInfo<T> = {
  item: T;
  index: number;
  target: 'Cell' | 'StickyHeader';
  extraData?: unknown;
  // LegendList's fields: the data of the render that last drew the row, and the item's type.
  data: readonly T[];
  type: string;
};

export type ReactCell<T> = (info: CellInfo<T>) => ReactNode;

// What ItemSeparatorComponent gets: FlatList's props and FlashList's trailingItem.
export type SeparatorProps<T> = {
  leadingItem: T;
  trailingItem: T | undefined;
  highlighted: boolean;
} & Record<string, unknown>;

export type FlumeOwnProps<T> = {
  data: readonly T[] | null | undefined;
  renderCell?: ((item: T, index: number) => CellNode) | null;
  // The React renderer for every type without a worklet cell or a reactCells entry.
  renderItem?: ReactCell<T> | null;
  // React renderers by item type; they take precedence over renderCell.
  reactCells?: Record<string, ReactCell<T>>;
  keyExtractor?: ((item: T, index: number) => string) | null;
  getItemType?: ((item: T, index: number) => string | number | null | undefined) | null;
  extraData?: unknown;
  itemAnimations?: boolean;
  onPress?: ((info: PressInfo<T>) => void) | null;
  onLongPress?: ((info: PressInfo<T>) => void) | null;
  style?: StyleProp<ViewStyle>;
  horizontal?: boolean | null;
  numColumns?: number;
  inverted?: boolean | null;
  // Padding only, as the spec says; other keys warn in development.
  contentContainerStyle?: StyleProp<ViewStyle>;
  // React cells only: the length in dp a cell takes until it measures; else the mean of its type, at least 50.
  estimatedItemSize?: number;
  // How far past the viewport, in dp, cells are bound ahead; the default is one viewport.
  drawDistance?: number;
  // LegendList's object form opens at its index. Its offsets are not read yet.
  initialScrollIndex?:
    | number
    | { index: number; viewOffset?: number; viewPosition?: number }
    | null;
  // A native line between consecutive rows, inset along the cross axis from the start and end edges.
  separator?: { size: number; color?: string; insetStart?: number; insetEnd?: number };
  // Drawn inside a React row's cell after the row, before it when inverted; none in the last grid row.
  ItemSeparatorComponent?: ComponentType<SeparatorProps<T>> | ReactElement | null;
  // Data indices, as FlashList counts them; FlatList counts the header too.
  stickyHeaderIndices?: readonly number[] | null;
  // Rows around the data; the empty one shows instead of the data while it is empty, filling the viewport.
  ListHeaderComponent?: ComponentType<object> | ReactElement | null;
  ListFooterComponent?: ComponentType<object> | ReactElement | null;
  ListEmptyComponent?: ComponentType<object> | ReactElement | null;
  // Each wraps its component in a View with this style.
  ListHeaderComponentStyle?: StyleProp<ViewStyle> | null;
  ListFooterComponentStyle?: StyleProp<ViewStyle> | null;
  ListEmptyComponentStyle?: StyleProp<ViewStyle> | null;
  // The ScrollView events with the ScrollView payload; contentOffset counts from the content start.
  onScroll?: ScrollHandler | null;
  onScrollBeginDrag?: ScrollHandler | null;
  onScrollEndDrag?: ScrollHandler | null;
  onMomentumScrollBegin?: ScrollHandler | null;
  onMomentumScrollEnd?: ScrollHandler | null;
  scrollEventThrottle?: number;
  onViewableItemsChanged?:
    | ((info: { viewableItems: ViewToken<T>[]; changed: ViewToken<T>[] }) => void)
    | null;
  viewabilityConfig?: ViewabilityConfig | null;
  onEndReached?: ((info: { distanceFromEnd: number }) => void) | null;
  onEndReachedThreshold?: number | null;
  onStartReached?: ((info: { distanceFromStart: number }) => void) | null;
  onStartReachedThreshold?: number | null;
  refreshing?: boolean | null;
  onRefresh?: (() => void) | null;
  // A RefreshControl element, or the spinner style alone (tintColor on iOS, colors on Android).
  refreshControl?:
    | ReactElement<RefreshControlProps>
    | { tintColor?: ColorValue; colors?: ReadonlyArray<ColorValue>; progressViewOffset?: number }
    | null;
  scrollEnabled?: boolean;
  nestedScrollEnabled?: boolean;
  overScrollMode?: 'auto' | 'always' | 'never';
  decelerationRate?: 'normal' | 'fast' | number;
  showsVerticalScrollIndicator?: boolean;
  showsHorizontalScrollIndicator?: boolean;
  // iOS only, as on ScrollView.
  contentInset?: Insets;
  scrollIndicatorInsets?: Insets;
  contentInsetAdjustmentBehavior?: 'automatic' | 'scrollableAxes' | 'never' | 'always';
  bounces?: boolean;
  alwaysBounceVertical?: boolean;
  // interactive is iOS only.
  keyboardDismissMode?: 'none' | 'on-drag' | 'interactive';
  keyboardShouldPersistTaps?: 'always' | 'never' | 'handled';
  // false turns anchoring off. FlatList and LegendList objects map onto the same switch and top threshold.
  maintainVisibleContentPosition?:
    | boolean
    | MaintainVisibleContentPosition
    | FlatListAnchoring
    | LegendListAnchoring<T>
    | null;
  onLayout?: ((event: LayoutChangeEvent) => void) | null;
  testID?: string;
  accessibilityLabel?: string;
};

export type FlumeProps<T> = FlumeOwnProps<T> &
  IgnoredProps<T> &
  Omit<ViewProps, keyof FlumeOwnProps<T> | keyof IgnoredProps<T> | 'children'>;

export type ListRenderItem<T> = ReactCell<T>;
export type ListRenderItemInfo<T> = CellInfo<T>;
