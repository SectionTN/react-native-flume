import { useMemo, useRef, useState } from 'react';
import { Button, Pressable, StyleSheet, Text, TextInput, ToastAndroid, View } from 'react-native';
import { type CellInfo, Flume, type FlumeRef } from 'react-native-flume';

type Row = { id: string; n: number; v: number };
type Section = { id: string; section: number };
type Item = Row | Section;

const isSection = (item: Item): item is Section => 'section' in item;

// A sticky section header before every 25 rows.
function makeItems(count = 500): Item[] {
  const items: Item[] = [];
  for (let n = 0; n < count; n++) {
    if (n % 25 === 0) items.push({ id: `s${n / 25}`, section: n / 25 });
    items.push({ id: `r${n}`, n, v: 0 });
  }
  return items;
}

// Same rows under new keys, so every visible cell is a new React cell.
const renew = (items: Item[]) =>
  items.map((item) =>
    isSection(item) ? item : { ...item, id: `r${item.n}v${item.v + 1}`, v: item.v + 1 },
  );

function blockJs(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: rows native binds now have to wait for their React render.
  }
}

const toast = (message: string) => ToastAndroid.show(message, ToastAndroid.SHORT);

function RowCell({ item }: { item: Row }) {
  return (
    <View style={styles.row}>
      <View style={styles.main}>
        <Text style={styles.title}>
          Row {item.n}
          {item.v > 0 ? ` v${item.v}` : null}
        </Text>
        <Pressable style={styles.button} onPress={() => toast(`row ${item.n}`)}>
          <Text style={styles.buttonText}>Toast</Text>
        </Pressable>
      </View>
      {item.n % 3 === 0 ? (
        <Text style={styles.detail}>Every third row has a second line</Text>
      ) : null}
    </View>
  );
}

function SectionCell({ item }: { item: Section }) {
  return (
    <View style={styles.section}>
      <Text style={styles.sectionTitle}>Section {item.section}</Text>
      <Pressable style={styles.button} onPress={() => toast(`section ${item.section}`)}>
        <Text style={styles.buttonText}>Toast</Text>
      </Pressable>
    </View>
  );
}

const renderItem = ({ item }: CellInfo<Item>) =>
  isSection(item) ? <SectionCell item={item} /> : <RowCell item={item} />;

const itemType = (item: Item) => (isSection(item) ? 'section' : 'row');

function Header() {
  return (
    <View style={styles.header}>
      <Text style={styles.headerTitle}>React cells</Text>
      <TextInput placeholder="Search" style={styles.input} />
    </View>
  );
}

function Empty() {
  return (
    <View style={styles.empty}>
      <Text style={styles.emptyText}>Nothing here</Text>
    </View>
  );
}

export default function ReactScreen() {
  const all = useMemo(() => makeItems(), []);
  const [rows, setRows] = useState(all);
  const [pad, setPad] = useState(16);
  const sticky = useMemo(
    () => rows.flatMap((item, index) => (isSection(item) ? [index] : [])),
    [rows],
  );
  const [refreshing, setRefreshing] = useState(false);
  const list = useRef<FlumeRef>(null);
  const prepended = useRef(0);
  const prepend = () => {
    prepended.current += 1;
    const n = -prepended.current;
    setRows((current) => [{ id: `p${-n}`, n, v: 0 }, ...current]);
  };
  const footer = useMemo(() => <Text style={styles.footer}>End of list</Text>, []);
  const refresh = () => {
    setRefreshing(true);
    setTimeout(() => setRefreshing(false), 1000);
  };
  return (
    <View style={styles.screen}>
      <View style={styles.controls}>
        <Button title="Block JS 3 s" onPress={() => blockJs(3000)} />
        <Button title="Clear" onPress={() => setRows([])} />
        <Button title="Refill" onPress={() => setRows(all)} />
        <Button title="To 300" onPress={() => list.current?.scrollToIndex({ index: 300 })} />
        <Button title="Prepend" onPress={prepend} />
        <Button title="Replace all" onPress={() => setRows(renew)} />
        <Button
          title={`Padding ${pad === 16 ? 32 : 16}`}
          onPress={() => setPad((value) => (value === 16 ? 32 : 16))}
        />
      </View>
      <Flume
        ref={list}
        data={rows}
        renderItem={renderItem}
        getItemType={itemType}
        stickyHeaderIndices={sticky}
        contentContainerStyle={{ paddingHorizontal: pad }}
        estimatedItemSize={56}
        ListHeaderComponent={Header}
        ListFooterComponent={footer}
        ListEmptyComponent={Empty}
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
  main: {
    height: 56,
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#dddddd',
  },
  title: { flex: 1, fontSize: 16, color: '#111111' },
  detail: { height: 24, paddingHorizontal: 16, fontSize: 13, color: '#555555' },
  button: {
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 6,
    backgroundColor: '#0066ff',
  },
  buttonText: { color: '#ffffff', fontWeight: '600' },
  section: {
    height: 48,
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    backgroundColor: '#dde6fb',
  },
  sectionTitle: { flex: 1, fontSize: 15, fontWeight: '700', color: '#1b2a4a' },
  header: { padding: 16, gap: 8, backgroundColor: '#f4f6fb' },
  headerTitle: { fontSize: 20, fontWeight: '700', color: '#111111' },
  input: {
    borderWidth: 1,
    borderColor: '#cccccc',
    borderRadius: 6,
    paddingHorizontal: 12,
    height: 40,
  },
  footer: { padding: 24, textAlign: 'center', color: '#777777' },
  empty: { flex: 1, alignItems: 'center', justifyContent: 'center' },
  emptyText: { fontSize: 18, color: '#555555' },
});
