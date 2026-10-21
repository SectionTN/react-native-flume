import { useMemo, useState } from 'react';
import { Button, Text, TextInput, ToastAndroid, View } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

type Row = { id: string; title: string; body: string };

const BODIES = [
  'A short line.',
  'Two lines of body text that wrap once on a phone screen at the default size.',
  'Three lines of body text that keep going past the second wrap so the row grows a little taller than the others around it.',
];

function makeRows(count = 500): Row[] {
  return Array.from({ length: count }, (_, i) => ({
    id: `r${i}`,
    title: `Row ${i}`,
    body: BODIES[i % 3] ?? '',
  }));
}

const renderRow = (item: Row): CellNode => {
  'worklet';
  return cell.view({ flexDirection: 'row', alignItems: 'center', padding: 12, gap: 12 }, [
    cell.view({ flex: 1, gap: 4 }, [
      cell.text(item.title, { fontSize: 16, fontWeight: '600', color: '#111111' }),
      cell.text(item.body, { fontSize: 13, color: '#555555' }),
    ]),
    cell.pressable({ id: 'like', padding: 8 }, [
      cell.text('Like', { color: '#0066ff', fontWeight: '600' }),
    ]),
  ]);
};

const toast = (message: string) => ToastAndroid.show(message, ToastAndroid.SHORT);

const TAPS = ['never', 'handled', 'always'] as const;

export default function ScrollScreen() {
  const rows = useMemo(() => makeRows(), []);
  const [status, setStatus] = useState({ offset: 0, first: -1 });
  const [taps, setTaps] = useState(0);
  const [onDrag, setOnDrag] = useState(false);
  const [scrollEnabled, setScrollEnabled] = useState(true);
  const [fast, setFast] = useState(false);
  return (
    <View style={{ flex: 1 }}>
      <View
        style={{
          flexDirection: 'row',
          flexWrap: 'wrap',
          justifyContent: 'center',
          gap: 8,
          padding: 8,
        }}
      >
        <Button title={`Taps: ${TAPS[taps]}`} onPress={() => setTaps((t) => (t + 1) % 3)} />
        <Button
          title={`Dismiss: ${onDrag ? 'on-drag' : 'none'}`}
          onPress={() => setOnDrag((v) => !v)}
        />
        <Button
          title={`Scroll: ${scrollEnabled ? 'on' : 'off'}`}
          onPress={() => setScrollEnabled((v) => !v)}
        />
        <Button title={`Rate: ${fast ? 'fast' : 'normal'}`} onPress={() => setFast((v) => !v)} />
      </View>
      <TextInput
        placeholder="Search"
        style={{ marginHorizontal: 8, padding: 8, borderWidth: 1, borderColor: '#cccccc' }}
      />
      <Text style={{ padding: 8 }} testID="scroll-status">
        offset {status.offset} first viewable {status.first}
      </Text>
      <Flume
        data={rows}
        keyExtractor={(r) => r.id}
        renderCell={renderRow}
        onPress={({ item, id }) => toast(id === 'like' ? `like ${item.title}` : item.title)}
        keyboardShouldPersistTaps={TAPS[taps]}
        keyboardDismissMode={onDrag ? 'on-drag' : 'none'}
        scrollEnabled={scrollEnabled}
        decelerationRate={fast ? 'fast' : 'normal'}
        scrollEventThrottle={16}
        onScroll={(e) => {
          // React Native still pools synthetic events, so the payload is read before the state update runs.
          const offset = Math.round(e.nativeEvent.contentOffset.y);
          setStatus((s) => ({ ...s, offset }));
        }}
        onScrollBeginDrag={() => toast('begin drag')}
        onScrollEndDrag={() => toast('end drag')}
        onMomentumScrollBegin={() => toast('momentum begin')}
        onMomentumScrollEnd={() => toast('momentum end')}
        viewabilityConfig={{ itemVisiblePercentThreshold: 50 }}
        onViewableItemsChanged={({ viewableItems }) =>
          setStatus((s) => ({ ...s, first: viewableItems[0]?.index ?? -1 }))
        }
        style={{ flex: 1 }}
      />
    </View>
  );
}
