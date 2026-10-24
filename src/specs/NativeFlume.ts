import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// Requiring the module installs the `__flume` host object through its JSI bindings.
export interface Spec extends TurboModule {
  setStats(enabled: boolean): void;
  logStats(): void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('NativeFlume');
