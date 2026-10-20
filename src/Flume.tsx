import { type ReactElement, useEffect, useLayoutEffect, useRef, useState } from 'react';
import { createSerializable } from 'react-native-worklets';
import { type DataState, defaultKey, diffData } from './data';
import FlumeViewNativeComponent from './specs/FlumeViewNativeComponent';
import { getHost } from './store';
import type { FlumeProps } from './types';

export function Flume<T>({ data, renderCell, keyExtractor, style }: FlumeProps<T>): ReactElement {
  const host = getHost();
  const [storeId] = useState(() => host.createStore());
  const prev = useRef<DataState<T> | undefined>(undefined);

  useEffect(() => () => host.releaseStore(storeId), [host, storeId]);

  // Declared before the data effect so the cell is in place when the first data lands.
  useLayoutEffect(() => {
    // The native side extracts a SerializableJSRef, which only createSerializable produces.
    host.setCell(storeId, createSerializable(renderCell));
  }, [host, storeId, renderCell]);

  useLayoutEffect(() => {
    const diff = diffData(prev.current, data, keyExtractor ?? defaultKey);
    prev.current = diff.next;
    host.setData(storeId, diff.keys, diff.types, diff.changedIndices, diff.changedItems);
  }, [host, storeId, data, keyExtractor]);

  return <FlumeViewNativeComponent storeId={storeId} style={style} />;
}
