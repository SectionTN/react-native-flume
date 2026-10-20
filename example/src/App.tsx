import { useMemo } from 'react';
import { StatusBar, View } from 'react-native';
import { Flume } from 'react-native-flume';
import { makeChats, makeRenderCell } from './chat';

export default function App() {
  const chats = useMemo(() => makeChats(), []);
  const renderCell = useMemo(() => makeRenderCell(false), []);
  return (
    <View style={{ flex: 1, paddingTop: StatusBar.currentHeight ?? 0 }}>
      <Flume data={chats} renderCell={renderCell} style={{ flex: 1 }} />
    </View>
  );
}
