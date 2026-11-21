import { describe, expect, it } from 'vitest';
import { createSeparatorLinks, lastRowStart } from '../separators';

// The same table as cpp/tests/separator_rule_test.cpp: count, columns, full-span indices, first index of the last row.
const CASES: [number, number, number[], number][] = [
  [0, 1, [], 0],
  [5, 1, [], 4],
  [9, 3, [], 6],
  [10, 3, [], 9],
  [10, 3, [4], 8],
  [10, 3, [9], 9],
  [6, 2, [0, 3], 4],
];

describe('lastRowStart', () => {
  it.each(CASES)(
    'count %i in %i columns with full spans %j starts the last row at %i',
    (count, columns, spans, start) => {
      expect(lastRowStart(count, columns, spans)).toBe(start);
    },
  );
});

describe('separator links', () => {
  const state = (keys: string[]) => ({
    current: { keys, index: new Map(keys.map((key, i) => [key, i] as const)) },
  });

  it('highlights this row and the row before it', () => {
    const links = createSeparatorLinks(state(['a', 'b', 'c']));
    const got: [string, Record<string, unknown>][] = [];
    for (const key of ['a', 'b', 'c']) links.register(key, (props) => got.push([key, props]));
    links.highlight('c', true);
    expect(got).toEqual([
      ['c', { highlighted: true }],
      ['b', { highlighted: true }],
    ]);
  });

  it('sends leading props to the row before and trailing props to this row', () => {
    const links = createSeparatorLinks(state(['a', 'b']));
    const got: [string, Record<string, unknown>][] = [];
    for (const key of ['a', 'b']) links.register(key, (props) => got.push([key, props]));
    links.update('b', 'leading', { color: 'red' });
    links.update('b', 'trailing', { color: 'blue' });
    expect(got).toEqual([
      ['a', { color: 'red' }],
      ['b', { color: 'blue' }],
    ]);
  });

  it('reaches every slot of a key and none after unregistering', () => {
    const links = createSeparatorLinks(state(['a']));
    const got: string[] = [];
    const off = links.register('a', () => got.push('row'));
    links.register('a', () => got.push('sticky'));
    links.highlight('a', true);
    off();
    links.highlight('a', false);
    expect(got).toEqual(['row', 'sticky', 'sticky']);
  });

  it('reads the row before from the latest data', () => {
    const data = state(['a', 'b', 'c']);
    const links = createSeparatorLinks(data);
    const got: string[] = [];
    for (const key of ['a', 'b', 'c']) links.register(key, () => got.push(key));
    data.current = state(['a', 'c']).current;
    links.highlight('c', true);
    expect(got).toEqual(['c', 'a']);
  });
});
