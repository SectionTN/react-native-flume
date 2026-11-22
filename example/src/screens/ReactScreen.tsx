import { useEffect, useMemo, useRef, useState } from 'react';
import {
  Button,
  Platform,
  Pressable,
  StyleSheet,
  Text,
  TextInput,
  ToastAndroid,
  View,
} from 'react-native';
import {
  type CellInfo,
  type CellNode,
  cell,
  Flume,
  type FlumeRef,
  type SeparatorProps,
  type Separators,
} from 'react-native-flume';
import ReanimatedSwipeable from 'react-native-gesture-handler/ReanimatedSwipeable';
import { CHIPS, type Chip, makeReactRows, type ReactRow, sectionIndices } from '../reactData';

const TAPS = ['never', 'handled', 'always'] as const;
type Taps = (typeof TAPS)[number];

const nextTaps = (taps: Taps) => TAPS[(TAPS.indexOf(taps) + 1) % TAPS.length] ?? 'never';

// A plain divider, or one that a pressed row highlights with the one above it.
const LINES = ['off', 'line', 'highlight'] as const;
type Lines = (typeof LINES)[number];

const nextLines = (lines: Lines) => LINES[(LINES.indexOf(lines) + 1) % LINES.length] ?? 'off';

function blockJs(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: rows native binds now have to wait for their React render.
  }
}

// ToastAndroid shows nothing on iOS, so the last press shows in the screen's status line there.
let showPressed: (message: string) => void = () => {};

const toast = (message: string) => {
  if (Platform.OS === 'android') ToastAndroid.show(message, ToastAndroid.SHORT);
  showPressed(message);
};

function RowCell({
  row,
  tinted,
  separators,
}: {
  row: ReactRow;
  tinted: boolean;
  separators?: Separators;
}) {
  return (
    <View style={[styles.row, tinted && styles.tinted]}>
      <View style={styles.main}>
        <Text style={styles.title}>{row.title}</Text>
        <Pressable
          style={styles.button}
          onPress={() => toast(row.title)}
          onPressIn={separators?.highlight}
          onPressOut={separators?.unhighlight}
        >
          <Text style={styles.buttonText}>Toast</Text>
        </Pressable>
      </View>
      {row.n % 3 === 0 ? (
        <Text numberOfLines={1} style={styles.detail}>
          Every third row has a second line
        </Text>
      ) : null}
    </View>
  );
}

// The count lives in the cell, so it shows whether the cell's slot survived scrolling away.
function CardCell({ row, tinted }: { row: ReactRow; tinted: boolean }) {
  const [count, setCount] = useState(0);
  return (
    <View style={[styles.card, tinted && styles.tinted]}>
      <Text style={styles.title}>{row.title}</Text>
      <Text numberOfLines={1} style={styles.detail}>
        A card keeps its state while it is away
      </Text>
      <Pressable style={styles.button} onPress={() => setCount((value) => value + 1)}>
        <Text style={styles.buttonText}>Count {count}</Text>
      </Pressable>
    </View>
  );
}

function SwipeCell({ row, onDelete }: { row: ReactRow; onDelete: (id: string) => void }) {
  return (
    <ReanimatedSwipeable
      renderRightActions={() => (
        <Pressable style={styles.delete} onPress={() => onDelete(row.id)}>
          <Text style={styles.buttonText}>Delete</Text>
        </Pressable>
      )}
    >
      <View style={[styles.main, styles.swipe]}>
        <Text style={styles.title}>{row.title}</Text>
        <Text style={styles.hint}>Swipe left</Text>
      </View>
    </ReanimatedSwipeable>
  );
}

function SectionCell({ row }: { row: ReactRow }) {
  return (
    <View style={[styles.main, styles.section]}>
      <Text style={styles.sectionTitle}>{row.title}</Text>
      <Pressable style={styles.button} onPress={() => toast(row.title)}>
        <Text style={styles.buttonText}>Toast</Text>
      </Pressable>
    </View>
  );
}

// Mixed mode draws plain rows natively, between the React cards, swipeable rows and sections.
const renderPlain = (row: ReactRow): CellNode => {
  'worklet';
  return cell.view(
    { height: 56, flexDirection: 'row', alignItems: 'center', paddingHorizontal: 16 },
    [
      cell.text(row.title, { flex: 1, fontSize: 16, color: '#111111' }),
      cell.text('worklet', { fontSize: 12, color: '#777777' }),
    ],
  );
};

const kindOf = (row: ReactRow) => row.kind;

const renderChip = ({ item }: CellInfo<Chip>) => (
  <View style={styles.chip}>
    <Text style={styles.chipText}>{item.label}</Text>
  </View>
);

const Header = () => (
  <View style={styles.header}>
    <Text style={styles.headerTitle}>React cells</Text>
    <TextInput placeholder="Search" style={styles.input} />
  </View>
);

const Empty = () => (
  <View style={styles.empty}>
    <Text style={styles.emptyText}>Nothing here</Text>
  </View>
);

const Divider = ({ highlighted }: SeparatorProps<ReactRow>) => (
  <View style={[styles.divider, highlighted && styles.dividerOn]} />
);
const ChipDivider = () => <View style={styles.chipDivider} />;

export default function ReactScreen() {
  const all = useMemo(() => makeReactRows(), []);
  const [rows, setRows] = useState(all);
  const [mode, setMode] = useState<'react' | 'mixed'>('react');
  const [pad, setPad] = useState(16);
  const [columns, setColumns] = useState(1);
  const [inverted, setInverted] = useState(false);
  const [animated, setAnimated] = useState(false);
  const [highlight, setHighlight] = useState(false);
  const [taps, setTaps] = useState<Taps>('never');
  const [refreshing, setRefreshing] = useState(false);
  const [pressed, setPressed] = useState('none');
  const [lines, setLines] = useState<Lines>('off');
  const [styled, setStyled] = useState(false);
  useEffect(() => {
    showPressed = setPressed;
    return () => {
      showPressed = () => {};
    };
  }, []);
  const sticky = useMemo(() => sectionIndices(rows), [rows]);
  const list = useRef<FlumeRef>(null);
  const prepended = useRef(0);
  const version = useRef(0);
  const footer = useMemo(() => <Text style={styles.footer}>End of list</Text>, []);
  const remove = (id: string) => setRows((current) => current.filter((row) => row.id !== id));
  const prepend = () => {
    const n = -++prepended.current;
    setRows((current) => [{ id: `p${n}`, kind: 'row', title: `Row ${n}`, n }, ...current]);
  };
  const appended = useRef(0);
  const append = () => {
    const n = ++appended.current;
    setRows((current) => [...current, { id: `a${n}`, kind: 'row', title: `Appended ${n}`, n }]);
  };
  const refresh = () => {
    setRefreshing(true);
    setTimeout(() => setRefreshing(false), 1000);
  };
  const renderRow = ({ item, extraData, separators }: CellInfo<ReactRow>) => {
    const tinted = extraData === true && item.n % 2 === 0;
    if (item.kind === 'section') return <SectionCell row={item} />;
    if (item.kind === 'card') return <CardCell row={item} tinted={tinted} />;
    if (item.kind === 'swipe') return <SwipeCell row={item} onDelete={remove} />;
    return (
      <RowCell
        row={item}
        tinted={tinted}
        separators={lines === 'highlight' ? separators : undefined}
      />
    );
  };
  const mixed = { card: renderRow, swipe: renderRow, section: renderRow };
  const controls: [string, () => void][] = [
    [`Mode ${mode}`, () => setMode((value) => (value === 'react' ? 'mixed' : 'react'))],
    ['Block JS 3 s', () => blockJs(3000)],
    ['Prepend', prepend],
    ['Append', append],
    [`Separators ${nextLines(lines)}`, () => setLines(nextLines)],
    [`Styles ${styled ? 'off' : 'on'}`, () => setStyled((on) => !on)],
    ['Replace all', () => setRows(makeReactRows(500, `v${++version.current}`))],
    ['Clear', () => setRows([])],
    ['Refill', () => setRows(all)],
    ['To 300', () => list.current?.scrollToIndex({ index: 300, viewPosition: 1, animated })],
    ['To end', () => list.current?.scrollToEnd({ animated })],
    [`Animated ${animated ? 'on' : 'off'}`, () => setAnimated((on) => !on)],
    [`Columns ${columns === 1 ? 2 : 1}`, () => setColumns((value) => (value === 1 ? 2 : 1))],
    [inverted ? 'Upright' : 'Inverted', () => setInverted((on) => !on)],
    [`Taps ${taps}`, () => setTaps(nextTaps)],
    [`Padding ${pad === 16 ? 32 : 16}`, () => setPad((value) => (value === 16 ? 32 : 16))],
    [`Highlight ${highlight ? 'on' : 'off'}`, () => setHighlight((on) => !on)],
  ];
  return (
    <View style={styles.screen}>
      <View style={styles.controls}>
        {controls.map(([title, onPress]) => (
          <Button key={title} title={title} onPress={onPress} />
        ))}
      </View>
      {Platform.OS === 'ios' ? (
        <Text style={styles.status} testID="react-status">
          pressed {pressed}
        </Text>
      ) : null}
      <Flume
        data={CHIPS}
        renderItem={renderChip}
        ItemSeparatorComponent={lines === 'off' ? undefined : ChipDivider}
        horizontal
        showsHorizontalScrollIndicator={false}
        style={styles.chips}
      />
      <Flume
        ref={list}
        data={rows}
        renderItem={mode === 'react' ? renderRow : undefined}
        renderCell={mode === 'mixed' ? renderPlain : undefined}
        reactCells={mode === 'mixed' ? mixed : undefined}
        getItemType={kindOf}
        extraData={highlight}
        stickyHeaderIndices={sticky}
        contentContainerStyle={
          styled ? { padding: pad, backgroundColor: '#eef7ee' } : { padding: pad }
        }
        keyboardShouldPersistTaps={taps}
        estimatedItemSize={56}
        numColumns={columns}
        inverted={inverted}
        ItemSeparatorComponent={lines === 'off' ? undefined : Divider}
        ListHeaderComponent={Header}
        ListHeaderComponentStyle={styled ? styles.headerFrame : undefined}
        ListFooterComponent={footer}
        ListFooterComponentStyle={styled ? styles.footerFrame : undefined}
        ListEmptyComponent={Empty}
        ListEmptyComponentStyle={styled ? styles.emptyFrame : undefined}
        refreshing={refreshing}
        onRefresh={refresh}
        style={styles.screen}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1 },
  controls: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
    gap: 8,
    padding: 8,
  },
  row: { backgroundColor: '#ffffff' },
  tinted: { backgroundColor: '#fff4d6' },
  main: { height: 56, flexDirection: 'row', alignItems: 'center', paddingHorizontal: 16 },
  title: { flex: 1, fontSize: 16, color: '#111111' },
  detail: { height: 24, paddingHorizontal: 16, fontSize: 13, color: '#555555' },
  status: { paddingHorizontal: 16, paddingBottom: 4, fontSize: 13, color: '#555555' },
  button: {
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 6,
    backgroundColor: '#0066ff',
  },
  buttonText: { color: '#ffffff', fontWeight: '600' },
  card: { margin: 8, padding: 16, gap: 8, borderRadius: 12, backgroundColor: '#eef3ff' },
  swipe: { backgroundColor: '#f1fbf3' },
  hint: { fontSize: 12, color: '#3a7d4a' },
  delete: { width: 96, alignItems: 'center', justifyContent: 'center', backgroundColor: '#d93025' },
  section: { height: 48, backgroundColor: '#dde6fb' },
  sectionTitle: { flex: 1, fontSize: 15, fontWeight: '700', color: '#1b2a4a' },
  chips: { height: 64 },
  divider: { height: 1, backgroundColor: '#d0d0d8' },
  dividerOn: { height: 3, backgroundColor: '#0066ff' },
  headerFrame: { borderBottomWidth: 2, borderBottomColor: '#0066ff' },
  footerFrame: { backgroundColor: '#fff3d6' },
  emptyFrame: { flex: 1, backgroundColor: '#fdecec' },
  chipDivider: { width: 1, marginVertical: 20, backgroundColor: '#b0b0b8' },
  chip: {
    marginVertical: 12,
    marginHorizontal: 4,
    paddingHorizontal: 14,
    justifyContent: 'center',
    borderRadius: 20,
    backgroundColor: '#e8eaf0',
  },
  chipText: { fontSize: 14, color: '#1b2a4a' },
  header: { padding: 16, gap: 8, backgroundColor: '#f4f6fb' },
  headerTitle: { fontSize: 20, fontWeight: '700', color: '#111111' },
  input: { height: 40, paddingHorizontal: 12, borderWidth: 1, borderColor: '#cccccc' },
  footer: { padding: 24, textAlign: 'center', color: '#777777' },
  empty: { flex: 1, alignItems: 'center', justifyContent: 'center' },
  emptyText: { fontSize: 18, color: '#555555' },
});
