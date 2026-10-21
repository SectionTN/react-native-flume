import type { StyleProp, ViewStyle } from 'react-native';
import type { NativeProps } from './specs/FlumeViewNativeComponent';
import type { FlumeProps } from './types';

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
  | 'drawDistance'
  | 'initialScrollIndex'
  | 'separatorSize'
  | 'separatorColor'
  | 'separatorInsetStart'
  | 'separatorInsetEnd'
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

export function nativeProps<T>(props: FlumeProps<T>, warn: Warn): FlatProps {
  const horizontal = props.horizontal === true;
  let numColumns = props.numColumns ?? 1;
  if (horizontal && numColumns > 1) {
    warn('numColumns does not support horizontal; using one column');
    numColumns = 1;
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
    drawDistance: props.drawDistance ?? -1,
    initialScrollIndex: props.initialScrollIndex ?? -1,
    separatorSize: props.separator?.size ?? 0,
    separatorColor: props.separator?.color,
    separatorInsetStart: props.separator?.insetStart ?? 0,
    separatorInsetEnd: props.separator?.insetEnd ?? 0,
  };
}
