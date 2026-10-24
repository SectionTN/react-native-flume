import { expect, it } from 'vitest';
import type { FrameStats, MemoryStats } from './lib/parse.mts';
import { type DeviceInfo, type RunResult, toMarkdown } from './lib/report.mts';

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
