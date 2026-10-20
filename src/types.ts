export type CellDimension = number | `${number}%`;

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
  flexBasis?: CellDimension;
  width?: CellDimension;
  height?: CellDimension;
  minWidth?: CellDimension;
  minHeight?: CellDimension;
  maxWidth?: CellDimension;
  maxHeight?: CellDimension;
  aspectRatio?: number;
  margin?: CellDimension;
  marginTop?: CellDimension;
  marginBottom?: CellDimension;
  marginLeft?: CellDimension;
  marginRight?: CellDimension;
  marginStart?: CellDimension;
  marginEnd?: CellDimension;
  marginHorizontal?: CellDimension;
  marginVertical?: CellDimension;
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
