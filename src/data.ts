export type KeyExtractor<T> = (item: T, index: number) => string;

export type DataState<T> = {
  keys: string[];
  byKey: Map<string, T>;
};

export type DataDiff<T> = {
  keys: string[];
  types: string[];
  changedIndices: number[];
  changedItems: T[];
  next: DataState<T>;
};

// FlatList's default: item.id, then item.key, then the index.
export function defaultKey(item: unknown, index: number): string {
  if (item !== null && typeof item === 'object') {
    const record = item as { id?: unknown; key?: unknown };
    if (record.id !== undefined && record.id !== null) return String(record.id);
    if (record.key !== undefined && record.key !== null) return String(record.key);
  }
  return String(index);
}

// An item is changed when its key is new or its reference differs from the previous item of that key.
export function diffData<T>(
  prev: DataState<T> | undefined,
  data: readonly T[],
  keyExtractor: KeyExtractor<T>,
): DataDiff<T> {
  const keys = new Array<string>(data.length);
  const types = new Array<string>(data.length);
  const changedIndices: number[] = [];
  const changedItems: T[] = [];
  const byKey = new Map<string, T>();
  for (let i = 0; i < data.length; i++) {
    const item = data[i] as T;
    const key = keyExtractor(item, i);
    keys[i] = key;
    types[i] = 'default';
    byKey.set(key, item);
    if (prev === undefined || prev.byKey.get(key) !== item) {
      changedIndices.push(i);
      changedItems.push(item);
    }
  }
  return { keys, types, changedIndices, changedItems, next: { keys, byKey } };
}
