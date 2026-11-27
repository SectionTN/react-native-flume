// Refs typed for FlatList, FlashList and LegendList fit Flume's ref prop, and Flume's own handle keeps its Promises.
import type { LegendListRef } from '@legendapp/list/react-native';
import type { FlashListRef } from '@shopify/flash-list';
import { useRef } from 'react';
import { type FlatList, Text } from 'react-native';
import { Flume, type FlumeRef } from 'react-native-flume';

type Item = { id: string; title: string };

const FIRST: Item = { id: 'a', title: 'First' };
const DATA: Item[] = [FIRST, { id: 'b', title: 'Second' }];

export function ForeignRefs() {
  const flatList = useRef<FlatList<Item>>(null);
  const untypedFlatList = useRef<FlatList>(null);
  const flashList = useRef<FlashListRef<Item>>(null);
  const legendList = useRef<LegendListRef>(null);
  return (
    <>
      <Flume ref={flatList} data={DATA} renderItem={({ item }) => <Text>{item.title}</Text>} />
      <Flume
        ref={untypedFlatList}
        data={DATA}
        renderItem={({ item }) => (
          // @ts-expect-error the item type comes from data, not from an untyped FlatList ref
          <Text>{item.missing}</Text>
        )}
      />
      <Flume ref={flashList} data={DATA} renderItem={({ item }) => <Text>{item.title}</Text>} />
      <Flume ref={legendList} data={DATA} renderItem={({ item }) => <Text>{item.title}</Text>} />
    </>
  );
}

export function FlumeRefs() {
  const ref = useRef<FlumeRef<Item>>(null);
  const plain = useRef<FlumeRef>(null);
  const scroll = async () => {
    await ref.current?.scrollToIndex({ index: 1 });
    await ref.current?.scrollToItem({ item: FIRST });
    await plain.current?.scrollToEnd();
  };
  return (
    <>
      <Flume
        ref={ref}
        data={DATA}
        renderItem={({ item }) => <Text onPress={scroll}>{item.title}</Text>}
      />
      <Flume ref={plain} data={DATA} renderItem={({ item }) => <Text>{item.title}</Text>} />
    </>
  );
}
