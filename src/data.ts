export type KeyExtractor<T> = (item: T, index: number) => string;
export type TypeExtractor<T> = (item: T, index: number) => string | number | undefined;

// Index-aligned arrays plus one key-to-index map; look up by key through itemOf and typeOf.
export type DataState<T> = {
  keys: readonly string[];
  types: readonly string[];
  items: readonly T[];
  index: ReadonlyMap<string, number>;
};

// Which types render natively and so send their items: every type when absent, none with 'none'.
export type NativeItems = 'none' | ((type: string) => boolean);

export type DataDiff<T> = {
  keys: string[];
  types: string[];
  changedIndices: number[];
  changedItems: T[];
  next: DataState<T>;
  stableKeys: boolean;
  duplicates: string[];
  // Same keys, types and native items as the previous state, so native needs no call.
  sameForNative: boolean;
  // Native gets each distinct type once and an index per key, so no per-key string crosses JSI.
  typeTable: string[];
  typeIds: Int32Array;
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

export function itemOf<T>(state: DataState<T> | undefined, key: string): T | undefined {
  const index = state?.index.get(key);
  return index === undefined ? undefined : state?.items[index];
}

export function typeOf(state: DataState<unknown> | undefined, key: string): string | undefined {
  const index = state?.index.get(key);
  return index === undefined ? undefined : state?.types[index];
}

// Changed means a new key, a new reference or a new type; only items of native types are compared and sent.
// Keys are stable (so moves can animate) with a user extractor or when every item carries an id or key.
// A repeated key gets a numeric suffix so the native diff stays consistent; the wrapper warns in development.
export function diffData<T>(
  prev: DataState<T> | undefined,
  data: readonly T[],
  keyExtractor: KeyExtractor<T> | undefined,
  getItemType?: TypeExtractor<T>,
  sendsItem?: NativeItems,
): DataDiff<T> {
  return (
    scan(prev, data, keyExtractor, getItemType, sendsItem, false) ??
    (scan(prev, data, keyExtractor, getItemType, sendsItem, true) as DataDiff<T>)
  );
}

const SHORT_TABLE = 8;

// Without dedupe a repeated key leaves the map short and the caller scans again with dedupe, so unique data pays
// one map write per item.
function scan<T>(
  prev: DataState<T> | undefined,
  data: readonly T[],
  keyExtractor: KeyExtractor<T> | undefined,
  getItemType: TypeExtractor<T> | undefined,
  sendsItem: NativeItems | undefined,
  dedupe: boolean,
): DataDiff<T> | undefined {
  const extract = keyExtractor ?? defaultKey;
  const userKeys = keyExtractor !== undefined && keyExtractor !== defaultKey;
  const count = data.length;
  const keys = new Array<string>(count);
  const types = new Array<string>(count);
  const index = new Map<string, number>();
  const changedIndices: number[] = [];
  const changedItems: T[] = [];
  const duplicates: string[] = [];
  const sendsAll = sendsItem === undefined;
  const sendsNone = sendsItem === 'none';
  const typeTable: string[] = [];
  const typeIds = new Int32Array(count);
  const typeIndex = new Map<string, number>();
  const sendsById: boolean[] = [];
  let idType: string | undefined;
  let typeId = 0;
  let stableKeys = true;
  let same = prev !== undefined && prev.keys.length === count;
  for (let i = 0; i < count; i++) {
    const item = data[i] as T;
    let key: string;
    if (userKeys) {
      key = extract(item, i);
    } else {
      const own = ownKey(item);
      if (own === undefined) stableKeys = false;
      key = own ?? String(i);
    }
    if (dedupe && index.has(key)) {
      const base = key;
      let suffix = 2;
      while (index.has(`${base}:${suffix}`)) suffix++;
      if (suffix === 2) duplicates.push(base);
      key = `${base}:${suffix}`;
    }
    index.set(key, i);
    keys[i] = key;
    const raw = getItemType?.(item, i);
    const type = raw === undefined ? 'default' : typeof raw === 'string' ? raw : String(raw);
    types[i] = type;
    // Lists switch type often and have few types: a short table is searched in place, a long one through the map.
    if (type !== idType) {
      idType = type;
      typeId =
        typeTable.length <= SHORT_TABLE ? typeTable.indexOf(type) : (typeIndex.get(type) ?? -1);
      if (typeId === -1) {
        typeId = typeTable.push(type) - 1;
        typeIndex.set(type, typeId);
        sendsById.push(sendsAll || (!sendsNone && (sendsItem as (type: string) => boolean)(type)));
      }
    }
    typeIds[i] = typeId;
    if (same && (prev?.keys[i] !== key || prev?.types[i] !== type)) same = false;
    if (!sendsById[typeId]) continue;
    let changed = true;
    if (prev !== undefined) {
      const previous = prev.keys[i] === key ? i : prev.index.get(key);
      changed =
        previous === undefined || prev.items[previous] !== item || prev.types[previous] !== type;
    }
    if (changed) {
      changedIndices.push(i);
      changedItems.push(item);
    }
  }
  if (!dedupe && index.size !== count) return undefined;
  return {
    keys,
    types,
    changedIndices,
    changedItems,
    next: { keys, types, items: data, index },
    stableKeys,
    duplicates,
    sameForNative: same && changedIndices.length === 0,
    typeTable,
    typeIds,
  };
}
