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
