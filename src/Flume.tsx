import {
  type ComponentRef,
  forwardRef,
  type ReactElement,
  type Ref,
  useCallback,
  useEffect,
  useImperativeHandle,
  useLayoutEffect,
  useRef,
  useState,
} from 'react';
import { createSerializable } from 'react-native-worklets';
import { type DataState, diffData } from './data';
import { nativeProps } from './props';
import FlumeViewNativeComponent, {
  Commands,
  type NativeProps,
  type PressEvent,
  type ViewabilityEvent,
} from './specs/FlumeViewNativeComponent';
import { getHost, isDev } from './store';
import type { FlumeProps, FlumeRef, PressInfo, ViewToken } from './types';

function FlumeInner<T>(props: FlumeProps<T>, ref: Ref<FlumeRef>): ReactElement {
  const {
    data,
    renderCell,
    renderItem,
    keyExtractor,
    getItemType,
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
  } = props;
  const host = getHost();
  const [storeId, setStoreId] = useState(() => host.createStore());
  const released = useRef(false);
  const prev = useRef<{ storeId: number; state?: DataState<T> }>({ storeId });
  const warned = useRef({ duplicates: false, messages: new Set<string>() });
  const nativeRef = useRef<ComponentRef<typeof FlumeViewNativeComponent>>(null);
  const lastViewable = useRef(new Map<string, T>());
  const count = useRef(data.length);
  count.current = data.length;

  // Each distinct message warns once per mount, in development only.
  const warn = useCallback((message: string) => {
    if (!isDev || warned.current.messages.has(message)) return;
    warned.current.messages.add(message);
    console.warn(`react-native-flume: ${message}`);
  }, []);

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

  useLayoutEffect(() => {
    if (prev.current.storeId !== storeId) prev.current = { storeId };
    const diff = diffData(prev.current.state, data, keyExtractor, getItemType);
    prev.current.state = diff.next;
    if (isDev && diff.duplicates.length > 0 && !warned.current.duplicates) {
      warned.current.duplicates = true;
      console.warn(
        `react-native-flume: duplicate keys (${diff.duplicates.join(', ')}); later items got a numeric suffix`,
      );
    }
    host.setData(
      storeId,
      diff.keys,
      diff.types,
      diff.changedIndices,
      diff.changedItems,
      diff.stableKeys,
    );
  }, [host, storeId, data, keyExtractor, getItemType]);

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
          const item = prev.current.state?.byKey.get(key);
          if (item !== undefined) handler({ item, index, id: id === '' ? undefined : id });
        };

  // Native reports keys; the exit of an item that left the data resolves from the last viewable items.
  const viewability =
    onViewableItemsChanged === undefined
      ? undefined
      : (event: { nativeEvent: ViewabilityEvent }) => {
          const tokens = (entries: ViewabilityEvent['viewable']): ViewToken<T>[] =>
            entries.flatMap(({ index, key, isViewable }) => {
              const item = prev.current.state?.byKey.get(key) ?? lastViewable.current.get(key);
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
    />
  );
}

// forwardRef drops the generic; the cast gives callers the item type back.
export const Flume = forwardRef(FlumeInner) as <T>(
  props: FlumeProps<T> & { ref?: Ref<FlumeRef> },
) => ReactElement;
