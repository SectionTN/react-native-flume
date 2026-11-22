# Benchmark

The benchmark flings through a chat and a feed built with five lists on an Android phone, then jumps far into them with `scrollToIndex`. It reports dropped frames, blank area, memory and Flume's own bind and `setData` times.

## Lists

All five render the same rows from the same data:

| Name | List | Cells |
| --- | --- | --- |
| `flume` | Flume | Worklet cells through `renderCell`. |
| `flume-react` | Flume | React cells through `renderItem`. |
| `flashlist` | FlashList 2.3.3 | React cells through `renderItem`. |
| `legendlist` | LegendList 3.6.0 | React cells through `renderItem`, with `recycleItems`. |
| `native` | RecyclerView | Rows built in Kotlin with the chat cell's values. Chat only. |

## Scenarios

| Scenario | Data |
| --- | --- |
| `chat` | 1,000 messages that open at the newest one. Every fifth message is a voice note. |
| `feed` | 1,000 posts with one to five lines of text. Every other post has a 4:3 photo. |

Each scenario runs with the JS thread idle and under load. Load is a timer that blocks the JS thread for 80 ms of every 100 ms.

## Method

A configuration runs on a freshly started release build:

1. The runner opens the Bench tab with a deep link and waits for the list to report its position on screen.
2. The list warms up with two flings each way.
3. The runner measures 12 flings forward and 12 back, 700 ms apart. A fling is a 200 ms swipe over 55% of the list's height.
4. A second pass records the screen during the same flings to measure the blank area.

Before the first configuration, 40 flings each way through each scenario fill the image cache that all the lists share. The repeats run every configuration once, in turn.

A jump pass records the screen while `scrollToIndex` sends the list to 50, 10, 90, 30 and 70 percent of its items, 800 ms apart. Each configuration runs one pass of instant jumps and one of animated jumps. The RecyclerView runs the instant pass only.

## Metrics

| Metric | Source |
| --- | --- |
| Janky frames, p90 frame time, deadlines missed | `dumpsys gfxinfo` over the measured flings. |
| Janky frames and p99 frame time in a jump pass | `dumpsys gfxinfo` over the five jumps. |
| Blank area | Every row paints an opaque background over a magenta list background. Each recorded frame counts the magenta pixels inside the list. A frame over 1% counts as blank. |
| Blank frames | The recorded frames of a jump pass that count as blank. |
| Memory | Total proportional set size (PSS) from `dumpsys meminfo` after the measured flings. |
| Bind p50 and p95 | Flume's stats switch, `unstable_setFlumeStats`, for worklet cells. |
| `setData` | Flume's time for the diff and the native call, with 10,000 React keys and with 500 changed worklet items. |

The summary shows the median of each value over the repeats.

## Results

Device: Redmi Note 15 Pro+ 5G (2510ERA8BG), Qualcomm SM7635, Android 16, 1280x2772 at 520 dpi and 120 Hz, on AC power.

The results come from three repeats on a release build. The flings ran on 2026-10-25, and the `setData` rounds and the jumps ran on 2026-10-26. The raw runs are in [`results/2026-10-26-redmi.json`](results/2026-10-26-redmi.json).

Limits:

- One phone
- Network images served from a cache that the priming flings fill
- Emulator numbers catch regressions only

### Flings

Chat:

| impl | load | janky % | p90 frame ms | deadlines missed | blank mean % | blank max % | PSS MB | bind p50/p95 ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| flume | off | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 166.27 | 0.78/1.40 |
| flume | on | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 165.91 | 0.69/1.20 |
| flume-react | off | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 216.47 | n/a |
| flume-react | on | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 211.62 | n/a |
| flashlist | off | 0.00 | 12.00 | 0.00 | 0.00 | 0.00 | 169.50 | n/a |
| flashlist | on | 0.00 | 12.00 | 0.00 | 0.00 | 0.00 | 188.35 | n/a |
| legendlist | off | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 170.11 | n/a |
| legendlist | on | 0.00 | 12.00 | 0.00 | 0.00 | 0.00 | 191.93 | n/a |
| native | off | 0.00 | 10.00 | 0.00 | 0.00 | 0.00 | 170.26 | n/a |
| native | on | 0.00 | 10.00 | 0.00 | 0.00 | 0.00 | 170.54 | n/a |

Feed:

| impl | load | janky % | p90 frame ms | deadlines missed | blank mean % | blank max % | PSS MB | bind p50/p95 ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| flume | off | 0.00 | 10.00 | 0.00 | 0.00 | 0.00 | 182.38 | 1.08/2.46 |
| flume | on | 0.00 | 10.00 | 0.00 | 0.00 | 0.00 | 181.85 | 1.11/2.19 |
| flume-react | off | 0.00 | 11.00 | 0.00 | 0.00 | 0.00 | 203.01 | n/a |
| flume-react | on | 0.00 | 10.00 | 0.00 | 0.00 | 0.00 | 204.47 | n/a |
| flashlist | off | 0.00 | 11.00 | 0.00 | 0.00 | 0.00 | 199.22 | n/a |
| flashlist | on | 0.00 | 13.00 | 0.00 | 0.00 | 0.00 | 197.72 | n/a |
| legendlist | off | 0.00 | 12.00 | 0.00 | 0.00 | 0.00 | 200.47 | n/a |
| legendlist | on | 0.00 | 11.00 | 0.00 | 0.00 | 0.00 | 197.26 | n/a |

### `setData`

| call | keys | items | diff ms | host ms |
| --- | --- | --- | --- | --- |
| react 10,000 keys, mount | 10000 | 0 | 5.71 | 1.20 |
| react 10,000 keys, replace | 10000 | 0 | 8.04 | 1.22 |
| worklet 1,000 items, mount | 1000 | 1000 | 0.95 | 2.60 |
| worklet 500 changed items | 1000 | 500 | 0.73 | 1.80 |

### Jumps

Chat:

| impl | load | scroll | blank frames | blank max % | blank mean % | janky frames | p99 frame ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| flume | off | instant | 0.00 | 0.00 | 0.00 | 5.00 | 53.00 |
| flume | off | animated | 0.00 | 0.00 | 0.00 | 5.00 | 46.00 |
| flume | on | instant | 0.00 | 0.00 | 0.00 | 5.00 | 44.00 |
| flume | on | animated | 0.00 | 0.00 | 0.00 | 5.00 | 48.00 |
| flume-react | off | instant | 0.00 | 0.00 | 0.00 | 10.00 | 65.00 |
| flume-react | off | animated | 0.00 | 0.00 | 0.00 | 5.00 | 73.00 |
| flume-react | on | instant | 0.00 | 0.00 | 0.00 | 10.00 | 57.00 |
| flume-react | on | animated | 0.00 | 0.00 | 0.00 | 6.00 | 65.00 |
| flashlist | off | instant | 0.00 | 0.00 | 0.00 | 9.00 | 85.00 |
| flashlist | off | animated | 0.00 | 0.00 | 0.00 | 10.00 | 48.00 |
| flashlist | on | instant | 0.00 | 0.00 | 0.00 | 10.00 | 69.00 |
| flashlist | on | animated | 37.00 | 48.00 | 2.30 | 8.00 | 46.00 |
| legendlist | off | instant | 7.00 | 100.00 | 3.30 | 9.00 | 32.00 |
| legendlist | off | animated | 81.00 | 100.00 | 25.60 | 12.00 | 53.00 |
| legendlist | on | instant | 7.00 | 100.00 | 2.50 | 9.00 | 40.00 |
| legendlist | on | animated | 109.00 | 100.00 | 31.50 | 8.00 | 48.00 |
| native | off | instant | 0.00 | 0.00 | 0.00 | 5.00 | 23.00 |
| native | on | instant | 0.00 | 0.00 | 0.00 | 4.00 | 26.00 |

Feed:

| impl | load | scroll | blank frames | blank max % | blank mean % | janky frames | p99 frame ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| flume | off | instant | 0.00 | 0.00 | 0.00 | 5.00 | 34.00 |
| flume | off | animated | 0.00 | 0.00 | 0.00 | 5.00 | 29.00 |
| flume | on | instant | 0.00 | 0.00 | 0.00 | 5.00 | 40.00 |
| flume | on | animated | 0.00 | 0.00 | 0.00 | 5.00 | 27.00 |
| flume-react | off | instant | 0.00 | 0.00 | 0.00 | 5.00 | 57.00 |
| flume-react | off | animated | 0.00 | 0.00 | 0.00 | 5.00 | 65.00 |
| flume-react | on | instant | 0.00 | 0.00 | 0.00 | 5.00 | 61.00 |
| flume-react | on | animated | 0.00 | 0.00 | 0.00 | 5.00 | 61.00 |
| flashlist | off | instant | 0.00 | 0.00 | 0.00 | 10.00 | 44.00 |
| flashlist | off | animated | 15.00 | 100.00 | 4.40 | 15.00 | 750.00 |
| flashlist | on | instant | 0.00 | 0.00 | 0.00 | 10.00 | 46.00 |
| flashlist | on | animated | 42.00 | 100.00 | 11.90 | 15.00 | 1350.00 |
| legendlist | off | instant | 2.00 | 100.00 | 0.70 | 5.00 | 28.00 |
| legendlist | off | animated | 75.00 | 100.00 | 19.50 | 8.00 | 48.00 |
| legendlist | on | instant | 2.00 | 100.00 | 0.70 | 5.00 | 30.00 |
| legendlist | on | animated | 108.00 | 100.00 | 26.80 | 5.00 | 38.00 |

### Item separators

With `--separators`, each list draws a 1 dp line between rows: `ItemSeparatorComponent` on the React lists and a worklet through `renderSeparatorCell` on Flume's worklet cells. These numbers come from a Pixel 9a emulator and an iPhone 17e simulator on 2026-11-22, three repeats on a release build, so they show what a separator costs and not how fast a phone is. On iOS, a script drives the same flings and reads the stats from the simulator's log.

Bind p50 and p95 in ms for worklet cells, with the JS thread idle:

| scenario | device | no separator | separator worklet |
| --- | --- | --- | --- |
| chat | Pixel 9a emulator | 0.32/0.95 | 0.34/0.98 |
| feed | Pixel 9a emulator | 0.46/1.01 | 0.49/1.34 |
| chat | iPhone 17e simulator | 0.64/1.50 | 0.65/1.51 |
| feed | iPhone 17e simulator | 1.33/2.63 | 1.37/2.52 |

`setData` with 10,000 React keys on the Pixel 9a emulator. An iOS release build doesn't log these times.

| call | separator | diff ms | host ms |
| --- | --- | --- | --- |
| react 10,000 keys, mount | none | 2.59 | 1.16 |
| react 10,000 keys, mount | `ItemSeparatorComponent` | 3.29 | 1.46 |
| react 10,000 keys, replace | none | 3.21 | 0.91 |
| react 10,000 keys, replace | `ItemSeparatorComponent` | 3.80 | 0.93 |

## Run it

Requirements:

- An Android phone over adb, unlocked and charging with Stay awake on
- ffmpeg and ffprobe on the `PATH`
- Node 26

Install the example's release build, then run the flings, the `setData` rounds and the jumps:

```sh
yarn example android:release --device <serial>
yarn bench --serial <serial>
yarn bench --serial <serial> --setdata
yarn bench --serial <serial> --jump
```

A run writes `results.json` and `summary.md` to `example/benchmark/out/`.

| Option | Default | Description |
| --- | --- | --- |
| `--serial` | | The adb serial of the phone. Required. |
| `--scenarios` | `chat,feed` | The scenarios to run. |
| `--impls` | All | The implementations to run. |
| `--load` | `0,1` | Runs without load, with load, or both. |
| `--repeat` | `3` | How many times each configuration runs. |
| `--count` | `1000` | The number of items in each list. |
| `--no-record` | | Skips the recorded pass and the blank area. |
| `--setdata` | | Runs the `setData` rounds instead of the flings. |
| `--jump` | | Runs the jump passes instead of the flings. |
| `--separators` | | Draws each list's item separator, through `sep=1` in the Bench link. |

The runner stops a run when:

- Another app comes to the front
- The list doesn't report its position within 60 seconds
- A fling pass draws fewer than 10 frames per fling
