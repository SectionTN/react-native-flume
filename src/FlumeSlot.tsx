import { memo, type ReactElement, useRef } from 'react';
import { Keyboard } from 'react-native';
import { SlotKind, type SlotLayout, slotStyle } from './slots';
import FlumeSlotViewNativeComponent from './specs/FlumeSlotViewNativeComponent';
import type { ReactCell } from './types';

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
  element: ReactElement | null;
  extraData: unknown;
  keyboardTaps: 'always' | 'never' | 'handled';
};

// ScrollView's handled rule: a tap no child of the cell claims closes the keyboard.
const claimsTap = () => Keyboard.isVisible();
const closeKeyboard = () => Keyboard.dismiss();

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
    element,
    extraData,
    keyboardTaps,
  } = props;
  const last = useRef<{ item?: unknown; render?: ReactCell<unknown> }>({});
  if (item !== undefined && render !== undefined) last.current = { item, render };
  const entry = { slot, kind, key: itemKey, index, bound: true, minMain };
  const style = slotStyle(entry, fullSpan, layout);
  if (style === null) return null;
  const cell = kind === SlotKind.row || kind === SlotKind.sticky;
  const handled = keyboardTaps === 'handled';
  const content = cell
    ? (last.current.render?.({
        item: last.current.item,
        index,
        target: kind === SlotKind.sticky ? 'StickyHeader' : 'Cell',
        extraData,
      }) ?? null)
    : element;
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
