import { expect, it, vi } from 'vitest';
import NativeFlume from '../specs/NativeFlume';
import { unstable_logFlumeStats, unstable_setFlumeStats } from '../stats';

vi.mock('../specs/NativeFlume', () => ({ default: { setStats: vi.fn(), logStats: vi.fn() } }));

it('forwards the switch and the log request to the native module', () => {
  unstable_setFlumeStats(true);
  unstable_logFlumeStats();
  expect(NativeFlume.setStats).toHaveBeenCalledWith(true);
  expect(NativeFlume.logStats).toHaveBeenCalledTimes(1);
});
