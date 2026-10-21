import { useMemo, useState } from 'react';
import { Button, ToastAndroid, View } from 'react-native';
import { Flume, type PressInfo } from 'react-native-flume';
import { type Chat, makeChats, makeRenderCell } from '../chat';

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

// The counters live with the list so queued presses never reuse an id or a seed.
type ListState = { chats: Chat[]; seed: number; added: number };

const toast = (message: string) => ToastAndroid.show(message, ToastAndroid.LONG);

export default function ChatScreen() {
  const [dark, setDark] = useState(false);
  const [mounted, setMounted] = useState(true);
  const [list, setList] = useState<ListState>(() => ({ chats: makeChats(), seed: 1, added: 0 }));
  const renderCell = useMemo(() => makeRenderCell(dark), [dark]);
  const onPress = ({ item, id }: PressInfo<Chat>) =>
    toast(id === 'like' ? `like ${item.id}` : `open ${item.id}`);
  const onLongPress = ({ item }: PressInfo<Chat>) => toast(`long ${item.id}`);
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
          title="Prepend"
          onPress={() =>
            setList((s) => ({ ...s, chats: [newChat(s.added), ...s.chats], added: s.added + 1 }))
          }
        />
        <Button
          title="Remove first"
          onPress={() => setList((s) => ({ ...s, chats: s.chats.slice(1) }))}
        />
        <Button title={mounted ? 'Unmount' : 'Mount'} onPress={() => setMounted((m) => !m)} />
      </View>
      {mounted ? (
        <Flume
          data={list.chats}
          keyExtractor={(c) => c.id}
          getItemType={(c) => c.kind}
          renderCell={renderCell}
          onPress={onPress}
          onLongPress={onLongPress}
          style={{ flex: 1 }}
        />
      ) : null}
    </View>
  );
}
