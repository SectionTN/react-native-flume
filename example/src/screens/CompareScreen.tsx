import { useMemo, useState } from 'react';
import { Button, FlatList, Text, View, type ViewStyle } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

type Item = { id: string; n: number };
type Case = {
  name: string;
  tile: ViewStyle;
  uneven?: boolean;
  columnWrapperStyle?: ViewStyle;
  contentContainerStyle?: ViewStyle;
};

const FIRST: Case = { name: 'flex 1', tile: { flex: 1 } };

const CASES: Case[] = [
  FIRST,
  { name: 'flex 1, margin 4', tile: { flex: 1, margin: 4 } },
  { name: "width '33%'", tile: { width: '33%' } },
  { name: "flex 1, maxWidth '33%'", tile: { flex: 1, maxWidth: '33%' } },
  { name: 'width 100', tile: { width: 100 } },
  { name: 'aspectRatio 1', tile: { aspectRatio: 1 } },
  { name: 'wrapper gap 8', tile: { flex: 1 }, columnWrapperStyle: { gap: 8 } },
  { name: 'wrapper padding 16', tile: { flex: 1 }, columnWrapperStyle: { paddingHorizontal: 16 } },
  {
    name: 'wrapper space-between',
    tile: { width: 100 },
    columnWrapperStyle: { justifyContent: 'space-between' },
  },
  {
    name: 'content padding 8, gap 8',
    tile: { flex: 1 },
    contentContainerStyle: { padding: 8, gap: 8 },
  },
  { name: 'uneven heights', tile: { flex: 1 }, uneven: true },
];

const IMPLS = ['FlatList', 'Flume React', 'Flume worklet'] as const;
type Impl = (typeof IMPLS)[number];

const COLORS = ['#ef9a9a', '#90caf9', '#a5d6a7'];
const DATA: Item[] = Array.from({ length: 10 }, (_, n) => ({ id: `i${n}`, n }));

// The tile's own height: aspectRatio decides it in that case, the row's index otherwise.
function tileStyle(c: Case, n: number): ViewStyle {
  const height = c.tile.aspectRatio !== undefined ? undefined : c.uneven ? 40 + (n % 3) * 20 : 60;
  return {
    ...c.tile,
    ...(height === undefined ? {} : { height }),
    backgroundColor: COLORS[n % 3],
    justifyContent: 'center',
    alignItems: 'center',
  };
}

const Divider = () => <View style={{ height: 4, backgroundColor: '#333333' }} />;

// The same data and props through FlatList and both Flume row kinds, one at a time at the same place.
export default function CompareScreen() {
  const [index, setIndex] = useState(0);
  const [impl, setImpl] = useState<Impl>('FlatList');
  const [separated, setSeparated] = useState(false);
  const c = CASES[index] ?? FIRST;
  const renderItem = ({ item }: { item: Item }) => (
    <View
      testID={`tile-${item.n}`}
      accessible
      accessibilityLabel={`tile-${item.n}`}
      style={tileStyle(c, item.n)}
    >
      <Text>{item.n}</Text>
    </View>
  );
  const styles = useMemo(() => DATA.map((item) => tileStyle(c, item.n)), [c]);
  const renderCell = useMemo(
    () =>
      (item: Item): CellNode => {
        'worklet';
        return {
          type: 'view',
          testID: `tile-${item.n}`,
          accessible: true,
          accessibilityLabel: `tile-${item.n}`,
          style: styles[item.n] as CellNode['style'],
          children: [cell.text(String(item.n))],
        };
      },
    [styles],
  );
  const renderSeparatorCell = () => {
    'worklet';
    return cell.view({ height: 4, backgroundColor: '#333333' });
  };
  const shared = {
    data: DATA,
    numColumns: 3,
    keyExtractor: (item: Item) => item.id,
    contentContainerStyle: c.contentContainerStyle,
    columnWrapperStyle: c.columnWrapperStyle,
    ItemSeparatorComponent: separated ? Divider : undefined,
    style: { flex: 1 },
  };
  return (
    <View style={{ flex: 1 }}>
      <View
        style={{ flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'center', padding: 4 }}
      >
        <Button
          title="Prev"
          onPress={() => setIndex((i) => (i + CASES.length - 1) % CASES.length)}
        />
        <Button title="Next" onPress={() => setIndex((i) => (i + 1) % CASES.length)} />
        <Button
          title={impl}
          onPress={() => setImpl((v) => IMPLS[(IMPLS.indexOf(v) + 1) % IMPLS.length] ?? 'FlatList')}
        />
        <Button
          title={separated ? 'Separators on' : 'Separators off'}
          onPress={() => setSeparated((s) => !s)}
        />
      </View>
      <Text testID="compare-case" style={{ textAlign: 'center', paddingBottom: 4 }}>
        {`${index + 1}/${CASES.length} ${c.name}`}
      </Text>
      {impl === 'FlatList' ? (
        <FlatList key={`flat-${index}`} {...shared} renderItem={renderItem} />
      ) : impl === 'Flume React' ? (
        <Flume key={`react-${index}`} {...shared} renderItem={renderItem} />
      ) : (
        <Flume
          key={`worklet-${index}`}
          {...shared}
          renderCell={renderCell}
          renderSeparatorCell={separated ? renderSeparatorCell : null}
        />
      )}
    </View>
  );
}
