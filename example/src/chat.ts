import { type CellNode, cell } from 'react-native-flume';

export type Chat = {
  id: string;
  kind: 'message' | 'voice' | 'day';
  text: string;
  time: string;
  avatar: string;
  duration?: string;
};

const WORDS =
  'the quick brown fox jumps over a lazy dog while the river keeps running past old stones and new bridges under a pale evening sky where swallows turn and the town settles into its slow quiet hour'.split(
    ' ',
  );

const MESSAGES_PER_DAY = 40;

// Deterministic data from firstIndex on: one, two and three line messages, every fifth one a voice note,
// and a day bar after every 40 messages, which the inverted chat pins above the day it closes.
export function makeChats(count = 1000, firstIndex = 0): Chat[] {
  let seed = 42 + firstIndex;
  const rand = () => {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return seed / 2147483648;
  };
  const chats: Chat[] = [];
  for (let n = 0; n < count; n++) {
    const i = firstIndex + n;
    const band = rand();
    const target =
      band < 0.4 ? 20 + rand() * 20 : band < 0.75 ? 60 + rand() * 40 : 120 + rand() * 40;
    let text = '';
    while (text.length < target) {
      text += (text ? ' ' : '') + WORDS[Math.floor(rand() * WORDS.length)];
    }
    const extra = rand();
    if (extra < 0.05) text += ' \u{1F44D}';
    else if (extra < 0.08) text += ` ${'x'.repeat(40)}`;
    const minute = (i * 7) % 60;
    const hour = 8 + ((i * 3) % 12);
    const voice = i % 5 === 4;
    chats.push({
      id: `m${i}`,
      kind: voice ? 'voice' : 'message',
      text,
      time: `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`,
      avatar: `https://i.pravatar.cc/80?img=${(i % 20) + 1}`,
      duration: voice
        ? `${Math.floor(rand() * 3)}:${String(Math.floor(rand() * 60)).padStart(2, '0')}`
        : undefined,
    });
    if ((i + 1) % MESSAGES_PER_DAY === 0) {
      const day = (i + 1) / MESSAGES_PER_DAY;
      chats.push({ id: `d${day}`, kind: 'day', text: `Day ${day}`, time: '', avatar: '' });
    }
  }
  return chats;
}

export function dayIndices(chats: readonly Chat[]): number[] {
  const out: number[] = [];
  chats.forEach((chat, index) => {
    if (chat.kind === 'day') out.push(index);
  });
  return out;
}

export const accentColor = (dark: boolean) => (dark ? '#6ea8ff' : '#0066ff');

export function makeRenderCell(dark: boolean) {
  const textColor = dark ? '#eeeeee' : '#111111';
  const timeColor = dark ? '#aaaaaa' : '#666666';
  const accent = accentColor(dark);
  const track = dark ? '#3a3a3a' : '#dddddd';
  const bar = dark ? '#1c1c1c' : '#f2f2f2';
  const chip = dark ? '#444444' : '#dddddd';
  return (item: Chat, _index: number): CellNode => {
    'worklet';
    if (item.kind === 'day') {
      return cell.view({ alignItems: 'center', paddingVertical: 6, backgroundColor: bar }, [
        cell.view(
          { paddingHorizontal: 12, paddingVertical: 4, borderRadius: 12, backgroundColor: chip },
          [cell.text(item.text, { fontSize: 12, fontWeight: '600', color: timeColor })],
        ),
      ]);
    }
    const body =
      item.kind === 'voice'
        ? cell.view({ flex: 1, flexDirection: 'row', alignItems: 'center', gap: 8 }, [
            cell.text('▶', { fontSize: 18, color: accent }),
            cell.view({ flex: 1, height: 4, borderRadius: 2, backgroundColor: track }),
            cell.text(item.duration ?? '', { fontSize: 12, color: timeColor }),
          ])
        : cell.view({ flex: 1 }, [
            cell.text(item.text, { fontSize: 16, color: textColor }),
            cell.text(item.time, { fontSize: 12, color: timeColor }),
          ]);
    return cell.view({ flexDirection: 'row', padding: 12, gap: 12, alignItems: 'center' }, [
      cell.image({ source: item.avatar, width: 40, height: 40, borderRadius: 20 }),
      body,
      cell.pressable({ id: 'like', padding: 8 }, [
        cell.text('Like', { color: accent, fontWeight: '600' }),
      ]),
    ]);
  };
}
