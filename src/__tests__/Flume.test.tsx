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
  };
  (globalThis as { __flume?: FlumeHost }).__flume = host;
  return { alive, data, cells };
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
