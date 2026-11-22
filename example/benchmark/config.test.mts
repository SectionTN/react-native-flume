import { expect, it } from 'vitest';
import { benchUrl, parseBenchParams } from '../src/bench/config.ts';

it('round-trips every field through the URL', () => {
  const params = {
    scenario: 'feed',
    impl: 'legendlist',
    load: true,
    count: 10000,
    action: 'jump',
    index: 499,
    animated: true,
    separators: true,
    nonce: '7',
  } as const;
  const url = new URL(benchUrl(params));
  expect(parseBenchParams(Object.fromEntries(url.searchParams))).toEqual(params);
});

it('falls back to chat, flume, no load, 1,000 items and no action', () => {
  const fallback = {
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
  expect(parseBenchParams(undefined)).toEqual(fallback);
  expect(parseBenchParams({ impl: 'other', count: 'many', action: 'other', index: '-1' })).toEqual(
    fallback,
  );
});
