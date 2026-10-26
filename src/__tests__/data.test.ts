import { describe, expect, it } from 'vitest';
import { defaultKey, diffData, itemOf, typeOf } from '../data';

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
    expect(d.next.index.size).toBe(0);
  });
});

describe('diffData types and keys', () => {
  it('types come from getItemType', () => {
    const d = diffData(undefined, [a, b], defaultKey, (item) => (item.n > 1 ? 'big' : 'small'));
    expect(d.types).toEqual(['small', 'big']);
  });

  it('keys are stable with an extractor or ids', () => {
    expect(diffData(undefined, [a, b], defaultKey).stableKeys).toBe(true);
    expect(diffData(undefined, [{ n: 1 }], defaultKey).stableKeys).toBe(false);
    expect(diffData(undefined, [{ n: 1 }], (_item, i) => `k${i}`).stableKeys).toBe(true);
  });

  it('duplicate keys get a suffix', () => {
    const d = diffData(undefined, [a, { id: 'a', n: 9 }, { id: 'a', n: 8 }], defaultKey);
    expect(d.keys).toEqual(['a', 'a:2', 'a:3']);
    expect(d.duplicates).toEqual(['a']);
    expect(itemOf(d.next, 'a:3')).toEqual({ id: 'a', n: 8 });
  });
});

describe('diffData by type', () => {
  it('sends only the items of types that render natively', () => {
    const items = [
      { id: 'a', t: 'w' },
      { id: 'b', t: 'r' },
    ];
    const d = diffData(
      undefined,
      items,
      undefined,
      (i) => i.t,
      (t) => t === 'w',
    );
    expect(d.changedIndices).toEqual([0]);
    expect(d.changedItems).toEqual([items[0]]);
    expect(typeOf(d.next, 'b')).toBe('r');
  });

  it('counts a type change as a change', () => {
    const x = { id: 'a', t: 'r' };
    const first = diffData(undefined, [x], undefined, (i) => i.t);
    expect(diffData(first.next, [x], undefined, () => 'w').changedIndices).toEqual([0]);
  });
});

describe('diffData state', () => {
  it('looks up an item and a type by key', () => {
    const d = diffData(undefined, [a, b], defaultKey, (item) => (item.n > 1 ? 'big' : 'small'));
    expect(itemOf(d.next, 'b')).toBe(b);
    expect(typeOf(d.next, 'b')).toBe('big');
    expect(itemOf(d.next, 'z')).toBeUndefined();
    expect(typeOf(d.next, 'z')).toBeUndefined();
    expect(itemOf(undefined, 'a')).toBeUndefined();
    expect(typeOf(undefined, 'a')).toBeUndefined();
  });

  it('skips the native call when nothing native changed', () => {
    const first = diffData(undefined, [a, b], defaultKey);
    expect(first.sameForNative).toBe(false);
    expect(diffData(first.next, [a, b], defaultKey).sameForNative).toBe(true);
    expect(diffData(first.next, [a, { id: 'b', n: 2 }], defaultKey).sameForNative).toBe(false);
    expect(diffData(first.next, [b, a], defaultKey).sameForNative).toBe(false);
    expect(
      diffData(first.next, [a, b], defaultKey, (item) => (item.n > 1 ? 'big' : 'default'))
        .sameForNative,
    ).toBe(false);
  });

  it('never compares items when no type renders natively', () => {
    const first = diffData(undefined, [a, b], defaultKey, undefined, 'none');
    expect(first.changedIndices).toEqual([]);
    const a2 = { id: 'a', n: 5 };
    const b2 = { id: 'b', n: 6 };
    const d = diffData(first.next, [a2, b2], defaultKey, undefined, 'none');
    expect(d.changedIndices).toEqual([]);
    expect(d.changedItems).toEqual([]);
    expect(d.sameForNative).toBe(true);
    expect(itemOf(d.next, 'a')).toBe(a2);
  });
});

describe('diffData types for native', () => {
  it('numbers each type by its first appearance', () => {
    const items = [
      { id: 'a', t: 'x' },
      { id: 'b', t: 'y' },
      { id: 'c', t: 'x' },
    ];
    const d = diffData(undefined, items, undefined, (i) => i.t);
    expect(d.typeTable).toEqual(['x', 'y']);
    expect([...d.typeIds]).toEqual([0, 1, 0]);
    expect(d.typeIds).toBeInstanceOf(Int32Array);
  });
});
