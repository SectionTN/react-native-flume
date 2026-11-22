# `react-native-flume`

[![npm](https://img.shields.io/npm/v/react-native-flume?style=flat-square)](https://www.npmjs.com/package/react-native-flume)
[![license](https://img.shields.io/npm/l/react-native-flume?style=flat-square&label=license)](https://github.com/SectionTN/react-native-flume/blob/main/LICENSE)

A native list for React Native. It's built on RecyclerView on Android and UICollectionView on iOS.

## Features

- Rows built on the UI thread from a worklet
- React rows through `renderItem`, or through `reactCells` next to worklet rows
- Most of FlatList's props, events and ref methods, under the same names ([migrating](#migrating-from-flatlist))
- Grids, sticky headers, inverted lists and separators
- Keeps the visible rows in place when items load above them
- Pull to refresh, viewability and end reached events

## Installation

```sh
npm install react-native-flume react-native-worklets
cd ios && pod install
```

Add the worklets plugin to `babel.config.js`. Expo adds it for you.

```js title="babel.config.js"
module.exports = {
  presets: ['module:@react-native/babel-preset'],
  plugins: ['react-native-worklets/plugin'],
};
```

Requirements:

- React Native 0.83+
- react-native-worklets 0.9+, in the version its [compatibility table](https://docs.swmansion.com/react-native-worklets/docs/guides/compatibility/) lists for your React Native
- iOS 15.1+
- Android 7.0+ (API 24)
- Expo: a development build (Expo Go isn't supported)

## Usage

The example uses `SafeAreaView` from `react-native-safe-area-context`, which new React Native apps already include:

```tsx
import { type CellNode, cell, Flume } from 'react-native-flume';
import { SafeAreaView } from 'react-native-safe-area-context';

type Contact = { id: string; name: string; phone: string };

const contacts: Contact[] = Array.from({ length: 1000 }, (_, i) => ({
  id: String(i),
  name: `Contact ${i}`,
  phone: `555 01${String(i % 100).padStart(2, '0')}`,
}));

function contactCell(item: Contact): CellNode {
  'worklet';
  return cell.view({ flexDirection: 'row', alignItems: 'center', padding: 16, gap: 12 }, [
    cell.view({ flex: 1 }, [
      cell.text(item.name, { fontSize: 16, fontWeight: '600', color: '#111111' }),
      cell.text(item.phone, { fontSize: 14, color: '#666666' }),
    ]),
    cell.pressable({ id: 'call', padding: 8 }, [cell.text('Call', { color: '#0066ff' })]),
  ]);
}

export default function App() {
  return (
    <SafeAreaView style={{ flex: 1 }}>
      <Flume
        data={contacts}
        renderCell={contactCell}
        onPress={({ item, id }) => console.log(id ?? 'row', item.name)}
        style={{ flex: 1 }}
      />
    </SafeAreaView>
  );
}
```

## API

### Props

Data and rows:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `data` | `readonly T[] \| null` | Required | Sets the items the list shows. `null` and `undefined` show an empty list. |
| `renderCell` | `(item, index) => CellNode` | | Builds each row from a worklet that returns [cell nodes](#cells). |
| `renderItem` | `({ item, index, separators, target, extraData, data, type }) => ReactNode` | | Renders each row with React when `renderCell` isn't set. `separators.highlight()` highlights the separators above and below the row, as on FlatList. |
| `ListItemComponent` | `ComponentType \| ReactElement` | | Renders each row as `renderItem` does, and replaces it when both are set. |
| `CellRendererComponent` | `ComponentType<CellRendererProps>` | | Wraps each React row and its separator in your own component, which gets `cellKey`, `index`, `item`, `style` and `children`. |
| `renderSeparatorCell` | `({ leadingItem, trailingItem }) => CellNode \| null` | | Builds the separator under each worklet row from a worklet. `trailingItem` is `undefined` when the next row is a React row, and `null` draws no separator. |
| `reactCells` | `Record<string, ReactCell<T>>` | | Renders the item types it names as React elements, in place of `renderCell`. |
| `keyExtractor` | `(item, index) => string` | `item.id`, then `item.key`, then the index | Returns a unique key for each item. |
| `getItemType` | `(item, index) => string \| number` | `'default'` | Returns each item's type. Rows are reused within a type, and `reactCells` picks a renderer by it. A number counts as its text, so `1` and `'1'` are one type. |
| `extraData` | `unknown` | | Re-renders the React rows when it changes. |
| `itemAnimations` | `boolean` | `true` | Animates rows that are added, removed or moved when `data` changes. |
| `onPress` | `({ item, index, id }) => void` | | Called when a worklet row or a `cell.pressable` is tapped. `id` is the pressable's `id`, or `undefined` for the row. |
| `onLongPress` | `({ item, index, id }) => void` | | Called when a worklet row or a `cell.pressable` is touched and held. |

Layout:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `horizontal` | `boolean` | `false` | Scrolls the list sideways. |
| `numColumns` | `number` | `1` | Lays the rows out in a grid with this many columns. It works on vertical lists only. |
| `columnWrapperStyle` | `StyleProp<ViewStyle>` | | Sets the gaps between a grid's columns and rows and its horizontal padding. Other style keys are ignored. |
| `inverted` | `boolean` | `false` | Flips the list so the first item shows at the bottom, as in a chat. |
| `contentContainerStyle` | `StyleProp<ViewStyle>` | | Sets the padding, the gaps between rows and the background behind them. `flexGrow` with `justifyContent` places short content, as on FlatList. Other style keys are ignored. |
| `ItemSeparatorComponent` | `ComponentType \| ReactElement` | | Renders a component between React rows, with `leadingItem`, `trailingItem` and `highlighted` as props. Worklet rows take `renderSeparatorCell` instead. |
| `separator` | `{ size, color?, insetStart?, insetEnd? }` | | Adds a line `size` dp thick between rows, inset from the start and end edges. |
| `stickyHeaderIndices` | `readonly number[]` | | Pins the items at these indices to the top while their section scrolls. |
| `ListHeaderComponent` | `ComponentType \| ReactElement` | | Renders a row before the first item. |
| `ListFooterComponent` | `ComponentType \| ReactElement` | | Renders a row after the last item. |
| `ListEmptyComponent` | `ComponentType \| ReactElement` | | Renders in place of the rows while `data` is empty, filling the list. |
| `ListHeaderComponentStyle` | `StyleProp<ViewStyle>` | | Styles the view that wraps the header. |
| `ListFooterComponentStyle` | `StyleProp<ViewStyle>` | | Styles the view that wraps the footer. |
| `ListEmptyComponentStyle` | `StyleProp<ViewStyle>` | | Styles the view that wraps the empty component. |
| `estimatedItemSize` | `number` | The mean of the item's type, at least 50 | Sets the length in dp that a React row takes until it's measured. |
| `drawDistance` | `number` | One screen | Sets how far past the screen edges, in dp, rows are prepared ahead of scrolling. |
| `initialScrollIndex` | `number \| { index }` | | Opens the list at this item. |
| `maintainVisibleContentPosition` | `boolean \| MaintainVisibleContentPosition` | On | Keeps the visible rows in place when items are added above them. Pass `false` to turn it off. |

`maintainVisibleContentPosition` takes these options:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `disabled` | `boolean` | `false` | Turns the behavior off. |
| `autoscrollToTopThreshold` | `number` | | Scrolls to new items at the top when the list is within this many points of the top. |
| `autoscrollToBottomThreshold` | `number` | | Scrolls to new items at the bottom when the list is this many screens from the bottom. |
| `animateAutoScrollToBottom` | `boolean` | `true` | Animates the scroll to new items at the bottom. |
| `startRenderingFromBottom` | `boolean` | `false` | Opens the list at its last item. |

Scrolling:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `onScroll` | `(event) => void` | | Called while the list scrolls, with the ScrollView event. |
| `scrollEventThrottle` | `number` | `16` | Sets the minimum time in ms between two `onScroll` calls. |
| `onScrollBeginDrag` | `(event) => void` | | Called when the user starts dragging the list. |
| `onScrollEndDrag` | `(event) => void` | | Called when the user lifts their finger. |
| `onMomentumScrollBegin` | `(event) => void` | | Called when the list starts gliding after a fling. |
| `onMomentumScrollEnd` | `(event) => void` | | Called when the list stops gliding. |
| `scrollEnabled` | `boolean` | `true` | Lets the user scroll the list. |
| `decelerationRate` | `'normal' \| 'fast' \| number` | `'normal'` | Sets how quickly the list slows down after a fling. |
| `showsVerticalScrollIndicator` | `boolean` | `true` | Shows the vertical scroll bar. |
| `showsHorizontalScrollIndicator` | `boolean` | `true` | Shows the horizontal scroll bar. |
| `keyboardDismissMode` | `'none' \| 'on-drag' \| 'interactive'` | `'none'` | Sets whether dragging the list hides the keyboard. `'interactive'` works on iOS only. |
| `keyboardShouldPersistTaps` | `'always' \| 'never' \| 'handled'` | `'never'` | Sets whether the keyboard stays up when the user taps the list, as on ScrollView. |
| `nestedScrollEnabled` | `boolean` | `false` | Lets the list scroll inside another scrolling view on Android. |
| `overScrollMode` | `'auto' \| 'always' \| 'never'` | `'auto'` | Sets when Android shows the overscroll effect at the ends of the list. |
| `bounces` | `boolean` | `true` | Bounces the list past its ends on iOS. |
| `alwaysBounceVertical` | `boolean` | `true` when vertical | Bounces the list on iOS even when its content is shorter than the list. |
| `contentInset` | `Insets` | | Adds space around the content on iOS. |
| `scrollIndicatorInsets` | `Insets` | | Insets the scroll bars on iOS. |
| `contentInsetAdjustmentBehavior` | `'automatic' \| 'scrollableAxes' \| 'never' \| 'always'` | `'never'` | Sets how iOS insets the content for the safe area. |

Visibility, loading and refresh:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `onViewableItemsChanged` | `({ viewableItems, changed }) => void` | | Called when items come into view or leave it. |
| `viewabilityConfig` | `ViewabilityConfig` | | Sets when an item counts as visible, with FlatList's options. |
| `onEndReached` | `({ distanceFromEnd }) => void` | | Called when the end of the list comes within `onEndReachedThreshold`. |
| `onEndReachedThreshold` | `number` | `2` | Sets the distance from the end, in screens, that calls `onEndReached`. |
| `onStartReached` | `({ distanceFromStart }) => void` | | Called when the start of the list comes within `onStartReachedThreshold`. |
| `onStartReachedThreshold` | `number` | `2` | Sets the distance from the start, in screens, that calls `onStartReached`. |
| `onRefresh` | `() => void` | | Adds pull to refresh and is called when the user pulls. |
| `refreshing` | `boolean` | `false` | Shows the refresh spinner while it's `true`. |
| `refreshControl` | `ReactElement \| { tintColor?, colors?, progressViewOffset? }` | | Takes a `RefreshControl` element, as on FlatList, or only the spinner style. `tintColor` works on iOS and `colors` on Android. |

The list view:

| Prop | Type | Default | Description |
| --- | --- | --- | --- |
| `style` | `StyleProp<ViewStyle>` | | Styles the list's own view. |
| `onLayout` | `(event) => void` | | Called when the list's size or position changes. |
| `testID` | `string` | | Sets the ID that tests find the list by. |
| `accessibilityLabel` | `string` | | Sets the label that screen readers read for the list. |

### Cells

`renderCell` is a worklet that returns a tree of cell nodes. Styles take React Native's names and meanings, and `CellStyle` lists every key.

Define `renderCell` and `renderSeparatorCell` outside your component, or wrap them in `useCallback`, because a new function rebuilds every row. The root node ignores `margin`, so put the spacing on a child or in the list's gaps.

| Builder | Description |
| --- | --- |
| `cell.view(style?, children?)` | Returns a box that lays out its children with flexbox. |
| `cell.text(text, style?, { allowFontScaling? }?)` | Returns a text that follows the system font size unless `allowFontScaling` is `false`. |
| `cell.image({ source, resizeMode?, tintColor?, ...style })` | Returns an image loaded from `source`, a URL or `{ uri, headers }`. |
| `cell.pressable({ id, pressedOpacity?, ...style }, children?)` | Returns a box that calls `onPress` and `onLongPress` with its `id` and fades to `pressedOpacity` (0.7) while pressed. |
| `cell.spacer(size?)` | Returns an empty box `size` dp long, or one that fills the free space when `size` is left out. |

### Methods

`scrollToIndex`, `scrollToOffset` and `scrollToEnd` animate unless `animated` is `false`.

| Method | Description |
| --- | --- |
| `scrollToIndex({ index, animated?, viewPosition?, viewOffset? })` | Scrolls to the item at `index`. A `viewPosition` of 0 puts it at the top, 0.5 in the middle and 1 at the bottom. |
| `scrollToOffset({ offset, animated? })` | Scrolls to `offset` dp from the start of the content. |
| `scrollToEnd({ animated? })` | Scrolls to the end of the list. |
| `getScrollableNode()` | Returns the native list view. |
| `getAnimatableRef()` | Returns the native list view for Animated and Reanimated. |

## Migrating from `FlatList`

Change the import:

```diff
-import { FlatList } from 'react-native';
+import { Flume } from 'react-native-flume';
```

Then rename `<FlatList>` to `<Flume>`. `renderItem`, `keyExtractor`, the list components, the scroll events, viewability, pull to refresh, `scrollToIndex`, `scrollToOffset` and `scrollToEnd` keep working. A few things change:

- `ItemSeparatorComponent` draws between React rows. With `renderCell`, draw separators with `renderSeparatorCell` or a `separator` line.
- In a grid, `ItemSeparatorComponent` draws under each item and gets that item as `leadingItem`, as on FlashList. FlatList draws it once under each row of items.
- In a grid, an item's `width: '33%'` is a third of its column, as on FlashList. FlatList measures it against the whole row.
- In a grid, an item with `flex: 1` in a last row that isn't full is one column wide, as on FlashList. FlatList stretches it across the row.
- In a grid, a React row stays at its content height, so a tile with `flex: 1` doesn't stretch to the tallest tile in its row. Worklet rows stretch as on FlatList.
- `columnWrapperStyle` takes gaps and horizontal padding. Its `rowGap` spaces the grid's rows, where FlatList ignores it.
- A horizontal list keeps each React row at its own height. FlatList stretches a row without a height to the list's height.
- `stickyHeaderIndices` counts items only. With a `ListHeaderComponent`, subtract 1 from FlatList's indices.
- `contentContainerStyle` takes padding, gaps, a background and `flexGrow`. Other style keys are ignored.
- Rows animate when `data` changes and stay in place when items load above them. `itemAnimations` and `maintainVisibleContentPosition` turn this off.

## Roadmap

- [x] 0.1: Android and iOS
- [ ] 1.0: First stable release

## License

MIT

---

<p align="center">Made by <a href="https://github.com/SectionTN">SectionTN</a></p>
