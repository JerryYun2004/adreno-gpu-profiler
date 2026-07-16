# Vendor vs. Turnip KGSL Trace Comparison

This directory preserves a historical experiment comparing the Qualcomm vendor
Vulkan driver with Mesa Turnip under compute-heavy and memory-heavy Vulkan
workloads.

The experiment uses focused KGSL/ftrace capture to compare:

- command-batch organization;
- command-batch active duration;
- queue-to-start delay;
- GMU retirement latency;
- coarse GPU busy percentage;
- RAM-wait percentage;
- GPU frequency;
- bus-level activity;
- GPU contexts; and
- KGSL allocation, map, and free events.

It was originally stored at:

```text
results/compute_trace_20260529/
```

and was moved during repository reorganization to:

```text
experiments/2026-05-29_vendor_turnip_trace_comparison/
```

The directory is a historical experiment bundle. It is not part of the runtime
implementation of the perf-counter streamer or sweeper.

---

# Experiment question

The experiment asks:

> How do the Qualcomm vendor Vulkan driver and Mesa Turnip differ when running
> the same broad classes of compute-heavy and memory-heavy GPU workloads?

Four experiment groups are represented:

```text
vendor driver + compute-heavy workload
vendor driver + memory-heavy workload
Turnip driver + compute-heavy workload
Turnip driver + memory-heavy workload
```

Each group contains five repeated trial summaries.

The final aggregation intentionally excludes early setup-heavy trials:

```text
compute/ALU groups:
    use trials 2–5

memory groups:
    use trials 3–5
```

---

# Directory layout

The original experiment inventory contains:

```text
2026-05-29_vendor_turnip_trace_comparison/
├── README.md
├── run_focused_kgsl_trace_compute.sh
├── run_focused_kgsl_trace_mem.sh
├── summarize_trials.py
├── trial_comparison_summary.txt
├── turnip_compute_summary_1.txt
├── turnip_compute_summary_2.txt
├── turnip_compute_summary_3.txt
├── turnip_compute_summary_4.txt
├── turnip_compute_summary_5.txt
├── turnip_mem_summary_1.txt
├── turnip_mem_summary_2.txt
├── turnip_mem_summary_3.txt
├── turnip_mem_summary_4.txt
├── turnip_mem_summary_5.txt
├── vendor_compute_summary_1.txt
├── vendor_compute_summary_2.txt
├── vendor_compute_summary_3.txt
├── vendor_compute_summary_4.txt
├── vendor_compute_summary_5.txt
├── vendor_mem_summary_1.txt
├── vendor_mem_summary_2.txt
├── vendor_mem_summary_3.txt
├── vendor_mem_summary_4.txt
└── vendor_mem_summary_5.txt
```

The repository also contains canonical analysis utilities under:

```text
analysis/kgsl_trace_analysis/
├── parse_focused_kgsl_trace.py
└── summarize_trials.py
```

The experiment-local `summarize_trials.py` should be treated as a historical
snapshot. The copy under `analysis/kgsl_trace_analysis/` should be treated as
the maintained location.

---

# High-level workflow

```text
Vulkan benchmark binary + SPIR-V shader
                    ↓
select vendor driver or Turnip
                    ↓
enable focused KGSL tracepoints
                    ↓
run benchmark while tracing
                    ↓
pull /sys/kernel/tracing/trace
                    ↓
filter KGSL / command-batch / frequency lines
                    ↓
parse trace into one detailed trial summary
                    ↓
repeat five times for every driver/workload group
                    ↓
aggregate selected steady-state trials
                    ↓
trial_comparison_summary.txt
```

---

# Relationship to the main profiler products

The project’s main hardware-counter tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

This experiment does not:

- implement hardware-counter access;
- define Adreno counter tables;
- program KGSL performance countables;
- create streamer CSV files;
- create sweeper group/chunk directories;
- become linked into either profiler binary; or
- need to be present to build the profiler.

It provides complementary kernel/driver evidence.

## This experiment answers

- How many command batches did each driver submit?
- Did one driver combine work into fewer batches?
- How long was the main command batch active?
- Did the memory workload increase KGSL RAM wait?
- Did extra contexts or surface mappings contaminate early trials?
- Were the repeated steady-state trials stable?
- Did both drivers run at the same reported frequency?

## The streamer/sweeper answer

- Which Adreno hardware block was active?
- Did SP ALU-working cycles increase?
- Did UCHE requests or cache traffic increase?
- Which TP, HLSQ, RBBM, or SP stalls occurred?
- Which hardware countables were supported?
- Did a counter scale with controlled workload size?

## Combined use

A stronger performance diagnosis combines:

```text
this experiment:
    driver scheduling
    batch organization
    busy percentage
    RAM wait
    frequency
    KGSL memory events

streamer/sweeper:
    detailed block-level counter deltas
```

---

# Status summary

| File group | Status | Purpose |
|---|---|---|
| `run_focused_kgsl_trace_compute.sh` | Historical capture wrapper | Capture one compute-heavy focused trace |
| `run_focused_kgsl_trace_mem.sh` | Historical capture wrapper with known bugs | Capture one memory-heavy focused trace |
| `summarize_trials.py` | Historical experiment-specific analyzer | Aggregate selected repeated summaries |
| `trial_comparison_summary.txt` | Final historical result | Report means, standard deviations, and ratios |
| `vendor_compute_summary_*.txt` | Derived per-trial evidence | Vendor compute trials |
| `vendor_mem_summary_*.txt` | Derived per-trial evidence | Vendor memory trials |
| `turnip_compute_summary_*.txt` | Derived per-trial evidence | Turnip compute trials |
| `turnip_mem_summary_*.txt` | Derived per-trial evidence | Turnip memory trials |

The summary files should be preserved as historical evidence rather than
manually reformatted.

---

# Workloads

# Compute-heavy workload

The historical capture workflow uses:

```text
vk_compute_probe
alu.comp.spv
```

The exact binary and shader used by the experiment should be identified through
their historical hashes or Git revision.

The canonical modern location for related ALU workload sources is:

```text
benchmarks/microbenchmarks/alu_calibration/
```

The historical script does not record:

- element count;
- ALU iteration count;
- dispatch repeat count;
- shader hash;
- runner hash; or
- verification result.

When the runner is launched without arguments beyond the shader path, its own
compiled defaults determine these settings.

# Memory-heavy workload

The uploaded memory capture script uses:

```text
vk_mem_probe
mem.comp.spv
```

The related current source is under:

```text
benchmarks/microbenchmarks/memory_stride/runner/
```

This is the verified four-stream memory-heavy workload, not one of the
compile-time stride variants.

The script also relies on runner defaults because it passes no explicit
element/iteration/repeat values.

---

# Driver selection

# Vendor driver

A normal shell launch without a custom Vulkan ICD environment generally uses
the phone’s vendor Vulkan driver.

Conceptually:

```bash
adb shell '
  cd /data/local/tmp/jerry_work
  ./vk_compute_probe alu.comp.spv
'
```

# Turnip

A Turnip launch must explicitly select the manually deployed ICD, unless another
driver-selection mechanism is already active.

Conceptually:

```bash
adb shell '
  export VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json
  export LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH

  cd /data/local/tmp/jerry_work
  ./vk_compute_probe alu.comp.spv
'
```

The exact Turnip path must match the device installation.

The capture scripts in this experiment do not themselves record or verify
driver identity. A successful trace does not prove that the intended driver was
loaded.

---

# Capture script reference

# `run_focused_kgsl_trace_compute.sh`

## Purpose

Historical host-side wrapper intended to:

1. create a device-side trace setup script;
2. stop tracing;
3. clear the trace buffer;
4. disable existing KGSL trace events;
5. enable a selected focused event set;
6. start tracing;
7. run the compute-heavy Vulkan benchmark;
8. stop tracing;
9. pull the trace to the host;
10. filter KGSL-related lines; and
11. generate a compact event-count summary.

## Expected outputs

The historical workflow writes files under:

```text
$HOME/adreno_turnip/
```

with names similar to:

```text
kgsl_focused_trace_compute.log
kgsl_focused_trace_compute_filtered.log
kgsl_focused_trace_compute_summary.txt
```

## Status

Legacy capture wrapper.

The current repository also contains more reusable capture scripts under:

```text
tools/capture/
```

Review those before creating a new formal experiment.

## Important limitation

The exact contents of the compute wrapper should be inspected before reuse:

```bash
sed -n '1,260p' \
  experiments/2026-05-29_vendor_turnip_trace_comparison/run_focused_kgsl_trace_compute.sh
```

Confirm:

- workload binary;
- shader path;
- output filenames;
- Turnip environment;
- tracepoint list;
- cleanup behavior; and
- current device workspace paths.

---

# `run_focused_kgsl_trace_mem.sh`

## Purpose

Historical host-side wrapper intended to capture the memory-heavy workload.

The uploaded script performs the following operations.

## 1. Enables strict shell error handling

```bash
set -euo pipefail
```

This causes many command failures, unset variables, and pipeline failures to
terminate the script.

## 2. Defines output and workload paths

```bash
OUT_DIR="$HOME/adreno_turnip"
WORKDIR=/data/local/tmp/jerry_work
WORKLOAD="./vk_mem_probe mem.comp.spv"
```

## 3. Generates a device-side setup script

The host script writes:

```text
/data/local/tmp/kgsl_trace_setup_compute.sh
```

on the phone.

That device-side script:

- turns tracing off;
- clears the trace buffer;
- disables existing KGSL events; and
- enables the experiment’s focused event set.

## 4. Enables tracepoints

The selected events include:

```text
adreno_cmdbatch_queued
adreno_cmdbatch_submitted
adreno_cmdbatch_ready
adreno_cmdbatch_retired
adreno_cmdbatch_done
kgsl_issueibcmds
kgsl_gpubusy
kgsl_pwrstats
kgsl_pwrlevel
kgsl_buslevel
gpu_frequency
kgsl_mem_alloc
kgsl_mem_map
kgsl_mem_free
kgsl_mem_sync_cache
kgsl_context_create
kgsl_pwr_set_state
kgsl_pwr_request_state
```

Unavailable events are skipped.

## 5. Starts tracing as root

```bash
adb shell 'su -c "echo 1 > /sys/kernel/tracing/tracing_on"'
```

## 6. Runs the memory benchmark

```bash
adb shell "cd $WORKDIR && $WORKLOAD"
```

## 7. Stops and pulls the trace

The script copies the trace through standard output into a host file.

## 8. Filters relevant lines

```bash
grep -E "kgsl|adreno_cmdbatch|gpu_frequency"
```

## 9. Generates event counts

An `awk` program scans tokens containing:

```text
kgsl_
adreno_cmdbatch_
gpu_frequency
```

then sorts the counts numerically.

---

# Critical issues in `run_focused_kgsl_trace_mem.sh`

The script should not be reused unchanged.

## 1. Memory script writes compute filenames

The uploaded script defines:

```bash
RAW_LOG="$OUT_DIR/kgsl_focused_trace_compute.log"
FILTERED_LOG="$OUT_DIR/kgsl_focused_trace_compute_filtered.log"
SUMMARY="$OUT_DIR/kgsl_focused_trace_compute_summary.txt"
```

even though it runs:

```bash
./vk_mem_probe mem.comp.spv
```

This can overwrite compute results and mislabel a memory capture as compute.

Correct names should use `mem`, for example:

```bash
RAW_LOG="$OUT_DIR/kgsl_focused_trace_mem.log"
FILTERED_LOG="$OUT_DIR/kgsl_focused_trace_mem_filtered.log"
SUMMARY="$OUT_DIR/kgsl_focused_trace_mem_summary.txt"
```

## 2. Device setup script is also compute-named

The script creates:

```text
/data/local/tmp/kgsl_trace_setup_compute.sh
```

for a memory capture.

This is mainly a naming problem, but it increases the risk of overwriting or
confusing setup files.

## 3. Terminal messages call the workload compute

The script prints:

```text
Run Vulkan compute workload...
```

while running the memory workload.

## 4. No output-directory creation

The script assumes:

```text
$HOME/adreno_turnip
```

already exists.

Add:

```bash
mkdir -p "$OUT_DIR"
```

before writing files.

## 5. No failure-safe tracing cleanup

Because `set -e` is enabled, a benchmark or ADB failure can terminate the script
before:

```text
tracing_on
```

is reset to zero.

A safer script should register a cleanup trap.

Example:

```bash
cleanup() {
  adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"' \
    >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM
```

## 6. Repeated trials overwrite one another

The script uses fixed filenames with no:

- trial number;
- driver label;
- workload label correction; or
- timestamp.

Five trials therefore require manual renaming between runs.

## 7. No driver selection or verification

The script does not set:

```text
VK_ICD_FILENAMES
LD_LIBRARY_PATH
```

and does not save Vulkan driver identity.

It cannot independently prove whether a capture used vendor Vulkan or Turnip.

## 8. Benchmark stdout/stderr is not saved

The benchmark runs in the terminal, but its output is not preserved as an
experiment artifact.

The directory therefore does not record:

- verification result;
- benchmark exit code;
- Vulkan driver name;
- shader path reported by the runner; or
- workload parameters.

## 9. Event-count parser can count process names

The token-based `awk` logic counts any token containing `kgsl_`.

This can produce entries such as:

```text
kgsl_hwsched-1260
```

which are process/thread names rather than tracepoint names.

A parser based on the text between trace separators:

```text
: event_name:
```

is more reliable.

## 10. Existing event state is not restored

The setup script disables all KGSL events and enables the focused set.

It does not save and restore the previous tracepoint state.

## 11. No trace-buffer size control

A workload or background activity burst can exceed the available trace buffer.

The script does not inspect:

```text
buffer_size_kb
entries-in-buffer/entries-written
```

## 12. Historical device paths may be stale

The project later organized device files into numbered directories.

Confirm current locations before reuse.

---

# Safer corrected memory capture

The following command pattern corrects the main naming and cleanup problems.

```bash
#!/usr/bin/env bash
set -euo pipefail

DRIVER="${1:?usage: $0 vendor|turnip TRIAL}"
TRIAL="${2:?usage: $0 vendor|turnip TRIAL}"

REPO_ROOT="$(git rev-parse --show-toplevel)"
OUT_DIR="$REPO_ROOT/experiments/2026-05-29_vendor_turnip_trace_comparison/regenerated/${DRIVER}/mem/trial_${TRIAL}"

REMOTE_WORKDIR="/data/local/tmp/jerry_work"
REMOTE_RUNNER="./vk_mem_probe"
REMOTE_SHADER="mem.comp.spv"

mkdir -p "$OUT_DIR"

RAW_LOG="$OUT_DIR/trace_raw.log"
FILTERED_LOG="$OUT_DIR/trace_filtered.log"
EVENT_SUMMARY="$OUT_DIR/event_summary.txt"
BENCHMARK_LOG="$OUT_DIR/benchmark.log"

cleanup() {
  adb shell \
    'su -c "echo 0 > /sys/kernel/tracing/tracing_on"' \
    >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM

adb shell 'su -c "
  TRACE=/sys/kernel/tracing
  EVENTS=\$TRACE/events/kgsl

  echo 0 > \$TRACE/tracing_on
  : > \$TRACE/trace

  for e in \$EVENTS/*/enable; do
    echo 0 > \$e 2>/dev/null || true
  done

  for ev in \
    adreno_cmdbatch_queued \
    adreno_cmdbatch_submitted \
    adreno_cmdbatch_ready \
    adreno_cmdbatch_retired \
    adreno_cmdbatch_done \
    kgsl_issueibcmds \
    kgsl_gpubusy \
    kgsl_pwrstats \
    kgsl_pwrlevel \
    kgsl_buslevel \
    gpu_frequency \
    kgsl_mem_alloc \
    kgsl_mem_map \
    kgsl_mem_free \
    kgsl_mem_sync_cache \
    kgsl_context_create \
    kgsl_pwr_set_state \
    kgsl_pwr_request_state
  do
    if [ -e \$EVENTS/\$ev/enable ]; then
      echo 1 > \$EVENTS/\$ev/enable
    fi
  done

  echo 1 > \$TRACE/tracing_on
"'

if [[ "$DRIVER" == "turnip" ]]; then
  adb shell "
    cd '$REMOTE_WORKDIR'

    VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
    LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:\$LD_LIBRARY_PATH \
    '$REMOTE_RUNNER' '$REMOTE_SHADER'
  " 2>&1 | tee "$BENCHMARK_LOG"
else
  adb shell "
    cd '$REMOTE_WORKDIR'
    '$REMOTE_RUNNER' '$REMOTE_SHADER'
  " 2>&1 | tee "$BENCHMARK_LOG"
fi

cleanup
trap - EXIT INT TERM

adb shell \
  'su -c "cat /sys/kernel/tracing/trace"' \
  > "$RAW_LOG"

grep -E \
  'kgsl|adreno_cmdbatch|gpu_frequency' \
  "$RAW_LOG" \
  > "$FILTERED_LOG" || true

python3 - \
  "$FILTERED_LOG" \
  "$EVENT_SUMMARY" <<'PY'
from collections import Counter
from pathlib import Path
import re
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])

event_re = re.compile(r": ([A-Za-z0-9_]+):")
counts = Counter()

for line in source.read_text(errors="replace").splitlines():
    match = event_re.search(line)
    if match:
        counts[match.group(1)] += 1

output.write_text(
    "\n".join(
        f"{count:4d} {event}"
        for event, count in sorted(
            counts.items(),
            key=lambda item: (-item[1], item[0]),
        )
    ) + "\n"
)
PY

python3 \
  "$REPO_ROOT/analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py" \
  --input "$FILTERED_LOG" \
  > "$OUT_DIR/detailed_summary.txt"
```

Review and adapt device paths before using this as a current workflow.

---

# `summarize_trials.py`

## Purpose

Aggregates detailed per-trial text summaries for four fixed experiment groups:

```text
vendor / ALU
Turnip / ALU
vendor / memory
Turnip / memory
```

## Input filename patterns

```python
vendor_compute_summary_*.txt
turnip_compute_summary_*.txt
vendor_mem_summary_*.txt
turnip_mem_summary_*.txt
```

## Trial cutoffs

```python
vendor compute: first included trial = 2
Turnip compute: first included trial = 2
vendor memory:  first included trial = 3
Turnip memory:  first included trial = 3
```

## Metrics extracted

The script uses regular expressions to extract:

```text
submitted_batches
retired_batches
avg_active_ticks
max_active_ticks
avg_queue_to_start_ticks
max_queue_to_start_ticks
avg_gmu_latency_ticks
max_gmu_latency_ticks
avg_busy_pct
max_busy_pct
avg_ram_wait_pct
max_ram_wait_pct
min_freq_khz
max_freq_khz
```

## Statistics

For every metric/group, it calculates:

```text
arithmetic mean
sample standard deviation
```

It then prints several ratios.

## Requirements

Only the Python standard library is required:

```text
re
statistics
pathlib
```

No `pip install` is needed.

## Important location behavior

The script searches:

```python
Path(".").glob(...)
```

so it must run with the experiment directory as the current working directory.

Correct:

```bash
cd /Users/jerryyun/adreno-gpu-profiler/experiments/2026-05-29_vendor_turnip_trace_comparison

python3 \
  ../../analysis/kgsl_trace_analysis/summarize_trials.py
```

## Reproduce without overwriting

```bash
cd /Users/jerryyun/adreno-gpu-profiler/experiments/2026-05-29_vendor_turnip_trace_comparison

python3 \
  ../../analysis/kgsl_trace_analysis/summarize_trials.py \
  > /tmp/trial_comparison_summary.regenerated.txt

diff -u \
  trial_comparison_summary.txt \
  /tmp/trial_comparison_summary.regenerated.txt
```

## Known limitations

- groups are hard-coded;
- filename patterns are hard-coded;
- trial cutoffs are hard-coded;
- it parses human-readable text rather than structured data;
- it takes the first matching `avg_busy_pct`;
- it assumes that first match belongs to `kgsl_pwrstats`;
- it does not report total active ticks;
- it does not record exclusion reasons per file;
- it can divide by zero when calculating ratios;
- it does not validate benchmark or driver identity; and
- it does not verify that all expected trials exist.

---

# Detailed summary file format

Every per-trial summary contains sections such as:

```text
=== Event counts ===
=== Command batch timing ===
=== Per-context retired batch counts ===
=== kgsl_pwrstats ===
=== kgsl_gpubusy ===
=== Frequency / power ===
=== Memory by usage ===
=== Memory event counts ===
=== Memory bytes by event type and usage ===
```

The files are produced by:

```text
analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py
```

---

# Metric definitions

# Submitted batches

Number of parsed:

```text
adreno_cmdbatch_submitted
```

events.

# Retired batches

Number of parsed:

```text
adreno_cmdbatch_retired
```

events.

# Active ticks

The `active` field reported by the retired command-batch tracepoint.

The experiment treats this as a relative command-batch duration.

Do not convert it to seconds without confirming the kernel’s units.

# Queue-to-start ticks

Derived as:

```text
start - submitted_to_rb
```

# GMU latency ticks

Derived as:

```text
retired_on_gmu - retire
```

# KGSL busy percentage

For every `kgsl_pwrstats` sample:

```text
busy / total × 100
```

The summary prints the mean and maximum.

# RAM-wait percentage

For every valid `kgsl_pwrstats` sample:

```text
ram_wait / ram_time × 100
```

# KGSL GPU busy

Alternative coarse busy measurement from:

```text
kgsl_gpubusy
```

Many trial files contain zero or one sample.

# Frequency

Parsed from:

```text
gpu_frequency
```

The selected steady-state trials all report:

```text
1,100,000 kHz
```

# Bus-level average bandwidth

Parsed from:

```text
kgsl_buslevel
```

The exact unit is driver/kernel-specific and should not be relabelled without
checking the tracepoint definition.

# Memory-by-usage totals

The parser sums event sizes grouped by usage labels.

These totals describe event volume, not peak resident memory.

---

# Per-trial classification

# Vendor compute trials

## `vendor_compute_summary_1.txt`

Setup-contaminated trial.

Observed:

```text
4 submitted batches
4 retired batches
2 contexts
2 main-context batches
2 context-8 batches
surface and egl-image mapping activity
```

Excluded from the final compute aggregate.

## `vendor_compute_summary_2.txt`

Steady-state candidate.

Observed:

```text
2 batches
one very short setup batch
one main compute batch around 3.11 million active ticks
```

Included.

## `vendor_compute_summary_3.txt`

Steady-state candidate with the same two-batch organization.

Included.

## `vendor_compute_summary_4.txt`

Included in the final aggregate.

The final statistics indicate the same stable two-batch pattern.

## `vendor_compute_summary_5.txt`

Included in the final aggregate.

---

# Turnip compute trials

## `turnip_compute_summary_1.txt`

Setup-contaminated trial.

Observed:

```text
3 batches
2 contexts
extra surface/egl-image map activity
one long main batch
two short context-8 batches
```

Excluded.

## `turnip_compute_summary_2.txt`

Clean trial:

```text
1 submitted batch
1 retired batch
4,103,693 active ticks
99.77% average busy
40.79% average RAM wait
```

Included.

## `turnip_compute_summary_3.txt`

Clean trial:

```text
1 batch
4,138,881 active ticks
99.74% average busy
41.08% average RAM wait
```

Included.

## `turnip_compute_summary_4.txt`

Clean trial:

```text
1 batch
4,152,874 active ticks
99.75% average busy
33.59% average RAM wait
```

Included.

## `turnip_compute_summary_5.txt`

Clean trial:

```text
1 batch
4,109,743 active ticks
98.45% average busy
39.06% average RAM wait
```

Included.

---

# Vendor memory trials

## `vendor_mem_summary_1.txt`

Setup-contaminated trial:

```text
4 batches
2 contexts
extra surface/egl-image maps
one main batch around 6.28 million ticks
```

Excluded.

## `vendor_mem_summary_2.txt`

Still contains:

```text
4 batches
2 contexts
extra mapping activity
```

Excluded.

## `vendor_mem_summary_3.txt`

Clean two-batch trial:

```text
one tiny setup batch
one main batch around 6.285 million active ticks
44.68% average RAM wait
```

Included.

## `vendor_mem_summary_4.txt`

Clean two-batch trial:

```text
main batch around 6.283 million active ticks
44.56% average RAM wait
```

Included.

## `vendor_mem_summary_5.txt`

Clean two-batch trial:

```text
main batch around 6.284 million active ticks
44.81% average RAM wait
```

Included.

---

# Turnip memory trials

## `turnip_mem_summary_1.txt`

Setup-contaminated trial:

```text
3 batches
2 contexts
one long main batch around 11.94 million ticks
two short context-8 batches
```

Excluded.

## `turnip_mem_summary_2.txt`

Heavily contaminated trial:

```text
5 batches
2 contexts
one long main batch around 12.03 million ticks
four short context-8 batches
surface/egl-image map and free activity
```

Excluded.

## `turnip_mem_summary_3.txt`

Clean trial:

```text
1 batch
11,950,555 active ticks
68.01% average RAM wait
```

Included.

## `turnip_mem_summary_4.txt`

Clean trial:

```text
1 batch
11,985,968 active ticks
74.69% average RAM wait
```

Included.

## `turnip_mem_summary_5.txt`

Clean trial:

```text
1 batch
11,989,287 active ticks
75.04% average RAM wait
```

Included.

---

# `trial_comparison_summary.txt`

## Purpose

Final steady-state comparison generated from the selected trials.

## Selected results

| Driver | Workload | Trials | Mean batches | Mean active ticks | Mean max active ticks | Busy | RAM wait |
|---|---|---|---:|---:|---:|---:|---:|
| Vendor | ALU | 2–5 | 2.00 | 1,555,063.8 | 3,109,948.2 | 65.61% | 3.37% |
| Turnip | ALU | 2–5 | 1.00 | 4,126,297.8 | 4,126,297.8 | 99.43% | 38.63% |
| Vendor | Memory | 3–5 | 2.00 | 3,141,993.5 | 6,283,810.3 | 80.79% | 44.68% |
| Turnip | Memory | 3–5 | 1.00 | 11,975,270.0 | 11,975,270.0 | 88.54% | 72.58% |

All groups report:

```text
maximum frequency = 1,100,000 kHz
```

---

# Important correction to the reported driver ratios

The historical aggregate reports Turnip/vendor ratios using:

```text
mean active ticks per batch
```

However, selected vendor trials contain two batches and selected Turnip trials
contain one.

The vendor average is lowered by one tiny setup batch.

The historical report gives:

```text
Turnip/vendor ALU avg_active:    2.65×
Turnip/vendor memory avg_active: 3.81×
```

These are mathematically correct ratios of **average batch duration**, but they
are misleading as workload-duration ratios.

Using the dominant/main batch (`max_active`) gives approximately:

```text
Turnip/vendor ALU main-batch ratio:
    4,126,298 / 3,109,948 ≈ 1.33×

Turnip/vendor memory main-batch ratio:
    11,975,270 / 6,283,810 ≈ 1.91×
```

Using estimated total active ticks:

```text
average active × batch count
```

gives nearly the same corrected ratios because the vendor setup batch is tiny.

Therefore:

```text
2.65× and 3.81×:
    average-per-batch ratios

approximately 1.33× and 1.91×:
    better approximations of main/total active-work ratios
```

The original report should remain unchanged as historical output, but this
distinction must be documented when interpreting it.

---

# Main experiment findings

# 1. Memory workloads show much higher RAM wait

Vendor:

```text
compute: 3.37%
memory:  44.68%
```

Turnip:

```text
compute: 38.63%
memory:  72.58%
```

The broad workload classification is visible for both drivers.

# 2. Turnip main batches are longer

Approximate main-batch ratios:

```text
Turnip/vendor compute: 1.33×
Turnip/vendor memory:  1.91×
```

# 3. Command organization differs

Selected vendor trials:

```text
one tiny setup batch
one main workload batch
```

Selected Turnip trials:

```text
one long workload batch
```

# 4. Turnip compute RAM wait is unexpectedly high

Turnip compute reports:

```text
38.63% average RAM wait
```

versus:

```text
3.37% for vendor compute
```

This requires hardware-counter cross-validation.

It does not automatically mean the Turnip compute shader is memory-bound.

# 5. Selected trials are highly stable

The selected active-tick standard deviations are small relative to their
million-tick means.

# 6. Early trials show visible contamination

Extra batches, context 8, surface mappings, and egl-image mappings occur in early
trials.

The exclusion rules are supported by trace structure.

---

# Why this experiment is needed

# 1. Validate workload distinction without direct counters

KGSL RAM-wait and busy data can broadly distinguish compute-heavy and
memory-heavy behavior.

# 2. Compare driver organization

Vendor and Turnip can compile and submit the same broad workload differently.

# 3. Detect setup contamination

Repeated trials reveal which captures contain extra contexts and mappings.

# 4. Provide context for perf counters

Hardware counter differences can be interpreted alongside:

- batch count;
- active time;
- frequency;
- RAM wait; and
- driver identity.

# 5. Preserve the early profiler-development workflow

This experiment records the trace-based investigation used before or alongside
the final direct performance-counter tools.

---

# Toolchain requirements

# Host

```text
Bash or Zsh
ADB
Python 3.9 or newer
grep
awk
sort
tee
diff
```

# Phone

```text
root access or sufficient tracefs permissions
/sys/kernel/tracing
KGSL tracepoints
Vulkan-capable Adreno GPU
vendor Vulkan driver
Turnip deployment for Turnip trials
benchmark binaries and SPIR-V modules
```

# Python dependencies

The parser and aggregator use only the standard library.

No external package installation is required.

---

# Build and push benchmark tools

This experiment directory does not contain benchmark source or build scripts.

Use the canonical benchmark directories.

# Compute workload

```text
benchmarks/microbenchmarks/alu_calibration/
```

# Memory workload

```text
benchmarks/microbenchmarks/memory_stride/
```

Review their READMEs for:

- GLSL compilation;
- SPIR-V validation;
- Android NDK build;
- ADB push;
- runner arguments; and
- verification behavior.

Confirm deployed files:

```bash
adb shell '
  find /data/local/tmp/jerry_work \
    -maxdepth 3 \
    \( -name "vk_compute_probe" \
       -o -name "vk_mem_probe" \
       -o -name "alu.comp.spv" \
       -o -name "mem.comp.spv" \) \
    -print
'
```

---

# Reproduce one vendor compute trial

```bash
cd /Users/jerryyun/adreno-gpu-profiler

bash \
  experiments/2026-05-29_vendor_turnip_trace_comparison/run_focused_kgsl_trace_compute.sh
```

Before running, inspect and update its paths.

Then parse the filtered trace:

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input "$HOME/adreno_turnip/kgsl_focused_trace_compute_filtered.log" \
  > \
  experiments/2026-05-29_vendor_turnip_trace_comparison/vendor_compute_summary_NEW.txt
```

Use a new filename rather than overwriting historical trials.

---

# Reproduce one vendor memory trial

Do not run the historical memory script until its compute-named outputs are
fixed.

After correcting or replacing it:

```bash
bash \
  experiments/2026-05-29_vendor_turnip_trace_comparison/run_focused_kgsl_trace_mem.sh
```

Parse:

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input "$HOME/adreno_turnip/kgsl_focused_trace_mem_filtered.log" \
  > \
  experiments/2026-05-29_vendor_turnip_trace_comparison/vendor_mem_summary_NEW.txt
```

---

# Reproduce Turnip trials

The workload process must receive the Turnip environment.

Do not apply the Turnip variables only to the streamer or only to the host
shell.

They must reach the Vulkan benchmark process launched on the phone.

Confirm driver identity before tracing with a Vulkan probe.

Save the probe output with every new trial.

---

# Rebuild the aggregate

From the experiment directory:

```bash
cd /Users/jerryyun/adreno-gpu-profiler/experiments/2026-05-29_vendor_turnip_trace_comparison

python3 \
  ../../analysis/kgsl_trace_analysis/summarize_trials.py \
  > /tmp/trial_comparison_summary.regenerated.txt
```

Compare:

```bash
diff -u \
  trial_comparison_summary.txt \
  /tmp/trial_comparison_summary.regenerated.txt
```

---

# Verify trial inventory

```bash
cd /Users/jerryyun/adreno-gpu-profiler

find \
  experiments/2026-05-29_vendor_turnip_trace_comparison \
  -maxdepth 1 \
  -type f \
  -print \
  | sort
```

Count numbered trial summaries:

```bash
find \
  experiments/2026-05-29_vendor_turnip_trace_comparison \
  -maxdepth 1 \
  -type f \
  \( -name 'vendor_compute_summary_[1-5].txt' \
     -o -name 'vendor_mem_summary_[1-5].txt' \
     -o -name 'turnip_compute_summary_[1-5].txt' \
     -o -name 'turnip_mem_summary_[1-5].txt' \) \
  | wc -l
```

Expected:

```text
20
```

---

# Compare with immutable evidence copies

The same historical summaries are also preserved under:

```text
evidence/summaries/vendor_turnip_comparison/
```

Check byte identity:

```bash
REPO_ROOT="/Users/jerryyun/adreno-gpu-profiler"
EXP="$REPO_ROOT/experiments/2026-05-29_vendor_turnip_trace_comparison"
EVID="$REPO_ROOT/evidence/summaries/vendor_turnip_comparison"

for FILE in "$EXP"/vendor_compute_summary_[1-5].txt; do
  NAME="$(basename "$FILE")"
  cmp -s "$FILE" "$EVID/vendor/$NAME" \
    && echo "OK   $NAME" \
    || echo "DIFF $NAME"
done

for FILE in "$EXP"/vendor_mem_summary_[1-5].txt; do
  NAME="$(basename "$FILE")"
  cmp -s "$FILE" "$EVID/vendor/$NAME" \
    && echo "OK   $NAME" \
    || echo "DIFF $NAME"
done

for FILE in "$EXP"/turnip_compute_summary_[1-5].txt; do
  NAME="$(basename "$FILE")"
  cmp -s "$FILE" "$EVID/turnip/$NAME" \
    && echo "OK   $NAME" \
    || echo "DIFF $NAME"
done

for FILE in "$EXP"/turnip_mem_summary_[1-5].txt; do
  NAME="$(basename "$FILE")"
  cmp -s "$FILE" "$EVID/turnip/$NAME" \
    && echo "OK   $NAME" \
    || echo "DIFF $NAME"
done
```

Use the experiment directory as the reproducibility bundle and the evidence
directory as the immutable preservation copy.

Do not edit both independently.

---

# Detect contaminated trials automatically

```bash
python3 - <<'PY'
from pathlib import Path
import re

root = Path(
    "experiments/2026-05-29_vendor_turnip_trace_comparison"
)

for path in sorted(root.glob("*_summary_[1-5].txt")):
    match = re.match(
        r"(vendor|turnip)_(compute|mem)_summary_(\d+)\.txt",
        path.name,
    )

    if not match:
        continue

    driver, workload, trial = match.groups()
    text = path.read_text(errors="replace")

    batch_match = re.search(
        r"submitted batches:\s+(\d+)",
        text,
    )

    contexts = re.findall(
        r"^ctx=(\d+):\s+(\d+)$",
        text,
        re.MULTILINE,
    )

    if not batch_match:
        print(f"REVIEW {path.name}: no batch metric")
        continue

    batches = int(batch_match.group(1))
    expected_batches = 2 if driver == "vendor" else 1

    extra_contexts = [
        ctx
        for ctx, count in contexts
        if ctx != "15" and int(count) > 0
    ]

    issues = []

    if batches != expected_batches:
        issues.append(
            f"batches={batches}, expected steady-state={expected_batches}"
        )

    if extra_contexts:
        issues.append(
            f"extra contexts={','.join(extra_contexts)}"
        )

    if issues:
        print(f"REVIEW {path.name}: {'; '.join(issues)}")
PY
```

This is an experiment-specific review heuristic, not a universal correctness
test.

---

# Integrity

The experiment copies should be preserved in Git.

The evidence copies are indexed by:

```text
evidence/manifests/vendor_turnip_comparison_manifest.txt
```

Verify hashes there rather than generating new hashes and replacing the old
manifest.

Check one example:

```bash
shasum -a 256 \
  experiments/2026-05-29_vendor_turnip_trace_comparison/turnip_compute_summary_2.txt
```

The preserved evidence copy has the historical SHA-256:

```text
d5905f67bc04cf21959668fb9501781a93819fe7167e17135d19746c6abaa2a1
```

---

# What the experiment does not prove

The experiment does not prove:

- Turnip is universally slower than the vendor driver;
- the command-batch tick unit equals a specific wall-clock unit;
- RAM wait alone explains runtime;
- the benchmark used identical physical memory placement;
- the shader compiler generated equivalent instruction sequences;
- all early extra contexts are harmless;
- the driver identity was correct for every trial;
- one GPU frequency sample proves constant frequency throughout;
- KGSL memory-event totals equal peak memory use;
- the results generalize to other Adreno generations; or
- the hardware perf-counter mapping is correct.

The results apply to this phone, kernel, benchmark, driver builds, and capture
workflow.

---

# Known limitations

## Raw traces are not stored here

The capture scripts write raw and filtered traces to:

```text
$HOME/adreno_turnip
```

The experiment directory contains summaries but not their exact source traces.

This prevents full offline re-parsing from the repository alone.

## Benchmark configuration is missing

The summary files do not embed workload arguments or hashes.

## Driver identity is missing

The summaries do not record vendor/Turnip probe output.

## Historical script naming bug

The memory script writes compute-named files.

## Fixed filenames overwrite data

Repeated captures require manual renaming.

## No cleanup trap

Tracing can remain enabled after failure.

## No tracepoint-state restoration

Existing trace configuration is destroyed.

## Sparse power samples

Many clean trials contain only three to nine `kgsl_pwrstats` samples.

## Batch-count bias

Average active ticks per batch cannot be compared directly across drivers with
different batch counts.

## Text-based aggregation

The trial aggregator parses human-readable output with regex.

## Hard-coded exclusions

Trial-selection rules are fixed in source.

## No structured metadata

There is no JSON, TOML, or CSV experiment metadata file.

## No timestamps in filenames

Trial chronology and exact capture time are unclear.

## Duplicate preservation locations

The same summaries exist in the experiment and evidence directories.

---

# Recommended maintenance

1. Keep this directory as a historical experiment snapshot.
2. Add this README.
3. Do not overwrite the existing summary files.
4. Fix the memory script’s output names.
5. Add a cleanup trap to both capture scripts.
6. Save benchmark stdout/stderr and exit code.
7. Save Vulkan driver identity for every trial.
8. Save raw and filtered traces inside a dated trial directory.
9. Save benchmark arguments and binary/shader hashes.
10. Save device, kernel, Android, and Mesa/vendor driver versions.
11. Replace fixed output names with driver/workload/trial directories.
12. Replace token-based event counting with colon-delimited event parsing.
13. Record trace-buffer capacity and dropped-event status.
14. Restore previous tracepoint state after capture.
15. Add explicit Turnip environment handling.
16. Generalize `summarize_trials.py` with CLI arguments.
17. Add total active ticks to the aggregate.
18. Report main-batch ratios separately from average-per-batch ratios.
19. Add a machine-readable `trial_metrics.csv`.
20. Keep evidence copies immutable and verify them through the manifest.
21. Cross-reference matching streamer/sweeper captures.
22. Use hardware counters to investigate Turnip compute RAM wait.

---

# Suggested future structure

```text
experiments/2026-05-29_vendor_turnip_trace_comparison/
├── README.md
├── experiment_metadata.json
├── scripts/
│   ├── capture_focused_trace.sh
│   └── aggregate_trials.py
├── vendor/
│   ├── compute/
│   │   ├── trial_01/
│   │   │   ├── trace_raw.log
│   │   │   ├── trace_filtered.log
│   │   │   ├── benchmark.log
│   │   │   ├── event_summary.txt
│   │   │   ├── detailed_summary.txt
│   │   │   └── metadata.json
│   │   └── ...
│   └── memory/
│       └── ...
├── turnip/
│   ├── compute/
│   │   └── ...
│   └── memory/
│       └── ...
└── derived/
    ├── trial_metrics.csv
    ├── trial_comparison_summary.md
    └── included_trials.txt
```

Do not reorganize the historical files without preserving Git history and
verifying hashes.

---

# Quick-reference commands

## Inspect files

```bash
cd /Users/jerryyun/adreno-gpu-profiler

find \
  experiments/2026-05-29_vendor_turnip_trace_comparison \
  -maxdepth 1 \
  -type f \
  -print \
  | sort
```

## Inspect memory capture bug

```bash
grep -nE \
  'RAW_LOG|FILTERED_LOG|SUMMARY|WORKLOAD|compute|mem' \
  experiments/2026-05-29_vendor_turnip_trace_comparison/run_focused_kgsl_trace_mem.sh
```

## Parse one filtered trace

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input /path/to/filtered_trace.log \
  > /tmp/detailed_summary.txt
```

## Regenerate aggregate

```bash
cd /Users/jerryyun/adreno-gpu-profiler/experiments/2026-05-29_vendor_turnip_trace_comparison

python3 \
  ../../analysis/kgsl_trace_analysis/summarize_trials.py \
  > /tmp/trial_comparison_summary.regenerated.txt
```

## Compare aggregate

```bash
diff -u \
  trial_comparison_summary.txt \
  /tmp/trial_comparison_summary.regenerated.txt
```

## Check experiment Git state

```bash
git status --short -- \
  experiments/2026-05-29_vendor_turnip_trace_comparison
```

---

# Quick file summary

```text
run_focused_kgsl_trace_compute.sh
    legacy compute trace-capture wrapper

run_focused_kgsl_trace_mem.sh
    legacy memory trace-capture wrapper
    currently contains compute-named outputs and must be fixed before reuse

summarize_trials.py
    experiment-specific repeated-trial text aggregator

trial_comparison_summary.txt
    final historical steady-state report

vendor_compute_summary_1..5.txt
    five vendor-driver compute trials
    trial 1 excluded

turnip_compute_summary_1..5.txt
    five Turnip compute trials
    trial 1 excluded

vendor_mem_summary_1..5.txt
    five vendor-driver memory trials
    trials 1–2 excluded

turnip_mem_summary_1..5.txt
    five Turnip memory trials
    trials 1–2 excluded
```

---

# Final interpretation

The experiment supports the following conclusions:

```text
1. Memory-heavy workloads have substantially higher KGSL RAM-wait percentages
   than compute-heavy workloads under both drivers.

2. Turnip's main command batches are approximately 1.33× longer for compute and
   1.91× longer for memory than the vendor driver's main batches in the selected
   steady-state trials.

3. The vendor driver uses one tiny setup batch plus one main batch, while Turnip
   uses one long main batch in clean trials.

4. Turnip compute reports unexpectedly high KGSL RAM wait and requires
   hardware-counter cross-validation.
```

The historical report’s 2.65× and 3.81× Turnip/vendor ratios are ratios of
average duration per batch and should not be presented as end-to-end workload
runtime ratios without the batch-count qualification.
