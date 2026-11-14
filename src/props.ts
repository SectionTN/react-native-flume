import type { StyleProp, ViewStyle } from 'react-native';
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
  | 'stickyHeaderIndices'
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

// Codegen structs read an unset member as 0 on iOS, so every field goes out; -1 is no threshold.
function anchoring(
  config: FlumeProps<unknown>['maintainVisibleContentPosition'],
): NonNullable<FlatProps['maintainVisibleContentPosition']> {
  return {
    disabled: config?.disabled === true,
    autoscrollToTopThreshold: config?.autoscrollToTopThreshold ?? -1,
    autoscrollToBottomThreshold: config?.autoscrollToBottomThreshold ?? -1,
    animateAutoScrollToBottom: config?.animateAutoScrollToBottom !== false,
    startRenderingFromBottom: config?.startRenderingFromBottom === true,
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

export function nativeProps<T>(props: FlumeProps<T>, warn: Warn): FlatProps {
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
    stickyHeaderIndices: stickyIndicesOf(props.stickyHeaderIndices),
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
    refreshEnabled: props.onRefresh !== undefined,
    refreshing: props.refreshing === true,
    refreshColors: props.refreshControl?.colors,
    refreshProgressViewOffset: props.refreshControl?.progressViewOffset ?? 0,
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
    refreshTintColor: props.refreshControl?.tintColor,
    keyboardDismissMode: props.keyboardDismissMode ?? 'none',
    keyboardShouldPersistTaps: props.keyboardShouldPersistTaps ?? 'never',
    rowsPressable: props.onPress !== undefined || props.onLongPress !== undefined,
    maintainVisibleContentPosition: anchoring(props.maintainVisibleContentPosition),
  };
}
