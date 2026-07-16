# KGSL Evidence Summaries

This directory contains derived summaries generated from historical Qualcomm
KGSL/ftrace captures.

The summaries describe GPU command submission, active time, busy time, RAM wait,
frequency, bus activity, contexts, and KGSL memory events for:

- the Qualcomm vendor Vulkan driver;
- Mesa Turnip;
- compute-heavy workloads;
- memory-heavy workloads; and
- general user-interface/background GPU activity.

These files are **derived evidence**. They are not raw trace logs, source code,
runtime configuration, or inputs required to build the perf-counter streamer or
sweeper.

The intended evidence chain is:

```text
focused KGSL trace capture
        ↓
raw trace log
        ↓
event-count extraction and/or parse_focused_kgsl_trace.py
        ↓
per-run text summaries
        ↓
summarize_trials.py
        ↓
cross-trial vendor/Turnip comparison
```

The summaries are useful for understanding driver- and workload-level behavior
that complements the block-level Adreno counters collected by the main profiler.

---

# Directory layout

The repository contains two summary collections:

```text
evidence/summaries/
├── README.md
├── kgsl_tracepoints/
│   ├── turnip/
│   │   ├── turnip_compute_event_summary.txt
│   │   ├── turnip_compute_event_summary_1.txt
│   │   ├── turnip_compute_event_summary_2.txt
│   │   ├── turnip_compute_event_summary_3.txt
│   │   ├── turnip_compute_event_summary_4.txt
│   │   ├── turnip_compute_event_summary_5.txt
│   │   ├── turnip_mem_event_summary_1.txt
│   │   ├── turnip_mem_event_summary_2.txt
│   │   ├── turnip_mem_event_summary_3.txt
│   │   ├── turnip_mem_event_summary_4.txt
│   │   └── turnip_mem_event_summary_5.txt
│   └── vendor/
│       ├── vendor_compute_event_summary.txt
│       ├── vendor_compute_event_summary_1.txt
│       ├── vendor_compute_event_summary_2.txt
│       ├── vendor_compute_event_summary_3.txt
│       ├── vendor_compute_event_summary_4.txt
│       ├── vendor_compute_event_summary_5.txt
│       ├── vendor_mem_event_summary_1.txt
│       ├── vendor_mem_event_summary_2.txt
│       ├── vendor_mem_event_summary_3.txt
│       ├── vendor_mem_event_summary_4.txt
│       └── vendor_mem_event_summary_5.txt
└── vendor_turnip_comparison/
    ├── trial_comparison_summary.txt
    ├── kgsl/
    │   ├── kgsl_focused_trace_compute_summary.txt
    │   ├── kgsl_focused_trace_mem_summary.txt
    │   ├── kgsl_focused_trace_ui_summary.txt
    │   └── kgsl_focused_trace_ui_analysis.txt
    ├── turnip/
    │   ├── turnip_compute_summary.txt
    │   ├── turnip_compute_summary_1.txt
    │   ├── turnip_compute_summary_2.txt
    │   ├── turnip_compute_summary_3.txt
    │   ├── turnip_compute_summary_4.txt
    │   ├── turnip_compute_summary_5.txt
    │   ├── turnip_mem_summary_1.txt
    │   ├── turnip_mem_summary_2.txt
    │   ├── turnip_mem_summary_3.txt
    │   ├── turnip_mem_summary_4.txt
    │   └── turnip_mem_summary_5.txt
    └── vendor/
        ├── vendor_compute_summary.txt
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

The authoritative inventory and SHA-256 values are recorded in:

```text
evidence/manifests/kgsl_tracepoints_manifest.txt
evidence/manifests/vendor_turnip_comparison_manifest.txt
```

---

# Relationship to the final profiler

The main profiling products are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

The summaries do not:

- build the streamer or sweeper;
- program hardware countables;
- read KGSL counter registers;
- start or stop a profiler process;
- contain streamer CSV data;
- contain sweeper group/chunk CSV data; or
- need to be copied to the phone.

They provide an independent kernel/driver-level view.

```text
KGSL trace summaries
    command batches
    contexts
    coarse busy time
    RAM wait
    frequency
    bus level
    allocations/maps/frees

perf-counter streamer/sweeper
    SP counters
    UCHE counters
    TP counters
    HLSQ counters
    RBBM counters
    detailed hardware-event deltas
```

The two evidence paths answer different questions.

## Questions answered by these summaries

- How many command batches did the driver submit?
- How long were command batches active?
- Did vendor Vulkan and Turnip organize work differently?
- Was the GPU broadly busy?
- Was the GPU waiting on RAM?
- What frequency was recorded?
- How much bus demand was reported?
- Which KGSL allocation usage categories appeared?
- Did startup/UI activity contaminate a focused benchmark trace?
- Were results stable across repeated trials?

## Questions better answered by the streamer/sweeper

- Which Adreno block was active?
- Did SP ALU cycles scale?
- Did UCHE requests increase?
- Which TP or HLSQ stalls occurred?
- Which hardware countables are supported?
- How does a specific counter change across benchmark widths?

## Combined interpretation

A stronger performance conclusion combines:

```text
hardware counter deltas
+ benchmark runtime
+ KGSL busy/RAM-wait evidence
+ frequency and bus context
+ command-batch structure
+ repeated-run stability
```

---

# Status summary

| Collection | Status | Purpose |
|---|---|---|
| `kgsl_tracepoints/` | Historical compact summaries | Fast event-count inventory for repeated vendor/Turnip runs |
| `vendor_turnip_comparison/kgsl/` | Historical focused-trace inspection | Compact counts and one detailed UI analysis |
| `vendor_turnip_comparison/vendor/` | Historical detailed per-trial summaries | Vendor compute/memory behavior |
| `vendor_turnip_comparison/turnip/` | Historical detailed per-trial summaries | Turnip compute/memory behavior |
| `trial_comparison_summary.txt` | Historical experiment-level result | Steady-state statistics and cross-driver/workload ratios |

These files remain useful for reproducibility even though they are not part of
the current profiler runtime.

---

# Summary types

# 1. Compact event-count summaries

Example:

```text
13 kgsl_mem_free
13 kgsl_mem_alloc
8 kgsl_hwsched-1260
4 kgsl_buslevel
...
```

These files answer:

```text
which event types were present?
how many times did each event occur?
```

They do not calculate:

- command-batch active duration;
- queue delay;
- GMU latency;
- busy percentages;
- RAM-wait percentages;
- memory totals by usage; or
- cross-trial statistics.

Files of this type include:

```text
kgsl_tracepoints/*/*_event_summary*.txt
vendor_turnip_comparison/kgsl/kgsl_focused_trace_*_summary.txt
```

# 2. Detailed per-run summaries

Example sections:

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

These are generated by:

```text
analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py
```

Files of this type include:

```text
vendor_turnip_comparison/vendor/*_summary*.txt
vendor_turnip_comparison/turnip/*_summary*.txt
vendor_turnip_comparison/kgsl/kgsl_focused_trace_ui_analysis.txt
```

# 3. Cross-trial aggregate summary

```text
vendor_turnip_comparison/trial_comparison_summary.txt
```

This file combines selected repeated-run summaries and reports:

- trial selection;
- means;
- sample standard deviations;
- memory-to-ALU ratios;
- Turnip-to-vendor ratios; and
- the common observed maximum frequency.

It is generated by:

```text
analysis/kgsl_trace_analysis/summarize_trials.py
```

---

# Naming conventions

# Driver prefix

```text
vendor_
    Qualcomm vendor Vulkan driver

turnip_
    Mesa Turnip Vulkan driver
```

# Workload name

```text
compute
    ALU-heavy Vulkan workload

mem
    memory-heavy Vulkan workload
```

The historical scripts use the word `compute` for the ALU-heavy workload.

# Trial suffix

```text
_summary_1.txt
_summary_2.txt
...
_summary_5.txt
```

The number identifies a repeated capture.

It does not necessarily indicate that every trial is equally clean or included
in the final steady-state statistics.

# Unnumbered summary files

Examples:

```text
turnip_compute_summary.txt
vendor_compute_summary.txt
```

These are preliminary, standalone, or earlier captures.

They are distinct files, not aliases for trial 1.

The historical trial summarizer searches for numbered patterns and does not use
the unnumbered file as one of trials 1–5.

Keep unnumbered files for historical provenance, but do not silently include
them in the steady-state aggregate.

# Event-summary naming

```text
*_event_summary*.txt
```

indicates compact event counts rather than the richer parsed report.

---

# File-group reference

# `kgsl_tracepoints/turnip/`

## Purpose

Contains compact event counts for repeated Turnip compute and memory captures.

The collection includes:

```text
turnip_compute_event_summary.txt
turnip_compute_event_summary_1.txt
...
turnip_compute_event_summary_5.txt

turnip_mem_event_summary_1.txt
...
turnip_mem_event_summary_5.txt
```

## Why these files exist

They provide a quick check that expected KGSL events appeared in each trace.

Useful checks include:

- command-batch events are present;
- power-stat events are present;
- frequency and bus events are present;
- allocations and frees are balanced;
- memory and compute traces have different event profiles; and
- a trial contains unexpected extra contexts or batches.

## Relationship to detailed summaries

These files contain only counts.

The detailed files under:

```text
vendor_turnip_comparison/turnip/
```

should be used when timing, busy, RAM-wait, bus, or memory-usage values are
required.

---

# `kgsl_tracepoints/vendor/`

## Purpose

Contains compact event counts for repeated vendor-driver compute and memory
captures.

The structure mirrors the Turnip directory so that trial presence and broad
event behavior can be compared consistently.

## Use

Use compact event summaries to identify:

- missing events;
- unexpected trial contamination;
- different command-batch counts;
- driver-specific allocation/map patterns; and
- captures requiring detailed re-parsing.

Do not use event counts alone as a performance metric.

---

# `vendor_turnip_comparison/kgsl/kgsl_focused_trace_compute_summary.txt`

## Purpose

A compact event-count summary for one focused compute trace.

The file records:

```text
13 kgsl_mem_free
13 kgsl_mem_alloc
8 kgsl_hwsched-1260
4 kgsl_buslevel
3 kgsl_pwrstats
...
1 adreno_cmdbatch_submitted
1 adreno_cmdbatch_retired
1 adreno_cmdbatch_ready
1 adreno_cmdbatch_queued
1 adreno_cmdbatch_done
```

## Interpretation

The focused compute trace contains one complete command-batch lifecycle and a
small number of power/bus events.

The `kgsl_hwsched-1260` entries are scheduler-related lines captured by the
event-count extraction. They are not parsed as one of the detailed event
families by the current `parse_focused_kgsl_trace.py`.

---

# `vendor_turnip_comparison/kgsl/kgsl_focused_trace_mem_summary.txt`

## Purpose

A compact event-count summary for one focused memory trace.

It records one complete command-batch lifecycle, nine `kgsl_pwrstats` samples,
and one `kgsl_gpubusy` sample.

## Interpretation

Compared with the compact focused compute summary, the memory trace contains
more power-stat samples:

```text
compute: 3 kgsl_pwrstats
memory:  9 kgsl_pwrstats
```

This is consistent with a longer or more heavily sampled active period, but
event counts alone do not establish execution time.

---

# `vendor_turnip_comparison/kgsl/kgsl_focused_trace_ui_summary.txt`

## Purpose

A compact event-count inventory for a broad UI/background trace.

It records thousands of command-batch events:

```text
2087 adreno_cmdbatch_queued
1481 adreno_cmdbatch_submitted
1481 adreno_cmdbatch_retired
1481 adreno_cmdbatch_ready
1481 adreno_cmdbatch_done
```

It also contains hundreds of power and memory events.

## Why it is useful

The UI trace demonstrates why benchmark captures must be focused.

A general system trace can include:

- multiple processes;
- many command contexts;
- display composition;
- application rendering;
- surface/egl-image activity;
- background GPU work; and
- lower-frequency interactive operation.

This file is a contamination/reference example, not one of the controlled
compute/memory trials.

---

# `vendor_turnip_comparison/kgsl/kgsl_focused_trace_ui_analysis.txt`

## Purpose

A detailed parse of the broad UI trace.

It reports:

```text
submitted batches: 1347
retired batches:   1336
average active:    19954.8 ticks
maximum active:    106326 ticks
average busy:      59.80%
average RAM wait:  22.19%
frequency range:   222000–342000 kHz
```

It includes several active contexts:

```text
ctx 8
ctx 3
ctx 18
ctx 20
```

and large surface/egl-image memory-event totals.

## Why it is not directly comparable to the benchmark trials

The controlled trials report a 1.1 GHz frequency and one main benchmark context
during steady-state captures.

The UI analysis instead includes:

- several contexts;
- more than one thousand batches;
- low interactive GPU frequencies;
- display-related allocations/maps; and
- broad system activity.

Use it as a reference for real-world background complexity, not as a controlled
vendor-versus-Turnip data point.

---

# `vendor_turnip_comparison/turnip/turnip_compute_summary_1.txt`

## Purpose

Detailed Turnip compute trial 1.

This trial includes:

```text
3 submitted batches
3 retired batches
2 contexts
extra surface/egl-image mapping activity
```

It is an early/warm-up trial and is excluded from the final ALU steady-state
aggregate.

## Why it is excluded

The later clean compute trials contain one main command batch and one benchmark
context.

Trial 1 contains setup or unrelated activity in addition to the benchmark.

---

# `turnip_compute_summary_2.txt` through `turnip_compute_summary_5.txt`

## Purpose

Detailed repeated Turnip compute trials used in the steady-state aggregate.

The selected trials have:

```text
1 submitted batch
1 retired batch
approximately 4.10–4.15 million active ticks
high kgsl_pwrstats busy percentage
recorded maximum frequency of 1.1 GHz
```

The aggregate for trials 2–5 is:

```text
submitted batches:        1.00 ± 0.00
average active ticks:     4,126,297.75 ± 23,449.73
average busy:             99.43% ± 0.65%
average RAM wait:         38.63% ± 3.48%
maximum RAM wait:         61.09% ± 1.21%
maximum frequency:        1,100,000 kHz
```

## Interpretation

Turnip’s compute workload appears as one long command batch with consistently
high coarse busy percentage.

The relatively high KGSL RAM-wait percentage must not be interpreted as proof
that the shader is memory-bound. It can reflect driver behavior, memory
accounting semantics, host-visible buffers, command organization, or other
system effects.

Hardware perf counters and runtime data are needed for a bottleneck conclusion.

---

# `turnip_compute_summary.txt`

## Purpose

An unnumbered preliminary/standalone Turnip compute summary.

It is similar in overall structure to trial 1 but has distinct measurements and
a distinct SHA-256 digest.

## Use

Keep for historical context.

Do not include it in the numbered steady-state set unless an experiment note
explicitly reclassifies it.

---

# `turnip_mem_summary_1.txt`

## Purpose

Detailed Turnip memory trial 1.

It contains:

```text
3 submitted batches
3 retired batches
2 contexts
approximately 11.94 million ticks for the longest batch
```

It is excluded from the final memory steady-state statistics.

## Interpretation

The extra batches and second context indicate startup/setup or unrelated work.

---

# `turnip_mem_summary_2.txt`

## Purpose

Detailed Turnip memory trial 2.

It contains:

```text
5 submitted batches
5 retired batches
4 short batches in a second context
1 long benchmark batch
```

It also contains additional surface and egl-image map/free activity.

## Why it matters

This is a clear example of a contaminated or setup-heavy trial.

The active-time average is misleading because it averages one very long batch
with four very short batches.

The trial is excluded from the final memory steady-state statistics.

---

# `turnip_mem_summary_3.txt` through `turnip_mem_summary_5.txt`

## Purpose

Clean repeated Turnip memory trials used in the steady-state aggregate.

These trials have:

```text
1 submitted batch
1 retired batch
approximately 11.95–11.99 million active ticks
one benchmark context
average RAM wait around 68–75%
maximum frequency of 1.1 GHz
```

The aggregate for trials 3–5 is:

```text
submitted batches:        1.00 ± 0.00
average active ticks:     11,975,270.00 ± 21,468.05
average busy:             88.54% ± 0.23%
average RAM wait:         72.58% ± 3.96%
maximum RAM wait:         96.64% ± 0.48%
maximum frequency:        1,100,000 kHz
```

## Interpretation

Turnip memory trials show substantially greater RAM-wait percentage than Turnip
compute trials.

This supports the workload-class distinction at the KGSL level.

---

# `vendor_turnip_comparison/vendor/vendor_compute_summary_*.txt`

## Purpose

Detailed repeated vendor-driver compute summaries.

The final aggregate uses trials:

```text
2, 3, 4, 5
```

Trial 1 and the unnumbered preliminary file are excluded from steady-state
statistics.

The selected aggregate is:

```text
submitted batches:        2.00 ± 0.00
average active ticks:     1,555,063.75 ± 368.19
maximum active ticks:     3,109,948.25 ± 734.53
average busy:             65.61% ± 3.44%
average RAM wait:         3.37% ± 1.79%
maximum RAM wait:         9.67% ± 4.50%
maximum frequency:        1,100,000 kHz
```

## Command organization

The selected vendor compute trials contain two batches.

One is a very short setup/initial batch and the second is the main workload
batch.

Therefore:

```text
average active ticks
```

is roughly half of:

```text
maximum active ticks
```

Do not compare only the average batch duration against Turnip’s one-batch
duration without considering batch count.

---

# `vendor_mem_summary_1.txt`

## Purpose

Detailed vendor memory trial 1.

It includes:

```text
4 submitted batches
4 retired batches
2 benchmark-context batches
2 additional context-8 batches
surface/egl-image mapping activity
```

It is excluded from the final memory steady-state aggregate.

---

# `vendor_mem_summary_2.txt`

## Purpose

Historical vendor memory trial 2.

It is present in the repository and manifest, but the final steady-state
aggregate excludes it along with trial 1.

The exclusion rule uses vendor memory trials 3–5.

---

# `vendor_mem_summary_3.txt` through `vendor_mem_summary_5.txt`

## Purpose

Clean repeated vendor memory trials used in the final steady-state aggregate.

These trials consistently report:

```text
2 submitted batches
2 retired batches
one tiny setup batch
one main batch around 6.28 million active ticks
average RAM wait around 44.6–44.8%
maximum frequency of 1.1 GHz
```

The aggregate is:

```text
submitted batches:        2.00 ± 0.00
average active ticks:     3,141,993.50 ± 382.22
maximum active ticks:     6,283,810.33 ± 763.42
average busy:             80.79% ± 2.09%
average RAM wait:         44.68% ± 0.13%
maximum RAM wait:         68.28% ± 0.65%
maximum frequency:        1,100,000 kHz
```

## Memory-usage categories

The vendor summaries report allocation usage labels such as:

```text
VK/others( 32)
VK/others( 35)
VK/others( 36)
VK/others( 37)
VK/others( 38)
VK/others( 51)
any(0)
```

These labels differ from some Turnip traces, which include:

```text
any(0)
surface
egl_image
```

The differences can reflect driver allocation behavior and trace context.

Do not compare summed “memory by usage” values as if they were equivalent
application working-set measurements across drivers.

---

# `trial_comparison_summary.txt`

## Purpose

Provides the final repeated-trial comparison.

It is generated by:

```text
analysis/kgsl_trace_analysis/summarize_trials.py
```

## Trial-selection rules

The file explicitly uses:

```text
ALU:
    trials 2–5

memory:
    trials 3–5
```

The exclusions reduce warm-up and setup contamination.

## Final steady-state table

```text
driver   workload  trials   batches   avg_active   max_active   busy%   ram_wait%   freq_kHz
vendor   alu       2–5      2.00      1,555,063.8  3,109,948.2 65.61   3.37        1,100,000
turnip   alu       2–5      1.00      4,126,297.8  4,126,297.8 99.43   38.63       1,100,000
vendor   mem       3–5      2.00      3,141,993.5  6,283,810.3 80.79   44.68       1,100,000
turnip   mem       3–5      1.00     11,975,270.0 11,975,270.0 88.54   72.58       1,100,000
```

## Reported ratios

```text
vendor memory / vendor ALU:
    average active: 2.02×
    RAM wait:      13.27×

Turnip memory / Turnip ALU:
    average active: 2.90×
    RAM wait:       1.88×

Turnip / vendor memory:
    average active: 3.81×
    RAM wait:       1.62×

Turnip / vendor ALU:
    average active: 2.65×
    RAM wait:      11.47×
```

---

# Interpretation cautions for the final ratios

# Batch count differs across drivers

Selected vendor trials contain two batches, while selected Turnip trials contain
one.

The report’s `avg_active` metric is the average active ticks per retired batch.

For vendor trials, one short setup batch lowers the average.

Therefore, the Turnip/vendor `avg_active` ratios are not exactly equivalent to
total workload-runtime ratios.

The `max_active` value is often a better approximation of the main benchmark
batch when one dominant batch and one tiny setup batch are present.

# Ticks are not automatically seconds

The summaries report KGSL trace fields labelled as ticks.

Do not convert them to seconds unless the trace clock and field units are
confirmed for the device/kernel.

Use the values as relative timing indicators within the same capture method.

# Busy and RAM-wait values are averages of sparse samples

The number of `kgsl_pwrstats` samples is small in many focused trials.

For example, clean compute trials can contain only three samples.

Averages based on three samples are useful but have limited temporal resolution.

# Frequency is controlled in the selected trials

All selected steady-state groups report:

```text
1,100,000 kHz
```

This removes one obvious frequency difference from the comparison.

It does not prove identical voltage, thermal state, or instantaneous frequency
throughout each workload.

# RAM wait is not a complete bottleneck diagnosis

A high RAM-wait percentage can support a memory-pressure hypothesis.

It does not identify:

- which cache level missed;
- which request type caused the wait;
- whether SP execution was stalled;
- whether memory latency or bandwidth dominated;
- or whether driver/runtime organization changed the accounting.

Use UCHE, TP, SP, and HLSQ perf counters for more detailed analysis.

---

# Main findings supported by the summaries

# 1. The memory workloads have greater RAM wait than ALU workloads

```text
vendor:
    ALU RAM wait:    3.37%
    memory RAM wait: 44.68%

Turnip:
    ALU RAM wait:    38.63%
    memory RAM wait: 72.58%
```

The memory-heavy workload produces a clear increase for both drivers.

# 2. Turnip produces longer main command batches

Main/maximum active ticks:

```text
vendor ALU:    approximately 3.11 million
Turnip ALU:    approximately 4.13 million

vendor memory: approximately 6.28 million
Turnip memory: approximately 11.98 million
```

The summaries show longer Turnip active batches for the tested workloads.

This does not by itself identify the cause.

Possible contributors include:

- shader compiler output;
- command-stream generation;
- synchronization;
- memory behavior;
- allocation strategy;
- host-visible memory behavior;
- driver scheduling; or
- trace accounting differences.

# 3. Command organization differs by driver

Clean selected trials show:

```text
vendor:
    2 retired batches

Turnip:
    1 retired batch
```

The vendor path includes one tiny setup batch and one main batch.

Turnip presents one long main batch.

# 4. Turnip ALU RAM-wait accounting is unexpectedly high

Turnip ALU trials report approximately:

```text
38.63% average RAM wait
```

while vendor ALU reports approximately:

```text
3.37%
```

This is a notable driver difference.

It should be investigated with hardware perf counters rather than accepted as
proof that Turnip’s ALU shader is memory-bound.

# 5. Repeated selected trials are stable

The reported standard deviations are small for active ticks:

```text
vendor ALU average active:
    standard deviation 368 ticks

Turnip ALU average active:
    standard deviation 23,450 ticks

vendor memory average active:
    standard deviation 382 ticks

Turnip memory average active:
    standard deviation 21,468 ticks
```

Relative to million-tick means, the selected trials are highly repeatable.

# 6. Early trials contain setup/background activity

Extra contexts, surface maps, egl-image maps, and additional batches appear in
early Turnip and vendor trials.

The warm-up exclusion is therefore justified by the recorded trace structure,
not only by a desire to improve statistics.

---

# Metrics reference

# Event counts

Number of matching trace lines by event name.

Use for completeness and contamination checks.

# Submitted/retired batches

Count of parsed command-batch submission and retirement events.

A mismatch can indicate:

- incomplete trace capture;
- parser mismatch;
- work still in flight at capture end;
- missing event lines; or
- unrelated event loss.

# Active ticks

The `active` field from `adreno_cmdbatch_retired`.

Use for relative command-batch duration.

# Queue-to-start ticks

Derived as:

```text
start - submitted_to_rb
```

Use as a scheduling/queue-delay indicator.

# GMU latency ticks

Derived as:

```text
retired_on_gmu - retire
```

Use as a coarse retirement-latency indicator.

# `kgsl_pwrstats` busy percentage

Calculated as:

```text
busy / total × 100
```

for each sample, then averaged in the parser output.

# RAM-wait percentage

Calculated as:

```text
ram_wait / ram_time × 100
```

for each valid sample.

# `kgsl_gpubusy`

Alternative coarse busy metric:

```text
busy / elapsed × 100
```

Many focused trials contain zero or one sample, so it is secondary evidence.

# Frequency

Parsed from:

```text
gpu_frequency: gpu_freq=<value>Khz
```

The historical parser prints the unit as kHz.

# Bus bandwidth

Parsed from `kgsl_buslevel`:

```text
avg_bw
```

The units are driver/kernel-specific and should not be relabelled without
checking the source tracepoint definition.

# Memory by usage

Sums the `size` field for every parsed allocation/map/free event grouped by
usage label.

This is event volume, not necessarily peak resident memory.

---

# Generation workflow

# Requirements

Host:

```text
Python 3.9 or newer
Bash or Zsh
standard Unix text tools
```

Capture side:

```text
ADB
root or sufficient tracefs permissions
KGSL tracepoints
Vulkan benchmark binaries
vendor and/or Turnip driver environment
```

The analysis scripts use only the Python standard library.

No `pip install` is required for:

```text
parse_focused_kgsl_trace.py
summarize_trials.py
```

---

# Generate one detailed summary

From the repository root:

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input /path/to/filtered_trace.log \
  > /path/to/output_summary.txt
```

Example:

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/turnip_compute_trial_2/raw/trace_raw.log \
  > evidence/summaries/vendor_turnip_comparison/turnip/turnip_compute_summary_2.txt
```

Use `tee` to see and save output:

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input results/kgsl_runs/turnip_mem_trial_3/raw/trace_raw.log \
  | tee evidence/summaries/vendor_turnip_comparison/turnip/turnip_mem_summary_3.txt
```

---

# Generate a compact event-count summary

For standard tracepoint lines containing:

```text
: event_name:
```

use:

```bash
awk '
  match($0, /: ([A-Za-z0-9_-]+):/, m) {
    count[m[1]]++
  }

  END {
    for (event in count) {
      print count[event], event
    }
  }
' /path/to/filtered_trace.log \
  | sort -nr \
  > /path/to/event_summary.txt
```

macOS’s default `awk` may not support the third argument to `match()`.

A portable Python version is safer:

```bash
python3 - \
  /path/to/filtered_trace.log \
  /path/to/event_summary.txt <<'PY'
from collections import Counter
from pathlib import Path
import re
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])

event_re = re.compile(r": ([A-Za-z0-9_-]+):")
counts = Counter()

for line in source.read_text(errors="replace").splitlines():
    match = event_re.search(line)

    if match:
        counts[match.group(1)] += 1

lines = [
    f"{count:4d} {event}"
    for event, count in sorted(
        counts.items(),
        key=lambda item: (-item[1], item[0]),
    )
]

output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("\n".join(lines) + "\n")
print(f"Wrote {output}")
PY
```

---

# Generate the repeated-trial comparison

The historical script expects all numbered per-run summary files in its current
working directory.

Create a temporary flat workspace:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
WORK_DIR="$REPO_ROOT/build/vendor_turnip_trial_summary"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
```

Copy or symlink the numbered files:

```bash
ln -s \
  "$REPO_ROOT"/evidence/summaries/vendor_turnip_comparison/vendor/vendor_compute_summary_[1-5].txt \
  "$WORK_DIR"/

ln -s \
  "$REPO_ROOT"/evidence/summaries/vendor_turnip_comparison/vendor/vendor_mem_summary_[1-5].txt \
  "$WORK_DIR"/

ln -s \
  "$REPO_ROOT"/evidence/summaries/vendor_turnip_comparison/turnip/turnip_compute_summary_[1-5].txt \
  "$WORK_DIR"/

ln -s \
  "$REPO_ROOT"/evidence/summaries/vendor_turnip_comparison/turnip/turnip_mem_summary_[1-5].txt \
  "$WORK_DIR"/
```

Run:

```bash
cd "$WORK_DIR"

python3 \
  "$REPO_ROOT/analysis/kgsl_trace_analysis/summarize_trials.py" \
  | tee \
    "$REPO_ROOT/evidence/summaries/vendor_turnip_comparison/trial_comparison_summary.txt"
```

Return to the repository root:

```bash
cd "$REPO_ROOT"
```

## Important warning

This command overwrites the historical aggregate file.

For verification, write to a temporary file first:

```bash
cd "$WORK_DIR"

python3 \
  "$REPO_ROOT/analysis/kgsl_trace_analysis/summarize_trials.py" \
  > /tmp/trial_comparison_summary.regenerated.txt
```

Compare:

```bash
diff -u \
  "$REPO_ROOT/evidence/summaries/vendor_turnip_comparison/trial_comparison_summary.txt" \
  /tmp/trial_comparison_summary.regenerated.txt
```

No diff indicates exact textual reproduction.

---

# Verify expected files

```bash
find evidence/summaries \
  -type f \
  -print \
  | sort
```

Count per-run detailed files:

```bash
find evidence/summaries/vendor_turnip_comparison \
  -type f \
  \( -name 'vendor_*_summary_[1-5].txt' \
     -o -name 'turnip_*_summary_[1-5].txt' \) \
  | wc -l
```

Expected:

```text
20
```

because:

```text
2 drivers × 2 workloads × 5 trials = 20 files
```

---

# Verify hashes against the manifests

```bash
python3 - <<'PY'
from pathlib import Path
import hashlib
import re

manifests = [
    Path("evidence/manifests/kgsl_tracepoints_manifest.txt"),
    Path("evidence/manifests/vendor_turnip_comparison_manifest.txt"),
]

entry_re = re.compile(
    r"^\S+\s+"
    r"(?P<sha>[0-9a-f]{64})\s+"
    r"(?P<path>.+)$"
)

failures = 0

for manifest in manifests:
    entries = []

    for line in manifest.read_text(errors="replace").splitlines():
        match = entry_re.match(line)

        if match:
            entries.append(
                (
                    match.group("sha"),
                    Path(match.group("path")),
                )
            )

    for expected, path in entries:
        if not path.is_file():
            print(f"MISSING  {path}")
            failures += 1
            continue

        digest = hashlib.sha256(path.read_bytes()).hexdigest()

        if digest != expected:
            print(f"MISMATCH {path}")
            print(f"  expected={expected}")
            print(f"  actual=  {digest}")
            failures += 1

    print(f"Checked {len(entries)} entries from {manifest}")

raise SystemExit(1 if failures else 0)
PY
```

For larger files, use streamed reads rather than `read_bytes()`. These summary
files are small enough that either method is safe.

---

# Extract a metric from all detailed summaries

```bash
python3 - <<'PY'
from pathlib import Path
import re

root = Path("evidence/summaries/vendor_turnip_comparison")

patterns = {
    "submitted": re.compile(r"submitted batches:\s+(\d+)"),
    "avg_active": re.compile(r"avg active:\s+([0-9.]+) ticks"),
    "busy": re.compile(
        r"=== kgsl_pwrstats ===.*?"
        r"avg busy_pct:\s+([0-9.]+)%",
        re.DOTALL,
    ),
    "ram_wait": re.compile(
        r"=== kgsl_pwrstats ===.*?"
        r"avg ram_wait_pct:\s+([0-9.]+)%",
        re.DOTALL,
    ),
}

for path in sorted(root.glob("*/*_summary_[1-5].txt")):
    text = path.read_text(errors="replace")

    values = {}

    for name, pattern in patterns.items():
        match = pattern.search(text)
        values[name] = match.group(1) if match else "NA"

    print(
        f"{path.name:34s} "
        f"batches={values['submitted']:>2s} "
        f"active={values['avg_active']:>10s} "
        f"busy={values['busy']:>6s}% "
        f"ram_wait={values['ram_wait']:>6s}%"
    )
PY
```

---

# Detect contaminated trials

A simple review can flag summaries containing more command batches than the
steady-state pattern.

```bash
python3 - <<'PY'
from pathlib import Path
import re

root = Path("evidence/summaries/vendor_turnip_comparison")

for path in sorted(root.glob("*/*_summary_[1-5].txt")):
    text = path.read_text(errors="replace")
    match = re.search(r"submitted batches:\s+(\d+)", text)

    if not match:
        continue

    count = int(match.group(1))

    expected = 1 if path.parent.name == "turnip" else 2

    if count != expected:
        print(
            f"REVIEW {path}: "
            f"submitted={count}, "
            f"steady-state expectation={expected}"
        )
PY
```

This is an experiment-specific heuristic, not a universal correctness test.

---

# Compare event-count summaries

```bash
python3 - <<'PY'
from collections import Counter
from pathlib import Path


def read_counts(path):
    counts = Counter()

    for line in path.read_text(errors="replace").splitlines():
        parts = line.split(maxsplit=1)

        if len(parts) != 2:
            continue

        try:
            count = int(parts[0])
        except ValueError:
            continue

        counts[parts[1]] = count

    return counts


compute = read_counts(
    Path(
        "evidence/summaries/vendor_turnip_comparison/kgsl/"
        "kgsl_focused_trace_compute_summary.txt"
    )
)

memory = read_counts(
    Path(
        "evidence/summaries/vendor_turnip_comparison/kgsl/"
        "kgsl_focused_trace_mem_summary.txt"
    )
)

events = sorted(set(compute) | set(memory))

print(f"{'event':36s} {'compute':>10s} {'memory':>10s}")

for event in events:
    print(
        f"{event:36s} "
        f"{compute[event]:10d} "
        f"{memory[event]:10d}"
    )
PY
```

---

# Reproduce the experiment safely

# 1. Capture raw traces first

Never generate a summary without retaining the raw trace.

Recommended layout:

```text
evidence/raw_logs/vendor_turnip_comparison/
├── vendor/
│   ├── compute_trial_1/
│   │   └── trace_raw.log
│   └── ...
└── turnip/
    ├── compute_trial_1/
    │   └── trace_raw.log
    └── ...
```

# 2. Record benchmark metadata

For every trial, save:

```text
driver
driver version
Mesa commit when using Turnip
benchmark binary hash
shader hash
benchmark command
element count
iteration count
dispatch repeats
device model
Android build
kernel version
GPU frequency policy
thermal state
tracepoint list
capture start/end procedure
benchmark exit code
verification result
```

# 3. Generate summaries as derived files

Do not edit the generated numbers manually.

# 4. Review early trials

Check:

- batch count;
- contexts;
- map/free activity;
- power-stat sample count;
- frequency;
- benchmark verification; and
- background activity.

# 5. Choose exclusion rules before final interpretation

The historical rules were:

```text
ALU:    exclude trial 1
memory: exclude trials 1 and 2
```

For a new experiment, do not copy these rules automatically.

Use criteria based on the new traces.

# 6. Report both included and excluded trials

Keep all trials and explain exclusions.

---

# Expected use cases

# Driver comparison

Compare vendor Vulkan and Turnip under identical workloads.

# Workload classification

Confirm that the memory-heavy workload produces more RAM wait than the ALU-heavy
workload.

# Trace contamination detection

Identify extra contexts, UI batches, or setup allocations.

# Regression analysis

Repeat after changing:

- Mesa revision;
- shader compiler options;
- benchmark implementation;
- Android build;
- kernel;
- buffer strategy; or
- profiler tool.

# Cross-validation

Compare KGSL findings with:

```text
SP_ALU_WORKING_CYCLES
SP_BUSY_CYCLES
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
SP/UCHE transaction counters
```

from the streamer/sweeper.

---

# What these summaries do not prove

They do not prove:

- a particular driver is universally faster;
- Turnip is always more memory-bound;
- RAM wait is the only cause of longer active time;
- all vendor and Turnip memory allocations are semantically equivalent;
- all traces contain only benchmark work;
- a count of one batch means less overhead;
- the command-batch tick unit equals a specific time unit;
- the driver compiler produced identical machine code;
- the benchmark used identical physical memory placement;
- the results generalize beyond the tested phone and software versions; or
- the hardware perf-counter mapping is correct.

The conclusions apply to the recorded experiment and should be restated with its
configuration.

---

# Integrity and provenance

The corresponding manifests record SHA-256 hashes.

Do not modify historical summary text to improve formatting.

If a correction is required:

1. preserve the original;
2. regenerate from the raw trace;
3. save the corrected file under a new dated name;
4. document the parser version and reason; and
5. create a new manifest.

A text reformat changes the hash even when the numerical meaning is unchanged.

---

# Public-release considerations

These summaries are generally safer to publish than raw logcat or complete
trace logs, but review them for:

- process names;
- local paths;
- device identifiers;
- internal usernames;
- proprietary driver labels;
- experiment-only context IDs; and
- unpublished implementation details.

The current summaries mainly contain technical event data.

The associated manifests include local source paths and should be reviewed
separately before public release.

---

# Known limitations

# Sparse power samples

Many focused trials contain only three to nine `kgsl_pwrstats` samples.

# Experiment-specific parser

The parser relies on trace-line formats from the tested KGSL kernel.

# Hard-coded trial aggregation

`summarize_trials.py` uses fixed filename patterns and exclusion rules.

# Text-based intermediate format

The aggregate parser extracts metrics from human-readable text.

A schema change can break reproduction.

# No raw-trace link inside each file

The summaries do not embed the path/hash of their exact source trace.

# No benchmark metadata inside each file

Commands and hashes are not embedded.

# Average active duration can be misleading

Drivers use different batch counts.

# Memory event totals are not peak residency

They sum event sizes.

# Frequency samples are sparse

One recorded frequency value does not guarantee a perfectly fixed clock.

# No perf counters in these files

These are KGSL trace summaries, not hardware-counter reports.

# UI and focused trials are different populations

Do not combine them statistically.

---

# Recommended maintenance

1. Keep all existing summary files immutable.
2. Keep the two manifest files beside the evidence workflow.
3. Add raw-trace SHA-256 to every future summary header.
4. Add parser Git commit and hash to every generated summary.
5. Replace text-to-text aggregation with CSV or JSON intermediate data.
6. Generalize trial selection through command-line arguments.
7. Report total active ticks as well as average/max per batch.
8. Report median values for larger repeated sets.
9. Record pwrstats sample count in aggregate tables.
10. Record benchmark verification and exit code.
11. Add device, kernel, and driver metadata.
12. Add exact benchmark and shader hashes.
13. Keep excluded trials and document the exclusion reason.
14. Add a script that detects extra contexts automatically.
15. Add a script that verifies summary hashes against manifests.
16. Add a direct link from every summary to its source raw trace.
17. Add a current three-way copy/ALU/memory summary collection.
18. Cross-reference streamer/sweeper runs taken during the same benchmark.
19. Avoid comparing average batch active time across drivers without batch-count
    context.
20. Use hardware perf counters to investigate Turnip’s high ALU RAM-wait value.

---

# Suggested future structure

```text
evidence/summaries/
├── README.md
├── kgsl_tracepoints/
│   └── ...
└── vendor_turnip_comparison/
    ├── experiment_metadata.json
    ├── trial_comparison_summary.md
    ├── trial_metrics.csv
    ├── included_trials.txt
    ├── excluded_trials.txt
    ├── kgsl/
    │   └── ...
    ├── vendor/
    │   ├── compute/
    │   │   ├── trial_01.txt
    │   │   └── ...
    │   └── memory/
    │       └── ...
    └── turnip/
        ├── compute/
        │   └── ...
        └── memory/
            └── ...
```

A machine-readable `trial_metrics.csv` should contain:

```text
driver
workload
trial
included
exclusion_reason
submitted_batches
retired_batches
total_active_ticks
avg_active_ticks
max_active_ticks
avg_queue_to_start_ticks
avg_gmu_latency_ticks
pwrstats_samples
avg_busy_pct
avg_ram_wait_pct
max_ram_wait_pct
max_freq_khz
source_trace_sha256
summary_sha256
```

---

# Quick-reference workflow

## Parse one raw trace

```bash
python3 \
  analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py \
  --input /path/to/trace_raw.log \
  > /path/to/trial_summary.txt
```

## Generate event counts

```bash
python3 - /path/to/trace_raw.log /path/to/event_summary.txt <<'PY'
from collections import Counter
from pathlib import Path
import re
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])
event_re = re.compile(r": ([A-Za-z0-9_-]+):")
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
    )
    + "\n"
)
PY
```

## Rebuild trial comparison

```bash
cd /path/to/flat/numbered/trial/summaries

python3 \
  /Users/jerryyun/adreno-gpu-profiler/analysis/kgsl_trace_analysis/summarize_trials.py \
  > /tmp/trial_comparison_summary.regenerated.txt
```

## Compare with historical result

```bash
diff -u \
  evidence/summaries/vendor_turnip_comparison/trial_comparison_summary.txt \
  /tmp/trial_comparison_summary.regenerated.txt
```

## Verify manifests

```bash
shasum -a 256 \
  evidence/summaries/vendor_turnip_comparison/turnip/turnip_mem_summary_3.txt
```

Compare the digest with:

```text
evidence/manifests/vendor_turnip_comparison_manifest.txt
```

---

# Quick collection summary

```text
kgsl_tracepoints/
    compact per-trial event counts
    useful for completeness and contamination checks

vendor_turnip_comparison/kgsl/
    focused compute/memory/UI event counts
    detailed UI reference analysis

vendor_turnip_comparison/vendor/
    detailed vendor compute and memory trial summaries

vendor_turnip_comparison/turnip/
    detailed Turnip compute and memory trial summaries

trial_comparison_summary.txt
    selected steady-state means, standard deviations, and ratios
```

---

# Main experiment conclusion

The summaries support three high-level observations:

```text
1. The memory-heavy workload produces much higher KGSL RAM-wait percentage
   than the ALU-heavy workload for both drivers.

2. Turnip produces longer main command batches than the vendor driver for the
   tested compute and memory workloads.

3. Driver command organization differs: clean vendor trials use one tiny setup
   batch plus one main batch, while clean Turnip trials use one long batch.
```

The summaries also expose an unresolved result:

```text
Turnip's ALU-heavy workload reports much higher KGSL RAM wait than the vendor
ALU workload.
```

That result should be investigated using the hardware perf-counter streamer and
sweeper rather than treated as a final bottleneck diagnosis.
