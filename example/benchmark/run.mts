import { spawn } from 'node:child_process';
import { existsSync, mkdirSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { parseArgs } from 'node:util';
import {
  type BenchAction,
  type BenchParams,
  benchUrl,
  IMPLS,
  type Impl,
  type ReadyInfo,
  type Scenario,
  SENTINEL_RGB,
} from '../src/bench/config.ts';
import { type Adb, connect } from './lib/adb.mts';
import { type BlankSummary, cropFor, summarizeBlank } from './lib/blank.mts';
import {
  type BindSummary,
  frameProblem,
  parseBindStats,
  parseGfxinfo,
  parseMeminfo,
  parseReady,
  parseSetData,
  type SetDataLine,
} from './lib/parse.mts';
import { median, type RunResult, toMarkdown } from './lib/report.mts';
import { blankFractions, videoSize } from './lib/video.mts';

const FLINGS = 12;
const WARMUP = 2;
const PRIMING = 40;
const FLING_MS = 200;
const FLING_EVERY_MS = 700;
const READY_TIMEOUT_MS = 60_000;
const SETTLE_MS = 3_000;
const BLANK_TOLERANCE = 24;
const BLANK_FRAME_THRESHOLD = 0.01;

type Config = Pick<BenchParams, 'scenario' | 'impl' | 'load' | 'count'>;
type SetDataResult = { round: number; label: string; line: SetDataLine };

const { values } = parseArgs({
  options: {
    serial: { type: 'string' },
    scenarios: { type: 'string', default: 'chat,feed' },
    impls: { type: 'string' },
    load: { type: 'string', default: '0,1' },
    repeat: { type: 'string', default: '3' },
    count: { type: 'string', default: '1000' },
    'no-record': { type: 'boolean', default: false },
    setdata: { type: 'boolean', default: false },
  },
});

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
const isFlume = (impl: Impl) => impl === 'flume' || impl === 'flume-react';
const label = (c: Config) =>
  `${c.scenario} ${c.impl} load=${c.load ? 'on' : 'off'} count=${c.count}`;

let nonce = 0;

async function waitForReady(adb: Adb, config: Config): Promise<ReadyInfo> {
  const deadline = Date.now() + READY_TIMEOUT_MS;
  while (Date.now() < deadline) {
    for (const line of adb.logcat(['ReactNativeJS:I']).split('\n')) {
      const ready = parseReady(line, config);
      if (ready !== null) return ready;
    }
    await sleep(500);
  }
  const tail = adb
    .logcat(['ReactNativeJS:V', 'AndroidRuntime:E'])
    .trim()
    .split('\n')
    .slice(-20)
    .join('\n');
  throw new Error(
    `no ready line for ${label(config)} within ${READY_TIMEOUT_MS / 1000} s; last log lines:\n${tail}`,
  );
}

async function launch(adb: Adb, config: Config): Promise<ReadyInfo> {
  adb.forceStop();
  adb.logcatClear();
  adb.start(benchUrl({ ...config, action: 'none', nonce: '' }));
  const ready = await waitForReady(adb, config);
  await sleep(SETTLE_MS);
  return ready;
}

async function action(adb: Adb, config: Config, name: BenchAction) {
  nonce += 1;
  adb.start(benchUrl({ ...config, action: name, nonce: String(nonce) }));
  await sleep(1_000);
}

// Forward is toward later posts in the feed and toward older messages in the chat.
async function flings(adb: Adb, ready: ReadyInfo, count: number, forward: boolean) {
  const [left, top, right, bottom] = ready.rect;
  const x = (left + right) / 2;
  const at = (share: number) => top + (bottom - top) * share;
  const up = ready.scenario === 'feed' ? forward : !forward;
  const [from, to] = up ? [at(0.8), at(0.25)] : [at(0.25), at(0.8)];
  for (let i = 0; i < count; i++) {
    const started = Date.now();
    adb.swipe(x, from, x, to, FLING_MS);
    await sleep(Math.max(0, FLING_EVERY_MS - (Date.now() - started)));
  }
}

async function warmUp(adb: Adb, ready: ReadyInfo) {
  adb.assertForeground();
  await flings(adb, ready, WARMUP, true);
  await flings(adb, ready, WARMUP, false);
}

async function measuredFlings(adb: Adb, ready: ReadyInfo) {
  adb.assertForeground();
  await flings(adb, ready, FLINGS, true);
  adb.assertForeground();
  await flings(adb, ready, FLINGS, false);
}

async function readBindStats(adb: Adb, config: Config): Promise<BindSummary | null> {
  adb.logcatClear();
  await action(adb, config, 'logStats');
  const deadline = Date.now() + 5_000;
  while (Date.now() < deadline) {
    const lines = adb.logcat(['FlumeStats:I']).split('\n');
    for (let i = lines.length - 1; i >= 0; i--) {
      const bind = parseBindStats(lines[i] ?? '');
      if (bind !== null) return bind.n > 0 ? bind : null;
    }
    await sleep(250);
  }
  return null;
}

type Recorder = { stop: () => Promise<string> };

function record(adb: Adb, out: string, name: string, screen: [number, number]): Recorder {
  if (adb.emulator) {
    const path = join(out, `${name}.webm`);
    const even = (value: number) => Math.round((value * 3) / 4 / 2) * 2;
    adb.run([
      'emu',
      'screenrecord',
      'start',
      '--time-limit',
      '180',
      '--fps',
      '60',
      '--size',
      `${even(screen[0])}x${even(screen[1])}`,
      path,
    ]);
    return {
      stop: async () => {
        adb.run(['emu', 'screenrecord', 'stop']);
        let size = -1;
        while (!existsSync(path) || statSync(path).size !== size) {
          size = existsSync(path) ? statSync(path).size : -1;
          await sleep(500);
        }
        return path;
      },
    };
  }
  const remote = '/sdcard/flume-bench.mp4';
  const child = spawn('adb', [
    '-s',
    adb.serial,
    'shell',
    'screenrecord',
    '--bit-rate',
    '20000000',
    remote,
  ]);
  const exited = new Promise((resolve) => child.on('close', resolve));
  return {
    stop: async () => {
      adb.shell('pkill -INT screenrecord');
      await exited;
      await sleep(1_000);
      const path = join(out, `${name}.mp4`);
      adb.run(['pull', remote, path], 120_000);
      adb.shell(`rm ${remote}`);
      return path;
    },
  };
}

async function blankPass(
  adb: Adb,
  config: Config,
  out: string,
  name: string,
  screen: [number, number],
): Promise<BlankSummary> {
  const ready = await launch(adb, config);
  await warmUp(adb, ready);
  adb.gfxinfoReset();
  const recorder = record(adb, out, name, screen);
  await sleep(500);
  await measuredFlings(adb, ready);
  const path = await recorder.stop();
  const problem = frameProblem(parseGfxinfo(adb.gfxinfo()), FLINGS * 2);
  if (problem !== null)
    throw new Error(
      `${label(config)} recording: ${problem}, the list was covered or the device stalled`,
    );
  const crop = cropFor(ready.rect, screen, videoSize(path));
  return summarizeBlank(
    await blankFractions(path, crop, SENTINEL_RGB, BLANK_TOLERANCE),
    BLANK_FRAME_THRESHOLD,
  );
}

async function runConfig(
  adb: Adb,
  config: Config,
  repeat: number,
  out: string,
  screen: [number, number],
): Promise<RunResult> {
  const ready = await launch(adb, config);
  await warmUp(adb, ready);
  if (isFlume(config.impl)) await action(adb, config, 'resetStats');
  adb.gfxinfoReset();
  const temperatureStart = adb.temperature();
  await measuredFlings(adb, ready);
  const frames = parseGfxinfo(adb.gfxinfo());
  const problem = frameProblem(frames, FLINGS * 2);
  if (problem !== null)
    throw new Error(`${label(config)}: ${problem}, the list was covered or the device stalled`);
  const memory = parseMeminfo(adb.meminfo());
  const temperatureEnd = adb.temperature();
  const bind = isFlume(config.impl) ? await readBindStats(adb, config) : null;
  const name = `${config.scenario}-${config.impl}-${config.load ? 'load' : 'idle'}-${repeat}`;
  const blank = values['no-record'] ? null : await blankPass(adb, config, out, name, screen);
  return { ...config, repeat, frames, memory, blank, bind, temperatureStart, temperatureEnd };
}

async function prime(adb: Adb, scenario: Scenario, count: number) {
  const ready = await launch(adb, { scenario, impl: 'flume', load: false, count });
  for (let done = 0; done < PRIMING; done += 10) {
    adb.assertForeground();
    await flings(adb, ready, 10, true);
  }
  for (let done = 0; done < PRIMING; done += 10) {
    adb.assertForeground();
    await flings(adb, ready, 10, false);
  }
}

async function newestSetData(adb: Adb, after: number): Promise<SetDataLine> {
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    const lines = adb
      .logcat(['ReactNativeJS:I'])
      .split('\n')
      .map(parseSetData)
      .filter((line) => line !== null);
    if (lines.length > after) return lines[lines.length - 1] as SetDataLine;
    await sleep(250);
  }
  throw new Error('no FlumeStats setData line within 10 s');
}

async function setDataRounds(adb: Adb, repeat: number): Promise<SetDataResult[]> {
  const results: SetDataResult[] = [];
  const rounds: [Config, BenchAction, string, string][] = [
    [
      { scenario: 'feed', impl: 'flume-react', load: false, count: 10_000 },
      'replace',
      'react 10,000 keys, mount',
      'react 10,000 keys, replace',
    ],
    [
      { scenario: 'chat', impl: 'flume', load: false, count: 1000 },
      'change500',
      'worklet 1,000 items, mount',
      'worklet 500 changed items',
    ],
  ];
  for (let round = 0; round < repeat; round++) {
    for (const [config, name, first, second] of rounds) {
      await launch(adb, config);
      results.push({ round, label: first, line: await newestSetData(adb, 0) });
      adb.logcatClear();
      await action(adb, config, name);
      results.push({ round, label: second, line: await newestSetData(adb, 0) });
    }
  }
  return results;
}

function setDataMarkdown(results: SetDataResult[]): string {
  if (results.length === 0) return '';
  const rows = [...new Set(results.map((result) => result.label))].map((name) => {
    const group = results.filter((result) => result.label === name).map((result) => result.line);
    const first = group[0];
    const diff = median(group.map((line) => line.diffMs)).toFixed(2);
    const host = median(group.map((line) => line.hostMs)).toFixed(2);
    return `| ${name} | ${first?.keys ?? 'n/a'} | ${first?.items ?? 'n/a'} | ${diff} | ${host} |`;
  });
  return [
    '',
    '### setData',
    '',
    '| call | keys | items | diff ms | host ms |',
    '| --- | --- | --- | --- | --- |',
    ...rows,
    '',
  ].join('\n');
}

async function main() {
  const serial = values.serial;
  if (serial === undefined) throw new Error('usage: yarn bench --serial <adb serial> [options]');
  const adb = connect(serial);
  const device = adb.deviceInfo();
  const startedAt = new Date().toISOString();
  const out = join(
    import.meta.dirname,
    'out',
    `${startedAt.replace(/[:.]/g, '-')}-${device.model.replace(/[^\w]+/g, '_')}`,
  );
  mkdirSync(out, { recursive: true });
  const runs: RunResult[] = [];
  let setData: SetDataResult[] = [];
  const write = () => {
    writeFileSync(
      join(out, 'results.json'),
      `${JSON.stringify({ device, startedAt, runs, setData }, null, 2)}\n`,
    );
    writeFileSync(join(out, 'summary.md'), toMarkdown(runs, device) + setDataMarkdown(setData));
  };
  try {
    if (values.setdata) {
      setData = await setDataRounds(adb, Number(values.repeat));
    } else {
      const scenarios = values.scenarios.split(',') as Scenario[];
      const impls = values.impls?.split(',') as Impl[] | undefined;
      const loads = values.load.split(',').map((value) => value === '1');
      const count = Number(values.count);
      const configs: Config[] = scenarios.flatMap((scenario) =>
        IMPLS[scenario]
          .filter((impl) => impls === undefined || impls.includes(impl))
          .flatMap((impl) => loads.map((load) => ({ scenario, impl, load, count }))),
      );
      for (const scenario of scenarios) {
        console.log(`priming ${scenario}`);
        await prime(adb, scenario, count);
      }
      for (let repeat = 0; repeat < Number(values.repeat); repeat++) {
        for (const config of configs) {
          console.log(`repeat ${repeat + 1}: ${label(config)}`);
          runs.push(await runConfig(adb, config, repeat, out, device.size));
          write();
        }
      }
    }
    write();
    console.log(`wrote ${out}`);
  } catch (error) {
    write();
    console.error(error instanceof Error ? error.message : error);
    console.error(`partial results in ${out}`);
    process.exitCode = 1;
  }
}

await main();
