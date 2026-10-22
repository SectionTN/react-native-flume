import { describe, expect, it } from 'vitest';
import { SlotKind, slotStyle } from '../slots';

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
