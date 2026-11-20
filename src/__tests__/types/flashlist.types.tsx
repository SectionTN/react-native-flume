// FlashList code from its README, with only the import and the component name changed.
import { Text } from 'react-native';
import { Flume } from 'react-native-flume';

const DATA = [
  { title: 'First Item', kind: 1 },
  { title: 'Second Item', kind: 2 },
];

export function FlashListExample() {
  return (
    <Flume
      data={DATA}
      renderItem={({ item, target, extraData }) => (
        <Text>{target === 'Cell' ? item.title : String(extraData)}</Text>
      )}
      getItemType={(item) => item.kind}
      estimatedItemSize={50}
      drawDistance={250}
      onEndReached={null}
      onViewableItemsChanged={null}
      maintainVisibleContentPosition={{
        autoscrollToBottomThreshold: 0.2,
        startRenderingFromBottom: true,
      }}
    />
  );
}
