import {
  type ComponentRef,
  type ComponentType,
  createElement,
  forwardRef,
  isValidElement,
  type ReactElement,
  type Ref,
  useCallback,
  useEffect,
  useImperativeHandle,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
} from 'react';
import { createSerializable } from 'react-native-worklets';
import { cellKinds, type ResolvedCell, resolveCell } from './cells';
import { type DataState, diffData, itemOf, type NativeItems, typeOf } from './data';
import { FlumeSlot } from './FlumeSlot';
import { contentPadding, nativeProps } from './props';
import {
  applySnapshot,
  prerender,
  type SlotEntry,
  SlotKind,
  type SlotLayout,
  type SlotTable,
} from './slots';
import FlumeViewNativeComponent, {
  Commands,
  type NativeProps,
  type PressEvent,
  type SlotsEvent,
  type ViewabilityEvent,
} from './specs/FlumeViewNativeComponent';
import { logSetData, statsEnabled } from './stats';
import { getHost, isDev } from './store';
import type { FlumeProps, FlumeRef, PressInfo, ReactCell, ViewToken } from './types';

// FlatList's rule for the list components: an element renders as given, a component through createElement.
function componentElement(
  component: ComponentType<object> | ReactElement | null | undefined,
): ReactElement | null {
  if (component == null) return null;
  return isValidElement(component) ? component : createElement(component);
}

function FlumeInner<T>(props: FlumeProps<T>, ref: Ref<FlumeRef>): ReactElement {
  const {
    data,
    renderCell,
    renderItem,
    reactCells,
    keyExtractor,
    getItemType,
    extraData,
    horizontal = false,
    numColumns = 1,
    stickyHeaderIndices,
    initialScrollIndex,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
    itemAnimations = true,
    onPress,
    onLongPress,
    style,
    onLayout,
    testID,
    accessibilityLabel,
    onScroll,
    onScrollBeginDrag,
    onScrollEndDrag,
    onMomentumScrollBegin,
    onMomentumScrollEnd,
    onViewableItemsChanged,
    onEndReached,
    onStartReached,
    onRefresh,
    keyboardShouldPersistTaps,
  } = props;
  const host = getHost();
  const [storeId, setStoreId] = useState(() => host.createStore());
  const released = useRef(false);
  const prev = useRef<{ storeId: number; signature?: string; state?: DataState<T> }>({ storeId });
  const warned = useRef({ duplicates: false, messages: new Set<string>() });
  const nativeRef = useRef<ComponentRef<typeof FlumeViewNativeComponent>>(null);
  const lastViewable = useRef(new Map<string, T>());
  const count = useRef(data.length);
  count.current = data.length;

  // Each distinct message reports once per mount, in development only.
  const report = useCallback((level: 'warn' | 'error', message: string) => {
    if (!isDev || warned.current.messages.has(message)) return;
    warned.current.messages.add(message);
    console[level](`react-native-flume: ${message}`);
  }, []);
  const warn = useCallback((message: string) => report('warn', message), [report]);

  if (renderCell !== undefined && renderItem !== undefined) {
    warn('renderItem is ignored when renderCell is given; use reactCells to mix the two');
  }

  // StrictMode replays this effect after its cleanup, so a released store gets replaced.
  useEffect(() => {
    if (released.current) {
      released.current = false;
      setStoreId(host.createStore());
      return;
    }
    return () => {
      released.current = true;
      host.releaseStore(storeId);
    };
  }, [host, storeId]);

  // Declared before the data effect so the cell is in place when the first data lands.
  useLayoutEffect(() => {
    // The native side extracts a SerializableJSRef, which only createSerializable produces.
    if (renderCell !== undefined) host.setCell(storeId, createSerializable(renderCell));
  }, [host, storeId, renderCell]);

  const kinds = cellKinds({ renderCell, reactCells });
  const prerendering = useRef(false);
  const latest = useRef({ renderCell, renderItem, reactCells, kinds });
  latest.current = { renderCell, renderItem, reactCells, kinds };
  const { signature } = kinds;

  // The diff runs during render so a React cell sees its new item in the commit that changes the data;
  // it starts over for a new store or new cell kinds, since native then needs every worklet item again.
  const commit = useMemo(() => {
    const last = prev.current;
    const base = last.storeId === storeId && last.signature === signature ? last.state : undefined;
    const kindOf = new Map<string, ResolvedCell<T>['kind']>();
    const missing: string[] = [];
    const sendsItem = (type: string) => {
      let kind = kindOf.get(type);
      if (kind === undefined) {
        kind = resolveCell(type, latest.current).kind;
        kindOf.set(type, kind);
        if (kind === 'none') missing.push(type);
      }
      return kind === 'worklet';
    };
    const start = statsEnabled() ? performance.now() : 0;
    // With renderItem and no renderCell every type is a React cell, so items are never compared.
    const current = latest.current;
    const native: NativeItems =
      current.renderCell === undefined && current.renderItem !== undefined ? 'none' : sendsItem;
    const diff = diffData(base, data, keyExtractor, getItemType, native);
    const diffMs = statsEnabled() ? performance.now() - start : 0;
    return { kinds: latest.current.kinds, diff, missing, first: base === undefined, diffMs };
  }, [storeId, signature, data, keyExtractor, getItemType]);

  useLayoutEffect(() => {
    const { kinds, diff, missing } = commit;
    if (prev.current.storeId !== storeId || prev.current.signature !== kinds.signature) {
      host.setCellKinds(storeId, kinds.reactByDefault, kinds.reactTypes);
    }
    prev.current = { storeId, signature: kinds.signature, state: diff.next };
    for (const type of missing) report('error', `no renderer for type "${type}"`);
    if (isDev && diff.duplicates.length > 0 && !warned.current.duplicates) {
      warned.current.duplicates = true;
      console.warn(
        `react-native-flume: duplicate keys (${diff.duplicates.join(', ')}); later items got a numeric suffix`,
      );
    }
    if (!commit.first && diff.sameForNative) return;
    const timed = statsEnabled();
    const start = timed ? performance.now() : 0;
    host.setData(
      storeId,
      diff.keys,
      diff.typeTable,
      diff.typeIds,
      diff.changedIndices,
      diff.changedItems,
      diff.stableKeys,
      !commit.first && prerendering.current,
    );
    if (timed)
      logSetData(
        diff.keys.length,
        diff.changedItems.length,
        commit.diffMs,
        performance.now() - start,
      );
  }, [host, storeId, commit, report]);

  // Every slot native holds or keeps for scroll-back, plus the pre-rendered ones it has not registered yet;
  // each renders as one React cell.
  const [table, setTable] = useState<SlotTable>({
    native: [],
    pending: [],
    jsSeen: 0,
    cellWidth: -1,
  });
  const nextSlot = useRef(-1);
  // React Native pools the event and nulls nativeEvent once this returns, before the updater runs.
  const onSlotsChanged = useCallback((event: { nativeEvent: SlotsEvent }) => {
    const snapshot = event.nativeEvent;
    setTable((current) => applySnapshot(current, snapshot));
  }, []);
  const padding = contentPadding(props.contentContainerStyle);
  const { cellWidth } = table;
  const layout = useMemo<SlotLayout>(
    () => ({
      horizontal,
      numColumns,
      padding: {
        top: padding.top,
        left: padding.left,
        bottom: padding.bottom,
        right: padding.right,
      },
      cellWidth,
    }),
    [horizontal, numColumns, padding.top, padding.left, padding.bottom, padding.right, cellWidth],
  );
  const sticky = useMemo(() => new Set(stickyHeaderIndices), [stickyHeaderIndices]);
  const header = useMemo(() => componentElement(ListHeaderComponent), [ListHeaderComponent]);
  const footer = useMemo(() => componentElement(ListFooterComponent), [ListFooterComponent]);
  const empty = useMemo(() => componentElement(ListEmptyComponent), [ListEmptyComponent]);
  const elementFor = (kind: number) =>
    kind === SlotKind.header
      ? header
      : kind === SlotKind.footer
        ? footer
        : kind === SlotKind.empty
          ? empty
          : null;
  // The cells native is about to show, rendered before it asks so their first frame has content; a grid cell
  // waits for its width.
  const gridWaiting = !horizontal && numColumns > 1 && cellWidth < 0;
  const fresh = useMemo(() => {
    const { next } = commit.diff;
    return prerender({
      table,
      keys: next.keys,
      header: header !== null,
      footer: footer !== null,
      start: initialScrollIndex ?? 0,
      nextId: nextSlot.current,
      react: (key) => {
        const type = typeOf(next, key);
        return (
          !gridWaiting && type !== undefined && resolveCell(type, latest.current).kind === 'react'
        );
      },
    });
  }, [table, commit, header, footer, initialScrollIndex, gridWaiting]);
  // React Native schedules the mount after the layout effects, so a data change that brings pre-rendered cells
  // waits in the store until native has registered them (first data waits for nothing: the list mounts with it).
  prerendering.current = fresh.length > 0;
  // StrictMode replays this effect, so an entry already pending is not added twice.
  useLayoutEffect(() => {
    const last = fresh.at(-1);
    if (last === undefined) return;
    nextSlot.current = Math.min(nextSlot.current, last.slot - 1);
    setTable((current) => {
      const known = new Set(current.pending.map((entry) => entry.slot));
      const added = fresh.filter((entry) => !known.has(entry.slot));
      return added.length === 0 ? current : { ...current, pending: [...current.pending, ...added] };
    });
  }, [fresh]);
  // Ordered by slot id, so an entry keeps its place among the host's children from pending to native.
  const entries = useMemo(() => {
    const bySlot = new Map<number, SlotEntry>();
    for (const entry of [...fresh, ...table.pending, ...table.native])
      bySlot.set(entry.slot, entry);
    return [...bySlot.values()].sort((a, b) => a.slot - b.slot);
  }, [fresh, table]);
  const { next } = commit.diff;
  const cells = entries.map((entry) => {
    const type = typeOf(next, entry.key);
    const resolved = type === undefined ? undefined : resolveCell(type, latest.current);
    return (
      <FlumeSlot
        key={entry.slot}
        slot={entry.slot}
        kind={entry.kind}
        itemKey={entry.key}
        index={entry.index}
        minMain={entry.minMain}
        fullSpan={entry.kind !== SlotKind.row || sticky.has(entry.index)}
        layout={layout}
        item={itemOf(next, entry.key)}
        render={resolved?.kind === 'react' ? (resolved.render as ReactCell<unknown>) : undefined}
        element={elementFor(entry.kind)}
        extraData={extraData}
        keyboardTaps={keyboardShouldPersistTaps ?? 'never'}
      />
    );
  });

  // FlatList's defaults: animated, the item at the start edge; native clamps the index.
  useImperativeHandle(
    ref,
    () => ({
      scrollToIndex: ({ index, animated = true, viewPosition = 0, viewOffset = 0 }) => {
        const native = nativeRef.current;
        if (native === null) return;
        if (index < 0 || index >= count.current) {
          warn(`scrollToIndex: index ${index} is out of range for ${count.current} items`);
        }
        Commands.scrollToIndex(native, index, animated, viewPosition, viewOffset);
      },
      scrollToOffset: ({ offset, animated = true }) => {
        const native = nativeRef.current;
        if (native !== null) Commands.scrollToOffset(native, offset, animated);
      },
      scrollToEnd: (params) => {
        const native = nativeRef.current;
        if (native !== null) Commands.scrollToEnd(native, params?.animated ?? true);
      },
      getScrollableNode: () => nativeRef.current,
      getAnimatableRef: () => nativeRef.current,
    }),
    [warn],
  );

  const press = (handler: ((info: PressInfo<T>) => void) | undefined) =>
    handler === undefined
      ? undefined
      : (event: { nativeEvent: PressEvent }) => {
          const { index, key, id } = event.nativeEvent;
          const item = itemOf(prev.current.state, key);
          if (item !== undefined) handler({ item, index, id: id === '' ? undefined : id });
        };

  // Native reports keys; the exit of an item that left the data resolves from the last viewable items.
  const viewability =
    onViewableItemsChanged === undefined
      ? undefined
      : (event: { nativeEvent: ViewabilityEvent }) => {
          const tokens = (entries: ViewabilityEvent['viewable']): ViewToken<T>[] =>
            entries.flatMap(({ index, key, isViewable }) => {
              const item = itemOf(prev.current.state, key) ?? lastViewable.current.get(key);
              return item === undefined ? [] : [{ item, key, index, isViewable }];
            });
          const viewableItems = tokens(event.nativeEvent.viewable);
          const changed = tokens(event.nativeEvent.changed);
          lastViewable.current = new Map(viewableItems.map((token) => [token.key, token.item]));
          onViewableItemsChanged({ viewableItems, changed });
        };

  return (
    <FlumeViewNativeComponent
      {...nativeProps(props, warn)}
      ref={nativeRef}
      storeId={storeId}
      itemAnimations={itemAnimations}
      onPress={press(onPress)}
      onLongPress={press(onLongPress)}
      style={style}
      onLayout={onLayout}
      testID={testID}
      accessibilityLabel={accessibilityLabel}
      onScroll={onScroll as NativeProps['onScroll']}
      onScrollBeginDrag={onScrollBeginDrag as NativeProps['onScrollBeginDrag']}
      onScrollEndDrag={onScrollEndDrag as NativeProps['onScrollEndDrag']}
      onMomentumScrollBegin={onMomentumScrollBegin as NativeProps['onMomentumScrollBegin']}
      onMomentumScrollEnd={onMomentumScrollEnd as NativeProps['onMomentumScrollEnd']}
      onViewableItemsChanged={viewability}
      onEndReached={
        onEndReached === undefined
          ? undefined
          : (event) => onEndReached({ distanceFromEnd: event.nativeEvent.distanceFromEnd })
      }
      onStartReached={
        onStartReached === undefined
          ? undefined
          : (event) => onStartReached({ distanceFromStart: event.nativeEvent.distanceFromStart })
      }
      onRefresh={onRefresh === undefined ? undefined : () => onRefresh()}
      onSlotsChanged={onSlotsChanged}
    >
      {cells}
    </FlumeViewNativeComponent>
  );
}

// forwardRef drops the generic; the cast gives callers the item type back.
export const Flume = forwardRef(FlumeInner) as <T>(
  props: FlumeProps<T> & { ref?: Ref<FlumeRef> },
) => ReactElement;
