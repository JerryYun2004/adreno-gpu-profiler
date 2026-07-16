# Capture Tools

This directory contains host-side Bash scripts for collecting Qualcomm KGSL traces, GPU busy/frequency sysfs samples, workload logs, and Turnip driver diagnostics from a rooted Android device.

```text
tools/capture/
├── run_focused_kgsl_trace_compute.sh
├── run_focused_kgsl_trace_mem.sh
├── run_focused_kgsl_trace_ui.sh
├── run_kgsl_busy_validation.sh
├── run_kgsl_fast_capture.sh
├── run_kgsl_threeway_capture.sh
├── run_kgsl_trace_ui.sh
└── run_vk_probe_with_turnip.sh
```

These scripts are capture and validation utilities. They are separate from the final raw hardware perf-counter tools:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

## How these tools fit into the profiler workflow

```text
Vulkan workload
    ├── controlled compute, memory, copy, or UI activity
    │
    ├── KGSL/ftrace capture
    │     ├── command-batch lifecycle
    │     ├── power and frequency events
    │     ├── memory events
    │     └── GPU busy events
    │
    ├── KGSL sysfs sampling
    │     ├── busy/load
    │     ├── clock/frequency
    │     └── bus-related values
    │
    └── perf-counter streamer/sweeper
          └── raw Adreno hardware counter deltas
```

The capture scripts provide kernel-level context and validation. They do not replace the streamer or sweeper.

Typical uses include:

- confirming that a benchmark actually submits GPU work;
- locating command-batch start and completion events;
- comparing ALU-heavy, memory-heavy, and copy workloads;
- validating KGSL sysfs busy values against tracepoint data;
- observing frequency and power-state changes;
- comparing vendor Vulkan with Mesa Turnip; and
- explaining unusual perf-counter captures.

---

# Script summary

| Script | Workload | Main purpose |
|---|---|---|
| `run_kgsl_trace_ui.sh` | Android UI swipes | Minimal GPU busy, frequency, and power-level trace. |
| `run_focused_kgsl_trace_ui.sh` | Android UI swipes | Broader focused trace with command, context, memory, power, and frequency events. |
| `run_focused_kgsl_trace_compute.sh` | `vk_compute_probe` | Focused KGSL trace for a compute workload. |
| `run_focused_kgsl_trace_mem.sh` | `vk_mem_probe mem.comp.spv` | Focused KGSL trace for a memory workload. |
| `run_kgsl_fast_capture.sh` | `vk_compute_probe` or `vk_mem_probe` | Repeated workload capture with broad tracepoints and a fast sysfs sampler. |
| `run_kgsl_threeway_capture.sh` | `vk_threeway_probe` | Controlled copy, ALU, or memory capture using one benchmark runner. |
| `run_kgsl_busy_validation.sh` | Compute or memory probe | Compares KGSL sysfs busy signals with KGSL tracepoint activity. |
| `run_vk_probe_with_turnip.sh` | `vk_probe` | Temporarily activates Turnip and verifies which Vulkan driver is loaded. |

---

# Requirements

## Host

- macOS or Linux
- Bash
- Android Debug Bridge (`adb`)
- Python 3
- standard tools such as `grep`, `awk`, `sed`, `sort`, `tee`, and `wc`

Check the connection:

```bash
adb devices
adb shell getprop ro.product.model
```

## Android device

- ARM64 Android device with a Qualcomm Adreno GPU
- root access through `su`
- KGSL tracepoints under:
  ```text
  /sys/kernel/tracing/events/kgsl
  ```
- KGSL sysfs nodes under:
  ```text
  /sys/class/kgsl/kgsl-3d0
  ```
- Vulkan support
- benchmark binaries and shader modules already pushed to the phone

The broad capture scripts also support the older tracefs location:

```text
/sys/kernel/debug/tracing
```

Some focused scripts currently assume `/sys/kernel/tracing` directly.

## Expected phone-side files

Most scripts assume this working directory:

```text
/data/local/tmp/jerry_work
```

Depending on the capture, it may need:

```text
vk_probe
vk_compute_probe
vk_mem_probe
vk_threeway_probe

mem.comp.spv
copy_baseline.comp.spv
alu_heavy.comp.spv
mem_heavy_clean.comp.spv
```

The Turnip probe also expects:

```text
/data/local/tmp/jerry_work/turnip/vulkan.adreno.so
```

The broad fast-capture scripts expect a sampler at:

```text
/data/local/tmp/kgsl_fast_sampler.sh
```

---

# Initial setup

Make the host scripts executable:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

chmod +x tools/capture/*.sh
```

Confirm root access:

```bash
adb shell 'su -c id'
```

Confirm tracefs:

```bash
adb shell 'su -c "
TR=/sys/kernel/tracing
[ -d \$TR/events ] || TR=/sys/kernel/debug/tracing
echo \$TR
ls \$TR/events/kgsl | head
"'
```

Confirm benchmark files:

```bash
adb shell \
  'ls -lh /data/local/tmp/jerry_work'
```

---

# Script reference

## `run_kgsl_trace_ui.sh`

### Purpose

Runs a small Android UI workload using ADB swipe commands while tracing three KGSL events:

```text
kgsl_gpubusy
gpu_frequency
kgsl_pwrlevel
```

It then parses:

- per-sample GPU busy percentages; and
- minimum and maximum reported GPU frequencies.

### Run

```bash
./tools/capture/run_kgsl_trace_ui.sh
```

### Expected output

```text
$HOME/adreno_turnip/kgsl_trace_adb_ui.log
$HOME/adreno_turnip/kgsl_trace_adb_ui_filtered.log
```

A Python block embedded in the script prints a basic summary to the terminal.

### Role

This is the smallest UI trace script. Use it for a quick check that KGSL busy and frequency tracepoints respond to display/UI activity.

For a broader UI trace, use `run_focused_kgsl_trace_ui.sh`.

---

## `run_focused_kgsl_trace_ui.sh`

### Purpose

Generates the same kind of UI activity, but enables a broader set of focused KGSL events:

```text
command-batch lifecycle
context creation and destruction
draw-context switching
memory allocation and mapping
GPU busy and power statistics
frequency and bus level
clock and rail changes
power-state requests
```

### Run

```bash
./tools/capture/run_focused_kgsl_trace_ui.sh
```

### Expected output

```text
$HOME/adreno_turnip/kgsl_focused_trace_ui.log
$HOME/adreno_turnip/kgsl_focused_trace_ui_filtered.log
$HOME/adreno_turnip/kgsl_focused_trace_ui_summary.txt
```

The summary counts each captured event type.

### Role

Use this script when validating the trace pipeline with normal Android UI rendering rather than a custom Vulkan benchmark.

---

## `run_focused_kgsl_trace_compute.sh`

### Purpose

Captures a focused KGSL trace while running:

```text
/data/local/tmp/jerry_work/vk_compute_probe
```

The enabled events cover:

- command-batch queue, ready, submit, done, and retire stages;
- issue-IB commands;
- GPU busy and power statistics;
- power level, bus level, and frequency;
- memory allocation, mapping, freeing, and cache synchronization;
- context creation; and
- power-state requests.

### Run

```bash
./tools/capture/run_focused_kgsl_trace_compute.sh
```

### Expected output

```text
$HOME/adreno_turnip/kgsl_focused_trace_compute.log
$HOME/adreno_turnip/kgsl_focused_trace_compute_filtered.log
$HOME/adreno_turnip/kgsl_focused_trace_compute_summary.txt
```

### Role

Use this script to inspect the kernel-side lifecycle of a compute workload before or alongside a focused hardware perf-counter capture.

---

## `run_focused_kgsl_trace_mem.sh`

### Purpose

Uses the same focused tracepoint set as the compute script, but runs:

```text
./vk_mem_probe mem.comp.spv
```

from:

```text
/data/local/tmp/jerry_work
```

### Run

```bash
./tools/capture/run_focused_kgsl_trace_mem.sh
```

### Expected output

The current script writes to the same filenames as the compute version:

```text
$HOME/adreno_turnip/kgsl_focused_trace_compute.log
$HOME/adreno_turnip/kgsl_focused_trace_compute_filtered.log
$HOME/adreno_turnip/kgsl_focused_trace_compute_summary.txt
```

### Important limitation

Because the memory and compute variants currently use the same output names, one run can overwrite the other.

Recommended memory-specific names are:

```text
kgsl_focused_trace_mem.log
kgsl_focused_trace_mem_filtered.log
kgsl_focused_trace_mem_summary.txt
```

### Role

Use this script to compare the command, memory, power, and frequency behavior of the memory probe against the compute probe.

---

## `run_kgsl_fast_capture.sh`

### Purpose

Runs a selected benchmark repeatedly while collecting:

1. all available KGSL tracepoints;
2. available `gpu_mem` tracepoints;
3. `power/gpu_work_period`, when present;
4. workload stdout and stderr;
5. fast KGSL/sysfs samples; and
6. metadata describing the run.

### Syntax

```text
run_kgsl_fast_capture.sh compute|mem [repeat_count] [sample_interval_s]
```

Defaults:

```text
repeat_count=30
sample_interval_s=0.005
```

### Examples

```bash
./tools/capture/run_kgsl_fast_capture.sh compute
```

```bash
./tools/capture/run_kgsl_fast_capture.sh mem 20 0.01
```

### Workloads

Compute:

```text
./vk_compute_probe
```

Memory:

```text
./vk_mem_probe
```

Both run from:

```text
/data/local/tmp/jerry_work
```

### Expected output

The script creates a timestamped directory under:

```text
/Users/jerryyun/adreno_turnip/kgsl_full_capture/
```

Example:

```text
<timestamp>_vendor_compute_repeat30_fast_full_clean/
├── metadata/
│   ├── benchmark_config.txt
│   └── tracepoints_enabled_state.csv
├── parsed/
└── raw/
    ├── kgsl_fast_sampler_stderr.log
    ├── sysfs_fast_samples.csv
    ├── trace_raw.log
    ├── workload_stderr.log
    └── workload_stdout.log
```

The `parsed/` directory is created but not populated by this script.

### Role

This is a broad repeat-capture tool for collecting enough kernel and sysfs context to compare workload classes and validate later analysis.

---

## `run_kgsl_threeway_capture.sh`

### Purpose

Captures one of three controlled workloads using `vk_threeway_probe`:

```text
copy
alu
mem
```

The same runner, element count, and dispatch-repeat structure are used to make the workload classes easier to compare.

### Syntax

```text
run_kgsl_threeway_capture.sh copy|alu|mem [repeat_count] [sample_interval_s]
```

Defaults:

```text
repeat_count=30
sample_interval_s=0.005
```

### Examples

```bash
./tools/capture/run_kgsl_threeway_capture.sh copy
```

```bash
./tools/capture/run_kgsl_threeway_capture.sh alu 30 0.005
```

```bash
./tools/capture/run_kgsl_threeway_capture.sh mem 30 0.005
```

### Workload commands

Copy baseline:

```text
./vk_threeway_probe copy copy_baseline.comp.spv 262144 1 64
```

ALU-heavy:

```text
./vk_threeway_probe alu alu_heavy.comp.spv 262144 2048 64
```

Memory-heavy:

```text
./vk_threeway_probe mem mem_heavy_clean.comp.spv 262144 512 64
```

### Expected output

The directory structure is the same as `run_kgsl_fast_capture.sh`, with a mode-specific timestamped run name.

Metadata records:

- mode;
- workload type;
- repeat count;
- element count;
- dispatch repeats;
- sampling interval;
- driver; and
- exact benchmark command.

### Role

This is the best script in this directory for controlled copy-versus-ALU-versus-memory KGSL comparisons.

It complements the raw perf-counter streamer and sweeper by providing command, busy, power, memory, and frequency context for the same workload classes.

---

## `run_kgsl_busy_validation.sh`

### Purpose

Validates KGSL sysfs busy values against KGSL tracepoint activity for either:

```text
compute
mem
```

It creates and pushes a dedicated device-side sysfs sampler, captures focused tracepoints, runs the workload, pulls the data, and invokes a Python analyzer when available.

### Syntax

```text
run_kgsl_busy_validation.sh compute|mem
```

### Optional environment variables

```text
SAMPLE_INTERVAL_S
WORKLOAD_REPEAT
POST_SAMPLE_S
OUT_ROOT
```

Example:

```bash
SAMPLE_INTERVAL_S=0.02 \
WORKLOAD_REPEAT=5 \
POST_SAMPLE_S=1.0 \
./tools/capture/run_kgsl_busy_validation.sh compute
```

### Expected output

Default location:

```text
$HOME/adreno-gpu-profiler/results/kgsl_busy_validation/<timestamp>_<kind>/
```

Files include:

```text
kgsl_trace_raw.log
kgsl_trace_filtered.log
kgsl_sysfs_busy_samples.csv
kgsl_sysfs_sampler_device.log
workload_stdout.log
validation_summary.txt
```

The analyzer may also generate additional CSV files or plots.

### Analyzer dependency

The script currently searches beside itself for:

```text
tools/capture/analyze_kgsl_busy_validation.py
```

After the repository reorganization, the analyzer is stored under:

```text
analysis/kgsl_trace_analysis/analyze_kgsl_busy_validation.py
```

Therefore, the current script may print:

```text
Analyzer not found
```

until its analyzer path is updated.

A repository-relative approach would be:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
ANALYZER="$REPO_ROOT/analysis/kgsl_trace_analysis/analyze_kgsl_busy_validation.py"
```

### Role

This is the most direct validation script for determining whether the lightweight KGSL sysfs busy metrics agree with kernel trace evidence.

---

## `run_vk_probe_with_turnip.sh`

### Purpose

Temporarily bind-mounts a Turnip Vulkan HAL over the vendor Adreno Vulkan HAL, runs `vk_probe`, collects driver information and logcat diagnostics, and then restores the vendor HAL.

### Expected phone files

```text
/data/local/tmp/jerry_work/vk_probe
/data/local/tmp/jerry_work/turnip/vulkan.adreno.so
```

Vendor HAL path:

```text
/vendor/lib64/hw/vulkan.adreno.so
```

### Run

```bash
./tools/capture/run_vk_probe_with_turnip.sh
```

### Expected output

```text
$HOME/adreno_turnip/turnip_probe_latest.log
$HOME/adreno_turnip/turnip_logcat_latest.txt
```

The script also prints:

- vendor driver information before the mount;
- the active bind mount;
- Turnip/Mesa driver information;
- relevant Vulkan, linker, KGSL, and SELinux logcat lines; and
- vendor driver information after cleanup.

### Cleanup behavior

A shell trap attempts to unmount the Turnip HAL when the script exits:

```text
umount /vendor/lib64/hw/vulkan.adreno.so
```

The script also performs cleanup explicitly before its final vendor-driver verification.

### Role

This is an optional driver-validation tool. It is not needed for normal streamer or sweeper operation with the vendor Vulkan driver.

---

# Compute shaders and benchmark runners

There are no `.comp` files in `tools/capture/`. The capture scripts use benchmark artifacts stored and built elsewhere in the repository.

Typical pipeline:

```text
shader.comp
    ↓ glslangValidator
shader.comp.spv
    ↓ loaded by
Vulkan benchmark runner
    ↓ traced by
tools/capture/*.sh
```

Compile a shader:

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  path/to/shader.comp \
  -o path/to/shader.comp.spv
```

Validate it:

```bash
spirv-val \
  --target-env vulkan1.1 \
  path/to/shader.comp.spv
```

Push the runner and shader:

```bash
adb push \
  path/to/runner \
  path/to/shader.comp.spv \
  /data/local/tmp/jerry_work/

adb shell \
  'chmod 755 /data/local/tmp/jerry_work/runner'
```

Always run the workload directly before tracing it:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./runner shader.comp.spv'
```

Confirm that:

- the workload exits successfully;
- verification passes, when implemented;
- the correct SPIR-V file is loaded; and
- the workload lasts long enough to produce useful trace activity.

---

# Relationship to the streamer and sweeper

## Streamer

Use `adreno_perf_stream` when only a small number of known hardware counters are needed at high temporal resolution.

The capture scripts add:

- command-submission timing;
- power-state information;
- frequency changes;
- sysfs busy/load values; and
- workload logs.

## Sweeper

Use `streamer_sweeper` to discover which counters respond across many Adreno groups and countables.

The capture scripts are normally run separately because:

- enabling many tracepoints adds overhead;
- broad sysfs sampling adds overhead;
- mixing every capture mechanism into each sweeper chunk would reduce reproducibility; and
- the sweeper reruns the workload many times.

A practical workflow is:

1. validate the benchmark with a focused trace;
2. run the hardware counter sweep without broad tracing;
3. identify useful counters;
4. run a focused streamer capture;
5. collect one KGSL trace capture for context; and
6. compare the active windows during analysis.

---

# Important compatibility issues

## Fast sampler interface mismatch

`run_kgsl_fast_capture.sh` and `run_kgsl_threeway_capture.sh` currently start the sampler as:

```bash
/data/local/tmp/kgsl_fast_sampler.sh "$SAMPLE_INTERVAL"
```

They then stop it by creating:

```text
/data/local/tmp/kgsl_fast_sampler.stop
```

and expect output at:

```text
/data/local/tmp/kgsl_fast_samples.csv
```

The current `scripts/kgsl_fast_sampler.sh` uses a different interface:

```text
kgsl_fast_sampler.sh [output_csv] [interval_seconds] [duration_seconds]
```

It does not use the stop flag.

Therefore, the current broad capture scripts and the current sampler are not directly compatible.

Before using these captures, either:

- restore the legacy stop-flag sampler expected by the capture scripts; or
- update the capture scripts to pass output, interval, and duration explicitly.

Do not assume a capture is valid until `sysfs_fast_samples.csv` contains the expected header and multiple data rows.

## Hardcoded host output paths

Several scripts write outside the repository:

```text
$HOME/adreno_turnip
/Users/jerryyun/adreno_turnip/kgsl_full_capture
```

For repository-local results, consider changing them to:

```text
/Users/jerryyun/adreno-gpu-profiler/results/kgsl_trace_capture
```

or using a configurable environment variable such as:

```bash
OUT_ROOT="${OUT_ROOT:-$REPO_ROOT/results/kgsl_trace_capture}"
```

## Shared output filenames

The focused compute and memory scripts currently write to the same `compute`-named files. Rename the memory outputs to avoid overwriting compute captures.

## Tracepoint availability

Not every kernel exposes every named event. The broad scripts ignore missing events, while the focused scripts check or suppress errors for most events.

Always record the enabled state when interpreting missing trace lines.

## Persistent event enables

Most scripts stop tracing but do not disable every event at the end. A later script usually disables old KGSL events during setup, but manual captures should still start from a known clean state.

---

# Recommended use

## Quick UI validation

```bash
./tools/capture/run_kgsl_trace_ui.sh
```

## Detailed UI trace

```bash
./tools/capture/run_focused_kgsl_trace_ui.sh
```

## Focused compute trace

```bash
./tools/capture/run_focused_kgsl_trace_compute.sh
```

## Focused memory trace

```bash
./tools/capture/run_focused_kgsl_trace_mem.sh
```

## Controlled three-way comparison

```bash
./tools/capture/run_kgsl_threeway_capture.sh copy
./tools/capture/run_kgsl_threeway_capture.sh alu
./tools/capture/run_kgsl_threeway_capture.sh mem
```

## Busy-node validation

```bash
WORKLOAD_REPEAT=5 \
SAMPLE_INTERVAL_S=0.02 \
./tools/capture/run_kgsl_busy_validation.sh compute
```

## Turnip driver check

```bash
./tools/capture/run_vk_probe_with_turnip.sh
```

---

# Expected results

A successful trace capture should contain:

- non-empty raw trace data;
- workload stdout and stderr;
- matching benchmark process names in trace lines;
- command-batch events around the workload;
- busy/frequency events during active intervals;
- a successful workload return code;
- verification success where implemented; and
- metadata sufficient to reproduce the run.

A successful sysfs capture should contain:

- a CSV header;
- multiple timestamped rows;
- non-empty busy or load values;
- frequency values during the workload; and
- timestamps that overlap the trace and workload interval.

A successful Turnip probe should show:

- vendor driver information before the bind mount;
- Mesa, Turnip, or Freedreno identifiers during the mounted run; and
- vendor driver information again after cleanup.

---

# Safety and reproducibility notes

- Use these scripts only on the intended development device.
- Keep the phone connected throughout the capture.
- Do not disconnect the device during the Turnip bind-mount test.
- Verify that the vendor HAL is restored after any interrupted Turnip run.
- Record device model, Android build, kernel, GPU, Vulkan driver, benchmark commit, shader hash, and script commit.
- Control temperature and run order when comparing workloads.
- Broad tracepoint and sysfs captures add overhead; use them for validation and context rather than final low-overhead performance measurements.
