// FlatList code from React Native's docs, with only the import and the component name changed.
import { useState } from 'react';
import { RefreshControl, Text } from 'react-native';
import { Flume, type ListRenderItem } from 'react-native-flume';

type ItemData = { id: string; title: string };

const DATA: ItemData[] | undefined = [
  { id: 'bd7acbea', title: 'First Item' },
  { id: '3ac68afc', title: 'Second Item' },
];

const renderItem: ListRenderItem<ItemData> = ({ item }) => <Text>{item.title}</Text>;
const sticky: readonly number[] = [0];

export function FlatListExample() {
  const [refreshing, setRefreshing] = useState(false);
  return (
    <Flume
      data={DATA}
      renderItem={renderItem}
      keyExtractor={(item) => item.id}
      ListHeaderComponent={<Text>Header</Text>}
      stickyHeaderIndices={sticky}
      refreshControl={
        <RefreshControl refreshing={refreshing} onRefresh={() => setRefreshing(true)} />
      }
      getItemLayout={(_data, index) => ({ length: 50, offset: 50 * index, index })}
      windowSize={5}
      initialNumToRender={10}
      removeClippedSubviews
      maintainVisibleContentPosition={{ minIndexForVisible: 0 }}
      nativeID="list"
      accessibilityRole="list"
    />
  );
}
