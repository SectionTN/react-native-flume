import { type FunctionComponent, useCallback, useMemo, useState } from 'react';
import { Button, Text, TextInput, ToastAndroid, View } from 'react-native';
import { type CellNode, cell, Flume, type FlumeProps } from 'react-native-flume';
import Animated, {
  useAnimatedScrollHandler,
  useAnimatedStyle,
  useSharedValue,
} from 'react-native-reanimated';
import { scheduleOnRN } from 'react-native-worklets';

type Row = { id: string; title: string; body: string };

const HEADER_HEIGHT = 120;

// Reanimated reads the list's scroll events on the UI runtime, so the header follows them while JS is busy.
const RowFlume: FunctionComponent<FlumeProps<Row>> = Flume;
const AnimatedFlume = Animated.createAnimatedComponent(RowFlume);

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

function blockJs(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: the header must keep following the scroll while the JS thread is stuck here.
  }
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
  const [refreshing, setRefreshing] = useState(false);
  // A pull only counts here: the list must stop its spinner although refreshing never turns on.
  const [pulls, setPulls] = useState(0);
  const refreshForASecond = useCallback(() => {
    setRefreshing(true);
    setTimeout(() => setRefreshing(false), 1000);
  }, []);
  const offset = useSharedValue(0);
  const reportOffset = useCallback(
    (value: number) => setStatus((s) => ({ ...s, offset: value })),
    [],
  );
  const onScroll = useAnimatedScrollHandler((e) => {
    offset.value = e.contentOffset.y;
    scheduleOnRN(reportOffset, Math.round(e.contentOffset.y));
  });
  const headerStyle = useAnimatedStyle(() => ({
    transform: [{ translateY: -Math.min(Math.max(offset.value, 0), HEADER_HEIGHT) }],
  }));
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
        <Button title="Block JS 3 s" onPress={() => blockJs(3000)} />
        <Button title="Refresh 1 s" onPress={refreshForASecond} />
      </View>
      <TextInput
        placeholder="Search"
        style={{ marginHorizontal: 8, padding: 8, borderWidth: 1, borderColor: '#cccccc' }}
      />
      <Text style={{ padding: 8 }} testID="scroll-status">
        offset {status.offset} first viewable {status.first} pulls {pulls}
      </Text>
      <View style={{ flex: 1, overflow: 'hidden' }}>
        <Animated.View
          style={[
            {
              position: 'absolute',
              top: 0,
              left: 0,
              right: 0,
              height: HEADER_HEIGHT,
              zIndex: 1,
              justifyContent: 'center',
              alignItems: 'center',
              backgroundColor: '#0066ff',
            },
            headerStyle,
          ]}
        >
          <Text style={{ color: '#ffffff', fontSize: 20, fontWeight: '600' }}>
            Collapsing header
          </Text>
        </Animated.View>
        <AnimatedFlume
          data={rows}
          keyExtractor={(r) => r.id}
          renderCell={renderRow}
          contentContainerStyle={{ paddingTop: HEADER_HEIGHT }}
          onPress={({ item, id }) => toast(id === 'like' ? `like ${item.title}` : item.title)}
          keyboardShouldPersistTaps={TAPS[taps]}
          keyboardDismissMode={onDrag ? 'on-drag' : 'none'}
          scrollEnabled={scrollEnabled}
          decelerationRate={fast ? 'fast' : 'normal'}
          refreshing={refreshing}
          onRefresh={() => setPulls((n) => n + 1)}
          refreshControl={{ progressViewOffset: HEADER_HEIGHT }}
          scrollEventThrottle={16}
          onScroll={onScroll}
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
    </View>
  );
}
