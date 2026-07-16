# Live KGSL Monitoring Tools

This directory contains host-side Python utilities for monitoring Qualcomm KGSL GPU activity while a workload runs on a rooted Android device.

```text
tools/live/
├── live_kgsl_plot.py
└── live_kgsl_pwrstats.py
```

These tools provide live visibility into GPU busy, frequency, power, RAM-wait, and bus-level signals. They are auxiliary monitoring tools and are not part of the final raw hardware perf-counter streamer or sweeper.

## Relationship to the profiler

The main profiling tools are:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

The live tools complement them as follows:

```text
GPU workload
    ├── live KGSL/sysfs monitoring
    │     ├── busy/load
    │     ├── frequency
    │     ├── power statistics
    │     ├── RAM wait
    │     └── bus-level events
    │
    └── raw perf-counter capture
          ├── focused streamer
          └── broad sweeper
```

Use the live tools to confirm that the GPU is active and to observe high-level kernel signals while a workload runs.

Use the streamer or sweeper when raw Adreno hardware counter values are required.

---

# Requirements

## Host

- macOS or Linux
- Python 3.9 or newer
- Android Debug Bridge (`adb`)
- Python packages:
  - Matplotlib for `live_kgsl_plot.py`

Install Matplotlib:

```bash
python3 -m pip install matplotlib
```

Check the environment:

```bash
python3 --version
adb version
adb devices
```

## Android device

- Qualcomm Adreno GPU
- KGSL kernel driver
- root access through `su`
- tracefs under:
  ```text
  /sys/kernel/tracing
  ```
- KGSL tracepoints:
  ```text
  kgsl_pwrstats
  kgsl_gpubusy
  kgsl_buslevel
  ```

For `live_kgsl_plot.py`, the phone must also contain:

```text
/data/local/tmp/kgsl_live_sampler.sh
```

---

# Initial setup

Make the Python files executable:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

chmod +x tools/live/*.py
```

Confirm root access:

```bash
adb shell 'su -c id'
```

Confirm the required tracepoints:

```bash
adb shell 'su -c "
for ev in kgsl_pwrstats kgsl_gpubusy kgsl_buslevel; do
  if [ -e /sys/kernel/tracing/events/kgsl/\$ev/enable ]; then
    echo \"FOUND: \$ev\"
  else
    echo \"MISSING: \$ev\"
  fi
done
"'
```

---

# `live_kgsl_plot.py`

## Purpose

Starts a phone-side KGSL sysfs sampler through ADB and displays a rolling live plot on the host.

The intended signals are:

```text
gpu_load
gpu_busy_percentage
clock_mhz
cur_ab
```

The plot uses:

- the left axis for busy/load percentages; and
- the right axis for frequency and bus-bandwidth values.

The script can also save every received CSV row to a host-side output file.

## Command-line options

```text
--interval   Phone-side sampling interval in seconds
--window     Rolling plot window in seconds
--out        Optional host-side CSV output path
```

Defaults:

```text
interval = 0.02 seconds
window   = 10 seconds
out      = disabled
```

## Run

Plot without saving:

```bash
python3 tools/live/live_kgsl_plot.py
```

Use a 5-second rolling window:

```bash
python3 tools/live/live_kgsl_plot.py \
  --interval 0.02 \
  --window 5
```

Plot and save the raw CSV stream:

```bash
mkdir -p results/live

python3 tools/live/live_kgsl_plot.py \
  --interval 0.02 \
  --window 10 \
  --out results/live/kgsl_live.csv
```

Stop the plotter with `Ctrl+C`.

## What it runs on the phone

The script starts:

```text
/data/local/tmp/kgsl_live_sampler.sh <interval>
```

through:

```text
adb exec-out su -c
```

The sampler must print a CSV header followed by CSV rows.

## Expected behavior

A Matplotlib window should show a rolling plot and update its title with the latest values:

```text
load
busy
clock
cur_ab
```

When `--out` is provided, the original sampler rows are copied to the requested CSV file.

## Current compatibility issue

The current `live_kgsl_plot.py` expects these CSV fields:

```text
timestamp_ns
gpu_load
gpu_busy_percentage
clock_mhz
cur_ab
```

The current sampler under `scripts/kgsl_live_sampler.sh` instead produces fields including:

```text
timestamp_ms
gpu_load
gpu_busy_percentage
clock_mhz
cur_freq
gpuclk
bus_split
gpubusy
```

Therefore, the current plotter and current sampler are not directly compatible.

The main mismatches are:

```text
timestamp_ns  versus  timestamp_ms
cur_ab        missing from current sampler
```

With the current sampler:

- `timestamp_ns` is not found, so rows are skipped;
- the graph may remain empty; and
- `cur_ab` cannot be plotted.

Before using this script, either:

1. restore the older sampler format expected by the plotter; or
2. update the plotter to use the current fields.

A suitable update would use:

```text
timestamp_ms
gpu_load
gpu_busy_percentage
clock_mhz
cur_freq
```

and convert time with:

```python
t = (raw_ts - t0) / 1000.0
```

instead of dividing by `1e9`.

## Push the sampler

```bash
adb push \
  scripts/kgsl_live_sampler.sh \
  /data/local/tmp/kgsl_live_sampler.sh

adb shell \
  'chmod 755 /data/local/tmp/kgsl_live_sampler.sh'
```

## Limitations

- Requires an interactive graphical desktop.
- Uses fixed y-axis ranges:
  ```text
  busy/load: 0 to 105
  frequency/cur_ab: 0 to 1200
  ```
- Missing values are plotted as zero.
- The script assumes the sampler emits correctly formatted CSV rows.
- The plotter terminates the ADB process on exit, but an interrupted phone-side process should still be checked if the connection drops.
- This tool provides high-level sysfs signals, not raw hardware performance counters.

---

# `live_kgsl_pwrstats.py`

## Purpose

Streams selected KGSL tracepoints from:

```text
/sys/kernel/tracing/trace_pipe
```

and records parsed events to a host-side CSV file.

It enables:

```text
kgsl_pwrstats
kgsl_gpubusy
kgsl_buslevel
```

## Parsed values

For `kgsl_pwrstats`, the script extracts:

```text
total
busy
ram_time
ram_wait
```

and calculates:

```text
busy_pct     = 100 × busy / total
ram_wait_pct = 100 × ram_wait / ram_time
```

For `kgsl_gpubusy`, it records:

```text
busy
elapsed
```

For `kgsl_buslevel`, it stores the raw trace line.

## Command-line options

```text
--out        Required host-side CSV output path
--duration   Capture duration in seconds
```

Default duration:

```text
15 seconds
```

## Run

Terminal 1:

```bash
mkdir -p results/live

python3 tools/live/live_kgsl_pwrstats.py \
  --out results/live/kgsl_pwrstats.csv \
  --duration 20
```

Terminal 2, while the monitor is running:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_compute_probe'
```

Or run another GPU workload such as:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_mem_probe mem.comp.spv'
```

## Expected output

The host-side CSV contains:

```text
host_time_s
event
busy_pct
ram_wait_pct
total
busy
ram_time
ram_wait
gpubusy_busy
gpubusy_elapsed
raw
```

Example event types:

```text
kgsl_pwrstats
kgsl_gpubusy
kgsl_buslevel
```

The terminal prints live `kgsl_pwrstats` summaries such as:

```text
pwrstats busy= 99.80% ram_wait= 12.40%
```

## Trace setup

Before streaming, the script:

1. stops tracing;
2. disables the KGSL event group;
3. enables the three required events;
4. clears the trace buffer; and
5. starts tracing.

On exit, it stops tracing and disables the three events.

## Role in workload analysis

This tool is useful for distinguishing broad workload behavior:

```text
High busy, low RAM wait
    often consistent with compute-heavy activity

High busy, high RAM wait
    often consistent with memory stalls or external-memory pressure

Bus-level changes
    indicate changes in the requested or selected memory-bus level
```

These are interpretation hints, not definitive bottleneck diagnoses. Raw counter captures and controlled workload comparisons are still required.

## Current limitations

### No tracefs fallback

The script assumes:

```text
/sys/kernel/tracing
```

It does not fall back to:

```text
/sys/kernel/debug/tracing
```

### Duration may overrun when no events arrive

The loop checks elapsed time only after reading a line from `trace_pipe`.

Because:

```python
proc.stdout.readline()
```

is blocking, the script can wait longer than `--duration` if no matching GPU events are produced.

Running a GPU workload during the capture normally avoids this.

### Kernel-format dependency

The regular expressions expect `kgsl_pwrstats` fields in this order:

```text
total
busy
ram_time
ram_wait
```

and `kgsl_gpubusy` fields containing:

```text
busy
elapsed
```

A different vendor-kernel print format may not parse correctly.

Inspect the actual formats with:

```bash
adb shell 'su -c "
cat /sys/kernel/tracing/events/kgsl/kgsl_pwrstats/format
cat /sys/kernel/tracing/events/kgsl/kgsl_gpubusy/format
"'
```

### Host timestamps

`host_time_s` is based on the host wall clock when the line is received.

It is not the original trace timestamp and may include ADB transport latency.

### No live graph

This script prints selected summaries and writes CSV data, but it does not display a Matplotlib plot.

---

# Typical workflows

## Quick live sysfs graph

After resolving the sampler-field mismatch:

```bash
python3 tools/live/live_kgsl_plot.py \
  --interval 0.02 \
  --window 10 \
  --out results/live/compute_sysfs.csv
```

Then run the workload in another terminal.

## Live power and RAM-wait capture

```bash
python3 tools/live/live_kgsl_pwrstats.py \
  --out results/live/memory_pwrstats.csv \
  --duration 20
```

Run a memory workload during the capture:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_mem_probe mem.comp.spv'
```

## Compare compute and memory behavior

Compute:

```bash
python3 tools/live/live_kgsl_pwrstats.py \
  --out results/live/compute_pwrstats.csv \
  --duration 20
```

Memory:

```bash
python3 tools/live/live_kgsl_pwrstats.py \
  --out results/live/memory_pwrstats.csv \
  --duration 20
```

Keep workload duration, repetition count, phone temperature, and run order controlled.

---

# Expected results

## `live_kgsl_plot.py`

After compatibility is fixed, expect:

- a live rolling graph;
- GPU load and busy activity during workloads;
- clock or frequency changes;
- an optional CSV copy of the sampler stream; and
- low values during idle periods.

## `live_kgsl_pwrstats.py`

Expect:

- `kgsl_pwrstats` rows during active GPU periods;
- `kgsl_gpubusy` rows with busy and elapsed values;
- occasional `kgsl_buslevel` rows;
- calculated busy and RAM-wait percentages; and
- raw trace lines retained for later verification.

A nearly empty output usually means:

- no GPU workload ran;
- required tracepoints are unavailable;
- root access failed;
- tracefs is mounted elsewhere; or
- the kernel event format does not match the parser.

---

# Connection to final measurements

These tools should normally be used before or beside final counter captures:

1. run the benchmark directly and verify correctness;
2. use a live tool to confirm GPU activity;
3. use a focused KGSL trace when command timing is needed;
4. run the perf-counter sweeper to identify responsive counters;
5. run the streamer with selected counters; and
6. compare kernel-level and hardware-counter behavior during similar workload windows.

Avoid running every monitoring mechanism simultaneously during final low-overhead measurements, because plotting, ADB streaming, tracepoints, and sysfs reads can add capture overhead.

---

# Troubleshooting

## No CSV header from `live_kgsl_plot.py`

Check the sampler manually:

```bash
adb shell \
  'su -c "/data/local/tmp/kgsl_live_sampler.sh 0.05"'
```

Confirm that the first line is a CSV header.

## Plot window opens but remains empty

Check the field names:

```bash
adb shell \
  'su -c "/data/local/tmp/kgsl_live_sampler.sh 0.05"' \
  | head
```

Compare the header against the fields expected by `live_kgsl_plot.py`.

## Matplotlib import fails

```bash
python3 -m pip install matplotlib
```

## Permission denied for tracepoints

```bash
adb shell 'su -c id'
```

Then test:

```bash
adb shell 'su -c "
echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_pwrstats/enable
echo 0 > /sys/kernel/tracing/events/kgsl/kgsl_pwrstats/enable
"'
```

## No `kgsl_pwrstats` rows

Confirm the tracepoint exists:

```bash
adb shell \
  'su -c "ls /sys/kernel/tracing/events/kgsl/kgsl_pwrstats"'
```

Then run an active GPU workload while the monitor is open.

## Script does not stop at the requested duration

This can occur when `trace_pipe` produces no new lines. Generate GPU activity or stop the script with `Ctrl+C`.

---

# Notes

- Results are device-, kernel-, and driver-specific.
- Regenerate validation data after Android, kernel, or driver updates.
- Do not interpret one busy or RAM-wait percentage in isolation.
- Use controlled workload comparisons and raw counters for final conclusions.
- Save the exact script commit, workload command, driver, device, and kernel with each result.
