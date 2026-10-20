import { View } from 'react-native';
import { type CellNode, cell, Flume } from 'react-native-flume';

const EMPTY: never[] = [];

const renderCell = (_item: never, _index: number): CellNode => {
  'worklet';
  return cell.view({}, [cell.text('x')]);
};

export default function App() {
  return (
    <View style={{ flex: 1 }}>
      <Flume data={EMPTY} renderCell={renderCell} style={{ flex: 1 }} />
    </View>
  );
}
