import type { CodegenTypes, HostComponent, ViewProps } from 'react-native';
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
  onPress?: CodegenTypes.DirectEventHandler<PressEvent>;
  onLongPress?: CodegenTypes.DirectEventHandler<PressEvent>;
}

export default codegenNativeComponent<NativeProps>('FlumeView') as HostComponent<NativeProps>;
