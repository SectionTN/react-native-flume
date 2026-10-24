import { IMPLS, type Impl, type Scenario } from '../../src/bench/config.ts';
import type { BlankSummary } from './blank.mts';
import type { BindSummary, FrameStats, MemoryStats } from './parse.mts';

export type RunResult = {
  scenario: Scenario;
  impl: Impl;
  load: boolean;
  repeat: number;
  frames: FrameStats;
  memory: MemoryStats;
  blank: BlankSummary | null;
  bind: BindSummary | null;
  temperatureStart: number | null;
  temperatureEnd: number | null;
};

export type DeviceInfo = {
  serial: string;
  model: string;
  android: string;
  size: [number, number];
  density: number;
  refreshRate: number | null;
  battery: number | null;
  ac: boolean;
};

export function median(values: number[]): number {
  if (values.length === 0) return Number.NaN;
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 1
    ? (sorted[mid] ?? Number.NaN)
    : ((sorted[mid - 1] ?? 0) + (sorted[mid] ?? 0)) / 2;
}

// The median over the repeats that have the value, two decimals; n/a when none has it.
function cell(runs: RunResult[], pick: (run: RunResult) => number | null | undefined): string {
  const values = runs
    .map(pick)
    .filter((value): value is number => typeof value === 'number' && !Number.isNaN(value));
  return values.length === 0 ? 'n/a' : median(values).toFixed(2);
}

const COLUMNS =
  '| impl | load | janky % | p90 frame ms | deadlines missed | blank mean % | blank max % | PSS MB | bind p50/p95 ms |';

export function toMarkdown(runs: RunResult[], device: DeviceInfo): string {
  const power = device.ac ? 'on AC' : 'on battery';
  const battery = device.battery === null ? 'battery n/a' : `battery ${device.battery}% ${power}`;
  const refresh = device.refreshRate === null ? 'n/a' : device.refreshRate.toFixed(0);
  const lines = [
    `Device: ${device.model}, Android ${device.android}, ${device.size[0]}x${device.size[1]} at ${device.density} dpi, ${refresh} Hz, ${battery} (${device.serial})`,
  ];
  for (const scenario of ['chat', 'feed'] as const) {
    const inScenario = runs.filter((run) => run.scenario === scenario);
    if (inScenario.length === 0) continue;
    lines.push(
      '',
      `### ${scenario}`,
      '',
      COLUMNS,
      '| --- | --- | --- | --- | --- | --- | --- | --- | --- |',
    );
    for (const impl of IMPLS[scenario]) {
      for (const load of [false, true]) {
        const group = inScenario.filter((run) => run.impl === impl && run.load === load);
        if (group.length === 0) continue;
        const pss = cell(group, (run) =>
          run.memory.totalPssKb === null ? null : run.memory.totalPssKb / 1024,
        );
        const bindP50 = cell(group, (run) => run.bind?.p50Ms);
        const bindP95 = cell(group, (run) => run.bind?.p95Ms);
        const bind = bindP50 === 'n/a' && bindP95 === 'n/a' ? 'n/a' : `${bindP50}/${bindP95}`;
        lines.push(
          `| ${impl} | ${load ? 'on' : 'off'} | ${cell(group, (run) => run.frames.jankyPercent)} | ${cell(group, (run) => run.frames.p90Ms)} | ${cell(group, (run) => run.frames.deadlineMissed)} | ${cell(group, (run) => run.blank?.meanPercent)} | ${cell(group, (run) => run.blank?.maxPercent)} | ${pss} | ${bind} |`,
        );
      }
    }
  }
  return `${lines.join('\n')}\n`;
}
