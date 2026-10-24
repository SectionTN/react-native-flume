import { execFileSync } from 'node:child_process';
import { foregroundPackage } from './parse.mts';
import type { DeviceInfo } from './report.mts';

export const PACKAGE = 'me.sectiontn.flume.example';

export type Adb = ReturnType<typeof connect>;

export function connect(serial: string) {
  const run = (args: string[], timeout = 60_000) =>
    execFileSync('adb', ['-s', serial, ...args], {
      encoding: 'utf8',
      timeout,
      maxBuffer: 64 * 1024 * 1024,
    });
  const shell = (command: string) => run(['shell', command]);
  const prop = (name: string) => shell(`getprop ${name}`).trim();
  const firstNumber = (text: string, pattern: RegExp) => {
    const value = pattern.exec(text)?.[1];
    return value === undefined ? null : Number(value);
  };

  return {
    serial,
    emulator: serial.startsWith('emulator-'),
    run,
    shell,
    start: (url: string) =>
      shell(`am start -W -a android.intent.action.VIEW -d '${url}' ${PACKAGE}`),
    forceStop: () => shell(`am force-stop ${PACKAGE}`),
    swipe: (x1: number, y1: number, x2: number, y2: number, ms: number) =>
      shell(
        `input swipe ${Math.round(x1)} ${Math.round(y1)} ${Math.round(x2)} ${Math.round(y2)} ${ms}`,
      ),
    gfxinfoReset: () => shell(`dumpsys gfxinfo ${PACKAGE} reset`),
    gfxinfo: () => shell(`dumpsys gfxinfo ${PACKAGE}`),
    meminfo: () => shell(`dumpsys meminfo ${PACKAGE}`),
    logcatClear: () => run(['logcat', '-c']),
    logcat: (filter: string[]) => run(['logcat', '-d', '-s', ...filter]),
    temperature: () => {
      const tenths = firstNumber(shell('dumpsys battery'), /^\s*temperature: (\d+)/m);
      return tenths === null ? null : tenths / 10;
    },
    deviceInfo: (): DeviceInfo => {
      const size = shell('wm size');
      const dims = (
        /Override size: (\d+)x(\d+)/.exec(size) ?? /Physical size: (\d+)x(\d+)/.exec(size)
      )?.slice(1, 3);
      const density = shell('wm density');
      const battery = shell('dumpsys battery');
      return {
        serial,
        model: prop('ro.product.model'),
        android: prop('ro.build.version.release'),
        size: [Number(dims?.[0] ?? 0), Number(dims?.[1] ?? 0)],
        density:
          firstNumber(density, /Override density: (\d+)/) ??
          firstNumber(density, /Physical density: (\d+)/) ??
          0,
        refreshRate: firstNumber(shell('dumpsys display'), /renderFrameRate (\d+(?:\.\d+)?)/),
        battery: firstNumber(battery, /^\s*level: (\d+)/m),
        ac: /^\s*AC powered: true/m.test(battery),
      };
    },
    // Never swipe into another app: the owner's phone may be picked up or a notification opened mid-run.
    assertForeground: () => {
      const resumed = foregroundPackage(
        shell('dumpsys activity activities | grep topResumedActivity'),
      );
      if (resumed !== PACKAGE) {
        throw new Error(`stopped: ${resumed ?? 'no app'} is in front, not ${PACKAGE}`);
      }
    },
  };
}
