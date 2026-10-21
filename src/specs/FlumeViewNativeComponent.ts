import type { CodegenTypes, ColorValue, HostComponent, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

// A press on a row: the pressable id is '' for a press on the cell outside any pressable.
export type PressEvent = Readonly<{
  index: CodegenTypes.Int32;
  key: string;
  id: string;
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
  onPress?: CodegenTypes.DirectEventHandler<PressEvent>;
  onLongPress?: CodegenTypes.DirectEventHandler<PressEvent>;
}

export default codegenNativeComponent<NativeProps>('FlumeView') as HostComponent<NativeProps>;
