import { useState } from 'react';
import { Button, Platform, ToastAndroid, View } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

type Showcase = {
  id: 'composed' | 'borders' | 'percent' | 'margins' | 'text' | 'images' | 'pressable';
  title: string;
};

const ROWS: Showcase[] = [
  { id: 'composed', title: 'Root margin, elevation, a pressable root and a testID' },
  { id: 'borders', title: 'Borders, radii and elevation' },
  { id: 'percent', title: 'Percent widths and aspect ratio' },
  { id: 'margins', title: 'Auto margins and display none' },
  { id: 'text', title: 'Text' },
  { id: 'images', title: 'Images' },
  { id: 'pressable', title: 'Pressable with pressedOpacity 0.3' },
];

const PHOTO = 'https://picsum.photos/id/1015/300/200';
const ICON = 'https://reactnative.dev/img/tiny_logo.png';
const FACE = 'https://i.pravatar.cc/128?img=5';
// Never loads, so only the image's own background shows.
const NEVER = 'https://invalid.invalid/image.png';
const LONG =
  'A paragraph long enough to need more than two lines on a phone, so the tail of it is cut with an ellipsis instead of growing the row.';

function borders(): CellNode {
  'worklet';
  return cell.view({ flexDirection: 'row', gap: 16 }, [
    cell.view({ width: 64, height: 64, borderWidth: 2, borderColor: '#0066ff', borderRadius: 12 }),
    cell.view({
      width: 64,
      height: 64,
      backgroundColor: '#ffcc00',
      borderTopLeftRadius: 32,
      borderBottomRightRadius: 32,
    }),
    cell.view({
      width: 64,
      height: 64,
      backgroundColor: '#ffffff',
      borderWidth: 1,
      borderColor: '#bbbbbb',
      borderRadius: 8,
      elevation: 4,
      shadowColor: '#000000',
      shadowOpacity: 0.25,
      shadowRadius: 4,
      shadowOffset: { width: 0, height: 2 },
    }),
  ]);
}

function percent(): CellNode {
  'worklet';
  return cell.view({ gap: 8 }, [
    cell.view({ width: '50%', aspectRatio: 2, backgroundColor: '#4caf50', borderRadius: 8 }),
    cell.view({ width: '25%', aspectRatio: 1, backgroundColor: '#8bc34a', borderRadius: 8 }),
  ]);
}

function margins(): CellNode {
  'worklet';
  return cell.view(
    { flexDirection: 'row', padding: 8, backgroundColor: '#eeeeee', borderRadius: 8 },
    [
      cell.view({ width: 40, height: 40, backgroundColor: '#e53935' }),
      cell.view({ width: 40, height: 40, backgroundColor: '#43a047', display: 'none' }),
      cell.view({ width: 40, height: 40, backgroundColor: '#1e88e5', marginLeft: 'auto' }),
    ],
  );
}

function text(): CellNode {
  'worklet';
  return cell.view({ gap: 8 }, [
    cell.text('A tag with its own box', {
      alignSelf: 'flex-start',
      paddingHorizontal: 10,
      paddingVertical: 4,
      borderRadius: 12,
      borderWidth: 1,
      borderColor: '#0066ff',
      backgroundColor: '#e8f0ff',
      color: '#0066ff',
      fontSize: 13,
    }),
    cell.text('9', {
      width: 28,
      height: 28,
      borderRadius: 14,
      borderWidth: 3,
      borderColor: '#ffcc00',
      backgroundColor: '#e53935',
      color: '#ffffff',
      textAlign: 'center',
      lineHeight: 22,
      fontSize: 13,
      fontWeight: '700',
    }),
    cell.text('Serif, italic and centered', {
      fontFamily: 'serif',
      fontStyle: 'italic',
      fontSize: 18,
      textAlign: 'center',
      color: '#111111',
    }),
    cell.text(
      'Bold at line height 28 with letter spacing 2, long enough to wrap to a second line',
      {
        fontWeight: '700',
        fontSize: 14,
        lineHeight: 28,
        letterSpacing: 2,
        color: '#111111',
      },
    ),
    cell.view({ flexDirection: 'row', gap: 16 }, [
      cell.text('Underlined', { textDecorationLine: 'underline', color: '#111111' }),
      cell.text('Struck through', { textDecorationLine: 'line-through', color: '#111111' }),
      cell.text('Right', { flex: 1, textAlign: 'right', color: '#111111' }),
    ]),
    cell.text(LONG, { numberOfLines: 2, ellipsizeMode: 'tail', fontSize: 15, color: '#111111' }),
    cell.text(
      'Fixed at 14 dp whatever the font scale',
      { fontSize: 14, color: '#111111' },
      { allowFontScaling: false },
    ),
  ]);
}

function imageBox(mode: 'cover' | 'contain' | 'stretch' | 'center'): CellNode {
  'worklet';
  return cell.view({ alignItems: 'center', gap: 4 }, [
    cell.image({
      source: PHOTO,
      resizeMode: mode,
      width: 72,
      height: 72,
      backgroundColor: '#dddddd',
    }),
    cell.text(mode, { fontSize: 12, color: '#666666' }),
  ]);
}

function images(): CellNode {
  'worklet';
  return cell.view({ gap: 12 }, [
    cell.view({ flexDirection: 'row', justifyContent: 'space-between' }, [
      imageBox('cover'),
      imageBox('contain'),
      imageBox('stretch'),
      imageBox('center'),
    ]),
    cell.view({ flexDirection: 'row', gap: 16, alignItems: 'center' }, [
      cell.image({ source: ICON, width: 48, height: 48, tintColor: '#e91e63' }),
      cell.image({
        source: PHOTO,
        width: 96,
        height: 64,
        borderTopLeftRadius: 24,
        borderBottomRightRadius: 24,
      }),
    ]),
    cell.view({ flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' }, [
      cell.image({
        source: NEVER,
        width: 56,
        height: 56,
        borderRadius: 28,
        backgroundColor: '#dddddd',
      }),
      cell.image({
        source: FACE,
        width: 56,
        height: 56,
        borderRadius: 28,
        borderWidth: 3,
        borderColor: '#0066ff',
      }),
      cell.image({
        source: FACE,
        width: 56,
        height: 56,
        padding: 6,
        borderRadius: 12,
        backgroundColor: '#ffcc00',
      }),
      cell.image({ source: FACE, width: 56, height: 56, borderRadius: 12, opacity: 0.4 }),
      cell.image({
        source: FACE,
        width: 56,
        height: 56,
        borderRadius: 12,
        backgroundColor: '#ffffff',
        elevation: 6,
      }),
    ]),
  ]);
}

function pressable(): CellNode {
  'worklet';
  return cell.pressable(
    {
      id: 'demo',
      pressedOpacity: 0.3,
      alignSelf: 'flex-start',
      paddingVertical: 12,
      paddingHorizontal: 20,
      backgroundColor: '#0066ff',
      borderRadius: 24,
    },
    [cell.text('Press me', { color: '#ffffff', fontWeight: '600' })],
  );
}

// A separator worklet must leave this root's margin, shadow, label, testID and press as they are.
function composed(title: string): CellNode {
  'worklet';
  const root = cell.pressable(
    {
      id: 'composed',
      pressedOpacity: 0.5,
      margin: 10,
      padding: 16,
      gap: 4,
      borderRadius: 12,
      backgroundColor: '#ffffff',
      elevation: 4,
      shadowColor: '#000000',
      shadowOpacity: 0.2,
      shadowRadius: 6,
      shadowOffset: { width: 0, height: 2 },
    },
    [
      cell.text(title, { fontSize: 15, fontWeight: '600', color: '#111111' }),
      cell.text('testID composed-row', { fontSize: 12, color: '#666666' }),
    ],
  );
  return { ...root, testID: 'composed-row' };
}

const renderSeparator = (): CellNode => {
  'worklet';
  return cell.view({ height: 1, marginHorizontal: 16, backgroundColor: '#c0c0c8' });
};

function renderCell(item: Showcase, _index: number): CellNode {
  'worklet';
  if (item.id === 'composed') return composed(item.title);
  let content: CellNode;
  if (item.id === 'borders') content = borders();
  else if (item.id === 'percent') content = percent();
  else if (item.id === 'margins') content = margins();
  else if (item.id === 'text') content = text();
  else if (item.id === 'images') content = images();
  else content = pressable();
  return cell.view({ padding: 16, gap: 12 }, [
    cell.text(item.title, {
      fontSize: 13,
      fontWeight: '600',
      letterSpacing: 0.5,
      color: '#666666',
    }),
    content,
  ]);
}

export default function StylesScreen() {
  const [lines, setLines] = useState(false);
  return (
    <View style={{ flex: 1 }}>
      <View style={{ flexDirection: 'row', justifyContent: 'center', padding: 8 }}>
        <Button
          title={`Separators ${lines ? 'off' : 'on'}`}
          onPress={() => setLines((on) => !on)}
        />
      </View>
      <Flume
        data={ROWS}
        renderCell={renderCell}
        renderSeparatorCell={lines ? renderSeparator : null}
        onPress={({ item, id }) => {
          if (Platform.OS === 'android')
            ToastAndroid.show(`pressed ${id ?? item.id}`, ToastAndroid.LONG);
        }}
        style={{ flex: 1, backgroundColor: '#fafafa' }}
      />
    </View>
  );
}
