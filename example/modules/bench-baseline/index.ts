import { requireNativeView } from 'expo';
import type { Ref } from 'react';
import type { ViewStyle } from 'react-native';
import type { BenchChat } from '../../src/bench/data';

export type BenchBaselineHandle = { scrollToIndex(index: number): Promise<void> };

export const BenchBaselineView = requireNativeView<{
  rows: BenchChat[];
  style?: ViewStyle;
  ref?: Ref<BenchBaselineHandle>;
}>('BenchBaseline');
