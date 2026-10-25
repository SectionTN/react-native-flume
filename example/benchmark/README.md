# Benchmark

The benchmark flings through a chat and a feed built with five lists on an Android phone. It reports dropped frames, blank area, memory and Flume's own bind and `setData` times.

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

## Metrics

| Metric | Source |
| --- | --- |
| Janky frames, p90 frame time, deadlines missed | `dumpsys gfxinfo` over the measured flings. |
| Blank area | Every row paints an opaque background over a magenta list background. Each recorded frame counts the magenta pixels inside the list. A frame over 1% counts as blank. |
| Memory | Total proportional set size (PSS) from `dumpsys meminfo` after the measured flings. |
| Bind p50 and p95 | Flume's stats switch, `unstable_setFlumeStats`, for worklet cells. |
| `setData` | Flume's time for the diff and the native call, with 10,000 React keys and with 500 changed worklet items. |

The summary shows the median of each value over the repeats.

## Run it

Requirements:

- An Android phone over adb, unlocked and charging with Stay awake on
- ffmpeg and ffprobe on the `PATH`
- Node 26

Install the example's release build, then run the flings and the `setData` rounds:

```sh
yarn example android:release --device <serial>
yarn bench --serial <serial>
yarn bench --serial <serial> --setdata
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

The runner stops a run when:

- Another app comes to the front
- The list doesn't report its position within 60 seconds
- A pass draws fewer than 10 frames per fling

The emulator runs the benchmark too, but its numbers only catch regressions.
