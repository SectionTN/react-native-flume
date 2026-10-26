import { ToastAndroid } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

type Showcase = {
  id: 'borders' | 'percent' | 'margins' | 'text' | 'images' | 'pressable';
  title: string;
};

const ROWS: Showcase[] = [
  { id: 'borders', title: 'Borders, radii and elevation' },
  { id: 'percent', title: 'Percent widths and aspect ratio' },
  { id: 'margins', title: 'Auto margins and display none' },
  { id: 'text', title: 'Text' },
  { id: 'images', title: 'Images' },
  { id: 'pressable', title: 'Pressable with pressedOpacity 0.3' },
];

const PHOTO = 'https://picsum.photos/id/1015/300/200';
const ICON = 'https://reactnative.dev/img/tiny_logo.png';
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

function renderCell(item: Showcase, _index: number): CellNode {
  'worklet';
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
  return (
    <Flume
      data={ROWS}
      renderCell={renderCell}
      onPress={({ item, id }) => ToastAndroid.show(`pressed ${id ?? item.id}`, ToastAndroid.LONG)}
      style={{ flex: 1, backgroundColor: '#fafafa' }}
    />
  );
}
