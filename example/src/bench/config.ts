// Shared with the Node runner in example/benchmark, so no React Native imports here.
export const SENTINEL = '#ff00ff';
export const SENTINEL_RGB: readonly [number, number, number] = [255, 0, 255];
export const READY_PREFIX = 'FlumeBench ready ';

export type Scenario = 'chat' | 'feed';
export type Impl = 'flume' | 'flume-react' | 'flashlist' | 'legendlist' | 'native';
export type BenchAction = 'none' | 'replace' | 'change500' | 'resetStats' | 'logStats' | 'jump';
export type BenchParams = {
  scenario: Scenario;
  impl: Impl;
  load: boolean;
  count: number;
  action: BenchAction;
  // The jump action's target and scroll kind.
  index: number;
  animated: boolean;
  // Each list draws its separator: ItemSeparatorComponent, or renderSeparatorCell for Flume's worklet cells.
  separators: boolean;
  nonce: string;
};

export const IMPLS: Record<Scenario, readonly Impl[]> = {
  chat: ['flume', 'flume-react', 'flashlist', 'legendlist', 'native'],
  feed: ['flume', 'flume-react', 'flashlist', 'legendlist'],
};

export type ReadyInfo = {
  scenario: Scenario;
  impl: Impl;
  load: boolean;
  count: number;
  rect: [number, number, number, number];
  density: number;
};

const SCENARIOS: readonly Scenario[] = ['chat', 'feed'];
const ALL_IMPLS: readonly Impl[] = IMPLS.chat;
const ACTIONS: readonly BenchAction[] = [
  'none',
  'replace',
  'change500',
  'resetStats',
  'logStats',
  'jump',
];
const DEFAULTS: BenchParams = {
  scenario: 'chat',
  impl: 'flume',
  load: false,
  count: 1000,
  action: 'none',
  index: 0,
  animated: false,
  separators: false,
  nonce: '',
};

function oneOf<T extends string>(value: string | undefined, allowed: readonly T[], fallback: T): T {
  return allowed.includes(value as T) ? (value as T) : fallback;
}

export function parseBenchParams(
  params: Readonly<Record<string, string | undefined>> | undefined,
): BenchParams {
  const count = Number(params?.count);
  const index = Number(params?.index);
  return {
    scenario: oneOf(params?.scenario, SCENARIOS, DEFAULTS.scenario),
    impl: oneOf(params?.impl, ALL_IMPLS, DEFAULTS.impl),
    load: params?.load === '1',
    count: Number.isInteger(count) && count >= 0 ? count : DEFAULTS.count,
    action: oneOf(params?.action, ACTIONS, DEFAULTS.action),
    index: Number.isInteger(index) && index >= 0 ? index : DEFAULTS.index,
    animated: params?.animated === '1',
    separators: params?.sep === '1',
    nonce: params?.n ?? DEFAULTS.nonce,
  };
}

export function benchUrl(params: Partial<BenchParams>): string {
  const p = { ...DEFAULTS, ...params };
  const query = new URLSearchParams({
    scenario: p.scenario,
    impl: p.impl,
    load: p.load ? '1' : '0',
    count: String(p.count),
    action: p.action,
    index: String(p.index),
    animated: p.animated ? '1' : '0',
    sep: p.separators ? '1' : '0',
    n: p.nonce,
  });
  return `flume-example://bench?${query.toString()}`;
}

export function readyLine(info: ReadyInfo): string {
  return READY_PREFIX + JSON.stringify(info);
}
