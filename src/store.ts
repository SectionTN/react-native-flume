import {
  createWorkletRuntime,
  getUISchedulerHolder,
  runOnRuntimeSync,
  type SerializableRef,
} from 'react-native-worklets';
import NativeFlume from './specs/NativeFlume';
import type { CellNode } from './types';

// The host object the native side installs as globalThis.__flume on the React runtime.
export type FlumeHost = {
  install(): void;
  setScheduler(holder: object): void;
  createStore(): number;
  releaseStore(storeId: number): void;
  setData(
    storeId: number,
    keys: string[],
    typeTable: string[],
    typeIds: Int32Array,
    changedIndices: number[],
    changedItems: unknown[],
    stableKeys: boolean,
    // Native applies the change once it has registered the pre-rendered cells of the same commit.
    deferred: boolean,
  ): void;
  setCell(
    storeId: number,
    renderCell: SerializableRef<(item: never, index: number) => CellNode>,
  ): void;
  setCellKinds(storeId: number, reactByDefault: boolean, reactTypes: string[]): void;
  setReporter(reporter: (level: number, message: string) => void): void;
};

export const isDev =
  typeof __DEV__ === 'undefined' ? process.env.NODE_ENV !== 'production' : __DEV__;

const state: { host?: FlumeHost; runtime?: ReturnType<typeof createWorkletRuntime> } = {};

export function getHost(): FlumeHost {
  if (state.host !== undefined) return state.host;
  // The read requires the module under inline requires, and the require installs `__flume`.
  void NativeFlume;
  const host = (globalThis as { __flume?: FlumeHost }).__flume;
  if (host === undefined) {
    throw new Error('react-native-flume: the native host is not installed');
  }
  host.setScheduler(getUISchedulerHolder());
  // Native warnings and renderCell errors surface in LogBox during development only.
  if (isDev) {
    host.setReporter((level, message) => {
      (level >= 2 ? console.error : console.warn)(`react-native-flume: ${message}`);
    });
  }
  // The worklet captures the host object by reference, so install() runs on the flume runtime under its lock.
  state.runtime = createWorkletRuntime({ name: 'flume', enableEventLoop: false });
  runOnRuntimeSync(state.runtime, () => {
    'worklet';
    host.install();
  });
  // Each property read on the host object builds a new native function, so every method is read once.
  state.host = {
    install: host.install,
    setScheduler: host.setScheduler,
    createStore: host.createStore,
    releaseStore: host.releaseStore,
    setData: host.setData,
    setCell: host.setCell,
    setCellKinds: host.setCellKinds,
    setReporter: host.setReporter,
  };
  return state.host;
}
