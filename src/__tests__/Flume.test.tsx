import { createElement, StrictMode } from 'react';
import { act, create, type ReactTestRenderer } from 'react-test-renderer';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { FlumeHost } from '../store';

(globalThis as { IS_REACT_ACT_ENVIRONMENT?: boolean }).IS_REACT_ACT_ENVIRONMENT = true;

vi.mock('react-native-worklets', () => ({
  createSerializable: (value: unknown) => ({ serializable: value }),
  createWorkletRuntime: () => ({}),
  getUISchedulerHolder: () => ({}),
  runOnRuntimeSync: (_runtime: unknown, worklet: () => void) => worklet(),
}));
vi.mock('../specs/NativeFlume', () => ({ default: {} }));
vi.mock('../specs/FlumeViewNativeComponent', () => ({ default: 'FlumeView' }));

type DataCall = { storeId: number; keys: string[]; changedIndices: number[]; alive: boolean };
type CellCall = { storeId: number; alive: boolean };

// Records what reaches the native host and whether the store was still alive when it did.
function installHost() {
  const alive = new Set<number>();
  const data: DataCall[] = [];
  const cells: CellCall[] = [];
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
    setData: (storeId, keys, _types, changedIndices) => {
      data.push({ storeId, keys, changedIndices, alive: alive.has(storeId) });
    },
    setCell: (storeId) => {
      cells.push({ storeId, alive: alive.has(storeId) });
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
    report: (level: number, message: string) => reporter?.(level, message),
    hasReporter: () => reporter !== undefined,
  };
}

const item = { id: 'a' };
const renderCell = () => ({ type: 'view' as const });

async function mount(element: Parameters<typeof create>[0]): Promise<ReactTestRenderer> {
  let tree: ReactTestRenderer | undefined;
  await act(async () => {
    tree = create(element);
  });
  return tree as ReactTestRenderer;
}

function renderedStoreId(tree: ReactTestRenderer): number {
  return tree.root.find((node) => String(node.type) === 'FlumeView').props.storeId as number;
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
    expect(host.data.at(-1)).toEqual({ storeId, keys: ['a'], changedIndices: [0], alive: true });
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
    native.props.onPress({ nativeEvent: { index: 0, key: 'a', id: '' } });
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
});
