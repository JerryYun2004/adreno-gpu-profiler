# Adreno A8xx KGSL Raw Performance Counter Streamer

This is a phone-side profiler for raw Adreno/KGSL performance counters. It uses the same KGSL ioctl path as the proven `adreno_perfcntr_test.c`, but replaces the fixed hard-coded counter list with a generated table from `a8xx_perfcntrs.xml`.

The program is intended to run on the phone through `adb shell`, usually with `su -c` if the KGSL counter ioctls require root.

## Files

- `adreno_perf_stream.c` — main phone-side profiler.
- `a8xx_perf_table.inc` — generated C counter table from `a8xx_perfcntrs.xml`.
- `generate_a8xx_perf_table.py` — regenerates the table if the XML changes.
- `a8xx_perfcntrs.xml` — copied source XML used for the generated table.
- `Makefile` — Android/arm64 build helper.
- `build_and_push.sh` — build and push helper for macOS/Linux host.

## Build on host

From the directory containing these files:

```bash
cd adreno_perf_streamer

# macOS default, matching your current setup style
make NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=darwin-x86_64

# Linux host example
make NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=linux-x86_64
```

Then push manually:

```bash
adb push adreno_perf_stream /data/local/tmp/adreno_perf_stream
adb shell chmod 755 /data/local/tmp/adreno_perf_stream
```

Or use:

```bash
NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=darwin-x86_64 ./build_and_push.sh
```

## Basic usage

List likely counters without starting the stream:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream -l alu'
adb shell su -c '/data/local/tmp/adreno_perf_stream -l busy'
adb shell su -c '/data/local/tmp/adreno_perf_stream -l instruction'
```

Run with exact or short names:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream -i 1 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES SP_FS_INSTRUCTIONS'
```

Run with fuzzy search terms and interactive selection:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream -i 0.5 busy alu instruction'
```

Run without giving an interval; the program will prompt for it:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES'
```

Use non-interactive fuzzy matching, choosing the best match automatically:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream -n -i 0.2 alu busy'
```

CSV mode for logs:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream --csv -i 0.2 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES' \
  > perf_stream.csv
```

## Output style

Default output is `name=value`, similar to the proven `adreno_perfcntr_test.c` style:

```text
elapsed_s=1.000421, SP_BUSY_CYCLES=123456, SP_ALU_WORKING_CYCLES=98765, SP_FS_INSTRUCTIONS=321
```

Values are deltas since the previous sample, not cumulative raw counter values.

Press `Ctrl+C` to stop. The program catches `SIGINT`/`SIGTERM` and calls `ADRENO_IOCTL_PERFCOUNTER_PUT` for every active counter before exiting.

## Important notes

1. The group IDs are hard-coded in `generate_a8xx_perf_table.py`. The proven values are `SP=0x0a` and `ALWAYSON=0x1b`. If a vendor kernel uses a different group mapping for less common groups, patch `GROUP_IDS` and regenerate the table.
2. Some groups may only support a limited number of simultaneously active counters. If `GET` fails for a counter, the program prints the failing group/selector and continues with counters that did activate.
3. This profiler does not launch a workload. Start the workload separately while this program streams counters.
4. The current program reads via `/dev/kgsl-3d0` by default. Override with `-d <device>` if needed.

## Regenerate the counter table

```bash
python3 generate_a8xx_perf_table.py a8xx_perfcntrs.xml > a8xx_perf_table.inc
make clean && make NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=darwin-x86_64
```
