import {
  createElement,
  createRef,
  Fragment,
  type ReactNode,
  StrictMode,
  Suspense,
  startTransition,
  useEffect,
  useState,
} from 'react';
import { act, create, type ReactTestInstance, type ReactTestRenderer } from 'react-test-renderer';
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
vi.mock('react-native', () => ({
  Keyboard: { isVisible: vi.fn(() => true), dismiss: vi.fn() },
  View: 'View',
}));

type DataCall = {
  storeId: number;
  keys: string[];
  changedIndices: number[];
  changedItems: unknown[];
  alive: boolean;
  deferred: boolean;
  typeTable: string[];
  typeIds: number[];
  sticky: number[];
};
type CellCall = { storeId: number; alive: boolean };
type KindsCall = { storeId: number; reactByDefault: boolean; reactTypes: string[] };
type StickyCall = { storeId: number; sticky: number[] };

// Records what reaches the native host, in order, and whether the store was still alive when it did.
function installHost() {
  const alive = new Set<number>();
  const data: DataCall[] = [];
  const cells: CellCall[] = [];
  const kinds: KindsCall[] = [];
  const stickies: StickyCall[] = [];
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
      sticky,
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
        sticky: [...sticky],
      });
      log.push(`data [${changedIndices.join(',')}]`);
    },
    setCell: (storeId) => {
      cells.push({ storeId, alive: alive.has(storeId) });
    },
    setSeparatorCell: (_storeId, separatorCell) => {
      log.push(`separator ${separatorCell === null ? 'null' : 'set'}`);
    },
    setCellKinds: (storeId, reactByDefault, reactTypes) => {
      kinds.push({ storeId, reactByDefault, reactTypes });
      log.push(`kinds ${reactByDefault} [${reactTypes.join(',')}]`);
    },
    setStickyHeaderIndices: (storeId, sticky) => {
      stickies.push({ storeId, sticky: [...sticky] });
      log.push(`sticky [${sticky.join(',')}]`);
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
    stickies,
    log,
    report: (level: number, message: string) => reporter?.(level, message),
    hasReporter: () => reporter !== undefined,
  };
}

const item = { id: 'a' };
const renderCell = () => ({ type: 'view' as const });
// Stands in for React Native's RefreshControl, which the react-native mock does not provide.
const RefreshControlStub = (_props: { refreshing: boolean; onRefresh?: () => void }) => null;

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
async function sendSlots(
  tree: ReactTestRenderer,
  slots: Entry[],
  cellWidth = -1,
  jsSeen = 0,
  width = -1,
) {
  const native = tree.root.find((node) => String(node.type) === 'FlumeView');
  await act(async () => {
    const event: { nativeEvent: unknown } = { nativeEvent: { slots, jsSeen, cellWidth, width } };
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
      sticky: [],
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

  it('renders an empty list for null and undefined data', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const renderItem = () => null;
    const tree = await mount(createElement(Flume, { data: null, renderItem }));
    for (const data of [undefined, [item], null]) {
      await act(async () => {
        tree.update(createElement(Flume, { data, renderItem }));
      });
    }
    expect(host.data.at(-1)?.keys).toEqual([]);
    expect(host.data.some((call) => call.keys.includes('a'))).toBe(true);
    expect(new Set(host.data.map((call) => call.storeId)).size).toBe(1);
  });

  it('turns anchoring off and on again without a remount', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, { data: [item], renderCell, maintainVisibleContentPosition: {} }),
    );
    const disabled = () =>
      tree.root.find((node) => String(node.type) === 'FlumeView').props
        .maintainVisibleContentPosition.disabled;
    const id = renderedStoreId(tree);
    for (const [value, expected] of [
      [false, true],
      [{ autoscrollToBottomThreshold: 0.2 }, false],
    ] as const) {
      await act(async () => {
        tree.update(
          createElement(Flume, { data: [item], renderCell, maintainVisibleContentPosition: value }),
        );
      });
      expect(disabled()).toBe(expected);
    }
    expect(renderedStoreId(tree)).toBe(id);
  });

  it('passes host view props to the native view', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderCell,
        nativeID: 'list',
        pointerEvents: 'box-none',
        accessibilityRole: 'list',
      }),
    );
    expect(tree.root.find((node) => String(node.type) === 'FlumeView').props).toMatchObject({
      nativeID: 'list',
      pointerEvents: 'box-none',
      accessibilityRole: 'list',
    });
  });

  it('keeps ignored props off the native view', async () => {
    installHost();
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderCell,
        windowSize: 5,
        getItemLayout: () => ({ length: 10, offset: 0, index: 0 }),
        removeClippedSubviews: true,
      }),
    );
    const props = tree.root.find((node) => String(node.type) === 'FlumeView').props;
    expect(props).not.toHaveProperty('windowSize');
    expect(props).not.toHaveProperty('getItemLayout');
    expect(props).not.toHaveProperty('removeClippedSubviews');
    expect(warn).not.toHaveBeenCalled();
  });

  it('scrolls to the index of an initialScrollIndex object', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [item, item, item, item],
        renderCell,
        initialScrollIndex: { index: 3, viewOffset: 10 },
      }),
    );
    expect(
      tree.root.find((node) => String(node.type) === 'FlumeView').props.initialScrollIndex,
    ).toBe(3);
  });

  it('treats null callbacks as not given', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderCell,
        onEndReached: null,
        onRefresh: null,
        onViewableItemsChanged: null,
        onPress: null,
      }),
    );
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    expect(native.props.onEndReached).toBeUndefined();
    expect(native.props.onRefresh).toBeUndefined();
    expect(native.props.onViewableItemsChanged).toBeUndefined();
    expect(native.props.onCellPress).toBeUndefined();
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
    expect(infos.at(-1)).toEqual({
      item,
      index: 0,
      target: 'Cell',
      extraData: undefined,
      data: [item],
      type: 'default',
      separators: expect.any(Object),
    });
  });

  it('gives renderItem the data and the item type', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const data = [{ id: 'a', kind: 'photo' }];
    const tree = await mount(
      createElement(Flume, {
        data,
        renderItem,
        getItemType: (i: unknown) => (i as { kind: string }).kind,
      }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    expect(infos.at(-1)?.data).toBe(data);
    expect(infos.at(-1)?.type).toBe('photo');
  });

  it('renders a fragment from renderItem', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const renderItem = () =>
      createElement(
        Fragment,
        null,
        createElement('Text', null, 'a'),
        createElement('Text', null, 'b'),
      );
    const tree = await mount(createElement(Flume, { data: [item], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    expect(slotViews(tree, 1)).toHaveLength(1);
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

  it('keeps the type and data of a row that left the data', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { infos, renderItem } = recordingCell();
    const photo = { id: 'a', kind: 'photo' };
    const getItemType = (i: unknown) => (i as { kind: string }).kind;
    const tree = await mount(createElement(Flume, { data: [photo], renderItem, getItemType }));
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    await act(async () => {
      tree.update(createElement(Flume, { data: [], renderItem, getItemType }));
    });
    expect(infos.map((info) => info.type)).not.toContain('default');
    expect(infos.at(-1)?.data).toEqual([photo]);
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
    expect(infos).toContainEqual({
      item,
      index: 0,
      target: 'StickyHeader',
      extraData: undefined,
      data: [item],
      type: 'default',
      separators: expect.any(Object),
    });
  });

  it('keeps a React cell mounted at its new width when numColumns changes', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    let unmounts = 0;
    const Cell = () => {
      useEffect(
        () => () => {
          unmounts++;
        },
        [],
      );
      return createElement('Row');
    };
    const renderItem = () => createElement(Cell);
    const contentContainerStyle = { paddingLeft: 16, paddingRight: 8 };
    const props = { data: [item], renderItem, contentContainerStyle };
    const tree = await mount(createElement(Flume, props));
    await sendSlots(tree, [rowEntry(1, 'a', 0)], -1, -1, 360);
    unmounts = 0;
    await act(async () => {
      tree.update(createElement(Flume, { ...props, numColumns: 2 }));
    });
    expect(unmounts).toBe(0);
    expect(slotViews(tree, 1)[0]?.props.style).toEqual({ position: 'absolute', width: 168 });
  });

  it('renders the new column width with gaps in the same render as a numColumns change', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { renderItem } = recordingCell();
    const columnWrapperStyle = { gap: 8, paddingHorizontal: 16 };
    const props = { data: [item], renderItem, columnWrapperStyle, numColumns: 3 };
    const tree = await mount(createElement(Flume, props));
    await sendSlots(tree, [rowEntry(1, 'a', 0)], -1, 84, 300);
    expect(slotViews(tree, 1)[0]?.props.style).toEqual({ position: 'absolute', width: 84 });
    await act(async () => {
      tree.update(createElement(Flume, { ...props, numColumns: 2 }));
    });
    expect(slotViews(tree, 1)[0]?.props.style).toEqual({ position: 'absolute', width: 130 });
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

  it('FlumeView gets no stickyHeaderIndices prop', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, { data: [item], renderCell, stickyHeaderIndices: [0] }),
    );
    const native = tree.root.find((node) => String(node.type) === 'FlumeView');
    expect(native.props).not.toHaveProperty('stickyHeaderIndices');
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
  const frames: Array<() => void> = [];
  const nextFrame = () => {
    for (const run of frames.splice(0)) run();
  };
  const flush = () => new Promise<void>((resolve) => setTimeout(resolve, 0));
  const track = (done: Promise<void> | undefined) => {
    const state = { resolved: false };
    void done?.then(() => {
      state.resolved = true;
    });
    return state;
  };
  const endCommand = (tree: ReactTestRenderer, id: number) =>
    act(() => {
      tree.root
        .find((node) => String(node.type) === 'FlumeView')
        .props.onScrollCommandEnd({ nativeEvent: { id } });
    });

  beforeEach(() => {
    vi.resetModules();
    frames.length = 0;
    vi.stubGlobal('requestAnimationFrame', (run: () => void) => frames.push(run));
  });

  it('scrollToIndex resolves when native reports its end', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    const state = track(ref.current?.scrollToIndex({ index: 0 }));
    await flush();
    expect(state.resolved).toBe(false);
    await endCommand(tree, 1);
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('resolves at once without a native view', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const ref = createRef<FlumeRef>();
    await act(async () => {
      create(createElement(Flume, { ref, data: [item], renderCell }), {
        createNodeMock: () => null,
      });
    });
    const state = track(ref.current?.scrollToOffset({ offset: 10 }));
    await flush();
    expect(state.resolved).toBe(true);
    expect(Commands.scrollToOffset).not.toHaveBeenCalled();
  });

  it('resolves pending scrolls on unmount', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    const state = track(ref.current?.scrollToEnd());
    await act(async () => tree.unmount());
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('sends a command again under a new id after data of the same frame', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    vi.mocked(Commands.scrollToIndex).mockImplementation((_view, id) => {
      host.log.push(`index ${id}`);
    });
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToIndex({ index: 1, animated: false });
    await act(async () =>
      tree.update(createElement(Flume, { ref, data: [item, { id: 'b' }], renderCell })),
    );
    expect(host.log.slice(-3)).toEqual(['index 1', 'data [1]', 'index 2']);
    expect(Commands.scrollToIndex).toHaveBeenLastCalledWith(expect.anything(), 2, 1, false, 0, 0);
  });

  it('keeps a scroll sent again pending until its new id ends', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    const state = track(ref.current?.scrollToIndex({ index: 1 }));
    await act(async () =>
      tree.update(createElement(Flume, { ref, data: [item, { id: 'b' }], renderCell })),
    );
    await endCommand(tree, 1);
    await flush();
    expect(state.resolved).toBe(false);
    await endCommand(tree, 2);
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('does not send a scroll again once its frame passed', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToEnd();
    nextFrame();
    await act(async () =>
      tree.update(createElement(Flume, { ref, data: [item, { id: 'b' }], renderCell })),
    );
    expect(Commands.scrollToEnd).toHaveBeenCalledTimes(1);
  });

  it('does not send a scroll again when native data is unchanged', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToOffset({ offset: 10 });
    await act(async () => tree.update(createElement(Flume, { ref, data: [item], renderCell })));
    expect(Commands.scrollToOffset).toHaveBeenCalledTimes(1);
  });

  it('checks an index against the data of the same frame', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const ref = createRef<FlumeRef>();
    const tree = await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToIndex({ index: 1 });
    await act(async () =>
      tree.update(createElement(Flume, { ref, data: [item, { id: 'b' }], renderCell })),
    );
    nextFrame();
    expect(warn).not.toHaveBeenCalled();
    warn.mockRestore();
  });

  it('warns about an index past the data once its frame passes', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const ref = createRef<FlumeRef>();
    await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToIndex({ index: 5 });
    expect(warn).not.toHaveBeenCalled();
    nextFrame();
    expect(warn).toHaveBeenCalledWith(
      'react-native-flume: scrollToIndex: index 5 is out of range for 1 items',
    );
    warn.mockRestore();
  });

  it('sends the commands with FlatList defaults and a growing id', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const ref = createRef<FlumeRef>();
    await mount(createElement(Flume, { ref, data: [item], renderCell }));
    ref.current?.scrollToIndex({ index: 0 });
    ref.current?.scrollToOffset({ offset: 20, animated: false });
    ref.current?.scrollToEnd();
    expect(Commands.scrollToIndex).toHaveBeenCalledWith(expect.anything(), 1, 0, true, 0, 0);
    expect(Commands.scrollToOffset).toHaveBeenCalledWith(expect.anything(), 2, 20, false);
    expect(Commands.scrollToEnd).toHaveBeenCalledWith(expect.anything(), 3, true);
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

  it('logs no line for a commit whose diff ran before stats were on', async () => {
    installHost();
    const { unstable_setFlumeStats } = await import('../stats');
    const { Flume } = await import('../Flume');
    unstable_setFlumeStats(false);
    // A sibling rendered after the list switches stats on before the list's layout effect runs.
    function StatsOn() {
      useState(() => {
        unstable_setFlumeStats(true);
        return null;
      });
      return null;
    }
    const log = vi.spyOn(console, 'log').mockImplementation(() => {});
    await mount(
      createElement(
        Fragment,
        null,
        createElement(Flume, { data: items, renderCell }),
        createElement(StatsOn),
      ),
    );
    expect(statsLines(log)).toHaveLength(0);
    unstable_setFlumeStats(false);
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

  it('setData carries the normalized sticky indices', async () => {
    const { data } = installHost();
    const { Flume } = await import('../Flume');
    const list = Array.from({ length: 10 }, (_, n) => ({ id: `k${n}` }));
    await mount(
      createElement(Flume, { data: list, renderCell, stickyHeaderIndices: [8, 0, 4, 4, -1] }),
    );
    expect(data.at(-1)?.sticky).toEqual([0, 4, 8]);
  });

  it('a sticky-only change calls setStickyHeaderIndices once', async () => {
    const { data, stickies } = installHost();
    const { Flume } = await import('../Flume');
    const list = [item, { id: 'b' }, { id: 'c' }];
    const tree = await mount(
      createElement(Flume, { data: list, renderCell, stickyHeaderIndices: [0] }),
    );
    await act(async () => {
      tree.update(createElement(Flume, { data: list, renderCell, stickyHeaderIndices: [2] }));
    });
    expect(data).toHaveLength(1);
    expect(stickies).toEqual([{ storeId: data[0]?.storeId, sticky: [2] }]);
    await act(async () => {
      tree.update(createElement(Flume, { data: list, renderCell, stickyHeaderIndices: [2] }));
    });
    expect(data).toHaveLength(1);
    expect(stickies).toHaveLength(1);
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

  it('drives refresh from a RefreshControl element', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Commands } = await import('../specs/FlumeViewNativeComponent');
    const fromElement = vi.fn();
    const fromProp = vi.fn();
    const control = (refreshing: boolean) =>
      createElement(RefreshControlStub, { refreshing, onRefresh: fromElement });
    const tree = await mount(
      createElement(Flume, {
        data: [item],
        renderCell,
        onRefresh: fromProp,
        refreshControl: control(true),
      }),
    );
    const native = () => tree.root.find((node) => String(node.type) === 'FlumeView');
    expect(native().props).toMatchObject({ refreshEnabled: true, refreshing: true });
    await act(async () => {
      tree.update(
        createElement(Flume, { data: [item], renderCell, refreshControl: control(false) }),
      );
    });
    expect(native().props.refreshing).toBe(false);
    await act(async () => {
      native().props.onRefresh();
    });
    expect(fromElement).toHaveBeenCalledTimes(1);
    expect(fromProp).not.toHaveBeenCalled();
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

describe('Flume component styles', () => {
  const Header = () => createElement('HeaderNode');
  const Footer = () => createElement('FooterNode');
  const Empty = () => createElement('EmptyNode');
  const entry = (slot: number, kind: number, key: string): Entry => ({
    slot,
    kind,
    key,
    index: -1,
    bound: true,
    minMain: 0,
  });
  const slotOf = (tree: ReactTestRenderer, kind: number) =>
    tree.root.findAll((node) => String(node.type) === 'FlumeSlotView' && node.props.kind === kind);

  it('wraps the header, footer and empty rows in a View with their style', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const style = { padding: 8 };
    const tree = await mount(
      createElement(Flume, {
        data: [],
        renderItem: () => null,
        ListHeaderComponent: Header,
        ListFooterComponent: Footer,
        ListEmptyComponent: Empty,
        ListHeaderComponentStyle: style,
        ListFooterComponentStyle: style,
        ListEmptyComponentStyle: style,
      }),
    );
    await sendSlots(tree, [entry(1, 1, 'header'), entry(2, 2, 'footer'), entry(3, 3, 'empty')]);
    for (const [kind, component] of [
      [1, Header],
      [2, Footer],
      [3, Empty],
    ] as const) {
      const slots = slotOf(tree, kind);
      expect(slots.length).toBeGreaterThan(0);
      for (const slot of slots) {
        const [wrapper] = slot.children as ReactTestInstance[];
        expect(String(wrapper?.type)).toBe('View');
        expect(wrapper?.props.style).toBe(style);
        expect(wrapper?.findAllByType(component)).toHaveLength(1);
      }
    }
  });

  it('adds no wrapper without a style', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, {
        data: [],
        renderItem: () => null,
        ListHeaderComponent: Header,
        ListEmptyComponent: Empty,
      }),
    );
    await sendSlots(tree, [entry(1, 1, 'header'), entry(3, 3, 'empty')]);
    for (const [kind, component] of [
      [1, Header],
      [3, Empty],
    ] as const) {
      const slots = slotOf(tree, kind);
      expect(slots.length).toBeGreaterThan(0);
      for (const slot of slots) {
        const [child] = slot.children as ReactTestInstance[];
        expect(child?.type).toBe(component);
      }
    }
  });
});

describe('Flume item separators', () => {
  const a = { id: 'a' };
  const b = { id: 'b' };
  const c = { id: 'c' };
  type SepProps = { leadingItem: unknown; trailingItem: unknown; highlighted: boolean };
  function separatorSpy() {
    const props: SepProps[] = [];
    const Sep = (p: SepProps) => {
      props.push(p);
      return createElement('Sep');
    };
    return { props, Sep };
  }
  function countingCell() {
    const calls = new Map<string, number>();
    const renderItem = (info: CellInfo<unknown>) => {
      const id = (info.item as { id: string }).id;
      calls.set(id, (calls.get(id) ?? 0) + 1);
      return createElement('Row', { id });
    };
    return { calls, renderItem };
  }
  const slotsOfKey = (tree: ReactTestRenderer, key: string, kind = 0) =>
    tree.root.findAll(
      (node) =>
        String(node.type) === 'FlumeSlotView' &&
        node.props.itemKey === key &&
        node.props.kind === kind,
    );
  const nameOf = (node: ReactTestInstance): string =>
    typeof node.type === 'string' ? node.type : (node.type as { name: string }).name;
  // The separator renders inside the slot's SeparatorCell, which holds its highlight state.
  const childTypes = (slot: ReactTestInstance) =>
    (slot.children as ReactTestInstance[]).map((child) => {
      const [inner] = child.children as ReactTestInstance[];
      return nameOf(child) === 'SeparatorCell' && inner !== undefined
        ? nameOf(inner)
        : nameOf(child);
    });
  function expectSeparated(tree: ReactTestRenderer, key: string, separated: boolean) {
    const slots = slotsOfKey(tree, key);
    expect(slots.length).toBeGreaterThan(0);
    for (const slot of slots)
      expect(childTypes(slot)).toEqual(separated ? ['Row', 'Sep'] : ['Row']);
  }

  it('renders the separator after every row but the last', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { props, Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, { data: [a, b, c], renderItem, ItemSeparatorComponent: Sep }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1), rowEntry(3, 'c', 2)]);
    expectSeparated(tree, 'a', true);
    expectSeparated(tree, 'b', true);
    expectSeparated(tree, 'c', false);
    expect(props).toContainEqual({ leadingItem: a, trailingItem: b, highlighted: false });
    expect(props).toContainEqual({ leadingItem: b, trailingItem: c, highlighted: false });
  });

  it('draws the React separator on a key that turns from a worklet row into a React row', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const typed = (kind: string) => [
      { ...a, kind },
      { ...b, kind: 'w' },
    ];
    const props = (kind: string) => ({
      data: typed(kind),
      getItemType: (item: unknown) => (item as { kind: string }).kind,
      renderCell,
      renderSeparatorCell: () => ({ type: 'view' as const }),
      reactCells: { r: renderItem },
      ItemSeparatorComponent: Sep,
    });
    const tree = await mount(createElement(Flume, props('w')));
    expect(slotsOfKey(tree, 'a')).toHaveLength(0);
    await act(async () => {
      tree.update(createElement(Flume, props('r')));
    });
    await sendSlots(tree, [rowEntry(1, 'a', 0)]);
    expectSeparated(tree, 'a', true);
  });

  it('re-renders only the old last row on an append', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { props, Sep } = separatorSpy();
    const { calls, renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, { data: [a, b], renderItem, ItemSeparatorComponent: Sep }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    calls.clear();
    props.length = 0;
    await act(async () => {
      tree.update(
        createElement(Flume, { data: [a, b, c], renderItem, ItemSeparatorComponent: Sep }),
      );
    });
    expect(calls.get('a')).toBeUndefined();
    expect(calls.get('b')).toBeUndefined();
    expect(props.length).toBeGreaterThan(0);
    expect(props.every((p) => p.leadingItem === b && p.trailingItem === c)).toBe(true);
    expectSeparated(tree, 'b', true);
  });

  it('re-renders the new last row when the last row is deleted', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { calls, renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, { data: [a, b, c], renderItem, ItemSeparatorComponent: Sep }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1), rowEntry(3, 'c', 2)]);
    calls.clear();
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, b], renderItem, ItemSeparatorComponent: Sep }));
    });
    expect(calls.get('a')).toBeUndefined();
    expect(calls.get('b')).toBeUndefined();
    expectSeparated(tree, 'b', false);
    // c left the data and still has its slot: it keeps what it showed.
    for (const slot of slotsOfKey(tree, 'c')) expect(childTypes(slot)).toEqual(['Row']);
  });

  it('keeps a leaving row separated as it last committed', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, { data: [a, b, c], renderItem, ItemSeparatorComponent: Sep }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1), rowEntry(3, 'c', 2)]);
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, c], renderItem, ItemSeparatorComponent: Sep }));
    });
    expectSeparated(tree, 'b', true);
  });

  it('keeps a row mounted when ItemSeparatorComponent comes and goes', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const mounts = new Map<string, number>();
    const Row = ({ id }: { id: string }) => {
      useEffect(() => {
        mounts.set(id, (mounts.get(id) ?? 0) + 1);
      }, [id]);
      return createElement('Row', { id });
    };
    const renderItem = (info: CellInfo<unknown>) =>
      createElement(Row, { id: (info.item as { id: string }).id });
    const tree = await mount(createElement(Flume, { data: [a, b], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    const mounted = mounts.get('a');
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, b], renderItem, ItemSeparatorComponent: Sep }));
    });
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, b], renderItem }));
    });
    expect(mounts.get('a')).toBe(mounted);
  });

  it('renders a leaving row with the index it last committed', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const x = { id: 'x' };
    const seen: { id: string; index: number }[] = [];
    const renderItem = (info: CellInfo<unknown>) => {
      const id = (info.item as { id: string }).id;
      seen.push({ id, index: info.index });
      return createElement('Row', { id });
    };
    const tree = await mount(createElement(Flume, { data: [a, b, c], renderItem }));
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1), rowEntry(3, 'c', 2)]);
    // Native still lists b at 1 while the data has moved it to 2.
    await act(async () => {
      tree.update(createElement(Flume, { data: [x, a, b, c], renderItem }));
    });
    seen.length = 0;
    await act(async () => {
      tree.update(createElement(Flume, { data: [x, a, c], renderItem }));
    });
    expect(seen.filter((call) => call.id === 'b').map((call) => call.index)).toEqual([]);
  });

  it('draws one separator per grid item and none in the last grid row', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const data = Array.from({ length: 10 }, (_, i) => ({ id: `i${i}` }));
    const element = (numColumns: number) =>
      createElement(Flume, { data, renderItem, numColumns, ItemSeparatorComponent: Sep });
    const tree = await mount(element(3));
    await sendSlots(
      tree,
      data.map((item, i) => rowEntry(i + 1, item.id, i)),
      100,
      0,
      300,
    );
    for (let i = 0; i < 10; i++) expectSeparated(tree, `i${i}`, i < 9);
    await act(async () => {
      tree.update(element(2));
    });
    for (let i = 0; i < 10; i++) expectSeparated(tree, `i${i}`, i < 8);
  });

  it('renders the separator before the row when inverted', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, {
        data: [a, b],
        renderItem,
        inverted: true,
        ItemSeparatorComponent: Sep,
      }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    for (const slot of slotsOfKey(tree, 'a')) expect(childTypes(slot)).toEqual(['Sep', 'Row']);
    for (const slot of slotsOfKey(tree, 'b')) expect(childTypes(slot)).toEqual(['Row']);
  });

  it('puts the row and its separator side by side when horizontal', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, {
        data: [a, b],
        renderItem,
        horizontal: true,
        ItemSeparatorComponent: Sep,
      }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    for (const [key, inside] of [
      ['a', ['Row', 'Sep']],
      ['b', ['Row']],
    ] as const) {
      for (const slot of slotsOfKey(tree, key)) {
        const [wrapper] = slot.children as ReactTestInstance[];
        expect(String(wrapper?.type)).toBe('View');
        expect(wrapper?.props.style).toEqual({ flexDirection: 'row', alignSelf: 'flex-start' });
        expect(childTypes(wrapper as ReactTestInstance)).toEqual(inside);
      }
    }
  });

  it('renders the separator on a sticky copy', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, { data: [a, b], renderItem, ItemSeparatorComponent: Sep }),
    );
    await sendSlots(tree, [{ ...rowEntry(7, 'a', 0), kind: 4 }]);
    const sticky = slotsOfKey(tree, 'a', 4);
    expect(sticky).toHaveLength(1);
    expect(childTypes(sticky[0] as ReactTestInstance)).toEqual(['Row', 'Sep']);
  });

  it('draws no separator next to the header and footer', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep } = separatorSpy();
    const { renderItem } = countingCell();
    const Header = () => createElement('HeaderNode');
    const tree = await mount(
      createElement(Flume, {
        data: [a],
        renderItem,
        ItemSeparatorComponent: Sep,
        ListHeaderComponent: Header,
        ListFooterComponent: Header,
      }),
    );
    await sendSlots(tree, [
      { ...rowEntry(1, 'header', -1), kind: 1 },
      rowEntry(2, 'a', 0),
      { ...rowEntry(3, 'footer', -1), kind: 2 },
    ]);
    expect(tree.root.findAllByType(Sep)).toHaveLength(0);
  });

  it('reads fullSpan from the data index of the key', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { renderItem } = countingCell();
    const tree = await mount(
      createElement(Flume, {
        data: [a, b, c],
        renderItem,
        numColumns: 2,
        stickyHeaderIndices: [1],
      }),
    );
    // Native's entry for b still carries index 0 from before a data change.
    await sendSlots(tree, [rowEntry(1, 'a', 1), rowEntry(2, 'b', 0)], 150, 0, 300);
    for (const slot of slotsOfKey(tree, 'b'))
      expect(slot.props.style).toEqual({ position: 'absolute', left: 0, right: 0 });
    for (const slot of slotsOfKey(tree, 'a'))
      expect(slot.props.style).toEqual({ position: 'absolute', width: 150 });
  });
});

describe('Flume separators in the info', () => {
  const rows = ['a', 'b', 'c', 'd', 'e'].map((id) => ({ id }));
  type SepProps = { leadingItem: { id: string }; highlighted: boolean; color?: string };
  function setup() {
    const seen: SepProps[] = [];
    const Sep = (p: SepProps) => {
      seen.push(p);
      return createElement('Sep');
    };
    const infos = new Map<string, CellInfo<{ id: string }>[]>();
    const renderItem = (info: CellInfo<{ id: string }>) => {
      infos.set(info.item.id, [...(infos.get(info.item.id) ?? []), info]);
      return createElement('Row');
    };
    const latest = (id: string) => seen.filter((p) => p.leadingItem.id === id).at(-1);
    const separatorsOf = (id: string) => infos.get(id)?.at(-1)?.separators;
    return { Sep, renderItem, infos, latest, separatorsOf };
  }
  async function mountRows(Sep: unknown, renderItem: unknown, data = rows, extraData?: unknown) {
    const { Flume } = await import('../Flume');
    const element = (list: typeof rows, extra?: unknown) =>
      createElement(Flume, {
        data: list,
        renderItem: renderItem as (info: CellInfo<unknown>) => ReturnType<typeof createElement>,
        ItemSeparatorComponent: Sep as unknown as () => ReturnType<typeof createElement>,
        extraData: extra,
      });
    const tree = await mount(element(data, extraData));
    await sendSlots(
      tree,
      data.map((row, i) => rowEntry(i + 1, row.id, i)),
    );
    return { tree, element };
  }

  it('highlights the separators of the row and the row before it', async () => {
    installHost();
    const { Sep, renderItem, latest, separatorsOf } = setup();
    await mountRows(Sep, renderItem);
    await act(async () => {
      separatorsOf('d')?.highlight();
    });
    expect(latest('c')?.highlighted).toBe(true);
    expect(latest('d')?.highlighted).toBe(true);
    for (const id of ['a', 'b']) expect(latest(id)?.highlighted).toBe(false);
    await act(async () => {
      separatorsOf('d')?.unhighlight();
    });
    expect(latest('c')?.highlighted).toBe(false);
    expect(latest('d')?.highlighted).toBe(false);
  });

  it('merges updateProps into the leading or trailing separator', async () => {
    installHost();
    const { Sep, renderItem, latest, separatorsOf } = setup();
    await mountRows(Sep, renderItem);
    await act(async () => {
      separatorsOf('c')?.updateProps('leading', { color: 'red' });
      separatorsOf('c')?.updateProps('trailing', { color: 'blue' });
    });
    expect(latest('b')?.color).toBe('red');
    expect(latest('c')?.color).toBe('blue');
    expect(latest('a')?.color).toBeUndefined();
  });

  it('routes a separator call by the rows on screen while a newer render suspends', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const { Sep, renderItem, latest, separatorsOf } = setup();
    const never = new Promise<void>(() => {});
    // Suspends the transition from outside the list, so React throws away the list's render too.
    const Gate = ({ open }: { open: boolean }) => {
      if (!open) throw never;
      return null;
    };
    const element = (list: { id: string }[], open: boolean) =>
      createElement(
        Suspense,
        { fallback: null },
        createElement(Flume, {
          data: list,
          renderItem: renderItem as (info: CellInfo<unknown>) => ReturnType<typeof createElement>,
          ItemSeparatorComponent: Sep as unknown as () => ReturnType<typeof createElement>,
        }),
        createElement(Gate, { open }),
      );
    const shown = rows.slice(0, 3);
    let tree: ReactTestRenderer | undefined;
    await act(async () => {
      tree = create(element(shown, true), {
        createNodeMock: () => ({}),
        unstable_isConcurrent: true,
      } as Parameters<typeof create>[1]);
    });
    await sendSlots(
      tree as ReactTestRenderer,
      shown.map((row, i) => rowEntry(i + 1, row.id, i)),
    );
    // React keeps a, b, c on screen and throws the render of a, c, b away.
    await act(async () => {
      startTransition(() => {
        tree?.update(element([{ id: 'a' }, { id: 'c' }, { id: 'b' }], false));
      });
    });
    await act(async () => {
      separatorsOf('c')?.highlight();
    });
    expect(latest('b')?.highlighted).toBe(true);
    expect(latest('a')?.highlighted).toBe(false);
  });
  it('keeps one separators object per row across renders', async () => {
    installHost();
    const { Sep, renderItem, infos } = setup();
    const { tree, element } = await mountRows(Sep, renderItem);
    // Native registered every pre-rendered slot, so each key has one slot from here on.
    await sendSlots(
      tree,
      rows.map((row, i) => rowEntry(i + 1, row.id, i)),
      -1,
      -100,
    );
    infos.clear();
    for (const extra of [1, 2]) {
      await act(async () => {
        tree.update(element(rows, extra));
      });
    }
    const seen = infos.get('b') ?? [];
    expect(seen.length).toBe(2);
    expect(seen[0]?.separators).toBeDefined();
    expect(new Set(seen.map((info) => info.separators)).size).toBe(1);
  });

  it('leaves the new row before unhighlighted when the highlighted row before is deleted', async () => {
    installHost();
    const { Sep, renderItem, latest, separatorsOf } = setup();
    const { tree, element } = await mountRows(Sep, renderItem);
    await act(async () => {
      separatorsOf('d')?.highlight();
    });
    await act(async () => {
      tree.update(element(rows.filter((row) => row.id !== 'c')));
    });
    expect(latest('b')?.highlighted).toBe(false);
    await act(async () => {
      separatorsOf('d')?.unhighlight();
    });
    expect(latest('b')?.highlighted).toBe(false);
    expect(latest('d')?.highlighted).toBe(false);
  });
});

describe('Flume ListItemComponent and CellRendererComponent', () => {
  const a = { id: 'a' };
  const b = { id: 'b' };
  const Sep = () => createElement('Sep');
  const keyed = (tree: ReactTestRenderer, key: string) =>
    tree.root.findAll(
      (node) => String(node.type) === 'FlumeSlotView' && node.props.itemKey === key,
    );

  it('renders ListItemComponent with the info in place of renderItem', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const got: CellInfo<unknown>[] = [];
    const Item = (info: CellInfo<unknown>) => {
      got.push(info);
      return createElement('ItemNode');
    };
    const renderItem = vi.fn(() => createElement('Row'));
    const tree = await mount(
      createElement(Flume, { data: [a, b], renderItem, ListItemComponent: Item }),
    );
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    expect(renderItem).not.toHaveBeenCalled();
    expect(
      got.some((info) => info.item === a && info.index === 0 && info.separators !== undefined),
    ).toBe(true);
    expect(tree.root.findAll((node) => String(node.type) === 'ItemNode').length).toBeGreaterThan(0);
    expect(warn.mock.calls.filter(([m]) => String(m).includes('ListItemComponent'))).toHaveLength(
      1,
    );
    warn.mockRestore();
  });

  it('keeps every slot memoized while ListItemComponent stays the same', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    let renders = 0;
    const Item = () => {
      renders++;
      return createElement('ItemNode');
    };
    const data = [a, b];
    const tree = await mount(createElement(Flume, { data, ListItemComponent: Item }));
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    const before = renders;
    expect(before).toBeGreaterThan(0);
    await act(async () => {
      tree.update(createElement(Flume, { data, ListItemComponent: Item }));
    });
    expect(renders).toBe(before);
  });

  it('wraps the row and its separator in CellRendererComponent', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const got: Record<string, unknown>[] = [];
    const Cell = (props: {
      cellKey: string;
      index: number;
      item: unknown;
      style?: unknown;
      children?: ReactNode;
    }) => {
      got.push({
        cellKey: props.cellKey,
        index: props.index,
        item: props.item,
        style: props.style,
      });
      return createElement('CellNode', null, props.children);
    };
    const element = (horizontal: boolean) =>
      createElement(Flume, {
        data: [a, b],
        renderItem: () => createElement('Row'),
        ItemSeparatorComponent: Sep,
        CellRendererComponent: Cell,
        horizontal,
      });
    const tree = await mount(element(false));
    await sendSlots(tree, [rowEntry(1, 'a', 0), rowEntry(2, 'b', 1)]);
    expect(got).toContainEqual({ cellKey: 'a', index: 0, item: a, style: undefined });
    for (const slot of keyed(tree, 'a')) {
      const [cell] = slot.children as ReactTestInstance[];
      expect(String(cell?.type)).not.toBe('View');
      const node = cell?.find((n) => String(n.type) === 'CellNode');
      expect(node?.findAll((n) => String(n.type) === 'Row')).toHaveLength(1);
      expect(node?.findAllByType(Sep)).toHaveLength(1);
    }
    got.length = 0;
    await act(async () => {
      tree.update(element(true));
    });
    expect(got).toContainEqual({
      cellKey: 'a',
      index: 0,
      item: a,
      style: { flexDirection: 'row' },
    });
  });

  it('warns once when CellRendererComponent is given with worklet rows', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const Cell = (props: { children?: ReactNode }) =>
      createElement('CellNode', null, props.children);
    const tree = await mount(
      createElement(Flume, { data: [a], renderCell, CellRendererComponent: Cell }),
    );
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, b], renderCell, CellRendererComponent: Cell }));
    });
    expect(
      warn.mock.calls.filter(([m]) => String(m).includes('CellRendererComponent')),
    ).toHaveLength(1);
    warn.mockRestore();
  });
});

describe('Flume worklet separators', () => {
  beforeEach(() => {
    vi.resetModules();
  });
  const a = { id: 'a' };
  const b = { id: 'b' };
  const renderSeparatorCell = () => ({ type: 'view' as const, style: { height: 1 } });

  it('sends renderSeparatorCell before the first data, and null clears it', async () => {
    const host = installHost();
    const { Flume } = await import('../Flume');
    const tree = await mount(
      createElement(Flume, { data: [a, b], renderCell, renderSeparatorCell }),
    );
    const separator = host.log.indexOf('separator set');
    expect(separator).toBeGreaterThanOrEqual(0);
    expect(separator).toBeLessThan(host.log.findIndex((line) => line.startsWith('data')));
    await act(async () => {
      tree.update(createElement(Flume, { data: [a, b], renderCell, renderSeparatorCell: null }));
    });
    expect(host.log).toContain('separator null');
  });

  it('warns once when ItemSeparatorComponent has only worklet rows to draw on', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const Divider = () => createElement('Divider');
    const tree = await mount(
      createElement(Flume, { data: [a], renderCell, ItemSeparatorComponent: Divider }),
    );
    await act(async () => {
      tree.update(
        createElement(Flume, { data: [a, b], renderCell, ItemSeparatorComponent: Divider }),
      );
    });
    expect(warn.mock.calls.filter(([m]) => String(m).includes('renderSeparatorCell'))).toHaveLength(
      1,
    );
    warn.mockRestore();
  });

  it('does not warn when renderSeparatorCell draws the worklet rows', async () => {
    installHost();
    const { Flume } = await import('../Flume');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const Divider = () => createElement('Divider');
    await mount(
      createElement(Flume, {
        data: [a, b],
        renderCell,
        renderSeparatorCell,
        ItemSeparatorComponent: Divider,
      }),
    );
    expect(warn.mock.calls.filter(([m]) => String(m).includes('renderSeparatorCell'))).toHaveLength(
      0,
    );
    warn.mockRestore();
  });
});
