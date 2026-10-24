import { LegendList } from '@legendapp/list/react-native';
import type { BottomTabScreenProps } from '@react-navigation/bottom-tabs';
import type { ParamListBase } from '@react-navigation/native';
import { FlashList } from '@shopify/flash-list';
import { type ComponentType, useCallback, useEffect, useRef, useState } from 'react';
import { Button, type HostInstance, PixelRatio, StyleSheet, Text, View } from 'react-native';
import {
  type CellNode,
  Flume,
  unstable_logFlumeStats,
  unstable_setFlumeStats,
} from 'react-native-flume';
import { chatCell, feedCell } from '../bench/cells';
import {
  type BenchAction,
  type Impl,
  parseBenchParams,
  readyLine,
  type Scenario,
  SENTINEL,
} from '../bench/config';
import {
  type BenchChat,
  changeChats,
  type FeedPost,
  makeBenchChats,
  makeFeed,
} from '../bench/data';
import { ChatRow, FeedRow } from '../bench/rows';

function busy(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: JS cells starve while the thread is stuck here.
  }
}

type Item = { id: string };

type ListProps<T extends Item> = {
  impl: Impl;
  chat: boolean;
  data: T[];
  renderCell: (item: T, index: number) => CellNode;
  Row: ComponentType<{ item: T }>;
  getItemType: (item: T, index: number) => string;
};

const keyExtractor = (item: Item) => item.id;
const chatType = (item: BenchChat) => item.kind;
const postType = (item: FeedPost) => (item.photo !== undefined ? 'photo' : 'text');
const fromBottom = { startRenderingFromBottom: true } as const;

function BenchList<T extends Item>({
  impl,
  chat,
  data,
  renderCell,
  Row,
  getItemType,
}: ListProps<T>) {
  const renderItem = useCallback(({ item }: { item: T }) => <Row item={item} />, [Row]);
  switch (impl) {
    case 'flume':
      return (
        <Flume
          data={data}
          renderCell={renderCell}
          keyExtractor={keyExtractor}
          getItemType={getItemType}
          maintainVisibleContentPosition={chat ? fromBottom : undefined}
          style={styles.fill}
        />
      );
    case 'flume-react':
      return (
        <Flume
          data={data}
          renderItem={renderItem}
          keyExtractor={keyExtractor}
          getItemType={getItemType}
          maintainVisibleContentPosition={chat ? fromBottom : undefined}
          style={styles.fill}
        />
      );
    case 'flashlist':
      return (
        <FlashList
          data={data}
          renderItem={renderItem}
          keyExtractor={keyExtractor}
          getItemType={getItemType}
          maintainVisibleContentPosition={chat ? fromBottom : undefined}
        />
      );
    case 'legendlist':
      return (
        <LegendList
          data={data}
          renderItem={renderItem}
          keyExtractor={keyExtractor}
          getItemType={getItemType}
          recycleItems
          maintainVisibleContentPosition
          alignItemsAtEnd={chat}
          initialScrollAtEnd={chat}
          maintainScrollAtEnd={chat}
          style={styles.fill}
        />
      );
    case 'native':
      return null;
  }
}

type BenchData = { version: number; chats: BenchChat[]; posts: FeedPost[] };

function makeData(scenario: Scenario, count: number, version: number): BenchData {
  return scenario === 'chat'
    ? { version, chats: makeBenchChats(count, version), posts: [] }
    : { version, chats: [], posts: makeFeed(count, version) };
}

type RunProps = {
  scenario: Scenario;
  impl: Impl;
  load: boolean;
  count: number;
  action: BenchAction;
  nonce: string;
};

// One configuration: the sentinel behind a transparent list, one ready line per mount, actions once per nonce.
function BenchRun({ scenario, impl, load, count, action, nonce }: RunProps) {
  const flume = impl === 'flume' || impl === 'flume-react';
  // Before the list's first commit: its setData layout effect runs before this component's effects.
  useState(() => {
    if (flume) unstable_setFlumeStats(true);
    return null;
  });
  const [data, setData] = useState(() => makeData(scenario, count, 0));
  const handled = useRef(nonce);
  useEffect(() => {
    if (nonce === handled.current) return;
    handled.current = nonce;
    if (action === 'replace') setData((d) => makeData(scenario, count, d.version + 1));
    else if (action === 'change500') setData((d) => ({ ...d, chats: changeChats(d.chats, 500) }));
    else if (action === 'resetStats') unstable_setFlumeStats(true);
    else if (action === 'logStats') unstable_logFlumeStats();
  }, [action, nonce, scenario, count]);

  const box = useRef<HostInstance>(null);
  const ready = useRef(false);
  const onLayout = () => {
    if (ready.current) return;
    ready.current = true;
    box.current?.measureInWindow((x, y, width, height) => {
      const ratio = PixelRatio.get();
      const rect: [number, number, number, number] = [
        Math.round(x * ratio),
        Math.round(y * ratio),
        Math.round((x + width) * ratio),
        Math.round((y + height) * ratio),
      ];
      console.log(readyLine({ scenario, impl, load, count, rect, density: ratio }));
    });
  };

  return (
    <View ref={box} onLayout={onLayout} style={styles.sentinel}>
      {scenario === 'chat' ? (
        <BenchList
          impl={impl}
          chat
          data={data.chats}
          renderCell={chatCell}
          Row={ChatRow}
          getItemType={chatType}
        />
      ) : (
        <BenchList
          impl={impl}
          chat={false}
          data={data.posts}
          renderCell={feedCell}
          Row={FeedRow}
          getItemType={postType}
        />
      )}
    </View>
  );
}

export default function BenchScreen({ route, navigation }: BottomTabScreenProps<ParamListBase>) {
  const params = parseBenchParams(route.params as Record<string, string | undefined> | undefined);
  const { scenario, impl, load, count } = params;
  useEffect(() => {
    if (!load) return;
    const id = setInterval(() => busy(80), 100);
    return () => clearInterval(id);
  }, [load]);
  return (
    <View style={styles.fill}>
      <View style={styles.bar}>
        <Text style={styles.label}>{`${scenario} ${impl} ${count}`}</Text>
        <Button
          title={load ? 'Load on' : 'Load off'}
          onPress={() => navigation.setParams({ load: load ? '0' : '1' })}
        />
      </View>
      <BenchRun key={`${scenario}-${impl}-${load}-${count}`} {...params} />
    </View>
  );
}

const styles = StyleSheet.create({
  fill: { flex: 1 },
  sentinel: { flex: 1, backgroundColor: SENTINEL },
  bar: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  label: { fontSize: 14, color: '#111111' },
});
