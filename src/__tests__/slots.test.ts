import { describe, expect, it } from 'vitest';
import {
  applySnapshot,
  gridCellWidth,
  prerender,
  SlotKind,
  type SlotTable,
  slotStyle,
} from '../slots';

const layout = {
  horizontal: false,
  numColumns: 1,
  padding: { top: 0, left: 16, bottom: 0, right: 8 },
  cellWidth: -1,
};
const row = { slot: 1, kind: SlotKind.row, key: 'a', index: 0, bound: true, minMain: 0 };

describe('slotStyle', () => {
  it('spans the padded width in a single column', () => {
    expect(slotStyle(row, false, layout)).toEqual({ position: 'absolute', left: 16, right: 8 });
  });

  it('sizes a grid cell from the reported width, and waits for it', () => {
    const grid = { ...layout, numColumns: 3 };
    expect(slotStyle(row, false, grid)).toBeNull();
    expect(slotStyle(row, false, { ...grid, cellWidth: 120 })).toEqual({
      position: 'absolute',
      width: 120,
    });
    expect(slotStyle(row, true, grid)).toEqual({ position: 'absolute', left: 16, right: 8 });
  });

  it('takes the padded height in a horizontal list', () => {
    const strip = {
      ...layout,
      horizontal: true,
      padding: { top: 4, left: 0, bottom: 6, right: 0 },
    };
    expect(slotStyle(row, false, strip)).toEqual({ position: 'absolute', top: 4, bottom: 6 });
  });
});

describe('slotStyle for the empty row', () => {
  it('stretches the empty row to the reported length', () => {
    const empty = { ...row, kind: SlotKind.empty, minMain: 300 };
    expect(slotStyle(empty, true, layout)).toEqual({
      position: 'absolute',
      left: 16,
      right: 8,
      minHeight: 300,
    });
    const strip = {
      ...layout,
      horizontal: true,
      padding: { top: 0, left: 0, bottom: 0, right: 0 },
    };
    expect(slotStyle(empty, true, strip)).toMatchObject({ minWidth: 300 });
  });
});

describe('gridCellWidth', () => {
  const padding = { left: 16, right: 8 };

  it('divides the list width inside the padding before native reports a column', () => {
    expect(gridCellWidth({ width: 360, cellWidth: -1 }, 2, padding)).toBe(168);
  });

  it("keeps native's column computed for the same columns and padding", () => {
    expect(gridCellWidth({ width: 360, cellWidth: 168.25 }, 2, padding)).toBe(168.25);
  });

  it("drops native's column computed for other columns", () => {
    expect(gridCellWidth({ width: 360, cellWidth: 168 }, 3, padding)).toBe(112);
  });

  it('never goes below zero when the padding is wider than the list', () => {
    expect(gridCellWidth({ width: 20, cellWidth: 0 }, 2, padding)).toBe(0);
    expect(gridCellWidth({ width: 24.2, cellWidth: -0.1 }, 2, padding)).toBe(0);
  });

  it('is unknown until the list is laid out', () => {
    expect(gridCellWidth({ width: -1, cellWidth: -1 }, 2, padding)).toBe(-1);
  });
});

const blank: SlotTable = { native: [], pending: [], jsSeen: 0, cellWidth: -1, width: -1 };
const bound = (slot: number, key: string, index: number) => ({ ...row, slot, key, index });

describe('prerender', () => {
  it('renders the first rows and the header before native reports', () => {
    const keys = Array.from({ length: 30 }, (_, i) => `k${i}`);
    const out = prerender({
      table: blank,
      keys,
      header: true,
      footer: false,
      start: 0,
      nextId: -1,
    });
    expect(out.filter((e) => e.kind === SlotKind.row).map((e) => e.key)).toEqual(keys.slice(0, 10));
    expect(out.find((e) => e.kind === SlotKind.header)?.slot).toBeLessThan(0);
  });

  it('renders a new key where native shows rows', () => {
    const native = ['a', 'b', 'c'].map((key, index) => bound(index + 1, key, index));
    const out = prerender({
      table: { ...blank, native },
      keys: ['n', 'a', 'b', 'c'],
      header: false,
      footer: false,
      start: 0,
      nextId: -5,
    });
    expect(out).toEqual([
      { slot: -5, kind: SlotKind.row, key: 'n', index: 0, bound: true, minMain: 0 },
    ]);
  });

  it('renders at the indices native binds, not the gap a far bound row spans', () => {
    const native = [bound(1, 'a0', 0), bound(2, 'a300', 300), bound(3, 'a301', 301)];
    const keys = Array.from({ length: 500 }, (_, i) => `b${i}`);
    const out = prerender({
      table: { ...blank, native },
      keys,
      header: false,
      footer: false,
      start: 0,
      nextId: -1,
    });
    expect(out.map((e) => e.index)).toEqual([0, 300, 301]);
    expect(out.map((e) => e.slot)).toEqual([-1, -2, -3]);
  });

  it('skips keys without a React cell and keys that have a slot', () => {
    const pending = [{ ...bound(-1, 'k1', 1), bound: true }];
    const out = prerender({
      table: { ...blank, pending },
      keys: ['k0', 'k1', 'k2', 'k3'],
      header: false,
      footer: false,
      start: 0,
      nextId: -2,
      react: (key) => key !== 'k2',
    });
    expect(out.map((e) => e.key)).toEqual(['k0', 'k3']);
  });

  it('renders the header and footer only when the rows reach them', () => {
    const native = [bound(1, 'k100', 100), bound(2, 'k101', 101)];
    const keys = Array.from({ length: 500 }, (_, i) => `k${i}`);
    const middle = {
      table: { ...blank, native },
      keys,
      header: true,
      footer: true,
      start: 0,
      nextId: -1,
    };
    expect(prerender(middle)).toEqual([]);
    const emptied = prerender({ ...middle, keys: [] }).map((e) => e.kind);
    expect(emptied).toEqual([SlotKind.header, SlotKind.footer]);
    const short = prerender({ ...middle, table: blank, keys: ['k0', 'k1'] }).map((e) => e.kind);
    expect(short).toEqual([SlotKind.header, SlotKind.row, SlotKind.row, SlotKind.footer]);
  });
});

describe('applySnapshot', () => {
  it('keeps a pre-rendered slot until native has registered it', () => {
    const pending = [{ slot: -1, kind: SlotKind.row, key: 'a', index: 0, bound: true, minMain: 0 }];
    const table = { ...blank, pending };
    expect(
      applySnapshot(table, { slots: [], jsSeen: 0, cellWidth: 360, width: 720 }).pending,
    ).toEqual(pending);
    expect(
      applySnapshot(table, { slots: [], jsSeen: -1, cellWidth: 360, width: 720 }).pending,
    ).toEqual([]);
  });

  it('keeps the index a pre-rendered slot rendered with until native binds it', () => {
    const table = { ...blank, pending: [bound(-1, 'a', 4)] };
    const parked = { ...bound(-1, 'a', -1), bound: false };
    const next = applySnapshot(table, { slots: [parked], jsSeen: -1, cellWidth: -1, width: 360 });
    expect(next.native).toEqual([{ ...parked, index: 4 }]);
    const later = applySnapshot(next, { slots: [parked], jsSeen: -1, cellWidth: -1, width: 360 });
    expect(later.native[0]?.index).toBe(4);
    const adopted = applySnapshot(later, {
      slots: [bound(-1, 'a', 6)],
      jsSeen: -1,
      cellWidth: -1,
      width: 360,
    });
    expect(adopted.native[0]?.index).toBe(6);
  });
});
