import { useMemo, useState } from 'react';
import { Button, StatusBar, View } from 'react-native';
import { Flume } from 'react-native-flume';
import { makeChats, makeRenderCell } from './chat';

function blockJs(ms: number) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // Busy loop: the list must keep binding while the JS thread is stuck here.
  }
}

export default function App() {
  const [dark, setDark] = useState(false);
  const [mounted, setMounted] = useState(true);
  const chats = useMemo(() => makeChats(), []);
  const renderCell = useMemo(() => makeRenderCell(dark), [dark]);
  return (
    <View style={{ flex: 1, paddingTop: StatusBar.currentHeight ?? 0 }}>
      <View style={{ flexDirection: 'row', justifyContent: 'space-around', paddingVertical: 8 }}>
        <Button title="Block JS 3 s" onPress={() => blockJs(3000)} />
        <Button title="Theme" onPress={() => setDark((d) => !d)} />
        <Button title={mounted ? 'Unmount' : 'Mount'} onPress={() => setMounted((m) => !m)} />
      </View>
      {mounted ? <Flume data={chats} renderCell={renderCell} style={{ flex: 1 }} /> : null}
    </View>
  );
}
