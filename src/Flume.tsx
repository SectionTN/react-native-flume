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
  useInsertionEffect,
  useLayoutEffect,
  useMemo,
  useReducer,
  useRef,
  useState,
} from 'react';
import { type StyleProp, View, type ViewStyle } from 'react-native';
import { createSerializable } from 'react-native-worklets';
import { cellKinds, itemComponentCell, type ResolvedCell, resolveCell } from './cells';
import { createScrollCommands } from './commands';
import { type DataState, diffData, itemOf, type NativeItems, typeOf } from './data';
import { FlumeSlot } from './FlumeSlot';
import { hostProps } from './foreign';
import { useKeyboardEvent } from './keyboard';
import {
  contentPadding,
  gapsOf,
  nativeProps,
  refreshOf,
  resolvePadding,
  settleProps,
  stickyIndicesOf,
} from './props';
import { createSeparatorLinks, lastRowStart } from './separators';
import {
  applySnapshot,
  gridCellWidth,
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
import type {
  CellRendererProps,
  FlumeProps,
  FlumeRef,
  ListRef,
  PressInfo,
  ReactCell,
  ScrollResponder,
  SeparatorProps,
  ViewToken,
} from './types';

type NativeView = ComponentRef<typeof FlumeViewNativeComponent>;
type RecentCommand = { send: (native: NativeView, id: number) => void; index?: number };

// FlatList's rule for the list components: an element renders as given, a component through createElement.
// A given style wraps it in a View, as FlatList and FlashList do.
function componentElement(
  component: ComponentType<object> | ReactElement | null | undefined,
  style: StyleProp<ViewStyle> | null | undefined,
): ReactElement | null {
  if (component == null) return null;
  const element = isValidElement(component) ? component : createElement(component);
  return style == null ? element : createElement(View, { style }, element);
}

function FlumeInner<T>(given: FlumeProps<T>, ref: Ref<FlumeRef>): ReactElement {
  const props = settleProps(given);
  const {
    data,
    renderCell,
    renderSeparatorCell,
    renderItem: givenRenderItem,
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
    ListHeaderComponentStyle,
    ListFooterComponentStyle,
    ListEmptyComponentStyle,
    ItemSeparatorComponent,
    ListItemComponent,
    CellRendererComponent,
    inverted,
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
    onScrollToTop,
    onViewableItemsChanged,
    onEndReached,
    onStartReached,
    keyboardShouldPersistTaps,
  } = props;
  useKeyboardEvent('keyboardWillShow', props.onKeyboardWillShow);
  useKeyboardEvent('keyboardWillHide', props.onKeyboardWillHide);
  useKeyboardEvent('keyboardDidShow', props.onKeyboardDidShow);
  useKeyboardEvent('keyboardDidHide', props.onKeyboardDidHide);
  const refresh = refreshOf(props);
  const { onRefresh } = refresh;
  const host = getHost();
  // StrictMode calls a state initializer twice and drops one result, so the first store is reused.
  const firstStore = useRef<number | null>(null);
  const [storeId, setStoreId] = useState(() => {
    if (firstStore.current === null) firstStore.current = host.createStore();
    return firstStore.current;
  });
  const released = useRef(false);
  const prev = useRef<{ storeId: number; signature?: string; state?: DataState<T> }>({ storeId });
  const warned = useRef({ duplicates: false, messages: new Set<string>() });
  const nativeRef = useRef<NativeView>(null);
  // Native reports the end of each scroll command by its id, which settles the command's Promise.
  const [commands] = useState(createScrollCommands);
  // The last scroll command until the next frame, sent again after data committed meanwhile.
  const recent = useRef<RecentCommand | null>(null);
  // React Native's RefreshControl rule: a pull starts native refreshing, so an unchanged prop is sent back.
  const wantsRefreshing = refresh.refreshing;
  const nativeRefreshing = useRef(wantsRefreshing);
  const lastRefreshing = useRef(wantsRefreshing);
  const [, afterRefresh] = useReducer((n: number) => n + 1, 0);
  useEffect(() => {
    if (wantsRefreshing !== lastRefreshing.current) {
      lastRefreshing.current = wantsRefreshing;
      nativeRefreshing.current = wantsRefreshing;
    } else if (wantsRefreshing !== nativeRefreshing.current && nativeRef.current !== null) {
      Commands.setNativeRefreshing(nativeRef.current, wantsRefreshing);
      nativeRefreshing.current = wantsRefreshing;
    }
  });
  const lastViewable = useRef(new Map<string, T>());
  const count = useRef(data.length);
  count.current = data.length;
  const dataRef = useRef<readonly unknown[]>(data);
  dataRef.current = data;
  // The keys separators.highlight reads when called, so the row before is the latest one.
  const separatorKeys = useRef<{ keys: readonly string[]; index: ReadonlyMap<string, number> }>({
    keys: [],
    index: new Map(),
  });
  const links = useMemo(() => createSeparatorLinks(separatorKeys), []);

  // Each distinct message reports once per mount, in development only.
  const report = useCallback((level: 'warn' | 'error', message: string) => {
    if (!isDev || warned.current.messages.has(message)) return;
    warned.current.messages.add(message);
    console[level](`react-native-flume: ${message}`);
  }, []);
  const warn = useCallback((message: string) => report('warn', message), [report]);
  // Memoized on the component, so every slot keeps its memo while it stays the same.
  const itemCell = useMemo(
    () => (ListItemComponent == null ? undefined : itemComponentCell(ListItemComponent)),
    [ListItemComponent],
  );
  const renderItem = itemCell ?? givenRenderItem;
  if (itemCell !== undefined && givenRenderItem !== undefined) {
    warn('ListItemComponent takes precedence over renderItem');
  }
  if (CellRendererComponent != null && renderCell !== undefined) {
    warn('CellRendererComponent wraps React rows only; worklet rows ignore it');
  }
  if (ItemSeparatorComponent != null && renderCell !== undefined && renderSeparatorCell == null) {
    warn(
      'ItemSeparatorComponent draws on React rows only; give renderSeparatorCell for worklet rows',
    );
  }

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

  // The store that holds a separator worklet, so null is sent only to clear one.
  const separatorStore = useRef<number | null>(null);
  useLayoutEffect(() => {
    if (renderSeparatorCell != null) {
      host.setSeparatorCell(storeId, createSerializable(renderSeparatorCell));
      separatorStore.current = storeId;
    } else if (separatorStore.current === storeId) {
      host.setSeparatorCell(storeId, null);
      separatorStore.current = null;
    }
  }, [host, storeId, renderSeparatorCell]);

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
    // Null when stats were off for the diff, so a commit that turned them on logs no made-up time.
    const start = statsEnabled() ? performance.now() : null;
    // With renderItem and no renderCell every type is a React cell, so items are never compared.
    const current = latest.current;
    const native: NativeItems =
      current.renderCell === undefined && current.renderItem !== undefined ? 'none' : sendsItem;
    const diff = diffData(base, data, keyExtractor, getItemType, native);
    const diffMs = start === null ? null : performance.now() - start;
    return { kinds: latest.current.kinds, diff, missing, first: base === undefined, diffMs };
  }, [storeId, signature, data, keyExtractor, getItemType]);

  // The sticky indices go to native with the data they index, or alone when only they changed.
  const stickyIndices = useMemo(() => stickyIndicesOf(stickyHeaderIndices), [stickyHeaderIndices]);
  const stickyKey = stickyIndices.join(',');
  const latestSticky = useRef(stickyIndices);
  latestSticky.current = stickyIndices;
  const sentSticky = useRef<{ storeId: number; key: string } | null>(null);

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
    const timed = statsEnabled() && commit.diffMs !== null;
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
      latestSticky.current,
    );
    sentSticky.current = { storeId, key: latestSticky.current.join(',') };
    if (timed && commit.diffMs !== null)
      logSetData(
        diff.keys.length,
        diff.changedItems.length,
        commit.diffMs,
        performance.now() - start,
      );
    // A scroll from this frame reached native before this data, so it goes again to land in the new rows.
    const command = recent.current;
    const native = nativeRef.current;
    if (command !== null && native !== null) command.send(native, commands.move());
  }, [host, storeId, commit, report, commands]);

  useLayoutEffect(() => {
    const sent = sentSticky.current;
    if (sent !== null && sent.storeId === storeId && sent.key === stickyKey) return;
    host.setStickyHeaderIndices(storeId, latestSticky.current);
    sentSticky.current = { storeId, key: stickyKey };
  }, [host, storeId, stickyKey]);

  // Every slot native holds or keeps for scroll-back, plus the pre-rendered ones it has not registered yet;
  // each renders as one React cell.
  const [table, setTable] = useState<SlotTable>({
    native: [],
    pending: [],
    jsSeen: 0,
    cellWidth: -1,
    width: -1,
  });
  const nextSlot = useRef(-1);
  // React Native pools the event and nulls nativeEvent once this returns, before the updater runs.
  const onSlotsChanged = useCallback((event: { nativeEvent: SlotsEvent }) => {
    const snapshot = event.nativeEvent;
    setTable((current) => applySnapshot(current, snapshot));
  }, []);
  const padding = resolvePadding(contentPadding(props.contentContainerStyle), table.width);
  const gaps = gapsOf(props, horizontal, numColumns);
  const lanes = {
    insetStart: gaps.laneInsetStart,
    insetEnd: gaps.laneInsetEnd,
    gap: gaps.crossGap,
  };
  const cellWidth =
    !horizontal && numColumns > 1 ? gridCellWidth(table, numColumns, padding, lanes) : -1;
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
      lanes: { insetStart: lanes.insetStart, insetEnd: lanes.insetEnd },
      cellWidth,
    }),
    [
      horizontal,
      numColumns,
      padding.top,
      padding.left,
      padding.bottom,
      padding.right,
      lanes.insetStart,
      lanes.insetEnd,
      cellWidth,
    ],
  );
  const sticky = useMemo(() => new Set(stickyHeaderIndices), [stickyHeaderIndices]);
  const header = useMemo(
    () => componentElement(ListHeaderComponent, ListHeaderComponentStyle),
    [ListHeaderComponent, ListHeaderComponentStyle],
  );
  const footer = useMemo(
    () => componentElement(ListFooterComponent, ListFooterComponentStyle),
    [ListFooterComponent, ListFooterComponentStyle],
  );
  const empty = useMemo(
    () => componentElement(ListEmptyComponent, ListEmptyComponentStyle),
    [ListEmptyComponent, ListEmptyComponentStyle],
  );
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
  // Before every layout effect of the commit, and never from a render React throws away.
  useInsertionEffect(() => {
    separatorKeys.current = next;
  }, [next]);
  const lastStart = lastRowStart(next.keys.length, horizontal ? 1 : numColumns, stickyIndices);
  const cellRenderer = (CellRendererComponent ?? null) as ComponentType<
    CellRendererProps<unknown>
  > | null;
  const separator = (ItemSeparatorComponent ?? null) as
    | ComponentType<SeparatorProps<unknown>>
    | ReactElement
    | null;
  const cells = entries.map((entry) => {
    const type = typeOf(next, entry.key);
    const resolved = type === undefined ? undefined : resolveCell(type, latest.current);
    const row = entry.kind === SlotKind.row || entry.kind === SlotKind.sticky;
    // By key from the data, since native's table can lag it; a row that left keeps native's index.
    const dataIndex = row ? next.index.get(entry.key) : undefined;
    const index = dataIndex ?? entry.index;
    return (
      <FlumeSlot
        key={entry.slot}
        slot={entry.slot}
        kind={entry.kind}
        itemKey={entry.key}
        index={index}
        minMain={entry.minMain}
        fullSpan={entry.kind !== SlotKind.row || sticky.has(index)}
        separator={row ? separator : null}
        last={dataIndex === undefined || dataIndex >= lastStart}
        trailingItem={dataIndex === undefined ? undefined : next.items[dataIndex + 1]}
        inverted={inverted === true}
        layout={layout}
        item={itemOf(next, entry.key)}
        render={resolved?.kind === 'react' ? (resolved.render as ReactCell<unknown>) : undefined}
        type={type ?? 'default'}
        dataRef={dataRef}
        links={links}
        cellRenderer={row ? cellRenderer : null}
        element={elementFor(entry.kind)}
        extraData={extraData}
        keyboardTaps={keyboardShouldPersistTaps ?? 'never'}
      />
    );
  });

  // The index is checked once the frame ends, against the data a handler committed with the scroll.
  const sendCommand = useCallback(
    (send: RecentCommand['send'], index?: number, method = 'scrollToIndex'): Promise<void> => {
      const native = nativeRef.current;
      if (native === null) return Promise.resolve();
      const { id, done } = commands.start();
      send(native, id);
      const command: RecentCommand = { send, index };
      recent.current = command;
      requestAnimationFrame(() => {
        if (recent.current !== command) return;
        recent.current = null;
        if (index !== undefined && (index < 0 || index >= count.current)) {
          warn(`${method}: index ${index} is out of range for ${count.current} items`);
        }
      });
      return done;
    },
    [commands, warn],
  );
  const endCommand = useCallback(
    (event: { nativeEvent: { id: number } }) => commands.endThrough(event.nativeEvent.id),
    [commands],
  );
  // StrictMode replays this effect, so the tracker keeps working after endAll.
  useEffect(
    () => () => {
      recent.current = null;
      commands.endAll();
    },
    [commands],
  );

  // ScrollView's methods, for the scroll view refs FlashList and LegendList hand out and for getScrollResponder.
  const responder = useMemo<ScrollResponder>(
    () => ({
      scrollTo: (params) => {
        const native = nativeRef.current;
        if (native === null) return;
        // Native ends the command in flight, and a data commit must not send it again.
        recent.current = null;
        Commands.scrollTo(native, params?.x ?? 0, params?.y ?? 0, params?.animated ?? true);
      },
      scrollToEnd: (params) =>
        sendCommand((native, id) => Commands.scrollToEnd(native, id, params?.animated ?? true)),
      flashScrollIndicators: () => {
        if (nativeRef.current !== null) Commands.flashScrollIndicators(nativeRef.current);
      },
      getScrollableNode: () => nativeRef.current,
      getNativeScrollRef: () => nativeRef.current,
    }),
    [sendCommand],
  );
  useImperativeHandle(props.scrollViewRef, () => responder, [responder]);
  useImperativeHandle(props.refScrollView, () => responder, [responder]);

  // FlatList's defaults: animated, the item at the start edge; native clamps the index.
  useImperativeHandle(ref, () => {
    const toIndex = (
      index: number,
      animated: boolean,
      viewPosition: number,
      viewOffset: number,
      method?: string,
    ) =>
      sendCommand(
        (native, id) =>
          Commands.scrollToIndex(native, id, index, animated, viewPosition, viewOffset),
        index,
        method,
      );
    const intoView = (index: number, animated: boolean, viewOffset: number, method: string) =>
      sendCommand(
        (native, id) => Commands.scrollIndexIntoView(native, id, index, animated, viewOffset),
        index,
        method,
      );
    // FlatList finds the item by identity in the data it rendered last.
    const found = (item: unknown, scroll: (index: number) => Promise<void>) => {
      const index = dataRef.current.indexOf(item);
      return index < 0 ? Promise.resolve() : scroll(index);
    };
    return {
      scrollToIndex: ({ index, animated = true, viewPosition = 0, viewOffset = 0 }) =>
        toIndex(index, animated, viewPosition, viewOffset),
      scrollToOffset: ({ offset, animated = true }) =>
        sendCommand((native, id) => Commands.scrollToOffset(native, id, offset, animated)),
      scrollToEnd: responder.scrollToEnd,
      scrollToItem: ({ item, animated = true, viewPosition = 0, viewOffset = 0 }) =>
        found(item, (index) => toIndex(index, animated, viewPosition, viewOffset, 'scrollToItem')),
      scrollToTop: (params) =>
        sendCommand((native, id) =>
          Commands.scrollToOffset(native, id, 0, params?.animated ?? true),
        ),
      scrollIndexIntoView: ({ index, animated = true, viewOffset = 0 }) =>
        intoView(index, animated, viewOffset, 'scrollIndexIntoView'),
      scrollItemIntoView: ({ item, animated = true, viewOffset = 0 }) =>
        found(item, (index) => intoView(index, animated, viewOffset, 'scrollItemIntoView')),
      setNativeProps: (nativeProps) => nativeRef.current?.setNativeProps(nativeProps),
      flashScrollIndicators: responder.flashScrollIndicators,
      getScrollResponder: () => responder,
      getNativeScrollRef: () => nativeRef.current,
      getScrollableNode: () => nativeRef.current,
      getAnimatableRef: () => nativeRef.current,
    };
  }, [sendCommand, responder]);

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
      {...hostProps(given)}
      {...nativeProps(props, warn)}
      ref={nativeRef}
      onScrollCommandEnd={endCommand}
      storeId={storeId}
      itemAnimations={itemAnimations}
      onCellPress={press(onPress)}
      onCellLongPress={press(onLongPress)}
      style={style}
      onLayout={onLayout}
      testID={testID}
      accessibilityLabel={accessibilityLabel}
      onScroll={onScroll as NativeProps['onScroll']}
      onScrollBeginDrag={onScrollBeginDrag as NativeProps['onScrollBeginDrag']}
      onScrollEndDrag={onScrollEndDrag as NativeProps['onScrollEndDrag']}
      onMomentumScrollBegin={onMomentumScrollBegin as NativeProps['onMomentumScrollBegin']}
      onMomentumScrollEnd={onMomentumScrollEnd as NativeProps['onMomentumScrollEnd']}
      onScrollToTop={onScrollToTop as NativeProps['onScrollToTop']}
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
      onRefresh={
        onRefresh === undefined
          ? undefined
          : () => {
              nativeRefreshing.current = true;
              onRefresh();
              afterRefresh();
            }
      }
      onSlotsChanged={onSlotsChanged}
    >
      {cells}
    </FlumeViewNativeComponent>
  );
}

// forwardRef drops the generic, so the cast gives callers the item type back. The ref prop takes a ref typed for
// FlatList, FlashList or LegendList through the first signature, and ComponentRef reads Flume's handle from the last.
type FlumeComponent = {
  <T>(props: FlumeProps<T> & { ref?: Ref<ListRef<NoInfer<T>>> }): ReactElement;
  <T>(props: FlumeProps<T> & { ref?: Ref<FlumeRef<NoInfer<T>>> }): ReactElement;
};

export const Flume = forwardRef(FlumeInner) as FlumeComponent;
