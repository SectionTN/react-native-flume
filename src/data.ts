export type KeyExtractor<T> = (item: T, index: number) => string;
export type TypeExtractor<T> = (item: T, index: number) => string;

export type DataState<T> = {
  keys: string[];
  byKey: Map<string, T>;
  types: Map<string, string>;
};

export type DataDiff<T> = {
  keys: string[];
  types: string[];
  changedIndices: number[];
  changedItems: T[];
  next: DataState<T>;
  stableKeys: boolean;
  duplicates: string[];
};

function ownKey(item: unknown): string | undefined {
  if (item === null || typeof item !== 'object') return undefined;
  const record = item as { id?: unknown; key?: unknown };
  if (record.id !== undefined && record.id !== null) return String(record.id);
  if (record.key !== undefined && record.key !== null) return String(record.key);
  return undefined;
}

// FlatList's default: item.id, then item.key, then the index.
export function defaultKey(item: unknown, index: number): string {
  return ownKey(item) ?? String(index);
}

// Changed means a new key, a new reference or a new type; only items sendsItem accepts are sent.
// Keys are stable (so moves can animate) with a user extractor or when every item carries an id or key.
// A repeated key gets a numeric suffix so the native diff stays consistent; the wrapper warns in development.
export function diffData<T>(
  prev: DataState<T> | undefined,
  data: readonly T[],
  keyExtractor: KeyExtractor<T> | undefined,
  getItemType?: TypeExtractor<T>,
  sendsItem?: (type: string) => boolean,
): DataDiff<T> {
  const extract = keyExtractor ?? defaultKey;
  const userKeys = keyExtractor !== undefined && keyExtractor !== defaultKey;
  const keys = new Array<string>(data.length);
  const types = new Array<string>(data.length);
  const changedIndices: number[] = [];
  const changedItems: T[] = [];
  const byKey = new Map<string, T>();
  const typeByKey = new Map<string, string>();
  const seen = new Map<string, number>();
  const duplicates: string[] = [];
  let stableKeys = true;
  for (let i = 0; i < data.length; i++) {
    const item = data[i] as T;
    const base = extract(item, i);
    if (!userKeys && ownKey(item) === undefined) stableKeys = false;
    const repeats = seen.get(base) ?? 0;
    seen.set(base, repeats + 1);
    let key = base;
    if (repeats > 0) {
      if (repeats === 1) duplicates.push(base);
      key = `${base}:${repeats + 1}`;
    }
    keys[i] = key;
    const type = getItemType?.(item, i) ?? 'default';
    types[i] = type;
    byKey.set(key, item);
    typeByKey.set(key, type);
    const changed =
      prev === undefined || prev.byKey.get(key) !== item || prev.types.get(key) !== type;
    if (changed && (sendsItem === undefined || sendsItem(type))) {
      changedIndices.push(i);
      changedItems.push(item);
    }
  }
  return {
    keys,
    types,
    changedIndices,
    changedItems,
    next: { keys, byKey, types: typeByKey },
    stableKeys,
    duplicates,
  };
}
