// The first index of the last grid row: rows at or past it get no separator. A full-span index ends a run of
// columns, and fullSpans is sorted. Mirrors flume::lastRowStart in cpp/core/SeparatorRule.h.
export function lastRowStart(
  count: number,
  numColumns: number,
  fullSpans: readonly number[],
): number {
  if (count <= 0) return 0;
  if (numColumns <= 1) return count - 1;
  let runStart = 0;
  for (let i = fullSpans.length - 1; i >= 0; i--) {
    const span = fullSpans[i] ?? -1;
    if (span >= count) continue;
    if (span === count - 1) return count - 1;
    runStart = span + 1;
    break;
  }
  const run = count - runStart;
  return count - (run % numColumns || numColumns);
}

export type SeparatorUpdate = (props: Record<string, unknown>) => void;

export type SeparatorLinks = {
  register(key: string, update: SeparatorUpdate): () => void;
  highlight(key: string, on: boolean): void;
  update(key: string, select: 'leading' | 'trailing', props: Record<string, unknown>): void;
};

type DataKeys = { readonly keys: readonly string[]; readonly index: ReadonlyMap<string, number> };

// FlatList's separators routing by key: a row reaches its own separator and the one of the row before it, read from
// the latest data when called. A key can have a row and a sticky copy, so each key holds a set.
export function createSeparatorLinks(data: { readonly current: DataKeys }): SeparatorLinks {
  const updates = new Map<string, Set<SeparatorUpdate>>();
  const send = (key: string | undefined, props: Record<string, unknown>) => {
    if (key === undefined) return;
    for (const update of updates.get(key) ?? []) update(props);
  };
  const before = (key: string): string | undefined => {
    const index = data.current.index.get(key);
    return index === undefined || index === 0 ? undefined : data.current.keys[index - 1];
  };
  return {
    register(key, update) {
      const set = updates.get(key) ?? new Set<SeparatorUpdate>();
      updates.set(key, set);
      set.add(update);
      return () => {
        set.delete(update);
        if (set.size === 0 && updates.get(key) === set) updates.delete(key);
      };
    },
    highlight(key, on) {
      send(key, { highlighted: on });
      send(before(key), { highlighted: on });
    },
    update(key, select, props) {
      send(select === 'leading' ? before(key) : key, props);
    },
  };
}
