import { useMemo, useState } from 'react';
import { Button, View } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

type Photo = { id: string; kind: 'photo' | 'header'; uri: string; caption: string };
type Story = { id: string; uri: string; name: string };

const PHOTOS_PER_SECTION = 30;

// A header before every 30 photos; headers are the sticky rows and take the full width of the grid.
function makePhotos(count = 300): Photo[] {
  const out: Photo[] = [];
  for (let i = 0; i < count; i++) {
    if (i % PHOTOS_PER_SECTION === 0) {
      const section = i / PHOTOS_PER_SECTION + 1;
      out.push({ id: `h${section}`, kind: 'header', uri: '', caption: `Section ${section}` });
    }
    out.push({
      id: `p${i}`,
      kind: 'photo',
      uri: `https://picsum.photos/seed/flume${i}/200/200`,
      caption: `Photo ${i}`,
    });
  }
  return out;
}

function makeStories(count = 20): Story[] {
  return Array.from({ length: count }, (_, i) => ({
    id: `s${i}`,
    uri: `https://i.pravatar.cc/120?img=${(i % 40) + 1}`,
    name: ['Ana', 'Ben', 'Cleo', 'Dan', 'Eve', 'Fay', 'Gus', 'Hal'][i % 8] ?? 'Ivy',
  }));
}

const renderPhoto = (item: Photo): CellNode => {
  'worklet';
  if (item.kind === 'header') {
    return cell.view({ paddingVertical: 8, paddingHorizontal: 4, backgroundColor: '#eeeeee' }, [
      cell.text(item.caption, { fontSize: 14, fontWeight: '700', color: '#111111' }),
    ]);
  }
  return cell.view({ padding: 4 }, [
    cell.image({ source: item.uri, width: '100%', aspectRatio: 1, borderRadius: 8 }),
    cell.text(item.caption, { fontSize: 12, color: '#333333', marginTop: 4 }),
  ]);
};

// Laid out at the strip's height with its own width: a horizontal cell.
const renderStory = (item: Story): CellNode => {
  'worklet';
  return cell.view({ alignItems: 'center', justifyContent: 'center', paddingHorizontal: 8 }, [
    cell.image({
      source: item.uri,
      width: 56,
      height: 56,
      borderRadius: 28,
      borderWidth: 2,
      borderColor: '#0066ff',
    }),
    cell.text(item.name, { fontSize: 11, color: '#333333', marginTop: 4 }),
  ]);
};

export default function GridScreen() {
  const [columns, setColumns] = useState(3);
  const [inverted, setInverted] = useState(false);
  const photos = useMemo(() => makePhotos(), []);
  const stories = useMemo(() => makeStories(), []);
  const headers = useMemo(
    () => photos.flatMap((p, i) => (p.kind === 'header' ? [i] : [])),
    [photos],
  );
  return (
    <View style={{ flex: 1 }}>
      <View style={{ flexDirection: 'row', justifyContent: 'center', gap: 8, padding: 8 }}>
        <Button
          title={`Columns ${columns === 3 ? 2 : 3}`}
          onPress={() => setColumns((c) => (c === 3 ? 2 : 3))}
        />
        <Button title={inverted ? 'Upright' : 'Inverted'} onPress={() => setInverted((v) => !v)} />
      </View>
      <Flume
        data={stories}
        keyExtractor={(s) => s.id}
        renderCell={renderStory}
        horizontal
        showsHorizontalScrollIndicator={false}
        style={{ height: 96 }}
      />
      <Flume
        data={photos}
        keyExtractor={(p) => p.id}
        getItemType={(p) => p.kind}
        renderCell={renderPhoto}
        numColumns={columns}
        inverted={inverted}
        stickyHeaderIndices={headers}
        contentContainerStyle={{ padding: 8 }}
        style={{ flex: 1 }}
      />
    </View>
  );
}
