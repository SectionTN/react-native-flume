import { createElement, createRef, StrictMode, useState } from 'react';
import { act, create, type ReactTestRenderer } from 'react-test-renderer';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { FlumeHost } from '../store';
import type { CellInfo, FlumeRef } from '../types';

(globalThis as { IS_REACT_ACT_ENVIRONMENT?: boolean }).IS_REACT_ACT_ENVIRONMENT = true;

vi.mock('react-native-worklets', () => ({
  createSerializable: (value: unknown) => ({ serializable: value }),
  createWorkletRuntime: () => ({}),
  getUISchedulerHolder: () => ({}),
  runOnRuntimeSync: (_runtime: unknown, worklet: () => void) => worklet(),
}));
vi.mock('../specs/NativeFlume', () => ({ default: { setStats: vi.fn(), logStats: vi.fn() } }));
vi.mock('../specs/FlumeViewNativeComponent', () => ({
  default: 'FlumeView',
  Commands: {
    scrollToIndex: vi.fn(),
    scrollToOffset: vi.fn(),
    scrollToEnd: vi.fn(),
    setNativeRefreshing: vi.fn(),
  },
}));
vi.mock('../specs/FlumeSlotViewNativeComponent', () => ({ default: 'FlumeSlotView' }));
vi.mock('react-native', () => ({ Keyboard: { isVisible: vi.fn(() => true), dismiss: vi.fn() } }));

type DataCall = {
  storeId: number;
  keys: string[];
  changedIndices: number[];
  changedItems: unknown[];
  alive: boolean;
  deferred: boolean;
  typeTable: string[];
  typeIds: number[];
};
type CellCall = { storeId: number; alive: boolean };
type KindsCall = { storeId: number; reactByDefault: boolean; reactTypes: string[] };

// Records what reaches the native host, in order, and whether the store was still alive when it did.
function installHost() {
  const alive = new Set<number>();
  const data: DataCall[] = [];
  const cells: CellCall[] = [];
  const kinds: KindsCall[] = [];
  const log: string[] = [];
  let reporter: ((level: number, message: string) => void) | undefined;
  let next = 1;
  const host: FlumeHost = {
    install: () => {},
    setScheduler: () => {},
    createStore: () => {
      const id = next++;
      alive.add(id);
      return id;
    },
    releaseStore: (storeId) => {
      alive.delete(storeId);
    },
    setData: (
      storeId,
      keys,
      typeTable,
      typeIds,
      changedIndices,
      changedItems,
      _stableKeys,
      deferred,
    ) => {
      data.push({
        storeId,
        keys,
        typeTable,
        typeIds: [...typeIds],
        changedIndices,
        changedItems,
        alive: alive.has(storeId),
        deferred,
      });
      log.push(`data [${changedIndices.join(',')}]`);
    },
    setCell: (storeId) => {
      cells.push({ storeId, alive: alive.has(storeId) });
    },
    setCellKinds: (storeId, reactByDefault, reactTypes) => {
      kinds.push({ storeId, reactByDefault, reactTypes });
      log.push(`kinds ${reactByDefault} [${reactTypes.join(',')}]`);
    },
    setReporter: (fn) => {
      reporter = fn;
    },
  };
  (globalThis as { __flume?: FlumeHost }).__flume = host;
  return {
    alive,
    data,
    cells,
    kinds,
    log,
    report: (level: number, message: string) => reporter?.(level, message),
    hasReporter: () => reporter !== undefined,
  };
}

const item = { id: 'a' };
const renderCell = () => ({ type: 'view' as const });

// Host refs resolve to a stub, as a native view would, so the ref commands have a target.
async function mount(element: Parameters<typeof create>[0]): Promise<ReactTestRenderer> {
  let tree: ReactTestRenderer | undefined;
  await act(async () => {
    tree = create(element, { createNodeMock: () => ({}) });
  });
  return tree as ReactTestRenderer;
}

function renderedStoreId(tree: ReactTestRenderer): number {
  return tree.root.find((node) => String(node.type) === 'FlumeView').props.storeId as number;
}

type Entry = {
  slot: number;
  kind: number;
  key: string;
  index: number;
  bound: boolean;
  minMain: number;
};

const rowEntry = (slot: number, key: string, index: number): Entry => ({
  slot,
  kind: 0,
  key,
  index,
  bound: true,
  minMain: 0,
});

// Delivers a native slot table the way the host's onSlotsChanged event does; React Native releases the pooled
// event once the handler returns.
async function sendSlots(tree: ReactTestRenderer, slots: Entry[], cellWidth = -1, jsSeen = 0) {
  const native = tree.root.find((node) => String(node.type) === 'FlumeView');
  await act(async () => {
    const event: { nativeEvent: unknown } = { nativeEvent: { slots, jsSeen, cellWidth } };
    native.props.onSlotsChanged(event);
    event.nativeEvent = null;
  });
}

function slotViews(tree: ReactTestRenderer, slot: number) {
  return tree.root.findAll(
    (node) => String(node.type) === 'FlumeSlotView' && node.props.slot === slot,
  );
}

// A renderItem that records every CellInfo it gets.
function recordingCell() {
  const infos: CellInfo<unknown>[] = [];
  const renderItem = (info: CellInfo<unknown>) => {
    infos.push(info);
    return createElement('Row', { id: (info.item as { id: string }).id });
  };
  return { infos, renderItem };
}

describe('Flume store lifecycle', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('rebinds to a live store after StrictMode replays the effects', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(StrictMode, null, createElement(Flume, { data: [item], renderCell })),
    );
    const storeId = renderedStoreId(tree);
    expect(host.alive.has(storeId)).toBe(true);
    expect(host.cells.at(-1)).toEqual({ storeId, alive: true });
    expect(host.data.at(-1)).toEqual({
      storeId,
      keys: ['a'],
      typeTable: ['default'],
      typeIds: [0],
      changedIndices: [0],
      changedItems: [item],
      alive: true,
      deferred: false,
    });
  });

  it('leaves no store behind after a StrictMode mount and unmount', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(StrictMode, null, createElement(Flume, { data: [item], renderCell })),
    );
    expect([...host.alive]).toEqual([renderedStoreId(tree)]);
    await act(async () => {
      tree.unmount();
    });
    expect(host.alive.size).toBe(0);
  });

  it('releases the rendered store on unmount', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(createElement(Flume, { data: [item], renderCell }));
    const storeId = renderedStoreId(tree);
    await act(async () => {
      tree.unmount();
    });
    expect(host.alive.has(storeId)).toBe(false);
  });
});

describe('Flume props', () => {
  beforeEach(() => {
    vi.resetModules();
    vi.restoreAllMocks();
  });

  it('maps a press event to the item by key', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const onPress = vi.fn();
    const tree = await mount(createElement(Flume, { data: [item], renderCell, onPress }));
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    native.props.onCellPress({ nativeEvent: { index: 0, key: 'a', id: '' } });
    expect(onPress).toHaveBeenCalledWith({ item, index: 0, id: undefined });
  });

  it('warns once when renderItem accompanies renderCell', async () => {
    installHost();
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const { Flume } = await import('../Flume');
    const renderItem = () => null;
    const tree = await mount(createElement(Flume, { data: [item], renderCell, renderItem }));
    await act(async () => {
      tree.update(createElement(Flume, { data: [item], renderCell, renderItem }));
    });
    const calls = warn.mock.calls.filter((call) => String(call[0]).includes('renderItem'));
    expect(calls).toHaveLength(1);
  });

  it('registers a reporter in development', async () => {
    const host = installHost();
    const error = vi.spyOn(console, 'error').mockImplementation(() => {});
    const { Flume } = await import('../Flume');
    await mount(createElement(Flume, { data: [item], renderCell }));
    expect(host.hasReporter()).toBe(true);
    host.report(2, 'boom');
    expect(error).toHaveBeenCalledWith('react-native-flume: boom');
  });
});

describe('Flume cell kinds', () => {
  beforeEach(() => {
    vi.resetModules();
    vi.restoreAllMocks();
  });

  it('a React list sends keys and types without items', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(createElement(Flume, { data: [item], renderItem: () => null }));
    const storeId = renderedStoreId(tree);
    expect(host.kinds.at(-1)).toEqual({ storeId, reactByDefault: true, reactTypes: [] });
    expect(host.data.at(-1)).toMatchObject({ storeId, keys: ['a'], changedIndices: [] });
  });

  it('a kinds change sends the worklet items again', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const data = [item];
    const reactCells = { default: () => null };
    const tree = await mount(createElement(Flume, { data, renderCell, reactCells }));
    expect(host.data.at(-1)?.changedIndices).toEqual([]);
    const mark = host.log.length;
    await act(async () => {
      tree.update(createElement(Flume, { data, renderCell }));
    });
    expect(host.log.slice(mark)).toEqual(['kinds false []', 'data [0]']);
  });

  it('reports a type without a renderer once', async () => {
    installHost();
    const error = vi.spyOn(console, 'error').mockImplementation(() => {});
    const { Flume } = await import('../Flume');
    const props = { data: [item], getItemType: () => 'x', reactCells: { voice: () => null } };
    const tree = await mount(createElement(Flume, props));
    await act(async () => {
      tree.update(createElement(Flume, { ...props, data: [{ ...item }] }));
    });
    const calls = error.mock.calls.filter((call) =>
      String(call[0]).includes('no renderer for type "x"'),
    );
    expect(calls).toHaveLength(1);
  });
});

describe('Flume React cells', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('renders a React cell for every slot native lists', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    const [view] = slotViews(tree, 1);
    expect(view?.props.style).toEqual({ position: 'absolute', left: 0, right: 0 });
    expect(infos.at(-1)).toEqual({ item, index: 0, target: 'Cell', extraData: undefined });
  });

  it('renders the new item in the commit that changes the data', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    const next = { id: 'a' };
    await act(async () => {
      tree.update(createElement(Flume, { data: [next], renderItem }));
    });
    expect(infos.at(-1)?.item).toBe(next);
  });

  it('keeps the last item when its key leaves the data', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    await act(async () => {
      tree.update(createElement(Flume, { data: [], renderItem }));
    });
    expect(slotViews(tree, 1)).toHaveLength(1);
    expect(infos.every((info) => info.item !== undefined)).toBe(true);
  });

  it('renders the list components in their slots', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const Header = () => createElement('HeaderNode');
    const Footer = () => createElement('FooterNode');
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderItem: () => null,
        ListHeaderComponent: Header,
        ListFooterComponent: createElement(Footer),
      }),
    );
    const entry = (slot: number, kind: number, key: string): Entry => ({
      slot,
      kind,
      key,
      index: -1,
      bound: true,
      minMain: 0,
    });
    await sendSlots(tree, [entry(1, 1, 'header'), entry(2, 2, 'footer')]);
    const ofKind = (kind: number) =>
      tree.root.findAll(
        (node) => String(node.type) === 'FlumeSlotView' && node.props.kind === kind,
      );
    expect(ofKind(1).length).toBeGreaterThan(0);
    expect(ofKind(2).length).toBeGreaterThan(0);
    for (const view of ofKind(1)) expect(view.findAllByType(Header)).toHaveLength(1);
    for (const view of ofKind(2)) expect(view.findAllByType(Footer)).toHaveLength(1);
  });

  it('re-renders mounted cells when extraData changes', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const data = [item];
    const tree = await mount(createElement(Flume, { data, renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    await act(async () => {
      tree.update(createElement(Flume, { data, renderItem, extraData: 1 }));
    });
    expect(infos.at(-1)?.extraData).toBe(1);
  });

  it('pre-renders the first rows in the mount commit', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const data = [{ id: 'a' }, { id: 'b' }, { id: 'c' }];
    const tree = await mount(createElement(Flume, { data, renderItem }));
    const views = tree.root.findAll((node) => String(node.type) === 'FlumeSlotView');
    expect(views.map((view) => [view.props.slot, view.props.itemKey])).toEqual([
      [-3, 'c'],
      [-2, 'b'],
      [-1, 'a'],
    ]);
    expect(infos.map((info) => info.index).sort()).toEqual([0, 1, 2]);
  });

  it('renders a sticky copy as a sticky header', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem, numColumns: 2 }));
    await sendSlots(tree, [{ ...rowEntry(7, 'a', 0), kind: 4 }], 180);
    const [view] = slotViews(tree, 7);
    expect(view?.props.style).toEqual({ position: 'absolute', left: 0, right: 0 });
    expect(infos).toContainEqual({ item, index: 0, target: 'StickyHeader', extraData: undefined });
  });

  it('a cell under handled closes the keyboard when no child takes the tap', async () => {
    installHost();
    const { Keyboard } = await import('react-native');
    const { Flume } = await import('../Flume');
    const { renderItem } = recordingCell();
    const props = { data: [item], renderItem };
    const tree = await mount(
      createElement(Flume, { ...props, keyboardShouldPersistTaps: 'handled' }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    const [view] = slotViews(tree, 1);
    expect(view?.props.onStartShouldSetResponder()).toBe(true);
    view?.props.onResponderRelease();
    expect(Keyboard.dismiss).toHaveBeenCalledTimes(1);
    await act(async () => {
      tree.update(createElement(Flume, { ...props, keyboardShouldPersistTaps: 'never' }));
    });
    const [after] = slotViews(tree, 1);
    expect(after?.props.onStartShouldSetResponder).toBeUndefined();
    expect(after?.props.onResponderRelease).toBeUndefined();
  });

  it('holds a data change for the mount that carries its pre-rendered cells', async () => {
    const { data } = installHost();
    const { Flume } = await import('../Flume');
    const { renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(-1, 'a', 0)], -1, -1);
    const added = { id: 'n' };
    await act(async () => {
      tree.update(createElement(Flume, { data: [added, item], renderItem }));
    });
    await act(async () => {
      tree.update(createElement(Flume, { data: [item], renderItem }));
    });
    expect(data.map((call) => call.deferred)).toEqual([false, true, false]);
  });

  it('keeps a pre-rendered cell mounted when native registers it', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    const [before] = slotViews(tree, -1);
    const rendered = infos.length;
    await sendSlots(tree, [{ ...rowEntry(-1, 'a', -1), bound: false }], -1, -1);
    const [after] = slotViews(tree, -1);
    expect(before).toBeDefined();
    expect(after).toBe(before);
    expect(infos.length).toBe(rendered);
  });
});

describe('Flume host props', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('forwards the host props', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderCell,
        testID: 'list',
        accessibilityLabel: 'Chats',
        horizontal: true,
      }),
    );
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    expect(native.props).toMatchObject({
      testID: 'list',
      accessibilityLabel: 'Chats',
      horizontal: true,
    });
  });
});

describe('Flume viewability', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('maps viewable keys to items', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const onViewableItemsChanged = vi.fn();
    const tree = await mount(
      createElement(Flume, { data: [item], renderCell, onViewableItemsChanged }),
    );
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    native.props.onViewableItemsChanged({
      nativeEvent: {
        viewable: [{ index: 0, key: 'a', isViewable: true }],
        changed: [
          { index: 0, key: 'a', isViewable: true },
          { index: 5, key: 'gone', isViewable: false },
        ],
      },
    });
    expect(onViewableItemsChanged).toHaveBeenCalledWith({
      viewableItems: [{ item, key: 'a', index: 0, isViewable: true }],
      changed: [{ item, key: 'a', index: 0, isViewable: true }],
    });
  });

  it('reports the exit of a visible item removed from the data', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const onViewableItemsChanged = vi.fn();
    const removed = { id: 'b' };
    const tree = await mount(
      createElement(Flume, { data: [item, removed], renderCell, onViewableItemsChanged }),
    );
    const native = () => tree.root.find((node) => String(node.type) === 'FlumeView');
    native().props.onViewableItemsChanged({
      nativeEvent: {
        viewable: [
          { index: 0, key: 'a', isViewable: true },
          { index: 1, key: 'b', isViewable: true },
        ],
        changed: [
          { index: 0, key: 'a', isViewable: true },
          { index: 1, key: 'b', isViewable: true },
        ],
      },
    });
    await act(async () => {
      tree.update(createElement(Flume, { data: [item], renderCell, onViewableItemsChanged }));
    });
    native().props.onViewableItemsChanged({
      nativeEvent: {
        viewable: [{ index: 0, key: 'a', isViewable: true }],
        changed: [{ index: 1, key: 'b', isViewable: false }],
      },
    });
    expect(onViewableItemsChanged).toHaveBeenLastCalledWith({
      viewableItems: [{ item, key: 'a', index: 0, isViewable: true }],
      changed: [{ item: removed, key: 'b', index: 1, isViewable: false }],
    });
  });
});

describe('Flume edges', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('unwraps the end reached event', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const onEndReached = vi.fn();
    const tree = await mount(createElement(Flume, { data: [item], renderCell, onEndReached }));
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    native.props.onEndReached({ nativeEvent: { distanceFromEnd: 12 } });
    expect(onEndReached).toHaveBeenCalledWith({ distanceFromEnd: 12 });
  });
});

describe('Flume ref', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('sends the commands with FlatList defaults', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const ref = createRef<FlumeRef>();
    await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToIndex({ index: 0 });
    ref.current?.scrollToOffset({ offset: 20, animated: false });
    ref.current?.scrollToEnd();
    expect(Commands.scrollToIndex).toHaveBeenCalledWith(expect.anything(), 0, true, 0, 0);
    expect(Commands.scrollToOffset).toHaveBeenCalledWith(expect.anything(), 20, false);
    expect(Commands.scrollToEnd).toHaveBeenCalledWith(expect.anything(), true);
  });
});

describe('Flume stats', () => {
  beforeEach(() => {
    vi.resetModules();
    vi.restoreAllMocks();
  });

  const items = [{ id: 'a' }, { id: 'b' }, { id: 'c' }];
  const statsLines = (log: { mock: { calls: unknown[][] } }) =>
    log.mock.calls.map((call) => String(call[0])).filter((line) => line.startsWith('FlumeStats'));

  it('logs one setData line while stats are on', async () => {
    installHost();
    const { unstable_setFlumeStats } = await import('../stats');
    const { Flume } = await import('../Flume');
    unstable_setFlumeStats(true);
    const log = vi.spyOn(console, 'log').mockImplementation(() => {});
    await mount(createElement(Flume, { data: items, renderCell }));
    const lines = statsLines(log);
    expect(lines).toHaveLength(1);
    expect(lines[0]).toMatch(/^FlumeStats setData keys=3 items=3 diff=\d+\.\d{2} host=\d+\.\d{2}$/);
  });

  it('logs nothing while stats are off', async () => {
    installHost();
    const { unstable_setFlumeStats } = await import('../stats');
    const { Flume } = await import('../Flume');
    unstable_setFlumeStats(false);
    const log = vi.spyOn(console, 'log').mockImplementation(() => {});
    await mount(createElement(Flume, { data: items, renderCell }));
    expect(statsLines(log)).toHaveLength(0);
  });
});

describe('Flume native calls', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('calls setData once when only the extractor functions change', async () => {
    const { data } = installHost();
    const { Flume } = await import('../Flume');
    const { renderItem } = recordingCell();
    const list = [item, { id: 'b' }];
    const props = () => ({
      data: list,
      renderItem,
      keyExtractor: (entry: unknown) => (entry as { id: string }).id,
      getItemType: () => 'row',
    });
    const tree = await mount(createElement(Flume, props()));
    await act(async () => {
      tree.update(createElement(Flume, props()));
    });
    expect(data).toHaveLength(1);
  });

  it('skips setData when React items change but keys and types do not', async () => {
    const { data } = installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    const next = { id: 'a' };
    await act(async () => {
      tree.update(createElement(Flume, { data: [next], renderItem }));
    });
    expect(data).toHaveLength(1);
    expect(infos.at(-1)?.item).toBe(next);
  });
});

describe('Flume types for native', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('sends each type once with an index per key', async () => {
    const { data } = installHost();
    const { Flume } = await import('../Flume');
    const { renderItem } = recordingCell();
    const list = [{ id: 'a' }, { id: 'b' }, { id: 'c' }];
    const getItemType = (_entry: unknown, index: number) => (index === 1 ? 'voice' : 'text');
    await mount(createElement(Flume, { data: list, renderItem, getItemType }));
    expect(data[0]?.typeTable).toEqual(['text', 'voice']);
    expect(data[0]?.typeIds).toEqual([0, 1, 0]);
  });
});

describe('Flume refresh', () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it('stops the native spinner when onRefresh leaves refreshing false', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const tree = await mount(
      createElement(Flume, { data: [item], renderCell, refreshing: false, onRefresh: () => {} }),
    );
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    await act(async () => {
      native.props.onRefresh();
    });
    expect(Commands.setNativeRefreshing).toHaveBeenCalledWith(expect.anything(), false);
  });

  it('sends nothing when onRefresh sets refreshing', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    function Refreshing() {
      const [refreshing, setRefreshing] = useState(false);
      return createElement(Flume, {
        data: [item],
        renderCell,
        refreshing,
        onRefresh: () => setRefreshing(true),
      });
    }
    const tree = await mount(createElement(Refreshing));
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    await act(async () => {
      native.props.onRefresh();
    });
    expect(Commands.setNativeRefreshing).not.toHaveBeenCalled();
  });
});
