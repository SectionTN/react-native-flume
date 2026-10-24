import { requireNativeView } from 'expo';
import type { ViewStyle } from 'react-native';
import type { BenchChat } from '../../src/bench/data';

export const BenchBaselineView = requireNativeView<{ rows: BenchChat[]; style?: ViewStyle }>(
  'BenchBaseline',
);
