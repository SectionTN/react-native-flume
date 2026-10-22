import type { CodegenTypes, HostComponent, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

// One React cell: native parks it and moves it into the holder that shows its item.
export interface NativeProps extends ViewProps {
  slot?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
  itemKey?: string;
  kind?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
}

export default codegenNativeComponent<NativeProps>('FlumeSlotView') as HostComponent<NativeProps>;
