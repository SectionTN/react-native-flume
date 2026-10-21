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
  // In dp; -1 means one viewport.
  drawDistance?: CodegenTypes.WithDefault<CodegenTypes.Double, -1>;
  initialScrollIndex?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  separatorSize?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  separatorColor?: ColorValue;
  separatorInsetStart?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  separatorInsetEnd?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  stickyHeaderIndices?: ReadonlyArray<CodegenTypes.Int32>;
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
  keyboardDismissMode?: CodegenTypes.WithDefault<'none' | 'on-drag', 'none'>;
  keyboardShouldPersistTaps?: CodegenTypes.WithDefault<'always' | 'never' | 'handled', 'never'>;
  // Rows are clickable only when the list has a press handler.
  rowsPressable?: CodegenTypes.WithDefault<boolean, false>;
  viewabilityConfig?: Readonly<{
    itemVisiblePercentThreshold?: CodegenTypes.Double;
    viewAreaCoveragePercentThreshold?: CodegenTypes.Double;
    minimumViewTime?: CodegenTypes.Double;
    waitForInteraction?: boolean;
  }>;
  onPress?: CodegenTypes.DirectEventHandler<PressEvent>;
  onLongPress?: CodegenTypes.DirectEventHandler<PressEvent>;
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
}

type FlumeViewType = HostComponent<NativeProps>;

// viewPosition is in content order; offsets are in dp.
interface NativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<FlumeViewType>,
    index: CodegenTypes.Int32,
    animated: boolean,
    viewPosition: CodegenTypes.Double,
    viewOffset: CodegenTypes.Double,
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<FlumeViewType>,
    offset: CodegenTypes.Double,
    animated: boolean,
  ) => void;
  scrollToEnd: (viewRef: React.ElementRef<FlumeViewType>, animated: boolean) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['scrollToIndex', 'scrollToOffset', 'scrollToEnd'],
});

export default codegenNativeComponent<NativeProps>('FlumeView') as FlumeViewType;
