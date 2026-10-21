import type { ReactElement } from 'react';
import type {
  LayoutChangeEvent,
  NativeScrollEvent,
  NativeSyntheticEvent,
  StyleProp,
  ViewStyle,
} from 'react-native';

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

export type ScrollHandler = (event: NativeSyntheticEvent<NativeScrollEvent>) => void;

export type FlumeProps<T> = {
  data: readonly T[];
  renderCell?: (item: T, index: number) => CellNode;
  // Accepted for migration; React cells render in a later version.
  renderItem?: (info: { item: T; index: number }) => ReactElement | null;
  keyExtractor?: (item: T, index: number) => string;
  getItemType?: (item: T, index: number) => string;
  extraData?: unknown;
  itemAnimations?: boolean;
  onPress?: (info: PressInfo<T>) => void;
  onLongPress?: (info: PressInfo<T>) => void;
  style?: StyleProp<ViewStyle>;
  horizontal?: boolean;
  numColumns?: number;
  inverted?: boolean;
  // Padding only, as the spec says; other keys warn in development.
  contentContainerStyle?: StyleProp<ViewStyle>;
  // How far past the viewport, in dp, cells are bound ahead; the default is one viewport.
  drawDistance?: number;
  initialScrollIndex?: number;
  // A native line between consecutive rows, inset along the cross axis from the start and end edges.
  separator?: { size: number; color?: string; insetStart?: number; insetEnd?: number };
  stickyHeaderIndices?: number[];
  // The ScrollView events with the ScrollView payload; contentOffset counts from the content start.
  onScroll?: ScrollHandler;
  onScrollBeginDrag?: ScrollHandler;
  onScrollEndDrag?: ScrollHandler;
  onMomentumScrollBegin?: ScrollHandler;
  onMomentumScrollEnd?: ScrollHandler;
  scrollEventThrottle?: number;
  onViewableItemsChanged?: (info: {
    viewableItems: ViewToken<T>[];
    changed: ViewToken<T>[];
  }) => void;
  viewabilityConfig?: ViewabilityConfig;
  onLayout?: (event: LayoutChangeEvent) => void;
  testID?: string;
  accessibilityLabel?: string;
};
