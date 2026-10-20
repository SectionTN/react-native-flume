import { type ReactElement, useEffect, useLayoutEffect, useRef, useState } from 'react';
import { createSerializable } from 'react-native-worklets';
import { type DataState, diffData } from './data';
import FlumeViewNativeComponent, { type PressEvent } from './specs/FlumeViewNativeComponent';
import { getHost, isDev } from './store';
import type { FlumeProps, PressInfo } from './types';

export function Flume<T>({
  data,
  renderCell,
  renderItem,
  keyExtractor,
  getItemType,
  itemAnimations = true,
  onPress,
  onLongPress,
  style,
}: FlumeProps<T>): ReactElement {
  const host = getHost();
  const [storeId, setStoreId] = useState(() => host.createStore());
  const released = useRef(false);
  const prev = useRef<{ storeId: number; state?: DataState<T> }>({ storeId });
  const warned = useRef({ renderItem: false, duplicates: false });

  if (isDev && renderCell !== undefined && renderItem !== undefined && !warned.current.renderItem) {
    warned.current.renderItem = true;
    console.warn(
      'react-native-flume: renderItem is ignored when renderCell is given; use reactCells to mix the two',
    );
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

  const press = (handler: ((info: PressInfo<T>) => void) | undefined) =>
    handler === undefined
      ? undefined
      : (event: { nativeEvent: PressEvent }) => {
          const { index, key, id } = event.nativeEvent;
          const item = prev.current.state?.byKey.get(key);
          if (item !== undefined) handler({ item, index, id: id === '' ? undefined : id });
        };

  return (
    <FlumeViewNativeComponent
      storeId={storeId}
      itemAnimations={itemAnimations}
      onPress={press(onPress)}
      onLongPress={press(onLongPress)}
      style={style}
    />
  );
}
