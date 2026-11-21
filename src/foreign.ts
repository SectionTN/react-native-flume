import type { ComponentType, ReactElement } from 'react';
import type { ScrollViewProps, ViewProps } from 'react-native';
import type { FlumeOwnProps, FlumeProps } from './types';

// The names other lists use to tune their JS rendering, accepted so their code type-checks.
export type IgnoredProps<T> = {
  /** @deprecated Flume lays rows out natively and does not use this. */
  getItemLayout?:
    | ((
        data: ArrayLike<T> | null | undefined,
        index: number,
      ) => { length: number; offset: number; index: number })
    | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  initialNumToRender?: number | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  maxToRenderPerBatch?: number | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  updateCellsBatchingPeriod?: number | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  windowSize?: number | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  removeClippedSubviews?: boolean | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  disableVirtualization?: boolean | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  debug?: boolean | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  strictMode?: boolean | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  onScrollToIndexFailed?:
    | ((info: {
        index: number;
        highestMeasuredFrameIndex: number;
        averageItemLength: number;
      }) => void)
    | null;
  /** @deprecated Flume lays rows out natively and does not use this. */
  automaticallyAdjustContentInsets?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  bouncesZoom?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  maximumZoomScale?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  minimumZoomScale?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  pinchGestureEnabled?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  zoomScale?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  centerContent?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  disableScrollViewPanResponder?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  scrollPerfTag?: string;
  /** @deprecated Flume lays rows out natively and does not use this. */
  maxItemsInRecyclePool?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  onCommitLayoutEffect?: () => void;
  /** @deprecated Flume lays rows out natively and does not use this. */
  alwaysRender?: object;
  /** @deprecated Flume lays rows out natively and does not use this. */
  estimatedListSize?: { height: number; width: number };
  /** @deprecated Flume lays rows out natively and does not use this. */
  estimatedHeaderSize?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  getFixedItemSize?: (item: T, index: number, type: string | undefined) => number | undefined;
  /** @deprecated Flume lays rows out natively and does not use this. */
  contentInsetEndAdjustment?: number;
  /** @deprecated Flume lays rows out natively and does not use this. */
  experimental_hideItemsUntilMeasured?: boolean;
  /** @deprecated Flume lays rows out natively and does not use this. */
  recycleItems?: boolean;
  /** @deprecated Flume cannot use this. See the migration guide. */
  renderScrollComponent?: (props: ScrollViewProps) => ReactElement;
  /** @deprecated Flume cannot use this. See the migration guide. */
  StickyHeaderComponent?: ComponentType<object>;
  /** @deprecated Flume cannot use this. See the migration guide. */
  overrideProps?: object;
  /** @deprecated Flume cannot use this. See the migration guide. */
  innerViewRef?: unknown;
  /** @deprecated Flume cannot use this. See the migration guide. */
  useWindowScroll?: boolean;
};

export const IGNORED_KEYS = [
  'getItemLayout',
  'initialNumToRender',
  'maxToRenderPerBatch',
  'updateCellsBatchingPeriod',
  'windowSize',
  'removeClippedSubviews',
  'disableVirtualization',
  'debug',
  'strictMode',
  'onScrollToIndexFailed',
  'automaticallyAdjustContentInsets',
  'bouncesZoom',
  'maximumZoomScale',
  'minimumZoomScale',
  'pinchGestureEnabled',
  'zoomScale',
  'centerContent',
  'disableScrollViewPanResponder',
  'scrollPerfTag',
  'maxItemsInRecyclePool',
  'onCommitLayoutEffect',
  'alwaysRender',
  'estimatedListSize',
  'estimatedHeaderSize',
  'getFixedItemSize',
  'contentInsetEndAdjustment',
  'experimental_hideItemsUntilMeasured',
  'recycleItems',
  'renderScrollComponent',
  'StickyHeaderComponent',
  'overrideProps',
  'innerViewRef',
  'useWindowScroll',
] as const satisfies readonly (keyof IgnoredProps<unknown>)[];

export const OWN_KEYS = [
  'data',
  'renderCell',
  'renderItem',
  'reactCells',
  'keyExtractor',
  'getItemType',
  'extraData',
  'itemAnimations',
  'onPress',
  'onLongPress',
  'style',
  'horizontal',
  'numColumns',
  'inverted',
  'contentContainerStyle',
  'estimatedItemSize',
  'drawDistance',
  'initialScrollIndex',
  'separator',
  'ItemSeparatorComponent',
  'ListItemComponent',
  'CellRendererComponent',
  'stickyHeaderIndices',
  'ListHeaderComponent',
  'ListFooterComponent',
  'ListEmptyComponent',
  'ListHeaderComponentStyle',
  'ListFooterComponentStyle',
  'ListEmptyComponentStyle',
  'onScroll',
  'onScrollBeginDrag',
  'onScrollEndDrag',
  'onMomentumScrollBegin',
  'onMomentumScrollEnd',
  'scrollEventThrottle',
  'onViewableItemsChanged',
  'viewabilityConfig',
  'onEndReached',
  'onEndReachedThreshold',
  'onStartReached',
  'onStartReachedThreshold',
  'refreshing',
  'onRefresh',
  'refreshControl',
  'scrollEnabled',
  'nestedScrollEnabled',
  'overScrollMode',
  'decelerationRate',
  'showsVerticalScrollIndicator',
  'showsHorizontalScrollIndicator',
  'contentInset',
  'scrollIndicatorInsets',
  'contentInsetAdjustmentBehavior',
  'bounces',
  'alwaysBounceVertical',
  'keyboardDismissMode',
  'keyboardShouldPersistTaps',
  'maintainVisibleContentPosition',
  'onLayout',
  'testID',
  'accessibilityLabel',
] as const satisfies readonly (keyof FlumeOwnProps<unknown>)[];

// Fails to compile when an own prop is missing from OWN_KEYS, so it cannot leak to the native view.
const everyOwnKey: [Exclude<keyof FlumeOwnProps<unknown>, (typeof OWN_KEYS)[number]>] extends [
  never,
]
  ? true
  : never = true;
void everyOwnKey;

const NOT_HOST = new Set<string>([...OWN_KEYS, ...IGNORED_KEYS, 'children']);

// The plain view props a list may carry (nativeID, pointerEvents, accessibility), for the native view.
export function hostProps<T>(props: FlumeProps<T>): ViewProps {
  const host: Record<string, unknown> = {};
  for (const key in props) {
    if (!NOT_HOST.has(key)) host[key] = (props as Record<string, unknown>)[key];
  }
  return host as ViewProps;
}
