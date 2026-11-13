import { expect, it } from 'vitest';
import type { FrameStats, MemoryStats } from './lib/parse.mts';
import {
  type DeviceInfo,
  type JumpResult,
  jumpMarkdown,
  type RunResult,
  toMarkdown,
} from './lib/report.mts';

const frames = (jankyPercent: number, p90Ms: number | null): FrameStats => ({
  totalFrames: 400,
  jankyFrames: 4,
  jankyPercent,
  p50Ms: 9,
  p90Ms,
  p95Ms: 14,
  p99Ms: 30,
  deadlineMissed: 4,
});

const memory: MemoryStats = {
  javaHeapKb: 25600,
  nativeHeapKb: 379200,
  graphicsKb: 0,
  totalPssKb: 539710,
  totalRssKb: 664752,
};

const run = (repeat: number, jankyPercent: number, p90Ms: number | null): RunResult => ({
  scenario: 'chat',
  impl: 'flume',
  load: true,
  repeat,
  frames: frames(jankyPercent, p90Ms),
  memory,
  blank: null,
  bind: null,
  temperatureStart: 31,
  temperatureEnd: 33,
});

const device: DeviceInfo = {
  serial: 'emulator-5554',
  model: 'Pixel 9a',
  android: '16',
  size: [1080, 2424],
  density: 420,
  refreshRate: 60,
  battery: 80,
  ac: false,
};

it('takes medians over repeats and writes n/a for missing values', () => {
  const runs = [run(0, 1, 12), run(1, 5, null), run(2, 3, 16)];
  const table = toMarkdown(runs, device);
  expect(table).toContain('| flume | on | 3.00 |');
  expect(table).toContain('n/a');
});

const jump = (repeat: number, framesOver: number, animated: boolean): JumpResult => ({
  scenario: 'chat',
  impl: 'flashlist',
  load: true,
  animated,
  repeat,
  frames: frames(2, 20),
  blank: { frames: 300, meanPercent: 1.5, maxPercent: 40, framesOver },
  temperatureStart: 31,
  temperatureEnd: 33,
});

it('writes one jump row per list, load and scroll kind with the medians', () => {
  const runs = [jump(0, 10, false), jump(1, 30, false), jump(2, 20, false), jump(0, 50, true)];
  const table = jumpMarkdown(runs);
  expect(table).toContain('| flashlist | on | instant | 20.00 | 40.00 | 1.50 | 4.00 | 30.00 |');
  expect(table).toContain('| flashlist | on | animated | 50.00 |');
});
