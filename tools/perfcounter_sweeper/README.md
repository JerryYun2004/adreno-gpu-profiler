# Adreno A8xx Perfcounter Sweeper

This folder contains a **separate phone-side program** built on top of the current `adreno_perf_stream` idea. It does **not** overwrite the existing streamer. The new binary is called:

```bash
streamer_sweeper
```

It automatically sweeps selected Adreno A8xx KGSL performance counters in chunks. For each chunk, it:

1. Reserves a group-specific number of counters.
2. Starts the benchmark automatically.
3. Optionally repeats the same benchmark as multiple CPU-separated bursts using `--bursts` and `--burst-sleep`.
4. Optionally runs an internal softmax width sequence using `--widths` and `--width-sleep`, so different widths appear as separated regions in the same CSV trace.
5. Streams counter deltas for a fixed duration.
6. Saves one CSV file.
7. Releases the counters.
8. Moves to the next chunk until all counters of interest are swept.
9. Prints `done` when the full sweep completes.

## Why this exists

The XML enum contains many possible counters, but the GPU/KGSL driver only exposes a limited number of **physical counter slots** per group at one time. For example, based on the current manual testing:

| Group | Generated counters | Physical slots used by sweeper | Included in sweep? | Notes |
|---|---:|---:|---|---|
| CP | 91 | 14 | Yes | Command Processor |
| RBBM | 17 | 4 | Yes | Register bus / top-level block |
| PC | 87 | 8 | Yes | Primitive/parameter related block |
| VFD | 28 | 8 | Yes | Vertex Fetch and Decode |
| HLSQ | 137 | 6 | Yes | High Level Sequencer |
| VPC | 71 | 6 | Yes | Varying/position cache block |
| TSE | 119 | 4 | Yes | Tessellator/setup related block |
| RAS | 39 | 4 | Yes | Rasterizer |
| UCHE | 203 | 24 | Yes | Unified cache block |
| TP | 143 | 12 | Yes | Texture Processor |
| SP | 205 | 24 | Yes | Shader Processor |
| RB | 56 | 8 | Yes | Render Backend |
| LRZ | 34 | 3 | Yes | Only the first 3 LRZ counters are swept |
| ALWAYSON | 1 | 1 | Yes | Always-on counter |
| CCU / VSC / CMP / GBIF / GMU / UFC / others | varies | not used | No | Explicitly excluded for this phase |

The selected group list is hard-coded in `streamer_sweeper.c` so this specific sweep stays focused on the groups of interest for the softmax/RMSNorm benchmarking stage.

## Output directory structure

All output stays under:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps
```

Each run creates a timestamped directory:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps/sweep_YYYYMMDD_HHMMSS/
```

Inside that directory:

```text
summary.csv
run_config.txt
01_CP/
  CP_chunk001.csv
  CP_chunk001_meta.txt
  CP_chunk001_benchmark.log
  CP_chunk002.csv
  ...
02_RBBM/
03_PC/
...
14_ALWAYSON/
```

Each `*_chunkXXX.csv` contains:

```text
elapsed_s,counter_0,counter_1,...
```

Values are deltas since the previous sample, same as the original streamer.

Each `*_meta.txt` records which counters were attempted, which ones activated successfully, which ones failed, and where the benchmark log was saved.

## Build and push

From this folder:

```bash
NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=darwin-x86_64 ./build_and_push.sh
```

Linux host example:

```bash
NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=linux-x86_64 ./build_and_push.sh
```

The binary is pushed to:

```text
/data/local/tmp/streamer_sweeper
```

## Basic usage

### Recommended fused-softmax form

The `benchmark:` argument is treated as arguments to the default fused-softmax benchmark command:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper time:2 benchmark:\"--width 1024 --rows 2048 --repeats 512 --csv\""'
```

This expands internally to:

```bash
/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
  --op softmax \
  --variant fused_lmem \
  --spv /data/local/tmp/jerry_work/ml_primitives/spv/softmax_fused_lmem.spv \
  --width 1024 \
  --rows 2048 \
  --repeats 512 \
  --csv
```

### Equivalent long-option form

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 2 --bench-args \"--width 1024 --rows 2048 --repeats 512 --csv\""'
```

### Burst-separated benchmark mode

Use this when repeated dispatches are too close together and appear as one fused activity window in the counter CSV. The sweeper will run the benchmark command multiple times inside each counter chunk, wait for each benchmark process to finish, then sleep for the selected interval before starting the next burst.

```bash
adb shell "su -c \"/data/local/tmp/streamer_sweeper \
  --time 3 \
  --bursts 10 \
  --burst-sleep 0.1 \
  --bench-args '--width 1024 --rows 256 --repeats 32 --csv'\""
```

This creates the intended pattern:

```text
run benchmark burst 1
sleep 0.1 s
run benchmark burst 2
sleep 0.1 s
...
```

For this mode, `--repeats` inside `--bench-args` becomes the amount of work per burst. For example, `--bursts 10 --bench-args '--repeats 32 ...'` gives 10 separated bursts, each containing 32 repeats. Make sure `--time` is long enough to cover all bursts and sleeps.

### Internal width-sequence mode

Use this when you want one recorded counter window to contain several fused-softmax widths separated by CPU-side waits. This is different from `--bursts`: `--bursts` repeats the same benchmark, while `--widths` changes the softmax width for each step.

Recommended form:

```bash
adb shell "su -c \"/data/local/tmp/streamer_sweeper \
  --time 4 \
  --widths 128,256,512,1024,2048 \
  --width-sleep 0.1 \
  --bench-args '--rows 128 --repeats 16 --csv'\""
```

This creates the intended pattern inside each counter chunk:

```text
start counter recording
run width=128,  rows=128, repeats=16
sleep 0.1 s
run width=256,  rows=128, repeats=16
sleep 0.1 s
run width=512,  rows=128, repeats=16
sleep 0.1 s
run width=1024, rows=128, repeats=16
sleep 0.1 s
run width=2048, rows=128, repeats=16
stop counter recording
```

In width-sequence mode, usually omit `--width` from `--bench-args`; the sweeper appends `--width <value>` for each width step. Make sure `--time` is long enough to cover all widths plus the sleeps.

For custom benchmark commands, you can use `{width}` as a placeholder. The sweeper replaces `{width}` with each value from `--widths`. If no `{width}` placeholder exists, the sweeper appends `--width <value>` to the end of the benchmark command.

### Full custom benchmark command

Use this for RMSNorm, another softmax variant, or another binary:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 2 --benchmark-cmd \"/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench --op rmsnorm --variant basic --spv /data/local/tmp/jerry_work/ml_primitives/spv/rmsnorm_basic.spv --width 1024 --rows 2048 --repeats 512 --csv\""'
```

## Other useful commands

Print the hard-coded sweep plan:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --list-plan"'
```

Use a different sampling interval:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 2 --interval 0.005 --bench-args \"--width 1024 --rows 2048 --repeats 512 --csv\""'
```

Use a selected wait time between benchmark bursts:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 3 --bursts 10 --burst-sleep 0.1 --bench-args \"--width 1024 --rows 256 --repeats 32 --csv\""'
```

Use an internal width sequence inside each recorded counter chunk:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 4 --widths 128,256,512,1024,2048 --width-sleep 0.1 --bench-args \"--rows 128 --repeats 16 --csv\""'
```

Use a different output root:

```bash
adb shell 'su -c "/data/local/tmp/streamer_sweeper --time 2 --out-root /data/local/tmp/jerry_work/my_sweeps --bench-args \"--width 1024 --rows 2048 --repeats 512 --csv\""'
```

## Important behavior and design decisions

### 1. Different groups use different chunk sizes

The sweeper uses the physical slot counts from the manual test table. For example, CP chunks use 14 counters, TP chunks use 12, and UCHE/SP chunks use 24.

### 2. Invalid counters are skipped safely

If a counter fails `GET`, the sweeper logs the failure in the chunk metadata and continues trying later counters until either:

- the chunk fills the physical slot capacity, or
- the group runs out of counters.

This means invalid counters do not crash the sweep.

### 3. Only selected groups are swept

The program intentionally ignores groups outside the current project scope. This prevents the sweep from spending time on CCU, GMU, GBIF, UFC, and other groups that are not part of this stage.

### 4. CSV files never overwrite each other during normal use

Each run gets a timestamped directory. Each group gets a separate subdirectory. Each chunk gets its own CSV, metadata file, and benchmark log.

### 5. Benchmark is launched automatically per chunk

By default, the benchmark is launched once for every counter chunk. The stream lasts for exactly the requested duration unless the program is interrupted.

With `--bursts N --burst-sleep S`, the sweeper launches the same benchmark command `N` times within the same counter chunk and sleeps `S` seconds between launches. This is useful for creating visible separated GPU activity windows in the CSV.

With `--widths W1,W2,... --width-sleep S`, the sweeper launches one benchmark per listed width inside each counter chunk and sleeps `S` seconds between widths. This is the preferred mode for comparing several fused-softmax vector widths in one counter trace. In this mode, `--bursts` is ignored because the width list defines the sequence.

By default, if the benchmark runner is still active when the stream window ends, the sweeper terminates the benchmark process group before moving on. This avoids overlapping benchmark processes between chunks.

Use `--no-kill-benchmark` only if you intentionally want the sweeper to wait for the benchmark runner to finish naturally after the stream window.

## Pull results back to host

After the run finishes, the terminal prints the result directory. Pull it with:

```bash
adb pull /data/local/tmp/jerry_work/perfcounter_sweeps/sweep_YYYYMMDD_HHMMSS ./
```

Or pull the whole sweep archive:

```bash
adb pull /data/local/tmp/jerry_work/perfcounter_sweeps ./perfcounter_sweeps
```

## Files

- `streamer_sweeper.c` — new phone-side sweeper.
- `a8xx_perf_table.inc` — generated counter table.
- `a8xx_perfcntrs.xml` — source XML used to generate the table.
- `generate_a8xx_perf_table.py` — generator for the counter table.
- `Makefile` — Android/arm64 build helper.
- `build_and_push.sh` — build and push helper.
