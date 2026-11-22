import type { ViewStyle } from 'react-native';

// Mirrors SlotKinds on the native side, number for number.
export const SlotKind = { row: 0, header: 1, footer: 2, empty: 3, sticky: 4 } as const;

export type SlotEntry = {
  slot: number;
  kind: number;
  key: string;
  index: number;
  bound: boolean;
  minMain: number;
};

export type SlotLayout = {
  horizontal: boolean;
  numColumns: number;
  padding: { top: number; left: number; bottom: number; right: number };
  // A grid's lane insets from columnWrapperStyle, which native also gives its full-span data rows.
  lanes: { insetStart: number; insetEnd: number };
  cellWidth: number;
};

// A slot sits absolute in the host across the padded cross axis, or at the grid cell width native reports;
// null while that width is unknown. The holder places it along the main axis; the empty row gets a minimum length.
export function slotStyle(
  entry: SlotEntry,
  fullSpan: boolean,
  layout: SlotLayout,
): ViewStyle | null {
  const { padding } = layout;
  const min = entry.minMain > 0 ? entry.minMain : undefined;
  if (layout.horizontal) {
    const style: ViewStyle = { position: 'absolute', top: padding.top, bottom: padding.bottom };
    return min === undefined ? style : { ...style, minWidth: min };
  }
  if (layout.numColumns <= 1 || fullSpan) {
    const data =
      layout.numColumns > 1 && (entry.kind === SlotKind.row || entry.kind === SlotKind.sticky);
    const style: ViewStyle = {
      position: 'absolute',
      left: padding.left + (data ? layout.lanes.insetStart : 0),
      right: padding.right + (data ? layout.lanes.insetEnd : 0),
    };
    return min === undefined ? style : { ...style, minHeight: min };
  }
  if (layout.cellWidth < 0) return null;
  return { position: 'absolute', width: layout.cellWidth };
}

export const INITIAL_SLOTS = 10;

// Native's slots plus the pre-rendered ones it has not registered yet; ids below zero come from JS.
export type SlotTable = {
  native: readonly SlotEntry[];
  pending: readonly SlotEntry[];
  jsSeen: number;
  cellWidth: number;
  width: number;
};

export type SlotSnapshot = {
  slots: readonly SlotEntry[];
  jsSeen: number;
  cellWidth: number;
  width: number;
};

// A grid column from the list width native reported, so a numColumns change renders at the new width at once.
// Native's column wins when it differs by under a point in total (Android rounds the padding to whole pixels).
export function gridCellWidth(
  table: { width: number; cellWidth: number },
  numColumns: number,
  padding: { left: number; right: number },
  lanes: { insetStart: number; insetEnd: number; gap: number } = {
    insetStart: 0,
    insetEnd: 0,
    gap: 0,
  },
): number {
  if (table.width < 0) return -1;
  const row = table.width - padding.left - padding.right - lanes.insetStart - lanes.insetEnd;
  const computed = Math.max(0, row - lanes.gap * (numColumns - 1)) / numColumns;
  return Math.abs(table.cellWidth - computed) * numColumns < 1
    ? Math.max(0, table.cellWidth)
    : computed;
}

// A pending slot stays until native has registered it (jsSeen reaches its id); one native has not bound yet keeps
// the index it was rendered with.
export function applySnapshot(table: SlotTable, event: SlotSnapshot): SlotTable {
  const indices = new Map<number, number>();
  for (const entry of [...table.native, ...table.pending]) indices.set(entry.slot, entry.index);
  const native = event.slots.map((entry) => {
    const index = entry.index < 0 ? indices.get(entry.slot) : undefined;
    return index === undefined || index < 0 ? entry : { ...entry, index };
  });
  return {
    native,
    pending: table.pending.filter((entry) => entry.slot < event.jsSeen),
    jsSeen: event.jsSeen,
    cellWidth: event.cellWidth,
    width: event.width,
  };
}

export type PrerenderArgs = {
  table: SlotTable;
  keys: readonly string[];
  header: boolean;
  footer: boolean;
  start: number;
  nextId: number;
  // Whether a key renders as a React cell; every key does when absent.
  react?: (key: string) => boolean;
};

// The cells native is about to show that have no slot: the keys now at the indices of the rows native binds, else
// INITIAL_SLOTS rows from start; the header and footer once those rows reach them. Never the empty row.
export function prerender(args: PrerenderArgs): SlotEntry[] {
  const { table, keys, header, footer, start, nextId, react } = args;
  const rows = new Set<string>();
  let hasHeader = false;
  let hasFooter = false;
  for (const entry of [...table.native, ...table.pending]) {
    if (entry.kind === SlotKind.row) rows.add(entry.key);
    else if (entry.kind === SlotKind.header) hasHeader = true;
    else if (entry.kind === SlotKind.footer) hasFooter = true;
  }
  const shown = new Set<number>();
  for (const entry of table.native) {
    if (
      entry.kind === SlotKind.row &&
      entry.bound &&
      entry.index >= 0 &&
      entry.index < keys.length
    ) {
      shown.add(entry.index);
    }
  }
  if (shown.size === 0) {
    const from = Math.max(0, Math.min(start, keys.length - INITIAL_SLOTS));
    for (let index = from; index < Math.min(keys.length, from + INITIAL_SLOTS); index++)
      shown.add(index);
  }
  const window = [...shown].sort((a, b) => a - b);
  const out: SlotEntry[] = [];
  const add = (kind: number, key: string, index: number) =>
    out.push({ slot: nextId - out.length, kind, key, index, bound: true, minMain: 0 });
  if (header && !hasHeader && (keys.length === 0 || window[0] === 0))
    add(SlotKind.header, 'header', -1);
  for (const index of window) {
    const key = keys[index];
    if (key === undefined || rows.has(key) || react?.(key) === false) continue;
    rows.add(key);
    add(SlotKind.row, key, index);
  }
  if (footer && !hasFooter && (keys.length === 0 || window.at(-1) === keys.length - 1)) {
    add(SlotKind.footer, 'footer', -1);
  }
  return out;
}
