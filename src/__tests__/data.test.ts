import { describe, expect, it } from 'vitest';
import { defaultKey, diffData } from '../data';

const a = { id: 'a', n: 1 };
const b = { id: 'b', n: 2 };

describe('defaultKey', () => {
  it('prefers id, then key, then the index', () => {
    expect(defaultKey({ id: 'x', key: 'y' }, 3)).toBe('x');
    expect(defaultKey({ key: 'y' }, 3)).toBe('y');
    expect(defaultKey({ n: 1 }, 3)).toBe('3');
    expect(defaultKey(null, 4)).toBe('4');
  });
});

describe('diffData', () => {
  it('sends everything on the first call', () => {
    const d = diffData(undefined, [a, b], defaultKey);
    expect(d.keys).toEqual(['a', 'b']);
    expect(d.types).toEqual(['default', 'default']);
    expect(d.changedIndices).toEqual([0, 1]);
    expect(d.changedItems).toEqual([a, b]);
  });

  it('sends nothing for the same references in a new array', () => {
    const first = diffData(undefined, [a, b], defaultKey);
    const d = diffData(first.next, [a, b], defaultKey);
    expect(d.changedIndices).toEqual([]);
    expect(d.changedItems).toEqual([]);
    expect(d.keys).toEqual(['a', 'b']);
  });

  it('sends only the replaced item, at its new index', () => {
    const first = diffData(undefined, [a, b], defaultKey);
    const b2 = { id: 'b', n: 3 };
    const d = diffData(first.next, [b2, a], defaultKey);
    expect(d.keys).toEqual(['b', 'a']);
    expect(d.changedIndices).toEqual([0]);
    expect(d.changedItems).toEqual([b2]);
  });

  it('handles empty data', () => {
    const first = diffData(undefined, [a], defaultKey);
    const d = diffData(first.next, [], defaultKey);
    expect(d.keys).toEqual([]);
    expect(d.changedIndices).toEqual([]);
    expect(d.next.byKey.size).toBe(0);
  });
});
