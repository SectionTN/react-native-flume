import { useMemo } from 'react';
import { Button, Pressable, StyleSheet, Text, ToastAndroid, View } from 'react-native';
import { type CellInfo, Flume } from 'react-native-flume';

type Row = { id: string; n: number };

function makeRows(count = 500): Row[] {
  return Array.from({ length: count }, (_, n) => ({ id: `r${n}`, n }));
}

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
        <Text style={styles.title}>Row {item.n}</Text>
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

const renderItem = ({ item }: CellInfo<Row>) => <RowCell item={item} />;

export default function ReactScreen() {
  const rows = useMemo(() => makeRows(), []);
  return (
    <View style={styles.screen}>
      <View style={styles.controls}>
        <Button title="Block JS 3 s" onPress={() => blockJs(3000)} />
      </View>
      <Flume data={rows} renderItem={renderItem} estimatedItemSize={56} style={styles.screen} />
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
});
