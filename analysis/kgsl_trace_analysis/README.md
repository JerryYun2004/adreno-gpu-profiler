# KGSL Trace Analysis Utilities

This directory contains offline analysis scripts for GPU telemetry collected from
Qualcomm's KGSL driver, Linux `tracefs`/ftrace, and Android GPU-related `sysfs`
nodes.

These scripts are **not part of the runtime path** of the main perf-counter tools:

- `tools/profiling/perfcounter_streamer/`
- `tools/profiling/perfcounter_sweeper/`

The streamer and sweeper read Adreno hardware performance counters through KGSL
perf-counter interfaces. The scripts in this directory instead analyze
kernel/driver-level signals such as command-batch timing, GPU busy time, RAM wait,
frequency, bus activity, and KGSL memory events.

They remain useful as:

1. a fallback when direct hardware performance counters are unavailable;
2. an independent cross-check for streamer/sweeper results;
3. a way to study driver, power, memory, and scheduling behavior that hardware
   counters alone do not explain; and
4. a record of the earlier KGSL/tracefs profiling workflow.

## Status and relationship to the main profiler

| Script | Status | Role | Direct dependency on streamer/sweeper |
|---|---|---|---|
| `aggregate_threeway_kgsl.py` | Reusable auxiliary tool | Compares copy, ALU-heavy, and memory-heavy capture runs | None |
| `compare_kgsl_runs.py` | Reusable auxiliary tool | Pairwise comparison of two capture runs | None |
| `analyze_kgsl_busy_validation.py` | Validation utility | Aligns tracepoint and sysfs busy samples and checks agreement | None |
| `parse_focused_kgsl_trace.py` | Legacy but useful | Parses one focused trace and prints a detailed text summary | None |
| `summarize_trials.py` | Legacy and experiment-specific | Aggregates repeated vendor/Turnip text summaries | None; consumes output from `parse_focused_kgsl_trace.py` |

“Legacy” here does not mean that the files are unusable. It means they belong to
the earlier KGSL trace/sysfs profiling path and are not required to build or run
the current perf-counter streamer or sweeper.

---

## Requirements

### Host-side requirements

- Python 3.9 or newer
- A POSIX-like shell such as Bash or Zsh
- No third-party Python packages

All five scripts use only the Python standard library. No `pip install` step is
required.

Check the Python version:

```bash
python3 --version
```

### Capture-side requirements

The scripts do not collect data themselves. Their inputs normally come from an
Android capture workflow using:

- `adb`;
- root access or sufficient tracefs/sysfs permissions;
- Qualcomm KGSL/Adreno tracepoints;
- Linux ftrace/tracefs logs; and
- sampled GPU sysfs nodes.

Typical trace events used by the scripts include:

- `adreno_cmdbatch_queued`
- `adreno_cmdbatch_submitted`
- `adreno_cmdbatch_retired`
- `kgsl_pwrstats`
- `kgsl_gpubusy`
- `gpu_work_period`
- `gpu_frequency`
- `kgsl_pwrlevel`
- `kgsl_buslevel`
- `kgsl_mem_alloc`
- `kgsl_mem_map` or `kgsl_mem_mmap`
- `kgsl_mem_free`

The exact tracepoint names and line formats can vary across kernels. These
parsers are tailored to the formats used by the project’s Android 15 / KGSL
capture environment.

---

## Expected capture directory layout

`aggregate_threeway_kgsl.py` and `compare_kgsl_runs.py` expect each capture run
to have this structure:

```text
<run-directory>/
├── metadata/
│   ├── benchmark_config.txt
│   └── tracepoints_enabled_state.csv
└── raw/
    ├── sysfs_fast_samples.csv
    ├── trace_raw.log
    ├── workload_stdout.log
    └── workload_stderr.log
```

Missing optional files are generally reported as empty or `NA`, but the run
directory itself must exist.

### `benchmark_config.txt`

This is parsed as simple `KEY=VALUE` lines. For example:

```text
benchmark=vk_threeway_probe
workload_type=alu
repeat_count=30
command=/data/local/tmp/jerry_work/vk_threeway_probe alu
```

### `tracepoints_enabled_state.csv`

The scripts count lines beginning with `1,` as enabled tracepoints.

### `sysfs_fast_samples.csv`

There are two historical sysfs schemas in these scripts.

The aggregate and comparison scripts look for columns such as:

```text
gpu_busy_percentage
gpu_load
cur_freq
target_freq
gpuclk
clock_mhz
cur_ab
busmon_cur_freq
thermal_pwrlevel
throttling
```

The busy-validation script expects timestamped columns such as:

```text
t_s
gpu_busy_percentage
gpubusy_busy
gpubusy_elapsed
gpubusy_pct
devfreq_gpu_load
cur_freq_hz
gpuclk_hz
```

Check the CSV header before choosing a script:

```bash
head -n 1 /path/to/run/raw/sysfs_fast_samples.csv
```

---

# Script reference

## 1. `aggregate_threeway_kgsl.py`

### Purpose

Aggregates three controlled captures:

- a copy or low-compute baseline;
- an ALU-heavy workload; and
- a memory-heavy workload.

It parses capture metadata, workload verification logs, sysfs samples, and KGSL
trace events. It then produces a side-by-side comparison and pairwise percentage
changes.

### Why it is useful

Use this script when validating that the KGSL/sysfs telemetry can distinguish
three broad workload classes. It is particularly useful for checking whether:

- the copy baseline has the lowest activity;
- the ALU workload has high GPU busy time but relatively low RAM wait;
- the memory workload has elevated RAM wait or bus activity; and
- differences are caused by runtime behavior rather than different command-batch
  counts or allocation structure.

This is the most directly reusable script for the project’s three-way
copy/ALU/memory experiment.

### Relation to streamer and sweeper

It does not call, import, or consume data directly from the perf-counter
streamer or sweeper. It is an independent validation path.

It can complement the main tools. For example, a sweeper result showing high
UCHE or memory-related counter activity can be compared with KGSL RAM-wait and
bus-level signals from the same workload class.

### Inputs

```text
--copy     Copy-baseline capture directory
--alu      ALU-heavy capture directory
--mem      Memory-heavy capture directory
--out-dir  Output directory
```

### Command

Run from the repository root:

```bash
python3 analysis/kgsl_trace_analysis/aggregate_threeway_kgsl.py \
  --copy results/kgsl_runs/copy_run \
  --alu results/kgsl_runs/alu_run \
  --mem results/kgsl_runs/mem_run \
  --out-dir results/kgsl_analysis/threeway
```

### Outputs

```text
aggregate_summary.txt
aggregate_key_metrics.csv
aggregate_all_metrics.csv
```

### Main mechanisms and libraries

- `argparse` for command-line arguments
- `pathlib.Path` for path handling
- `csv.DictReader` and `csv.writer` for CSV input/output
- regular expressions through `re`
- arithmetic means through `statistics`
- event counting from trace log lines
- weighted overall busy and RAM-wait percentages:
  - overall busy = `sum(busy) / sum(total) × 100`
  - overall RAM wait = `sum(ram_wait) / sum(ram_time) × 100`
- pairwise percentage-difference calculations

---

## 2. `compare_kgsl_runs.py`

### Purpose

Compares any two KGSL/sysfs capture directories, commonly an ALU-heavy run and
a memory-heavy run.

It extracts the same broad classes of information as the three-way aggregator:

- workload verification;
- tracepoint counts;
- command-batch counts and active time;
- GPU work-period duration;
- KGSL memory allocations;
- power and RAM-wait statistics;
- sysfs GPU load, frequency, bandwidth, and throttling indicators.

### Why it is useful

Use it for focused A/B comparisons, including:

- ALU-heavy versus memory-heavy;
- vendor driver versus Turnip;
- before versus after a code change;
- one shader version versus another; or
- two capture configurations.

It is simpler than the three-way aggregator when only two runs matter.

### Relation to streamer and sweeper

This script is independent of the main tools. It does not parse streamer or
sweeper CSV files.

Use it alongside the main tools when kernel-level behavior may explain a
hardware-counter result, such as frequency scaling, bus pressure, RAM wait,
thermal throttling, or different command submission behavior.

### Inputs

```text
--a         First capture directory
--b         Second capture directory
--a-label   Display label for run A
--b-label   Display label for run B
--out-dir   Optional output directory
```

### Command

```bash
python3 analysis/kgsl_trace_analysis/compare_kgsl_runs.py \
  --a results/kgsl_runs/alu_run \
  --b results/kgsl_runs/mem_run \
  --a-label alu \
  --b-label mem \
  --out-dir results/kgsl_analysis/alu_vs_mem
```

When `--out-dir` is omitted, the script creates `parsed_compare/` beneath the
common parent of the two run directories.

### Outputs

```text
profile_summary_<A-label>.txt
profile_summary_<B-label>.txt
compare_summary.txt
compare_summary.csv
```

### Main mechanisms and libraries

- standard-library CSV and text parsing
- regex extraction of numeric fields from trace events
- summary statistics using `statistics.mean`
- percentage comparison of run B relative to run A
- defensive handling of absent files and missing values

### Known limitation

The `important_keys` list currently contains a duplicated block of
`kgsl_pwrstats` fields. This can cause those fields to appear twice in each
per-run text summary, although the underlying values and comparison results are
not changed.

---

## 3. `analyze_kgsl_busy_validation.py`

### Purpose

Validates whether GPU busy values reported by tracepoints agree directionally
with sampled sysfs values.

It:

1. parses timestamps and GPU events from a filtered KGSL trace;
2. identifies the workload window from Vulkan command-batch activity;
3. filters trace and sysfs samples to that window;
4. aligns each `kgsl_pwrstats` sample with the nearest sysfs sample;
5. computes average busy values, absolute/relative error, and Pearson
   correlation; and
6. writes the aligned sample pairs to CSV.

### Why it is useful

Use this script when deciding whether a sysfs busy node is trustworthy enough
for coarse workload characterization.

It is especially useful when:

- direct performance counters are blocked;
- sysfs and tracefs report noticeably different busy percentages;
- a capture window contains idle time before or after the workload;
- sampling intervals differ; or
- you need an independent sanity check before interpreting KGSL busy values.

### Relation to streamer and sweeper

It is a validation tool, not part of the streamer/sweeper pipeline.

The streamer and sweeper expose hardware counter deltas. This script checks
coarser kernel-level utilization measurements. Agreement strengthens confidence;
disagreement may indicate different averaging windows, sparse sysfs updates, or
incorrect workload-window selection.

### Inputs

```text
--trace             Filtered KGSL trace log
--sysfs             Timestamped sysfs CSV
--out-dir           Output directory
--workload-kind     Descriptive label, such as compute or mem
--align-max-dt-s    Maximum timestamp separation for an aligned pair
```

### Command

```bash
python3 analysis/kgsl_trace_analysis/analyze_kgsl_busy_validation.py \
  --trace results/kgsl_runs/alu_run/raw/trace_raw.log \
  --sysfs results/kgsl_runs/alu_run/raw/sysfs_fast_samples.csv \
  --out-dir results/kgsl_analysis/alu_busy_validation \
  --workload-kind compute \
  --align-max-dt-s 0.15 \
  | tee results/kgsl_analysis/alu_busy_validation/validation_summary.txt
```

### Output

```text
aligned_trace_vs_sysfs.csv
```

The main analysis summary is printed to standard output, so use `tee` or output
redirection to save it.

### Main mechanisms and libraries

- timestamp extraction with regular expressions
- context-aware workload-window inference
- nearest-neighbor timestamp alignment
- average and error calculations
- Pearson correlation implemented with the standard library
- the following busy definitions:
  - `kgsl_pwrstats`: `busy / total × 100`
  - `kgsl_gpubusy`: `busy / elapsed × 100`
  - RAM wait: `ram_wait / ram_time × 100`

### Known limitations

- Workload-context detection looks specifically for process strings containing
  `vk_compute_prob` or `vk_mem_probe`.
- A trace from a differently named executable may fall back to all command
  batches rather than isolating the intended context.
- Nearest-neighbor alignment does not interpolate samples.
- Correlation can be undefined when there are too few aligned pairs or one
  signal is nearly constant.
- The script expects the timestamped historical sysfs CSV schema described
  earlier.

---

## 4. `parse_focused_kgsl_trace.py`

### Purpose

Parses one filtered KGSL trace log and prints a detailed human-readable summary.

It reports:

- counts of each trace event;
- submitted and retired command batches;
- active command-batch duration;
- queue-to-start delay;
- GMU retirement latency;
- per-context batch counts;
- GPU busy and RAM-wait percentages;
- GPU frequency and power-level changes;
- bus bandwidth;
- memory allocation/map/free activity by usage; and
- the longest active command batches.

### Status

This is a legacy foundational parser from the earlier focused-trace workflow.
It remains useful for manually inspecting a trace or generating the text
summaries consumed by `summarize_trials.py`.

It is not needed by the current perf-counter streamer or sweeper.

### Why it is useful

Use it when:

- investigating one trace in detail;
- checking whether expected tracepoints were captured;
- finding unusually long command batches;
- comparing command scheduling or GMU latency;
- examining KGSL memory usage categories; or
- reproducing the earlier vendor-versus-Turnip analysis.

### Input

```text
--input  Path to a filtered KGSL trace log
```

### Command

```bash
python3 analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/vendor_alu/raw/trace_raw.log
```

Save the printed summary:

```bash
python3 analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/vendor_alu/raw/trace_raw.log \
  | tee results/kgsl_analysis/vendor_compute_summary_1.txt
```

### Output

The script writes its report to standard output. It does not create a file
unless the shell redirects the output.

### Main mechanisms and libraries

- `collections.defaultdict` for event and memory aggregation
- regex parsers for each tracepoint format
- derived command-batch timing:
  - queue-to-start = `start - submitted_to_rb`
  - GMU latency = `retired_on_gmu - retire`
- per-context grouping
- memory-byte aggregation by KGSL usage label
- sorting to identify longest-running command batches

### Known limitations

- The default input path points to an old local directory and should normally be
  overridden with `--input`.
- Parsing depends on exact trace-line field names and order.
- It processes the complete log in memory.
- It does not isolate a workload time window.
- It prints zero rather than `NA` for some missing percentage data.

---

## 5. `summarize_trials.py`

### Purpose

Aggregates multiple text summaries produced by
`parse_focused_kgsl_trace.py`.

It compares four fixed experiment groups:

- vendor / ALU;
- Turnip / ALU;
- vendor / memory; and
- Turnip / memory.

For each group, it calculates means and sample standard deviations, then prints
useful memory-to-ALU and Turnip-to-vendor ratios.

### Status

This is the most experiment-specific legacy script in the directory.

It is tied to fixed filenames, fixed driver/workload labels, and fixed trial
exclusion rules. Keep it for reproducibility of the original repeated-trial
study. For new experiments, the script should be generalized before being
treated as a reusable tool.

### Why it is useful

Use it to reproduce the original steady-state comparison after generating
multiple per-trial summaries.

It intentionally excludes early trials to reduce warm-up/setup effects:

- ALU groups use trials 2 and later;
- memory groups use trials 3 and later.

### Expected filenames

Run it in a directory containing files such as:

```text
vendor_compute_summary_1.txt
vendor_compute_summary_2.txt
...
turnip_compute_summary_1.txt
...
vendor_mem_summary_1.txt
...
turnip_mem_summary_1.txt
...
```

### Example workflow

Generate individual summaries:

```bash
mkdir -p results/kgsl_analysis/repeated_trials
```

```bash
python3 analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/vendor_alu_trial_1/raw/trace_raw.log \
  > results/kgsl_analysis/repeated_trials/vendor_compute_summary_1.txt
```

Repeat for all vendor/Turnip and ALU/memory trials.

Then run the aggregation from the summary directory:

```bash
cd results/kgsl_analysis/repeated_trials

python3 ../../../analysis/kgsl_trace_analysis/summarize_trials.py \
  | tee steady_state_summary.txt
```

A path-independent alternative is:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT/results/kgsl_analysis/repeated_trials"

python3 "$REPO_ROOT/analysis/kgsl_trace_analysis/summarize_trials.py" \
  | tee steady_state_summary.txt
```

### Output

The script prints:

- a compact steady-state table;
- detailed mean ± standard deviation values; and
- useful workload/driver ratios.

### Main mechanisms and libraries

- filename matching with `Path.glob`
- trial-number extraction with regex
- metric extraction from human-readable text summaries
- `statistics.mean`
- `statistics.stdev`
- fixed experiment grouping and warm-up exclusion

### Known limitations

- Input filename patterns are hard-coded.
- Trial cutoffs are hard-coded.
- The script has no command-line arguments.
- It must be launched from the directory containing the summary files.
- It depends on the exact text output format of `parse_focused_kgsl_trace.py`.
- It reads the first match for duplicate metric names, assuming the first
  `avg_busy_pct` belongs to the `kgsl_pwrstats` section.
- A zero ALU RAM-wait mean can cause division by zero in the printed ratio
  calculation.

---

# Recommended workflows

## A. Pairwise ALU-versus-memory analysis

```bash
python3 analysis/kgsl_trace_analysis/compare_kgsl_runs.py \
  --a results/kgsl_runs/alu_run \
  --b results/kgsl_runs/mem_run \
  --a-label alu \
  --b-label mem \
  --out-dir results/kgsl_analysis/alu_vs_mem
```

Use this for a quick A/B comparison.

## B. Three-way workload validation

```bash
python3 analysis/kgsl_trace_analysis/aggregate_threeway_kgsl.py \
  --copy results/kgsl_runs/copy_run \
  --alu results/kgsl_runs/alu_run \
  --mem results/kgsl_runs/mem_run \
  --out-dir results/kgsl_analysis/threeway
```

Use this when validating the broad behavior of copy, compute, and memory
workloads.

## C. Busy-signal cross-validation

```bash
python3 analysis/kgsl_trace_analysis/analyze_kgsl_busy_validation.py \
  --trace results/kgsl_runs/mem_run/raw/trace_raw.log \
  --sysfs results/kgsl_runs/mem_run/raw/sysfs_fast_samples.csv \
  --out-dir results/kgsl_analysis/mem_busy_validation \
  --workload-kind mem \
  | tee results/kgsl_analysis/mem_busy_validation/validation_summary.txt
```

Use this before relying on sysfs busy measurements.

## D. Detailed single-trace inspection

```bash
python3 analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/mem_run/raw/trace_raw.log \
  | less
```

Use this for debugging individual traces.

## E. Historical repeated-trial reproduction

1. Run `parse_focused_kgsl_trace.py` on every trial.
2. Name the outputs according to the expected vendor/Turnip patterns.
3. Run `summarize_trials.py` from that output directory.

---

# How these tools complement hardware perf counters

KGSL trace/sysfs signals and hardware perf counters answer different questions.

| Question | KGSL trace/sysfs tools | Perf-counter streamer/sweeper |
|---|---:|---:|
| Was the GPU broadly busy? | Yes | Yes, indirectly |
| Was the GPU waiting on RAM? | Yes, through coarse KGSL RAM-wait fields | Yes, through detailed memory/cache counters |
| Which GPU block was active? | Limited | Yes |
| How many command batches were submitted? | Yes | No |
| Was frequency or bus demand changing? | Yes | Limited |
| Were allocations or driver contexts changing? | Yes | No |
| Can individual Adreno countables be swept? | No | Yes |
| Can results be collected when detailed counter reads are blocked? | Often yes | No |

The KGSL scripts should therefore be treated as complementary diagnostics and
historical validation tools, not as replacements for the streamer and sweeper.

---

# Maintenance recommendations

For future work:

1. Keep `aggregate_threeway_kgsl.py`, `compare_kgsl_runs.py`, and
   `analyze_kgsl_busy_validation.py` as supported auxiliary analysis tools.
2. Keep `parse_focused_kgsl_trace.py` and `summarize_trials.py` for historical
   reproducibility.
3. Avoid adding new experiment-specific logic to `summarize_trials.py`; create a
   generalized repeated-run aggregator instead.
4. Add tests using small representative trace and sysfs fixtures before changing
   tracepoint regexes.
5. Consider consolidating duplicated parsing code in
   `aggregate_threeway_kgsl.py` and `compare_kgsl_runs.py` into a shared module.
6. Record the kernel/device capture format alongside each dataset because KGSL
   trace formats can change across devices and kernel versions.
