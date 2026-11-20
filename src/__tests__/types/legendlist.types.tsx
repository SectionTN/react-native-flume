// LegendList code from its README, with only the import and the names changed.
import { useRef } from 'react';
import { Image, Text, View } from 'react-native';
import { Flume, type FlumeRef, type ListRenderItemInfo } from 'react-native-flume';

interface UserData {
  id: string;
  name: string;
  photoUri: string;
}

export function LegendListExample({ data }: { data: UserData[] }) {
  const listRef = useRef<FlumeRef | null>(null);
  const renderItem = ({ item, data: all, type }: ListRenderItemInfo<UserData>) => (
    <View>
      <Image source={{ uri: item.photoUri }} />
      <Text>{`${item.name} ${all.length} ${type}`}</Text>
    </View>
  );
  return (
    <>
      <Flume
        data={data}
        renderItem={renderItem}
        keyExtractor={(item) => item.id}
        recycleItems={true}
        maintainVisibleContentPosition
        ref={listRef}
      />
      <Flume
        data={data}
        renderItem={renderItem}
        maintainVisibleContentPosition={{ data: true }}
        initialScrollIndex={{ index: 10, viewOffset: 0 }}
        estimatedItemSize={60}
      />
    </>
  );
}
