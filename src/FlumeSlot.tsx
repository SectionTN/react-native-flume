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
  useState,
} from 'react';
import { Keyboard, View } from 'react-native';
import type { SeparatorLinks } from './separators';
import { SlotKind, type SlotLayout, slotStyle } from './slots';
import FlumeSlotViewNativeComponent from './specs/FlumeSlotViewNativeComponent';
import type { CellRendererProps, ReactCell, SeparatorProps, Separators } from './types';

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
  links: SeparatorLinks;
  cellRenderer: ComponentType<CellRendererProps<unknown>> | null;
};

type SeparatorCellProps = {
  component: ComponentType<SeparatorProps<unknown>>;
  itemKey: string;
  links: SeparatorLinks;
  leadingItem: unknown;
  trailingItem: unknown;
};

// Holds what highlight and updateProps sent, so they re-render the separator alone.
const SeparatorCell = memo(function SeparatorCell(props: SeparatorCellProps): ReactElement {
  const { component, itemKey, links, leadingItem, trailingItem } = props;
  const [sent, setSent] = useState<Record<string, unknown>>({});
  useLayoutEffect(
    () => links.register(itemKey, (update) => setSent((current) => ({ ...current, ...update }))),
    [links, itemKey],
  );
  return createElement(component, { leadingItem, trailingItem, highlighted: false, ...sent });
});

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
const ROW = { flexDirection: 'row' } as const;

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
    links,
    cellRenderer,
  } = props;
  const separators = useMemo<Separators>(
    () => ({
      highlight: () => links.highlight(itemKey, true),
      unhighlight: () => links.highlight(itemKey, false),
      updateProps: (select, newProps) => links.update(itemKey, select, newProps),
    }),
    [links, itemKey],
  );
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
            separators,
            data: shown.data ?? dataRef.current,
            type: shown.type ?? type,
          }) ?? null)
        : null,
    [cell, shown.render, shown.item, rowIndex, target, extraData, separators, shown.type, type],
  );
  const entry = { slot, kind, key: itemKey, index: rowIndex, bound: true, minMain };
  const style = slotStyle(entry, span, layout);
  if (style === null) return null;
  const handled = keyboardTaps === 'handled';
  let content: ReactNode = element;
  if (cell) {
    const drawn =
      separator !== null && shown.last === false
        ? isValidElement(separator)
          ? separator
          : createElement(SeparatorCell, {
              component: separator,
              itemKey,
              links,
              leadingItem: shown.item,
              trailingItem: shown.trailingItem,
            })
        : null;
    // Keyed, so the row keeps its state when its separator or the separator prop comes or goes.
    const parts = [
      createElement(Fragment, { key: 'row' }, rowContent),
      drawn === null ? null : createElement(Fragment, { key: 'separator' }, drawn),
    ];
    if (inverted) parts.reverse();
    content =
      layout.horizontal && separator !== null && cellRenderer === null
        ? createElement(View, { style: SIDE_BY_SIDE }, parts)
        : parts;
  }
  // FlatList's cell renderer holds the row and its separator, laid out in a row when horizontal.
  if (cell && cellRenderer !== null) {
    content = createElement(
      cellRenderer,
      {
        cellKey: itemKey,
        index: rowIndex,
        item: shown.item,
        style: layout.horizontal ? ROW : undefined,
      },
      content,
    );
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
