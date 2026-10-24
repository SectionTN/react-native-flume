import { type CellNode, cell } from 'react-native-flume';
import type { BenchChat, FeedPost } from './data';

// The Chat tab's light message and voice cell; every bench cell paints an opaque background over the sentinel.
export function chatCell(item: BenchChat, _index: number): CellNode {
  'worklet';
  const body =
    item.kind === 'voice'
      ? cell.view({ flex: 1, flexDirection: 'row', alignItems: 'center', gap: 8 }, [
          cell.text('▶', { fontSize: 18, color: '#0066ff' }),
          cell.view({ flex: 1, height: 4, borderRadius: 2, backgroundColor: '#dddddd' }),
          cell.text(item.duration ?? '', { fontSize: 12, color: '#666666' }),
        ])
      : cell.view({ flex: 1 }, [
          cell.text(item.text, { fontSize: 16, color: '#111111' }),
          cell.text(item.time, { fontSize: 12, color: '#666666' }),
        ]);
  return cell.view(
    {
      flexDirection: 'row',
      padding: 12,
      gap: 12,
      alignItems: 'center',
      backgroundColor: '#ffffff',
    },
    [
      cell.image({ source: item.avatar, width: 40, height: 40, borderRadius: 20 }),
      body,
      cell.pressable({ id: 'like', padding: 8 }, [
        cell.text('Like', { color: '#0066ff', fontWeight: '600' }),
      ]),
    ],
  );
}

export function feedCell(item: FeedPost, _index: number): CellNode {
  'worklet';
  const children: CellNode[] = [
    cell.view({ flexDirection: 'row', padding: 12, gap: 10, alignItems: 'center' }, [
      cell.image({ source: item.avatar, width: 40, height: 40, borderRadius: 20 }),
      cell.view({}, [
        cell.text(item.author, { fontSize: 15, fontWeight: '600', color: '#111111' }),
        cell.text(item.time, { fontSize: 12, color: '#666666' }),
      ]),
    ]),
    cell.text(item.text, { fontSize: 15, color: '#111111', paddingHorizontal: 12 }),
  ];
  if (item.photo !== undefined) {
    children.push(
      cell.image({
        source: item.photo,
        alignSelf: 'stretch',
        aspectRatio: 4 / 3,
        marginTop: 8,
        backgroundColor: '#e8e8e8',
      }),
    );
  }
  children.push(
    cell.view({ flexDirection: 'row', padding: 12, gap: 16, alignItems: 'center' }, [
      cell.text(`${item.likes} likes`, { fontSize: 13, color: '#666666' }),
      cell.text(`${item.comments} comments`, { fontSize: 13, color: '#666666' }),
      cell.pressable({ id: 'share' }, [
        cell.text('Share', { fontSize: 13, fontWeight: '600', color: '#0066ff' }),
      ]),
    ]),
    cell.view({ height: 8, backgroundColor: '#f0f0f0' }),
  );
  return cell.view({ backgroundColor: '#ffffff' }, children);
}
