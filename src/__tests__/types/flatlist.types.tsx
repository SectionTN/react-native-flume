// FlatList code from React Native's docs, with only the import and the component name changed.
import { useState } from 'react';
import {
  RefreshControl,
  type ListRenderItem as RNListRenderItem,
  StyleSheet,
  Text,
  TouchableHighlight,
  View,
} from 'react-native';
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

type SeparatorItem = { title: string; key: string };

const style = StyleSheet.create({ separator: { height: 1, marginLeft: 16 } });

// A renderItem typed with React Native's own ListRenderItem, as FlatList code declares it.
const renderSeparatorRow: RNListRenderItem<SeparatorItem> = ({ item, separators }) => (
  <TouchableHighlight
    key={item.key}
    onPress={() => separators.updateProps('trailing', { pressed: true })}
    onPressIn={() => separators.highlight()}
    onShowUnderlay={separators.highlight}
    onHideUnderlay={separators.unhighlight}
  >
    <View style={{ backgroundColor: 'white' }}>
      <Text>{item.title}</Text>
    </View>
  </TouchableHighlight>
);

export function FlatListSeparatorExample() {
  return (
    <>
      <Flume
        ItemSeparatorComponent={({ highlighted }) => (
          <View style={[style.separator, highlighted && { marginLeft: 0 }]} />
        )}
        data={[{ title: 'Title Text', key: 'item1' }]}
        renderItem={renderSeparatorRow}
      />
      <Flume
        data={DATA}
        ListItemComponent={({ item }) => <Text>{item.title}</Text>}
        CellRendererComponent={({ children, style: cellStyle, ...props }) => (
          <View style={[cellStyle, { zIndex: props.index }]} {...props}>
            {children}
          </View>
        )}
        numColumns={2}
        columnWrapperStyle={{ gap: 8, justifyContent: 'space-between' }}
        contentContainerStyle={{ flexGrow: 1, padding: '5%' }}
        ListHeaderComponent={<Text>Header</Text>}
        ListHeaderComponentStyle={{ borderBottomWidth: 1 }}
        ListFooterComponent={<Text>Footer</Text>}
        ListFooterComponentStyle={{ flexGrow: 1, justifyContent: 'flex-end' }}
      />
    </>
  );
}
