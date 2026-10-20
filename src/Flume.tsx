import { type ReactElement, useEffect, useLayoutEffect, useRef, useState } from 'react';
import { createSerializable } from 'react-native-worklets';
import { type DataState, defaultKey, diffData } from './data';
import FlumeViewNativeComponent from './specs/FlumeViewNativeComponent';
import { getHost } from './store';
import type { FlumeProps } from './types';

export function Flume<T>({ data, renderCell, keyExtractor, style }: FlumeProps<T>): ReactElement {
  const host = getHost();
  const [storeId, setStoreId] = useState(() => host.createStore());
  const released = useRef(false);
  const prev = useRef<{ storeId: number; state?: DataState<T> }>({ storeId });

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
    host.setCell(storeId, createSerializable(renderCell));
  }, [host, storeId, renderCell]);

  useLayoutEffect(() => {
    if (prev.current.storeId !== storeId) prev.current = { storeId };
    const diff = diffData(prev.current.state, data, keyExtractor ?? defaultKey);
    prev.current.state = diff.next;
    host.setData(
      storeId,
      diff.keys,
      diff.types,
      diff.changedIndices,
      diff.changedItems,
      keyExtractor !== undefined,
    );
  }, [host, storeId, data, keyExtractor]);

  return <FlumeViewNativeComponent storeId={storeId} style={style} />;
}
