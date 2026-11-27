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
  // Fractions of the list width, one per edge. An edge has points or a fraction, never both.
  percent: { top: number; left: number; bottom: number; right: number };
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
  | 'paddingPercentTop'
  | 'paddingPercentLeft'
  | 'paddingPercentBottom'
  | 'paddingPercentRight'
  | 'contentBackgroundColor'
  | 'contentJustify'
  | 'growFooter'
  | 'mainGap'
  | 'gridRowGap'
  | 'crossGap'
  | 'laneInsetStart'
  | 'laneInsetEnd'
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

const GAP_KEYS = new Set(['gap', 'rowGap', 'columnGap']);

const WRAPPER_KEYS = new Set([
  ...GAP_KEYS,
  'padding',
  'paddingHorizontal',
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

type Amount = { points: number; fraction: number };

const PERCENT = /^-?(\d+(\.\d+)?|\.\d+)%$/;

const JUSTIFY = new Set(['flex-start', 'center', 'flex-end']);

// Shorthands resolve as in React Native: padding, then the axis pairs, then each side. Start and end map to left and right.
// A percent stays a fraction of the list width, Yoga's base for every edge, and resolves where the width is known.
export function contentPadding(style: StyleProp<ViewStyle> | undefined): ContentPadding {
  const flat: Record<string, unknown> = {};
  flatten(style, flat);
  const unsupported: string[] = [];
  const amount = (key: string): Amount | undefined => {
    const value = flat[key];
    if (value === undefined || value === null) return undefined;
    if (typeof value === 'number') return { points: value, fraction: 0 };
    if (PADDING_KEYS.has(key) && typeof value === 'string' && PERCENT.test(value)) {
      return { points: 0, fraction: Number.parseFloat(value) / 100 };
    }
    unsupported.push(key);
    return undefined;
  };
  for (const key of Object.keys(flat)) {
    if (GAP_KEYS.has(key)) amount(key);
    else if (key === 'flexGrow') amount(key);
    else if (key === 'justifyContent') {
      if (!JUSTIFY.has(flat[key] as string)) unsupported.push(key);
    } else if (!PADDING_KEYS.has(key) && key !== 'backgroundColor') unsupported.push(key);
  }
  const none: Amount = { points: 0, fraction: 0 };
  const all = amount('padding') ?? none;
  const vertical = amount('paddingVertical') ?? all;
  const horizontal = amount('paddingHorizontal') ?? all;
  const top = amount('paddingTop') ?? vertical;
  const bottom = amount('paddingBottom') ?? vertical;
  const left = amount('paddingStart') ?? amount('paddingLeft') ?? horizontal;
  const right = amount('paddingEnd') ?? amount('paddingRight') ?? horizontal;
  return {
    top: top.points,
    left: left.points,
    bottom: bottom.points,
    right: right.points,
    percent: {
      top: top.fraction,
      left: left.fraction,
      bottom: bottom.fraction,
      right: right.fraction,
    },
    unsupported,
  };
}

function contentBackground(style: StyleProp<ViewStyle> | undefined): ColorValue | undefined {
  const flat: Record<string, unknown> = {};
  flatten(style, flat);
  return flat.backgroundColor as ColorValue | undefined;
}

type Edges = { top: number; left: number; bottom: number; right: number };

// The padding in points once the list width is known. A percent counts as 0 before that.
export function resolvePadding(padding: ContentPadding, width: number): Edges {
  const base = width > 0 ? width : 0;
  return {
    top: padding.top + padding.percent.top * base,
    left: padding.left + padding.percent.left * base,
    bottom: padding.bottom + padding.percent.bottom * base,
    right: padding.right + padding.percent.right * base,
  };
}

export type Gaps = {
  mainGap: number;
  gridRowGap: number;
  crossGap: number;
  laneInsetStart: number;
  laneInsetEnd: number;
};

function numberOf(flat: Record<string, unknown>, key: string): number | undefined {
  const value = flat[key];
  return typeof value === 'number' ? value : undefined;
}

// FlatList's meanings: the content gap spaces every pair of rows, the column wrapper's spaces a grid row's columns.
export function gapsOf<T>(
  props: Pick<FlumeProps<T>, 'contentContainerStyle' | 'columnWrapperStyle'>,
  horizontal: boolean,
  numColumns: number,
  warn: Warn = () => {},
): Gaps {
  const content: Record<string, unknown> = {};
  flatten(props.contentContainerStyle, content);
  // Yoga reads a negative gap as 0.
  const mainGap = Math.max(
    0,
    numberOf(content, horizontal ? 'columnGap' : 'rowGap') ?? numberOf(content, 'gap') ?? 0,
  );
  const wrapper: Record<string, unknown> = {};
  flatten(props.columnWrapperStyle, wrapper);
  const none = { mainGap, gridRowGap: 0, crossGap: 0, laneInsetStart: 0, laneInsetEnd: 0 };
  if (Object.keys(wrapper).length === 0) return none;
  if (horizontal || numColumns <= 1) {
    warn('columnWrapperStyle applies to grids only (numColumns above 1); ignored');
    return none;
  }
  const unsupported = Object.keys(wrapper).filter(
    (key) => !WRAPPER_KEYS.has(key) || numberOf(wrapper, key) === undefined,
  );
  const all = numberOf(wrapper, 'padding');
  if (all !== undefined && all !== 0) unsupported.push('the vertical part of padding');
  if (unsupported.length > 0) {
    warn(
      `columnWrapperStyle supports gap, columnGap, rowGap and horizontal padding only; ignored: ${unsupported.join(', ')}`,
    );
  }
  const across = numberOf(wrapper, 'paddingHorizontal') ?? all ?? 0;
  return {
    mainGap,
    gridRowGap: Math.max(0, numberOf(wrapper, 'rowGap') ?? 0),
    crossGap: Math.max(0, numberOf(wrapper, 'columnGap') ?? numberOf(wrapper, 'gap') ?? 0),
    laneInsetStart: numberOf(wrapper, 'paddingStart') ?? numberOf(wrapper, 'paddingLeft') ?? across,
    laneInsetEnd: numberOf(wrapper, 'paddingEnd') ?? numberOf(wrapper, 'paddingRight') ?? across,
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
  // RefreshControl's enabled, which turns pull to refresh off on Android.
  enabled: boolean;
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
      enabled: given.enabled !== false,
    };
  }
  return {
    onRefresh: props.onRefresh ?? undefined,
    refreshing: props.refreshing === true,
    colors: control?.colors,
    tintColor: control?.tintColor,
    progressViewOffset: props.progressViewOffset ?? control?.progressViewOffset ?? 0,
    enabled: true,
  };
}
// Props that FlashList and LegendList type as nullable, where null means not given.
const NULLABLE = [
  'renderCell',
  'renderSeparatorCell',
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

// FlatList's flexGrow content: justifyContent places short content, and a footer with flex takes the slack.
function contentGrowth<T>(
  props: SettledProps<T>,
): Pick<FlatProps, 'contentJustify' | 'growFooter'> {
  const content: Record<string, unknown> = {};
  flatten(props.contentContainerStyle, content);
  const grows = typeof content.flexGrow === 'number' && content.flexGrow > 0;
  // LegendList's alignItemsAtEnd places short content without growing it, so a footer keeps its length.
  if (!grows) return { contentJustify: props.alignItemsAtEnd ? 'end' : 'start', growFooter: false };
  const footer: Record<string, unknown> = {};
  flatten(props.ListFooterComponentStyle, footer);
  // Yoga takes an explicit flexGrow, 0 included, before the flex shorthand.
  const grow = typeof footer.flexGrow === 'number' ? footer.flexGrow : footer.flex;
  const justify = content.justifyContent ?? (props.alignItemsAtEnd ? 'flex-end' : undefined);
  return {
    contentJustify: justify === 'center' ? 'center' : justify === 'flex-end' ? 'end' : 'start',
    growFooter: props.ListFooterComponent != null && typeof grow === 'number' && grow > 0,
  };
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
    warn(
      `contentContainerStyle supports padding, gaps, backgroundColor, flexGrow and justifyContent only; ignored: ${padding.unsupported.join(', ')}`,
    );
  }
  return {
    ...gapsOf(props, horizontal, numColumns, warn),
    horizontal,
    numColumns,
    inverted: props.inverted === true,
    contentPaddingTop: padding.top,
    contentPaddingLeft: padding.left,
    contentPaddingBottom: padding.bottom,
    contentPaddingRight: padding.right,
    paddingPercentTop: padding.percent.top,
    paddingPercentLeft: padding.percent.left,
    paddingPercentBottom: padding.percent.bottom,
    paddingPercentRight: padding.percent.right,
    contentBackgroundColor: contentBackground(props.contentContainerStyle),
    ...contentGrowth(props),
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
    refreshEnabled: refresh.enabled && refresh.onRefresh !== undefined,
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
