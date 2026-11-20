import { isValidElement } from 'react';
import type { ColorValue, RefreshControlProps, StyleProp, ViewStyle } from 'react-native';
import type { NativeProps } from './specs/FlumeViewNativeComponent';
import type { FlumeProps, ViewabilityConfig } from './types';

export type Warn = (message: string) => void;

export type ContentPadding = {
  top: number;
  left: number;
  bottom: number;
  right: number;
  unsupported: string[];
};

// The flat props the native component takes, built from the public ones; the host props travel on their own.
export type FlatProps = Pick<
  NativeProps,
  | 'horizontal'
  | 'numColumns'
  | 'inverted'
  | 'contentPaddingTop'
  | 'contentPaddingLeft'
  | 'contentPaddingBottom'
  | 'contentPaddingRight'
  | 'estimatedItemSize'
  | 'listHeader'
  | 'listFooter'
  | 'listEmpty'
  | 'drawDistance'
  | 'initialScrollIndex'
  | 'separatorSize'
  | 'separatorColor'
  | 'separatorInsetStart'
  | 'separatorInsetEnd'
  | 'scrollEventThrottle'
  | 'emitScrollEvents'
  | 'emitViewabilityEvents'
  | 'viewabilityConfig'
  | 'endReachedThreshold'
  | 'startReachedThreshold'
  | 'refreshEnabled'
  | 'refreshing'
  | 'refreshColors'
  | 'refreshProgressViewOffset'
  | 'scrollEnabled'
  | 'nestedScrollEnabled'
  | 'overScrollMode'
  | 'decelerationRate'
  | 'showsVerticalScrollIndicator'
  | 'showsHorizontalScrollIndicator'
  | 'contentInset'
  | 'scrollIndicatorInsets'
  | 'contentInsetAdjustmentBehavior'
  | 'bounces'
  | 'alwaysBounceVertical'
  | 'refreshTintColor'
  | 'keyboardDismissMode'
  | 'keyboardShouldPersistTaps'
  | 'rowsPressable'
  | 'maintainVisibleContentPosition'
>;

const PADDING_KEYS = new Set([
  'padding',
  'paddingVertical',
  'paddingHorizontal',
  'paddingTop',
  'paddingBottom',
  'paddingLeft',
  'paddingRight',
  'paddingStart',
  'paddingEnd',
]);

function flatten(style: StyleProp<ViewStyle>, into: Record<string, unknown>): void {
  if (!style) return;
  if (Array.isArray(style)) {
    for (const entry of style) flatten(entry as StyleProp<ViewStyle>, into);
    return;
  }
  if (typeof style === 'object') Object.assign(into, style);
}

// Shorthands resolve as in React Native: padding, then the axis pairs, then each side; start and end map to left and right.
export function contentPadding(style: StyleProp<ViewStyle> | undefined): ContentPadding {
  const flat: Record<string, unknown> = {};
  flatten(style, flat);
  const unsupported: string[] = [];
  const number = (key: string): number | undefined => {
    const value = flat[key];
    if (value === undefined || value === null) return undefined;
    if (typeof value === 'number') return value;
    unsupported.push(key);
    return undefined;
  };
  for (const key of Object.keys(flat)) {
    if (!PADDING_KEYS.has(key)) unsupported.push(key);
  }
  const all = number('padding') ?? 0;
  const vertical = number('paddingVertical') ?? all;
  const horizontal = number('paddingHorizontal') ?? all;
  return {
    top: number('paddingTop') ?? vertical,
    bottom: number('paddingBottom') ?? vertical,
    left: number('paddingStart') ?? number('paddingLeft') ?? horizontal,
    right: number('paddingEnd') ?? number('paddingRight') ?? horizontal,
    unsupported,
  };
}

// FlatList takes exactly one threshold; with both, the item one wins and development warns.
function viewability(
  config: ViewabilityConfig | undefined,
  warn: Warn,
): ViewabilityConfig | undefined {
  if (config === undefined) return undefined;
  if (
    config.itemVisiblePercentThreshold === undefined ||
    config.viewAreaCoveragePercentThreshold === undefined
  )
    return config;
  warn(
    'viewabilityConfig takes exactly one of itemVisiblePercentThreshold and viewAreaCoveragePercentThreshold; using itemVisiblePercentThreshold',
  );
  const { viewAreaCoveragePercentThreshold: _ignored, ...rest } = config;
  return rest;
}

// Every field the three anchoring shapes can carry. The others are ignored.
type AnchoringFields = {
  disabled?: boolean;
  autoscrollToTopThreshold?: number | null;
  autoscrollToBottomThreshold?: number;
  animateAutoScrollToBottom?: boolean;
  startRenderingFromBottom?: boolean;
  data?: boolean;
  size?: boolean;
};

// Codegen structs read an unset member as 0 on iOS, so every field goes out, and -1 means no threshold.
function anchoring<T>(
  given: FlumeProps<T>['maintainVisibleContentPosition'],
  inverted: boolean,
): NonNullable<FlatProps['maintainVisibleContentPosition']> {
  const config: AnchoringFields = typeof given === 'object' && given !== null ? given : {};
  // FlatList measures its top threshold from the content start, the visual bottom of an inverted list.
  const flatListInverted = inverted && 'minIndexForVisible' in config;
  return {
    disabled:
      given === false ||
      config.disabled === true ||
      (config.data === false && config.size === false),
    autoscrollToTopThreshold: flatListInverted ? -1 : (config.autoscrollToTopThreshold ?? -1),
    autoscrollToBottomThreshold: config.autoscrollToBottomThreshold ?? -1,
    animateAutoScrollToBottom: config.animateAutoScrollToBottom !== false,
    startRenderingFromBottom: config.startRenderingFromBottom === true,
  };
}

// React Native's constants: normal is 0.998 and fast 0.99.
function decelerationRate(rate: 'normal' | 'fast' | number | undefined): number {
  if (rate === 'fast') return 0.99;
  if (rate === undefined || rate === 'normal') return 0.998;
  return rate;
}

// Each index once, none negative, ascending.
export function stickyIndicesOf(indices: readonly number[] | undefined): number[] {
  return [...new Set(indices ?? [])].filter((index) => index >= 0).sort((a, b) => a - b);
}

export type Refresh = {
  onRefresh?: () => void;
  refreshing: boolean;
  colors?: ReadonlyArray<ColorValue>;
  tintColor?: ColorValue;
  progressViewOffset: number;
};

// As in VirtualizedList, a RefreshControl element replaces the list's own, so its props win.
export function refreshOf<T>(props: FlumeProps<T>): Refresh {
  const control = props.refreshControl;
  if (isValidElement<RefreshControlProps>(control)) {
    const given = control.props;
    return {
      onRefresh: given.onRefresh ?? undefined,
      refreshing: given.refreshing === true,
      colors: given.colors,
      tintColor: given.tintColor,
      progressViewOffset: given.progressViewOffset ?? 0,
    };
  }
  return {
    onRefresh: props.onRefresh ?? undefined,
    refreshing: props.refreshing === true,
    colors: control?.colors,
    tintColor: control?.tintColor,
    progressViewOffset: control?.progressViewOffset ?? 0,
  };
}
// Props that FlashList and LegendList type as nullable, where null means not given.
const NULLABLE = [
  'renderCell',
  'renderItem',
  'keyExtractor',
  'getItemType',
  'onPress',
  'onLongPress',
  'onScroll',
  'onScrollBeginDrag',
  'onScrollEndDrag',
  'onMomentumScrollBegin',
  'onMomentumScrollEnd',
  'onViewableItemsChanged',
  'onEndReached',
  'onStartReached',
  'onRefresh',
  'onLayout',
  'stickyHeaderIndices',
  'horizontal',
  'inverted',
  'viewabilityConfig',
  'onEndReachedThreshold',
  'onStartReachedThreshold',
  'refreshing',
] as const;

type Nullable = (typeof NULLABLE)[number];

export type SettledProps<T> = Omit<FlumeProps<T>, 'data' | 'initialScrollIndex' | Nullable> & {
  data: readonly T[];
  initialScrollIndex?: number;
} & { [K in Nullable]?: NonNullable<FlumeProps<T>[K]> };

const EMPTY: readonly never[] = [];

// The props in one shape: data defaulted, nulls removed, initialScrollIndex as a number. Unchanged props pass as is.
export function settleProps<T>(props: FlumeProps<T>): SettledProps<T> {
  const initial = props.initialScrollIndex;
  if (
    props.data != null &&
    (initial === undefined || typeof initial === 'number') &&
    NULLABLE.every((key) => props[key] !== null)
  ) {
    return props as SettledProps<T>;
  }
  const settled: Record<string, unknown> = {
    ...props,
    data: props.data ?? EMPTY,
    initialScrollIndex: typeof initial === 'object' && initial !== null ? initial.index : initial,
  };
  for (const key of [...NULLABLE, 'initialScrollIndex']) {
    if (settled[key] == null) delete settled[key];
  }
  return settled as SettledProps<T>;
}

export function nativeProps<T>(given: FlumeProps<T>, warn: Warn): FlatProps {
  const props = settleProps(given);
  const refresh = refreshOf(props);
  const horizontal = props.horizontal === true;
  let numColumns = props.numColumns ?? 1;
  if (horizontal && numColumns > 1) {
    warn('numColumns does not support horizontal; using one column');
    numColumns = 1;
  }
  const initial = props.initialScrollIndex;
  if (initial !== undefined && props.data.length > 0 && initial >= props.data.length) {
    warn(
      `initialScrollIndex ${initial} is past the ${props.data.length} items; scrolling to the end`,
    );
  }
  const padding = contentPadding(props.contentContainerStyle);
  if (padding.unsupported.length > 0) {
    warn(`contentContainerStyle supports padding only; ignored: ${padding.unsupported.join(', ')}`);
  }
  return {
    horizontal,
    numColumns,
    inverted: props.inverted === true,
    contentPaddingTop: padding.top,
    contentPaddingLeft: padding.left,
    contentPaddingBottom: padding.bottom,
    contentPaddingRight: padding.right,
    estimatedItemSize: props.estimatedItemSize ?? -1,
    listHeader: props.ListHeaderComponent != null,
    listFooter: props.ListFooterComponent != null,
    listEmpty: props.ListEmptyComponent != null,
    drawDistance: props.drawDistance ?? -1,
    initialScrollIndex: props.initialScrollIndex ?? -1,
    separatorSize: props.separator?.size ?? 0,
    separatorColor: props.separator?.color,
    separatorInsetStart: props.separator?.insetStart ?? 0,
    separatorInsetEnd: props.separator?.insetEnd ?? 0,
    scrollEventThrottle: props.scrollEventThrottle ?? 16,
    emitScrollEvents:
      props.onScroll !== undefined ||
      props.onScrollBeginDrag !== undefined ||
      props.onScrollEndDrag !== undefined ||
      props.onMomentumScrollBegin !== undefined ||
      props.onMomentumScrollEnd !== undefined,
    emitViewabilityEvents: props.onViewableItemsChanged !== undefined,
    viewabilityConfig: viewability(props.viewabilityConfig, warn),
    endReachedThreshold: props.onEndReachedThreshold ?? 2,
    startReachedThreshold: props.onStartReachedThreshold ?? 2,
    refreshEnabled: refresh.onRefresh !== undefined,
    refreshing: refresh.refreshing,
    refreshColors: refresh.colors,
    refreshProgressViewOffset: refresh.progressViewOffset,
    scrollEnabled: props.scrollEnabled !== false,
    nestedScrollEnabled: props.nestedScrollEnabled === true,
    overScrollMode: props.overScrollMode ?? 'auto',
    decelerationRate: decelerationRate(props.decelerationRate),
    showsVerticalScrollIndicator: props.showsVerticalScrollIndicator !== false,
    showsHorizontalScrollIndicator: props.showsHorizontalScrollIndicator !== false,
    contentInset: props.contentInset,
    scrollIndicatorInsets: props.scrollIndicatorInsets,
    contentInsetAdjustmentBehavior: props.contentInsetAdjustmentBehavior ?? 'never',
    bounces: props.bounces !== false,
    alwaysBounceVertical: props.alwaysBounceVertical ?? !horizontal,
    refreshTintColor: refresh.tintColor,
    keyboardDismissMode: props.keyboardDismissMode ?? 'none',
    keyboardShouldPersistTaps: props.keyboardShouldPersistTaps ?? 'never',
    rowsPressable: props.onPress !== undefined || props.onLongPress !== undefined,
    maintainVisibleContentPosition: anchoring(
      props.maintainVisibleContentPosition,
      props.inverted === true,
    ),
  };
}
