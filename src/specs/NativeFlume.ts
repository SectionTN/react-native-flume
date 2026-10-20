import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// The module has no methods: requiring it installs the `__flume` host object through its JSI bindings.
export interface Spec extends TurboModule {}

export default TurboModuleRegistry.getEnforcing<Spec>('NativeFlume');
