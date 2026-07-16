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

From the repository root:

```bash
cd tools/perfcounter_streamer

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

Print CSV rows on the terminal:

```bash
adb shell su -c '/data/local/tmp/adreno_perf_stream --csv -i 0.2 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES' \
  > perf_stream.csv
```

## Output style

Default terminal output is `name=value`, similar to the proven `adreno_perfcntr_test.c` style. The terminal output does not print `elapsed_s=`:

```text
SP_BUSY_CYCLES=123456, SP_ALU_WORKING_CYCLES=98765, SP_FS_INSTRUCTIONS=321
```

Values are deltas since the previous sample, not cumulative raw counter values.

The program also writes every sample to a temporary CSV file on the phone:

```text
/data/local/tmp/jerry_work/adreno_perf_stream_last.csv
```

The CSV includes `elapsed_s` as the first column, followed by the selected counter deltas.

Press `Ctrl+C` to stop. The program catches `SIGINT`/`SIGTERM`, calls `ADRENO_IOCTL_PERFCOUNTER_PUT` for every active counter, then asks whether to keep the CSV and where to save it. The destination path is a path on the phone, not on the host Mac. To copy it back to the Mac, use `adb pull`.

Example:

```bash
adb shell
su
/data/local/tmp/adreno_perf_stream -i 1 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES
# Press Ctrl+C, answer Y, then choose for example:
# /data/local/tmp/sp_alu_test.csv
exit
exit
adb pull /data/local/tmp/sp_alu_test.csv ./sp_alu_test.csv
```

If stdin is not interactive, the program keeps the temporary CSV at `/data/local/tmp/jerry_work/adreno_perf_stream_last.csv`.

With `--csv`, the terminal stream itself is CSV-formatted and includes an `elapsed_s` column so it can still be redirected on the host.

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


## How to test fuzzy counter input

First list fuzzy matches without starting profiling:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream -l alu"'
adb shell 'su -c "/data/local/tmp/adreno_perf_stream -l busy"'
adb shell 'su -c "/data/local/tmp/adreno_perf_stream -l instruction"'
adb shell 'su -c "/data/local/tmp/adreno_perf_stream -l fs_instruction"'
```

Then test interactive fuzzy selection:

```bash
adb shell
su
/data/local/tmp/adreno_perf_stream -i 1 alu busy
```

For each fuzzy term, the program should print a numbered list of close matches. Press Enter to choose the first match, or type a number such as `2` and press Enter.

For non-interactive best-match selection, use `-n`:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream -n -i 1 alu busy"'
```

## Group-wide presets

Use `--list-presets` to see all generated perfcounter enum groups:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream --list-presets"'
```

Use `--preset <group>` to select every counter from one generated enum group. For example, `--preset cp` selects every counter generated from `enum name="a8xx_cp_perfcounter_select"`:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream --preset cp -i 0.001 --csv"'
```

The full enum-style spelling is also accepted:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream --preset a8xx_cp_perfcounter_select -i 0.001 --csv"'
```

`--preset` is repeatable and can be combined with normal counter queries:

```bash
adb shell 'su -c "/data/local/tmp/adreno_perf_stream --preset sp --preset uche -i 0.001 --csv SP_BUSY_CYCLES"'
```

Important: this selects the whole generated enum, but the kernel/hardware may not allow every counter in that group to be active at the same time. Counters that fail `GET` are reported and skipped; the CSV header only contains counters that successfully activated.
