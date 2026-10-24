import { READY_PREFIX, type ReadyInfo } from '../../src/bench/config.ts';

export type FrameStats = {
  totalFrames: number | null;
  jankyFrames: number | null;
  jankyPercent: number | null;
  p50Ms: number | null;
  p90Ms: number | null;
  p95Ms: number | null;
  p99Ms: number | null;
  deadlineMissed: number | null;
};

export type MemoryStats = {
  javaHeapKb: number | null;
  nativeHeapKb: number | null;
  graphicsKb: number | null;
  totalPssKb: number | null;
  totalRssKb: number | null;
};

export type BindSummary = {
  n: number;
  p50Ms: number;
  p95Ms: number;
  maxMs: number;
  workletP50: number;
  layoutP50: number;
  applyP50: number;
  measureHitsPercent: number;
  firstP50: number;
};

export type SetDataLine = { keys: number; items: number; diffMs: number; hostMs: number };

const NUMBER = '(\\d+(?:\\.\\d+)?)';

// The first capture of a line-anchored pattern; OEM builds drop or rename lines, so a miss is null, never 0.
function field(text: string, pattern: string, group = 1): number | null {
  const match = new RegExp(pattern, 'm').exec(text);
  const value = match?.[group];
  return value === undefined ? null : Number(value);
}

export function parseGfxinfo(text: string): FrameStats {
  const percentile = (p: number) => field(text, `^\\s*${p}th percentile: ${NUMBER}ms`);
  return {
    totalFrames: field(text, `^\\s*Total frames rendered: ${NUMBER}`),
    jankyFrames: field(text, `^\\s*Janky frames: ${NUMBER}`),
    jankyPercent: field(text, `^\\s*Janky frames: \\d+ \\(${NUMBER}%\\)`),
    p50Ms: percentile(50),
    p90Ms: percentile(90),
    p95Ms: percentile(95),
    p99Ms: percentile(99),
    deadlineMissed: field(text, `^\\s*Number Frame deadline missed: ${NUMBER}`),
  };
}

// App Summary rows, PSS column; the per-heap table above it has no colons.
export function parseMeminfo(text: string): MemoryStats {
  return {
    javaHeapKb: field(text, `^\\s*Java Heap:\\s+${NUMBER}`),
    nativeHeapKb: field(text, `^\\s*Native Heap:\\s+${NUMBER}`),
    graphicsKb: field(text, `^\\s*Graphics:\\s+${NUMBER}`),
    totalPssKb: field(text, `^\\s*TOTAL PSS:\\s+${NUMBER}`),
    totalRssKb: field(text, `TOTAL RSS:\\s+${NUMBER}`),
  };
}

export function foregroundPackage(activities: string): string | null {
  return /topResumedActivity=ActivityRecord\{\S+ \S+ ([\w.]+)\//.exec(activities)?.[1] ?? null;
}

export function parseReady(
  line: string,
  expected: Pick<ReadyInfo, 'scenario' | 'impl' | 'load' | 'count'>,
): ReadyInfo | null {
  const at = line.indexOf(READY_PREFIX);
  if (at < 0) return null;
  let info: ReadyInfo;
  try {
    info = JSON.parse(line.slice(at + READY_PREFIX.length)) as ReadyInfo;
  } catch {
    return null;
  }
  const same =
    info.scenario === expected.scenario &&
    info.impl === expected.impl &&
    info.load === expected.load &&
    info.count === expected.count;
  return same && Array.isArray(info.rect) && info.rect.length === 4 ? info : null;
}

const BIND = new RegExp(
  `FlumeStats recycled n=(\\d+) total p50=${NUMBER} p95=${NUMBER} max=${NUMBER}` +
    ` \\| worklet ${NUMBER}/[\\d.]+ \\| reconcile [\\d.]+/[\\d.]+ \\| layout ${NUMBER}/[\\d.]+` +
    ` \\| apply (-?\\d+(?:\\.\\d+)?)/-?[\\d.]+ \\| measure [\\d.]+/bind hits (\\d+)% \\| first n=\\d+ p50=${NUMBER}`,
);

export function parseBindStats(line: string): BindSummary | null {
  const m = BIND.exec(line);
  if (m === null) return null;
  const [n, p50, p95, max, worklet, layout, apply, hits, first] = m.slice(1).map(Number);
  return {
    n: n ?? 0,
    p50Ms: p50 ?? 0,
    p95Ms: p95 ?? 0,
    maxMs: max ?? 0,
    workletP50: worklet ?? 0,
    layoutP50: layout ?? 0,
    applyP50: apply ?? 0,
    measureHitsPercent: hits ?? 0,
    firstP50: first ?? 0,
  };
}

export function parseSetData(line: string): SetDataLine | null {
  const m = new RegExp(
    `FlumeStats setData keys=(\\d+) items=(\\d+) diff=${NUMBER} host=${NUMBER}`,
  ).exec(line);
  if (m === null) return null;
  return { keys: Number(m[1]), items: Number(m[2]), diffMs: Number(m[3]), hostMs: Number(m[4]) };
}
