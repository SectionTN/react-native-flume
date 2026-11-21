import { describe, expect, it } from 'vitest';
import { lastRowStart } from '../separators';

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
