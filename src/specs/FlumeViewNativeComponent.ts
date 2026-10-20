import type { CodegenTypes, HostComponent, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

export interface NativeProps extends ViewProps {
  storeId?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
}

export default codegenNativeComponent<NativeProps>('FlumeView') as HostComponent<NativeProps>;
