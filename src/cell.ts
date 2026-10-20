import type {
  CellImageProps,
  CellNode,
  CellPressableProps,
  CellStyle,
  CellTextOptions,
} from './types';

// Each builder is a worklet so a renderCell closure can capture `cell`.
function view(style?: CellStyle, children?: CellNode[]): CellNode {
  'worklet';
  const node: CellNode = { type: 'view' };
  if (style !== undefined) node.style = style;
  if (children !== undefined) node.children = children;
  return node;
}

function text(content: string, style?: CellStyle, options?: CellTextOptions): CellNode {
  'worklet';
  const node: CellNode = { type: 'text', text: content };
  if (style !== undefined) node.style = style;
  if (options?.allowFontScaling !== undefined) node.allowFontScaling = options.allowFontScaling;
  return node;
}

function image(props: CellImageProps): CellNode {
  'worklet';
  const { source, resizeMode, tintColor, ...style } = props;
  const node: CellNode = { type: 'image', source, style };
  if (resizeMode !== undefined) node.resizeMode = resizeMode;
  if (tintColor !== undefined) node.tintColor = tintColor;
  return node;
}

function pressable(props: CellPressableProps, children?: CellNode[]): CellNode {
  'worklet';
  const { id, pressedOpacity, ...style } = props;
  const node: CellNode = { type: 'pressable', id, style };
  if (pressedOpacity !== undefined) node.pressedOpacity = pressedOpacity;
  if (children !== undefined) node.children = children;
  return node;
}

function spacer(size?: number): CellNode {
  'worklet';
  return size === undefined
    ? { type: 'view', style: { flex: 1 } }
    : { type: 'view', style: { flexBasis: size } };
}

export const cell = Object.freeze({ view, text, image, pressable, spacer });
