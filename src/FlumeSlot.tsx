import {
  type ComponentType,
  createElement,
  Fragment,
  isValidElement,
  memo,
  type ReactElement,
  type ReactNode,
  useLayoutEffect,
  useMemo,
  useRef,
} from 'react';
import { Keyboard, View } from 'react-native';
import { SlotKind, type SlotLayout, slotStyle } from './slots';
import FlumeSlotViewNativeComponent from './specs/FlumeSlotViewNativeComponent';
import type { ReactCell, SeparatorProps } from './types';

type FlumeSlotProps = {
  slot: number;
  kind: number;
  itemKey: string;
  index: number;
  minMain: number;
  fullSpan: boolean;
  layout: SlotLayout;
  item: unknown;
  render: ReactCell<unknown> | undefined;
  type: string;
  // One ref for the whole list, so a data change does not re-render every slot.
  dataRef: { readonly current: readonly unknown[] };
  element: ReactElement | null;
  extraData: unknown;
  keyboardTaps: 'always' | 'never' | 'handled';
  separator: ComponentType<SeparatorProps<unknown>> | ReactElement | null;
  // The row is in the last grid row, so it draws no separator.
  last: boolean;
  trailingItem: unknown;
  inverted: boolean;
};

type Shown = {
  item?: unknown;
  render?: ReactCell<unknown>;
  type?: string;
  data?: readonly unknown[];
  last?: boolean;
  trailingItem?: unknown;
  index?: number;
  fullSpan?: boolean;
};

// ScrollView's handled rule: a tap no child of the cell claims closes the keyboard.
const claimsTap = () => Keyboard.isVisible();
const closeKeyboard = () => Keyboard.dismiss();

// FlashList's horizontal cell: the row keeps its height and the separator stretches to it.
const SIDE_BY_SIDE = { flexDirection: 'row', alignSelf: 'flex-start' } as const;

// One React cell. A key that left the data keeps its last item and renderer while native still lists the slot.
export const FlumeSlot = memo(function FlumeSlot(props: FlumeSlotProps): ReactElement | null {
  const {
    slot,
    kind,
    itemKey,
    index,
    minMain,
    fullSpan,
    layout,
    item,
    render,
    type,
    dataRef,
    element,
    extraData,
    keyboardTaps,
    separator,
    last,
    trailingItem,
    inverted,
  } = props;
  const snapshot = useRef<Shown>({});
  // A row that left the data renders its exit with what it last committed, type and data included.
  const live = item !== undefined && render !== undefined;
  const shown: Shown = live
    ? { item, render, type, data: dataRef.current, last, trailingItem, index, fullSpan }
    : snapshot.current;
  // Native's index for a row that left can differ from the one it rendered with.
  const rowIndex = shown.index ?? index;
  const span = shown.fullSpan ?? fullSpan;
  // Written after the commit, so a render React discards cannot change the snapshot.
  useLayoutEffect(() => {
    if (live) snapshot.current = shown;
  });
  const cell = kind === SlotKind.row || kind === SlotKind.sticky;
  const target = kind === SlotKind.sticky ? 'StickyHeader' : 'Cell';
  // Apart from the separator, so a row that only gains or loses its separator does not render again.
  // biome-ignore lint/correctness/useExhaustiveDependencies: data is the data of the render that drew the row
  const rowContent = useMemo(
    () =>
      cell
        ? (shown.render?.({
            item: shown.item,
            index: rowIndex,
            target,
            extraData,
            data: shown.data ?? dataRef.current,
            type: shown.type ?? type,
          }) ?? null)
        : null,
    [cell, shown.render, shown.item, rowIndex, target, extraData, shown.type, type],
  );
  const entry = { slot, kind, key: itemKey, index: rowIndex, bound: true, minMain };
  const style = slotStyle(entry, span, layout);
  if (style === null) return null;
  const handled = keyboardTaps === 'handled';
  let content: ReactNode = cell ? rowContent : element;
  if (cell && separator !== null) {
    const drawn =
      shown.last === false
        ? isValidElement(separator)
          ? separator
          : createElement(separator, {
              leadingItem: shown.item,
              trailingItem: shown.trailingItem,
              highlighted: false,
            })
        : null;
    // Keyed, so the row keeps its state when its separator comes or goes.
    const parts = [
      createElement(Fragment, { key: 'row' }, rowContent),
      drawn === null ? null : createElement(Fragment, { key: 'separator' }, drawn),
    ];
    if (inverted) parts.reverse();
    content = layout.horizontal ? createElement(View, { style: SIDE_BY_SIDE }, parts) : parts;
  }
  return (
    <FlumeSlotViewNativeComponent
      slot={slot}
      itemKey={itemKey}
      kind={kind}
      style={style}
      onStartShouldSetResponder={handled ? claimsTap : undefined}
      onResponderRelease={handled ? closeKeyboard : undefined}
    >
      {content}
    </FlumeSlotViewNativeComponent>
  );
});
