import NativeFlume from './specs/NativeFlume';

let enabled = false;

export function statsEnabled(): boolean {
  return enabled;
}

export function logSetData(keys: number, items: number, diffMs: number, hostMs: number): void {
  console.log(
    `FlumeStats setData keys=${keys} items=${items} diff=${diffMs.toFixed(2)} host=${hostMs.toFixed(2)}`,
  );
}

// Benchmark switch, off by default: native bind stage times and a log line per host setData; enabling starts an empty window.
export function unstable_setFlumeStats(on: boolean): void {
  enabled = on;
  NativeFlume.setStats(on);
}

// Logs the native bind summary now (tag FlumeStats).
export function unstable_logFlumeStats(): void {
  NativeFlume.logStats();
}
