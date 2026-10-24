import { readFileSync } from 'node:fs';
import { expect, it } from 'vitest';
import { readyLine } from '../src/bench/config.ts';
import {
  foregroundPackage,
  parseBindStats,
  parseGfxinfo,
  parseMeminfo,
  parseReady,
  parseSetData,
} from './lib/parse.mts';

const fixture = (name: string) =>
  readFileSync(new URL(`./fixtures/${name}`, import.meta.url), 'utf8');

it('reads the frame summary', () => {
  expect(parseGfxinfo(fixture('gfxinfo.txt'))).toEqual({
    totalFrames: 55,
    jankyFrames: 7,
    jankyPercent: 12.73,
    p50Ms: 18,
    p90Ms: 38,
    p95Ms: 48,
    p99Ms: 150,
    deadlineMissed: 7,
  });
});

it('reads the memory summary in KB', () => {
  expect(parseMeminfo(fixture('meminfo.txt'))).toEqual({
    javaHeapKb: 25600,
    nativeHeapKb: 379200,
    graphicsKb: 0,
    totalPssKb: 539710,
    totalRssKb: 664752,
  });
});

it('returns null for lines it cannot find', () => {
  expect(parseGfxinfo('Total frames rendered: 3').p90Ms).toBeNull();
  expect(parseMeminfo('').totalPssKb).toBeNull();
});

it('reads the resumed package from dumpsys', () => {
  expect(
    foregroundPackage(
      '  topResumedActivity=ActivityRecord{50215660 u0 me.sectiontn.flume.example/.MainActivity t28}',
    ),
  ).toBe('me.sectiontn.flume.example');
  expect(foregroundPackage('nothing resumed')).toBeNull();
});

it('ignores a ready line for another configuration', () => {
  const line = readyLine({
    scenario: 'chat',
    impl: 'flume',
    load: true,
    count: 1000,
    rect: [0, 325, 1080, 1555],
    density: 2.625,
  });
  expect(
    parseReady(line, { scenario: 'chat', impl: 'flume', load: true, count: 1000 })?.rect,
  ).toEqual([0, 325, 1080, 1555]);
  expect(
    parseReady(line, { scenario: 'chat', impl: 'flashlist', load: true, count: 1000 }),
  ).toBeNull();
});

it('reads the stats lines', () => {
  expect(
    parseBindStats(
      'FlumeStats recycled n=400 total p50=0.72 p95=1.46 max=1.91 | worklet 0.16/0.23 | reconcile 0.02/0.02 | layout 0.06/0.68 | apply 0.46/0.71 | measure 1.7/bind hits 79% | first n=31 p50=1.79',
    ),
  ).toEqual({
    n: 400,
    p50Ms: 0.72,
    p95Ms: 1.46,
    maxMs: 1.91,
    workletP50: 0.16,
    layoutP50: 0.06,
    applyP50: 0.46,
    measureHitsPercent: 79,
    firstP50: 1.79,
  });
  expect(parseSetData('FlumeStats setData keys=10000 items=0 diff=1.10 host=0.21')).toEqual({
    keys: 10000,
    items: 0,
    diffMs: 1.1,
    hostMs: 0.21,
  });
});
