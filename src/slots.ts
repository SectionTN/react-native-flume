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
  cellWidth: number;
};

// A slot sits absolute in the host across the padded cross axis, or at the grid cell width native reports;
// null while that width is unknown. The holder places it along the main axis.
export function slotStyle(
  _entry: SlotEntry,
  fullSpan: boolean,
  layout: SlotLayout,
): ViewStyle | null {
  const { padding } = layout;
  if (layout.horizontal) return { position: 'absolute', top: padding.top, bottom: padding.bottom };
  if (layout.numColumns <= 1 || fullSpan) {
    return { position: 'absolute', left: padding.left, right: padding.right };
  }
  if (layout.cellWidth < 0) return null;
  return { position: 'absolute', width: layout.cellWidth };
}
