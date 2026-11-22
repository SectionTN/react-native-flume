import { useMemo, useRef, useState } from 'react';
import { Button, Platform, ToastAndroid, View } from 'react-native';
import { Flume, type FlumeRef, type PressInfo } from 'react-native-flume';
import {
  accentColor,
  type Chat,
  dayIndices,
  makeChats,
  makeRenderCell,
  makeRenderSeparatorCell,
} from '../chat';

function blockJs(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: the list must keep binding while the JS thread is stuck here.
  }
}

// A seeded Fisher-Yates, so a shuffle is reproducible from its seed.
function shuffle<T>(items: readonly T[], seed: number): T[] {
  const out = items.slice();
  let s = seed;
  for (let i = out.length - 1; i > 0; i--) {
    s = (s * 1103515245 + 12345) % 2147483648;
    const j = Math.floor((s / 2147483648) * (i + 1));
    const swapped = out[i] as T;
    out[i] = out[j] as T;
    out[j] = swapped;
  }
  return out;
}

function newChat(n: number): Chat {
  return {
    id: `new${n}`,
    kind: 'message',
    text: `New message ${n}`,
    time: '12:00',
    avatar: `https://i.pravatar.cc/80?img=${(n % 20) + 21}`,
  };
}

// The counters live with the list so queued presses never reuse an id, a seed or an older batch.
type ListState = { chats: Chat[]; seed: number; added: number; older: number };

const LINES = ['native', 'worklet', 'both'] as const;
type Lines = (typeof LINES)[number];

const nextLines = (lines: Lines) => LINES[(LINES.indexOf(lines) + 1) % LINES.length] ?? 'native';

const NATIVE_LINE = { size: 1, color: '#dddddd', insetStart: 64 };

const toast = (message: string) => {
  if (Platform.OS === 'android') ToastAndroid.show(message, ToastAndroid.SHORT);
};

export default function ChatScreen() {
  const ref = useRef<FlumeRef>(null);
  const [dark, setDark] = useState(false);
  const [mounted, setMounted] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [list, setList] = useState<ListState>(() => ({
    chats: makeChats(),
    seed: 1,
    added: 0,
    older: 0,
  }));
  const renderCell = useMemo(() => makeRenderCell(dark), [dark]);
  // Native hairlines from the separator prop, a worklet that draws each one next to its rows, or both.
  const [lines, setLines] = useState<Lines>('native');
  const renderSeparatorCell = useMemo(() => makeRenderSeparatorCell(dark), [dark]);
  const sticky = useMemo(() => dayIndices(list.chats), [list.chats]);
  const onPress = ({ item, id }: PressInfo<Chat>) =>
    toast(id === 'like' ? `like ${item.id}` : `open ${item.id}`);
  const onLongPress = ({ item }: PressInfo<Chat>) => toast(`long ${item.id}`);
  const onRefresh = () => {
    setRefreshing(true);
    setTimeout(() => setRefreshing(false), 1000);
  };
  // Older messages go to the content end, which the inverted list shows at the top.
  // Three batches, then the history is complete and a pull from the oldest end can refresh.
  const onEndReached = () => {
    setList((s) => {
      if (s.older >= 3) return s;
      toast(`older loaded ${s.older + 1}`);
      return {
        ...s,
        chats: [...s.chats, ...makeChats(100, 1000 + s.older * 100)],
        older: s.older + 1,
      };
    });
  };
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
        <Button title="Block JS 3 s" onPress={() => blockJs(3000)} />
        <Button title="Theme" onPress={() => setDark((d) => !d)} />
        <Button
          title="Shuffle"
          onPress={() =>
            setList((s) => ({ ...s, chats: shuffle(s.chats, s.seed), seed: s.seed + 1 }))
          }
        />
        <Button
          title="New message"
          onPress={() =>
            setList((s) => ({ ...s, chats: [newChat(s.added), ...s.chats], added: s.added + 1 }))
          }
        />
        <Button
          title="Remove first"
          onPress={() => setList((s) => ({ ...s, chats: s.chats.slice(1) }))}
        />
        <Button title={mounted ? 'Unmount' : 'Mount'} onPress={() => setMounted((m) => !m)} />
        <Button title="To end" onPress={() => ref.current?.scrollToEnd()} />
        <Button title="To 500" onPress={() => ref.current?.scrollToIndex({ index: 500 })} />
        <Button title="To top" onPress={() => ref.current?.scrollToOffset({ offset: 0 })} />
        <Button title={`Separators ${nextLines(lines)}`} onPress={() => setLines(nextLines)} />
      </View>
      {mounted ? (
        <Flume
          ref={ref}
          data={list.chats}
          keyExtractor={(c) => c.id}
          getItemType={(c) => c.kind}
          renderCell={renderCell}
          renderSeparatorCell={lines === 'native' ? null : renderSeparatorCell}
          inverted
          stickyHeaderIndices={sticky}
          separator={lines === 'worklet' ? undefined : NATIVE_LINE}
          maintainVisibleContentPosition={{ autoscrollToBottomThreshold: 0.2 }}
          refreshing={refreshing}
          refreshControl={{ tintColor: accentColor(dark) }}
          onRefresh={onRefresh}
          onEndReached={onEndReached}
          onPress={onPress}
          onLongPress={onLongPress}
          style={{ flex: 1 }}
        />
      ) : null}
    </View>
  );
}
