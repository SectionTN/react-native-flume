import { type CellNode, cell } from 'react-native-flume';

export type Chat = { id: string; text: string; time: string; avatar: string };

const WORDS =
  'the quick brown fox jumps over a lazy dog while the river keeps running past old stones and new bridges under a pale evening sky where swallows turn and the town settles into its slow quiet hour'.split(
    ' ',
  );

// Deterministic data: one, two and three line messages at 16 sp on a 360 dp screen.
export function makeChats(count = 1000): Chat[] {
  let seed = 42;
  const rand = () => {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return seed / 2147483648;
  };
  const chats: Chat[] = [];
  for (let i = 0; i < count; i++) {
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
    chats.push({
      id: `m${i}`,
      text,
      time: `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`,
      avatar: `https://i.pravatar.cc/80?img=${(i % 20) + 1}`,
    });
  }
  return chats;
}

export function makeRenderCell(dark: boolean) {
  const textColor = dark ? '#eeeeee' : '#111111';
  const timeColor = dark ? '#aaaaaa' : '#666666';
  return (item: Chat, _index: number): CellNode => {
    'worklet';
    return cell.view({ flexDirection: 'row', padding: 12, gap: 12, alignItems: 'center' }, [
      cell.image({ source: item.avatar, width: 40, height: 40, borderRadius: 20 }),
      cell.view({ flex: 1 }, [
        cell.text(item.text, { fontSize: 16, color: textColor }),
        cell.text(item.time, { fontSize: 12, color: timeColor }),
      ]),
    ]);
  };
}
