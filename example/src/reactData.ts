export type ReactRow = {
  id: string;
  kind: 'row' | 'card' | 'swipe' | 'section';
  title: string;
  n: number;
};

export type Chip = { id: string; label: string };

const LABELS = { row: 'Row', card: 'Card', swipe: 'Swipe' } as const;

function kindAt(n: number): ReactRow['kind'] {
  if (n % 25 === 0) return 'section';
  if (n % 7 === 0) return 'card';
  if (n % 11 === 0) return 'swipe';
  return 'row';
}

// A section every 25 items, a card every 7, a swipeable row every 11; a new prefix gives the same rows new keys.
export function makeReactRows(count = 500, prefix = 'r'): ReactRow[] {
  return Array.from({ length: count }, (_, n) => {
    const kind = kindAt(n);
    const label = kind === 'section' ? `Section ${n / 25}` : `${LABELS[kind]} ${n}`;
    return { id: `${prefix}-${n}`, kind, title: prefix === 'r' ? label : `${label} ${prefix}`, n };
  });
}

export function sectionIndices(rows: readonly ReactRow[]): number[] {
  return rows.flatMap((row, index) => (row.kind === 'section' ? [index] : []));
}

export const CHIPS: Chip[] = [
  'All',
  'Unread',
  'Starred',
  'Photos',
  'Videos',
  'Documents',
  'Music',
  'Links',
  'Archived',
  'Drafts',
  'Sent',
  'Spam',
  'Travel',
  'Receipts',
  'Newsletters',
  'Work',
  'Family',
  'Friends',
  'Later',
  'Notes',
].map((label, i) => ({ id: `c${i}`, label }));
