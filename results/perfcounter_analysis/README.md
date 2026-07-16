# Perf-Counter Width-Sequence Analysis Results

This directory contains derived analyses of perf-counter sweeps collected while
running a fused-softmax width sequence.

The analyzed widths are:

```text
128
256
512
1024
2048
```

The largest width is 16 times the smallest width:

```text
2048 / 128 = 16
```

This makes counters that scale close to 16× useful indicators that the analysis
captured work proportional to input width.

The directory stores several generations of the analysis workflow:

1. legacy per-chunk local burst detection;
2. local detection with a lower threshold;
3. single-run reference-window analysis;
4. SP-reference and padding experiments;
5. repeated-run reference-window reproducibility analysis; and
6. repeated-run local-window cross-checking.

These are post-processing results. They are not source sweeps and are not
required to build or run the perf-counter streamer or sweeper.

---

# Data lineage

```text
fused-softmax width-sequence benchmark
        ↓
streamer_sweeper / perf-counter sweeper
        ↓
results/perfcounter_sweeps/<sweep directory>/
        ↓
analysis/perfcounter_width_sequence/*.py
        ↓
results/perfcounter_analysis/
```

The analysis scripts consume the chunk CSV files after collection has finished.

The main scripts are:

```text
analysis/perfcounter_width_sequence/
├── analyze_width_sequence.py
├── analyze_width_sequence_ref_windows.py
├── analyze_repeated_width_sweeps.py
├── analyze_repeated_width_sweeps_local_windows.py
└── make_repro_report.py
```

---

# Directory layout

```text
results/perfcounter_analysis/
├── README.md
├── .DS_Store
├── latest_width_sequence/
│   ├── counter_classification.csv
│   ├── counter_width_summary.csv
│   ├── flat_or_saturated_counters.csv
│   ├── region_detection_quality.csv
│   ├── strong_width_scaling_counters.csv
│   └── zero_counters.csv
├── latest_width_sequence_ref_windows/
│   ├── counter_classification.csv
│   ├── counter_width_summary.csv
│   ├── flat_or_saturated_counters.csv
│   ├── reference_width_windows.csv
│   ├── width_scaling_counters.csv
│   └── zero_counters.csv
├── latest_width_sequence_ref_windows_sp/
│   ├── counter_classification.csv
│   ├── counter_width_summary.csv
│   ├── flat_or_saturated_counters.csv
│   ├── reference_width_windows.csv
│   ├── width_scaling_counters.csv
│   └── zero_counters.csv
├── latest_width_sequence_ref_windows_sp_pad20/
│   ├── counter_classification.csv
│   ├── counter_width_summary.csv
│   ├── flat_or_saturated_counters.csv
│   ├── reference_width_windows.csv
│   ├── width_scaling_counters.csv
│   └── zero_counters.csv
├── latest_width_sequence_thr03/
│   ├── counter_classification.csv
│   ├── counter_width_summary.csv
│   ├── flat_or_saturated_counters.csv
│   ├── region_detection_quality.csv
│   ├── strong_width_scaling_counters.csv
│   └── zero_counters.csv
├── repro_width_sequence_latest2/
│   ├── .Rhistory
│   ├── all_runs_counter_classification.csv
│   ├── all_runs_counter_width_summary.csv
│   ├── all_runs_reference_windows.csv
│   ├── cross_run_stability_summary.csv
│   ├── reproducibility_report.md
│   ├── stable_near_16x_counters.csv
│   ├── stable_width_scaling_counters.csv
│   └── stable_zero_counters.csv
└── repro_width_sequence_latest2_local_windows/
    ├── all_runs_counter_classification.csv
    ├── all_runs_counter_width_summary.csv
    ├── cross_run_stability_summary.csv
    ├── local_region_detection_quality.csv
    ├── local_width_windows.csv
    ├── stable_near_16x_counters.csv
    └── stable_width_scaling_counters.csv
```

---

# Status summary

| Directory | Method | Status | Recommended use |
|---|---|---|---|
| `latest_width_sequence/` | Per-file local burst detection | Legacy exploratory result | Diagnose chunks and failed region detection |
| `latest_width_sequence_thr03/` | Local detection with threshold 0.03 | Legacy threshold experiment | Compare sensitivity to threshold |
| `latest_width_sequence_ref_windows/` | One shared reference timeline | Strongest inspected single-run result | Main single-run interpretation |
| `latest_width_sequence_ref_windows_sp/` | SP-reference experiment | Parameter experiment | Compare reference choice |
| `latest_width_sequence_ref_windows_sp_pad20/` | SP reference with wider padding | Parameter experiment | Test window-boundary sensitivity |
| `repro_width_sequence_latest2/` | Shared reference windows across two runs | Main reproducibility result | Formal repeated-run interpretation |
| `repro_width_sequence_latest2_local_windows/` | Local windows across two runs | Independent cross-check | Verify shared-reference assumptions |

---

# Relationship to the main profiler products

The main runtime tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

This directory does not:

- access `/dev/kgsl-3d0`;
- allocate hardware counter slots;
- configure selectors;
- launch Vulkan workloads;
- create raw chunk CSVs;
- build the streamer;
- build the sweeper; or
- need to be pushed to the phone.

It analyzes data already produced by the sweeper.

## Sweeper responsibility

The sweeper creates raw files such as:

```text
sweep_<timestamp>/
├── 01_CP/
│   ├── CP_chunk001.csv
│   ├── CP_chunk001_meta.txt
│   └── CP_chunk001_benchmark.log
├── ...
└── summary.csv
```

## Analysis responsibility

The scripts in `analysis/perfcounter_width_sequence/`:

- locate workload bursts;
- assign bursts to widths;
- calculate per-counter totals;
- classify counter behavior;
- compare repeated runs; and
- generate compact result tables.

---

# Benchmark interpretation

The results are associated with a fused-softmax width-sequence experiment.

The width sequence changes:

```text
input vector width
```

while other benchmark parameters should remain controlled.

The analysis files do not embed the full benchmark command.

For formal reproduction, read the source sweep's:

```text
run_config.txt
benchmark logs
chunk metadata
```

before relying on assumptions about:

- row count;
- repeat count;
- workgroup size;
- driver;
- shader version;
- benchmark binary; or
- counter interval.

---

# Analysis methods

# 1. Local per-chunk detection

Implemented by:

```text
analyze_width_sequence.py
```

Every chunk CSV independently calculates aggregate activity:

```python
activity = sum(abs(all counter columns))
```

A sample is considered active when:

```text
activity > maximum activity × threshold fraction
```

The script then:

1. detects continuous active regions;
2. merges regions separated by at most one sample;
3. keeps the five strongest regions if too many are detected;
4. assigns the regions to widths by chronological order;
5. summarizes every counter inside each detected region; and
6. falls back to a whole-file summary when exactly five regions are not found.

Fallback rows use:

```text
width = -1
region_index = -1
detection_status = fallback_whole_file
```

Only correctly detected width rows participate in classification.

## Strength

No shared reference counter is required.

## Main weakness

Sparse, zero, flat, or unrelated chunks often do not visibly contain five
detectable bursts.

---

# 2. Shared reference-window analysis

Implemented by:

```text
analyze_width_sequence_ref_windows.py
```

One reliable CSV defines the five workload windows.

The script then applies those windows by elapsed time to every chunk CSV.

This avoids requiring each counter chunk to detect its own bursts.

The historical reference workflow uses an SP chunk such as:

```text
11_SP/SP_chunk001.csv
```

## Strength

Every chunk receives the same width boundaries.

## Main assumption

The chunk CSV timelines are sufficiently aligned for reference times to apply
across all files.

---

# 3. Repeated reference-window analysis

Implemented by:

```text
analyze_repeated_width_sweeps.py
```

For every selected sweep:

1. load the same relative reference CSV;
2. detect five reference windows;
3. summarize every chunk using those times;
4. classify every counter for that run;
5. combine runs;
6. calculate ratio and correlation stability; and
7. export stable scaling/zero subsets.

The default repeated reference is:

```text
11_SP/SP_chunk001.csv
```

---

# 4. Repeated local-window analysis

Implemented by:

```text
analyze_repeated_width_sweeps_local_windows.py
```

Every chunk detects its own width windows.

This is useful as an independent cross-check of the shared-reference method.

It can restrict analysis to groups such as:

```text
SP,TP,UCHE,HLSQ,RBBM,CP
```

Files that fail to detect exactly five windows receive quality records but no
per-width summary rows.

---

# Classification rules

For each counter, the analysis calculates:

```text
total_all_widths
total_width_first
total_width_last
scaling_ratio_last_over_first
corr_total_vs_width
slope_total_vs_width
total_w128
total_w256
total_w512
total_w1024
total_w2048
```

The heuristic labels are:

## `zero_all_widths`

```text
sum of all five width totals = 0
```

## `strong_width_scaling`

```text
last / first > 8
and
correlation(width, total) > 0.7
```

## `moderate_width_scaling`

```text
last / first > 2
and
correlation(width, total) > 0.5
```

## `flat_or_saturated`

```text
last / first < 1.5
and
total activity is nonzero
```

## `active_mixed`

Any nonzero behavior not matching the other rules.

## First width equals zero

When width 128 is zero and width 2048 is nonzero:

```text
scaling_ratio_last_over_first = infinity
```

Because the ratio is not finite, the counter is normally classified as
`active_mixed`, even when it activates only at larger widths.

---

# Important interpretation caution

A strong scaling counter is not automatically a bottleneck.

It may count:

- useful instructions;
- transactions proportional to element count;
- pixels or quads;
- cache requests;
- memory beats;
- synchronization operations;
- occupancy;
- or another event that naturally grows with workload size.

A bottleneck conclusion requires combining:

```text
counter scaling
runtime
busy cycles
stall/starvation counters
frequency
cross-run stability
workload knowledge
```

---

# Inspected single-run source

The inspected CSV paths point to:

```text
sweep_20260625_152944
```

and fourteen groups:

```text
CP
RBBM
PC
VFD
HLSQ
VPC
TSE
RAS
UCHE
TP
SP
RB
LRZ
ALWAYSON
```

The stored `csv_path` columns contain historical absolute paths such as:

```text
/Users/jerryyun/adreno-gpu-profiler/results/perfcounter_sweeps/
sweep_20260625_152944/...
```

After repository reorganization, the source sweep may instead be under:

```text
results/perfcounter_sweeps/full_sweeps/
```

Treat `csv_path` as historical provenance, not a guaranteed current live path.

---

# `latest_width_sequence/`

## Purpose

Legacy single-run local-window analysis using the default-style threshold.

## Region-detection quality

```text
chunk CSVs examined:       141
correct five-region files:   4
failed files:              137
success rate:             2.84%
```

The four successful files were:

| Group | Chunk | Counters | File |
|---|---:|---:|---|
| `CP` | 4 | 14 | `CP_chunk004.csv` |
| `CP` | 6 | 14 | `CP_chunk006.csv` |
| `TSE` | 16 | 4 | `TSE_chunk016.csv` |
| `RAS` | 2 | 4 | `RAS_chunk002.csv` |

## Summary rows

```text
counter_width_summary rows:   1,344
region-detected rows:           180
fallback whole-file rows:     1,164
```

The 180 detected rows correspond to:

```text
36 counters × 5 widths
```

## Classified counters

```text
total classified:       36
zero all widths:        19
flat or saturated:      14
active mixed:            3
moderate scaling:        0
strong scaling:          0
```

`strong_width_scaling_counters.csv` is header-only.

## Interpretation

This output mainly demonstrates that independent local detection is too fragile
for the full sweep.

It should not be used as the primary counter-classification result.

---

# `latest_width_sequence_thr03/`

## Purpose

A lower-threshold local-detection experiment.

The name indicates:

```text
threshold fraction ≈ 0.03
```

rather than the default 0.08-style threshold.

The exact command is not embedded in the output and should be recovered from
shell history or experiment notes.

## Region-detection quality

```text
chunk CSVs examined:       141
correct five-region files:   4
failed files:              137
success rate:             2.84%
```

Successful files:

| Group | Chunk | Counters | File |
|---|---:|---:|---|
| `CP` | 1 | 14 | `CP_chunk001.csv` |
| `CP` | 3 | 14 | `CP_chunk003.csv` |
| `CP` | 4 | 14 | `CP_chunk004.csv` |
| `RB` | 5 | 8 | `RB_chunk005.csv` |

The count of successful files did not increase, but the successful files contain
more counters.

## Classified counters

```text
total classified:       50
zero all widths:        15
flat or saturated:      27
active mixed:            5
moderate scaling:        3
strong scaling:          0
```

## Scaling file naming issue

`strong_width_scaling_counters.csv` contains both:

```text
strong_width_scaling
moderate_width_scaling
```

because the legacy script writes both classes to that filename.

The three rows in this directory are all moderate CP counters:

```text
CP_SQE_T4_EXEC
CP_SQE_SAVE_SDS_STATE
CP_SQE_CTXT_REG_BUNCH_EXEC
```

The values contain zeros at widths 256 and 512, so this is not strong evidence
of smooth workload scaling.

## Interpretation

Lowering the threshold changes which chunks pass but does not make local
detection broadly reliable.

---

# `latest_width_sequence_ref_windows/`

## Purpose

Single-run reference-window analysis across all chunk CSVs.

This is the strongest inspected single-run output because one reliable timeline
defines the five windows for every counter.

## Detected reference windows

| Region | Width | Start index | End index exclusive | Start (s) | End (s) | Samples in reference |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 128 | 27 | 69 | 0.049915 | 0.112017 | 42 |
| 2 | 256 | 157 | 198 | 0.248662 | 0.307354 | 41 |
| 3 | 512 | 285 | 326 | 0.441789 | 0.500608 | 41 |
| 4 | 1024 | 411 | 452 | 0.634295 | 0.693474 | 41 |
| 5 | 2048 | 549 | 590 | 0.844253 | 0.902618 | 41 |

The bursts are separated by roughly 0.14–0.15 seconds.

Each active reference window contains approximately 41–42 samples.

Target chunk files can contain a slightly different number of samples inside the
same time boundaries because individual CSV timelines and sampling jitter differ.

## Coverage

```text
counter classifications: 1,200
per-width summary rows:   6,000
widths per counter:           5
groups represented:          14
```

## Classification counts

```text
zero all widths:        856
flat or saturated:      171
active mixed:           38
moderate scaling:       63
strong scaling:         72
```

The width-scaling subset contains:

```text
135 counters
```

and includes:

```text
72 strong
63 moderate
```

## Scaling counters by group

| Group | Strong | Moderate | Total scaling |
|---|---:|---:|---:|
| `CP` | 0 | 5 | 5 |
| `HLSQ` | 0 | 1 | 1 |
| `LRZ` | 2 | 0 | 2 |
| `PC` | 0 | 2 | 2 |
| `RB` | 0 | 2 | 2 |
| `RBBM` | 0 | 5 | 5 |
| `SP` | 17 | 46 | 63 |
| `TP` | 25 | 1 | 26 |
| `UCHE` | 28 | 0 | 28 |
| `VFD` | 0 | 1 | 1 |

SP, TP, and UCHE provide most of the scaling counters.

---

# Near-16× counters in the inspected reference analysis

Using the repeated-analysis-style criterion:

```text
15 ≤ ratio ≤ 17
correlation ≥ 0.95
```

the inspected single-run classification contains:

```text
45 counters
```

Examples with the largest all-width totals:

| Group | Counter | 2048/128 ratio | Correlation |
|---|---|---:|---:|
| `TP` | `TP_FILTER_POINT_FP32` | 16.000× | 1.000000 |
| `TP` | `TP_FILTER_WORKLOAD_32BIT` | 16.000× | 1.000000 |
| `TP` | `TP_OUTPUT_PIXELS` | 16.000× | 1.000000 |
| `TP` | `TP_OUTPUT_PIXELS_POINT` | 16.000× | 1.000000 |
| `TP` | `TP_OUTPUT_PIXELS_ZERO_LOD` | 16.000× | 1.000000 |
| `SP` | `SP_LOW_EFFICIENCY_STARVED_BY_TP` | 15.533× | 0.954833 |
| `TP` | `TP_L1_5_MISS_LATENCY_CYCLES` | 15.817× | 0.970135 |
| `SP` | `SP_FULL_ALU_MUL_INSTRUCTIONS` | 16.000× | 1.000000 |
| `TP` | `TP_TP_SP_TRANS` | 16.000× | 1.000000 |
| `UCHE` | `UCHE_BUSY_CYCLES` | 15.911× | 0.998969 |
| `SP` | `SP_FS_STAGE_TEX_INSTRUCTIONS` | 16.000× | 1.000000 |
| `SP` | `SP_GM_STORE_INSTRUCTIONS` | 16.000× | 1.000000 |
| `SP` | `SP_TEXTURE_FETCH_LATENCY_SAMPLES` | 16.000× | 1.000000 |
| `TP` | `TP_QUADS_BUFFER` | 16.000× | 1.000000 |
| `TP` | `TP_QUADS_RECEIVED` | 16.000× | 1.000000 |
| `UCHE` | `UCHE_WRITE_REQUESTS_SP` | 16.000× | 1.000000 |
| `UCHE` | `UCHE_RAM_READ_REQ` | 16.206× | 0.999991 |
| `UCHE` | `UCHE_READ_REQUESTS_TP` | 16.517× | 0.999911 |
| `UCHE` | `UCHE_READ_REQUESTS_TP_GBIF` | 16.517× | 0.999911 |
| `SP` | `SP_CCHE_NONUAV_TOTAL_DUALQUAD` | 16.000× | 1.000000 |
| `SP` | `SP_CCHE_UAV_TOTAL_DUALQUAD` | 16.000× | 1.000000 |
| `SP` | `SP_CCHE_UAV_TOTAL_REQ` | 16.000× | 1.000000 |
| `SP` | `SP_UCHE_WRITE_TRANS` | 16.000× | 1.000000 |
| `TP` | `TP_BACKEND_WORKING_CYCLES` | 16.000× | 1.000000 |
| `TP` | `TP_FRONTEND_WORKING_CYCLES` | 16.000× | 1.000000 |

## Interpretation

Exact or near-exact 16× behavior in counters such as:

```text
SP_FULL_ALU_MUL_INSTRUCTIONS
SP_GM_STORE_INSTRUCTIONS
TP_OUTPUT_PIXELS
TP_L1_CACHELINE_REQUESTS
UCHE_WRITE_REQUESTS_SP
UCHE_RAM_READ_REQ
UCHE_BUSY_CYCLES
```

shows that the width windows are capturing workload-proportional activity.

It does not by itself establish that each of these resources limits performance.

---

# `latest_width_sequence_ref_windows_sp/`

## Purpose

An SP-reference analysis variant.

The output schema matches:

```text
latest_width_sequence_ref_windows/
```

Use it to determine whether explicitly choosing an SP reference changes:

- detected start/end times;
- per-width totals;
- number of scaling counters;
- counter labels; or
- near-16× results.

## Recommended comparison

Compare by `(group, counter)`:

```bash
python3 - <<'PY'
import pandas as pd

root = "results/perfcounter_analysis"

a = pd.read_csv(
    f"{root}/latest_width_sequence_ref_windows/"
    "counter_classification.csv"
)

b = pd.read_csv(
    f"{root}/latest_width_sequence_ref_windows_sp/"
    "counter_classification.csv"
)

keys = ["group", "counter"]

joined = a.merge(
    b,
    on=keys,
    suffixes=("_base", "_sp"),
)

changed = joined[
    joined["label_base"] != joined["label_sp"]
]

print(changed[keys + ["label_base", "label_sp"]].to_string(index=False))
PY
```

---

# `latest_width_sequence_ref_windows_sp_pad20/`

## Purpose

An SP-reference analysis with wider window padding.

The directory name indicates approximately:

```text
pad samples = 20
```

Wider padding can:

- include startup and tail samples;
- capture counter activity that lags the reference;
- increase totals;
- reduce sensitivity to slight timing offsets; and
- also include idle or neighboring activity.

## Recommended use

Treat this as a sensitivity test.

A counter is more trustworthy when its broad trend remains similar across
reasonable padding values.

---

# `repro_width_sequence_latest2/`

## Purpose

Repeated reference-window analysis across the latest two selected sweeps.

The inspected window file contains:

```text
sweep_20260625_152944
sweep_20260625_174559
```

## Reference windows

| Run | Region | Width | Start (s) | End (s) | Samples |
|---|---:|---:|---:|---:|---:|
| `sweep_20260625_152944` | 1 | 128 | 0.049915 | 0.112017 | 42 |
| `sweep_20260625_152944` | 2 | 256 | 0.248662 | 0.307354 | 41 |
| `sweep_20260625_152944` | 3 | 512 | 0.441789 | 0.500608 | 41 |
| `sweep_20260625_152944` | 4 | 1024 | 0.634295 | 0.693474 | 41 |
| `sweep_20260625_152944` | 5 | 2048 | 0.844253 | 0.902618 | 41 |
| `sweep_20260625_174559` | 1 | 128 | 0.057689 | 0.119569 | 42 |
| `sweep_20260625_174559` | 2 | 256 | 0.262054 | 0.320504 | 41 |
| `sweep_20260625_174559` | 3 | 512 | 0.468723 | 0.527212 | 41 |
| `sweep_20260625_174559` | 4 | 1024 | 0.675249 | 0.735577 | 42 |
| `sweep_20260625_174559` | 5 | 2048 | 0.880872 | 0.940251 | 41 |

The second run's windows are shifted later by approximately 8–37 ms, but the
width order and window lengths remain consistent.

## Expected files

### `all_runs_counter_width_summary.csv`

One per-run, per-counter, per-width row.

Adds:

```text
run
```

to the single-run summary schema.

### `all_runs_counter_classification.csv`

One classification row for each:

```text
run × group × counter
```

### `all_runs_reference_windows.csv`

Five reference windows per accepted run.

### `cross_run_stability_summary.csv`

One row per `(group, counter)` containing:

```text
n_runs
majority_label
majority_label_count
label_consistency
ratio_mean
ratio_std
ratio_cv
corr_mean
corr_std
total_all_mean
total_all_std
per-width means
per-width standard deviations
per-width coefficients of variation
stable_strong_scaling
near_16x_scaling
```

### `stable_width_scaling_counters.csv`

Counters that satisfy:

```text
majority label is strong or moderate scaling
label consistency ≥ 0.67
ratio CV ≤ 0.25
mean correlation ≥ 0.90
```

### `stable_near_16x_counters.csv`

Counters that satisfy:

```text
15 ≤ mean ratio ≤ 17
mean correlation ≥ 0.95
```

### `stable_zero_counters.csv`

Counters classified as zero in the majority of runs.

### `reproducibility_report.md`

Human-readable report generated from the repeated reference-window results.

---

# `.Rhistory` issue

The file:

```text
repro_width_sequence_latest2/.Rhistory
```

is not a normal R command history.

Its contents are an incomplete Markdown report scaffold titled:

```text
Fused Softmax Width-Sequence Reproducibility Report
```

It contains:

- incomplete fenced code blocks;
- headings without generated tables;
- a duplicated interpretation sentence; and
- no R commands.

Recommended action:

```text
rename it to reproducibility_report_draft.md
or
delete it if it was created accidentally
```

Do not treat it as analysis input.

The real report file is:

```text
reproducibility_report.md
```

---

# `repro_width_sequence_latest2_local_windows/`

## Purpose

Repeated-run analysis where every chunk detects its own local windows.

Expected outputs:

```text
local_region_detection_quality.csv
local_width_windows.csv
all_runs_counter_width_summary.csv
all_runs_counter_classification.csv
cross_run_stability_summary.csv
stable_width_scaling_counters.csv
stable_near_16x_counters.csv
```

## Role

Use this directory as an independent check of the repeated shared-reference
method.

A counter trend is more convincing when it appears under both:

```text
shared reference windows
local per-file windows
```

## Limitation

Local detection can fail for many sparse or zero-heavy chunks.

Failed files are recorded in the quality table but excluded from classification.

The local repeated analysis can also use different time boundaries for different
counter chunks, making totals less directly controlled than shared-reference
totals.

---

# File schemas

# `region_detection_quality.csv`

| Column | Meaning |
|---|---|
| `csv_path` | Historical path to source chunk CSV |
| `group` | Parsed perf-counter group |
| `chunk` | Numeric chunk index |
| `n_rows` | Number of raw samples |
| `n_counters` | Number of counter columns |
| `n_regions_detected` | Number of candidate workload bursts |
| `expected_regions` | Number of requested widths |
| `region_ok` | Whether detected count equals expected count |

---

# `reference_width_windows.csv`

| Column | Meaning |
|---|---|
| `region_index` | Chronological burst number |
| `width` | Assigned softmax width |
| `start_idx` | Start row in reference CSV |
| `end_idx_exclusive` | Exclusive end row in reference CSV |
| `start_s` | Reference start time |
| `end_s` | Reference end time |
| `samples` | Samples in the reference window |

---

# `counter_width_summary.csv`

| Column | Meaning |
|---|---|
| `group` | Counter group |
| `chunk` | Source chunk number |
| `csv_file` | Source filename in legacy single-run outputs |
| `csv_path` | Historical source path |
| `counter` | Counter name |
| `width` | Assigned width, or `-1` for fallback |
| `region_index` | Region number, or `-1` for fallback |
| `region_start_s` | Window start |
| `region_end_s` | Window end |
| `samples` | Samples included |
| `total` | Sum of deltas |
| `max` | Largest sample delta |
| `mean` | Mean over all included samples |
| `active_mean` | Mean over nonzero samples |
| `active_samples` | Count of nonzero samples |
| `detection_status` | Local-analysis status; absent in reference output |

---

# `counter_classification.csv`

| Column | Meaning |
|---|---|
| `group` | Counter group |
| `counter` | Counter name |
| `label` | Heuristic class |
| `total_all_widths` | Sum across five widths |
| `total_width_first` | Width-128 total |
| `total_width_last` | Width-2048 total |
| `scaling_ratio_last_over_first` | 2048 total divided by 128 total |
| `corr_total_vs_width` | Pearson correlation between width and total |
| `slope_total_vs_width` | Linear-fit slope |
| `total_w*` | Total at one width |

Repeated local outputs may omit the slope column.

---

# Subset files

## `zero_counters.csv`

Rows from the classification where:

```text
label == zero_all_widths
```

## `width_scaling_counters.csv`

Rows where:

```text
label is strong_width_scaling or moderate_width_scaling
```

## `strong_width_scaling_counters.csv`

In the legacy local analyzer, this filename also contains moderate scaling rows.

## `flat_or_saturated_counters.csv`

Rows where:

```text
label == flat_or_saturated
```

---

# Integrity inventory for inspected files

The following inspected files were matched to their repository directories.

| Repository-relative path | Bytes | Rows | Columns | SHA-256 |
|---|---:|---:|---:|---|
| `latest_width_sequence/counter_classification.csv` | 4,619 | 36 | 14 | `d3390e0eba6a405db5fe40091c2ef70b136d3d9d5721efad7ece964014097732` |
| `latest_width_sequence/counter_width_summary.csv` | 309,940 | 1,344 | 16 | `8a304bdb21436156e689ce47a6c345309415ba224a29ecf63f4ecf311f7430f2` |
| `latest_width_sequence/flat_or_saturated_counters.csv` | 2,675 | 14 | 14 | `7db6663a982998ff16ae336134ad1bbecf2db4a9c2d379b691cc1aa8b9cc7708` |
| `latest_width_sequence/region_detection_quality.csv` | 18,761 | 141 | 8 | `7afb467dbeb7ff14a099a5216e3349389de187600d03a818bec6da4c4890bc71` |
| `latest_width_sequence/strong_width_scaling_counters.csv` | 200 | 0 | 14 | `72df0d7ce33f69b6d2bdccae9ac5e9173254b69cca333423ed398bb9c37eb2fd` |
| `latest_width_sequence/zero_counters.csv` | 1,678 | 19 | 14 | `5417205704fe6ee1899daa5a54a55f6f21aa6f2bd28c218a6ade8cace7a6d95b` |
| `latest_width_sequence_thr03/counter_classification.csv` | 7,236 | 50 | 14 | `60201422c4c0330f7d4140ea6f9a801615f844662aab57ebb79f3731a2118cfe` |
| `latest_width_sequence_thr03/counter_width_summary.csv` | 322,360 | 1,400 | 16 | `f0fd46e29280706285936498699959baf259d0cdcc2e0688a847ca29913d5ba8` |
| `latest_width_sequence_thr03/flat_or_saturated_counters.csv` | 4,712 | 27 | 14 | `e75618a805b9349f35bcb696ef31677707cc086fe60a762cc8563be2e4d1ce75` |
| `latest_width_sequence_thr03/region_detection_quality.csv` | 18,761 | 141 | 8 | `9c60a6ff9e51563deb00e6faeb91eaf58c85dd7b746d980d8a4ff89300609fc7` |
| `latest_width_sequence_thr03/strong_width_scaling_counters.csv` | 644 | 3 | 14 | `ee5bacaff41d78bde15feef646cbf691eccdee50b2e4dbb8c945ccd60d3728d3` |
| `latest_width_sequence_thr03/zero_counters.csv` | 1,390 | 15 | 14 | `a85b37c0598a90f4b35eb9ee74c7fb549df69b75a3a9fc32db610ed4757fc356` |
| `latest_width_sequence_ref_windows/counter_classification.csv` | 123,085 | 1,200 | 14 | `b5414fd6510a94c44c6f3de42f80c3ba9864d5c3ded08d296982696572fa0a80` |
| `latest_width_sequence_ref_windows/counter_width_summary.csv` | 1,240,811 | 6,000 | 15 | `20bb75b9717dcee134a965529d24b4d401e919bd47f7882ad9f76c479aa83d15` |
| `latest_width_sequence_ref_windows/flat_or_saturated_counters.csv` | 25,078 | 171 | 14 | `8005abad45293929cb0be2a176224f509f5f77b7cf1e10e0fbea6925db7463bc` |
| `latest_width_sequence_ref_windows/reference_width_windows.csv` | 244 | 5 | 7 | `45d636f92c4d8c01eb42b72462d9c586b38147c848b5de3e18561ff3cccfc784` |
| `latest_width_sequence_ref_windows/width_scaling_counters.csv` | 22,591 | 135 | 14 | `1b99ab41e3caa10bc2f2feb858fc58335427a923bfa6b4b415a9644e703100f6` |
| `latest_width_sequence_ref_windows/zero_counters.csv` | 69,699 | 856 | 14 | `6bad7755663b5d162cb472f7b1f7bacd04a4a8f8b567b40f6724fcb37300f8bd` |
| `repro_width_sequence_latest2/.Rhistory` | 1,584 |  |  | `bc61e99ea83e3bdf1460a9f3f2887eb8ea2e7c89ff8e7eef981fa7c0b7bfae25` |
| `repro_width_sequence_latest2/all_runs_reference_windows.csv` | 643 | 10 | 8 | `dfc338123cc970ccc033e7ab99d4123f5a51fe71805c3d31a759a477acef7455` |

Files not present in the uploaded inspection set are intentionally omitted from
this hash table.

Verify the current repository files with:

```bash
find results/perfcounter_analysis   -type f   ! -name '.DS_Store'   -print0   | sort -z   | xargs -0 shasum -a 256
```

---

# Reproduce the legacy local analysis

From the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

SWEEP="results/perfcounter_sweeps/full_sweeps/sweep_20260625_152944"
```

Default-style local detection:

```bash
python3   analysis/perfcounter_width_sequence/analyze_width_sequence.py   --sweep-dir "$SWEEP"   --widths 128,256,512,1024,2048   --out-dir results/perfcounter_analysis/latest_width_sequence   --threshold-frac 0.08   --min-active-samples 2
```

Threshold-0.03 experiment:

```bash
python3   analysis/perfcounter_width_sequence/analyze_width_sequence.py   --sweep-dir "$SWEEP"   --widths 128,256,512,1024,2048   --out-dir results/perfcounter_analysis/latest_width_sequence_thr03   --threshold-frac 0.03   --min-active-samples 2
```

Do not overwrite historical directories during verification.

Use:

```bash
--out-dir /tmp/perfcounter_analysis_check
```

then compare.

---

# Reproduce single-run reference-window analysis

```bash
python3   analysis/perfcounter_width_sequence/analyze_width_sequence_ref_windows.py   --sweep-dir "$SWEEP"   --reference-csv "$SWEEP/11_SP/SP_chunk001.csv"   --widths 128,256,512,1024,2048   --out-dir     results/perfcounter_analysis/latest_width_sequence_ref_windows   --threshold-frac 0.08   --min-active-samples 2   --pad-samples 1
```

A pad-20 sensitivity run:

```bash
python3   analysis/perfcounter_width_sequence/analyze_width_sequence_ref_windows.py   --sweep-dir "$SWEEP"   --reference-csv "$SWEEP/11_SP/SP_chunk001.csv"   --widths 128,256,512,1024,2048   --out-dir     results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20   --threshold-frac 0.08   --min-active-samples 1   --pad-samples 20
```

---

# Reproduce repeated reference-window analysis

```bash
python3   analysis/perfcounter_width_sequence/analyze_repeated_width_sweeps.py   --sweeps-root results/perfcounter_sweeps/full_sweeps   --latest-n 2   --widths 128,256,512,1024,2048   --reference-relpath 11_SP/SP_chunk001.csv   --out-dir     results/perfcounter_analysis/repro_width_sequence_latest2   --threshold-frac 0.08   --min-active-samples 1   --pad-samples 20
```

The script selects “latest” using directory modification time.

Copying or editing a sweep directory can therefore change which runs are chosen.

For formal reproducibility, explicitly record the selected sweep names in
metadata.

---

# Reproduce repeated local-window analysis

```bash
python3   analysis/perfcounter_width_sequence/analyze_repeated_width_sweeps_local_windows.py   --sweeps-root results/perfcounter_sweeps/full_sweeps   --latest-n 2   --widths 128,256,512,1024,2048   --out-dir     results/perfcounter_analysis/repro_width_sequence_latest2_local_windows   --threshold-frac 0.08   --min-active-samples 1   --pad-samples 10   --groups SP,TP,UCHE,HLSQ,RBBM,CP
```

---

# Generate the reproducibility report

```bash
python3   analysis/perfcounter_width_sequence/make_repro_report.py   --analysis-dir     results/perfcounter_analysis/repro_width_sequence_latest2   --out     results/perfcounter_analysis/repro_width_sequence_latest2/reproducibility_report.md
```

The reporting script is experiment-specific and assumes fused-softmax widths
128–2048.

---

# Inspect classification counts

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_analysis/"
    "latest_width_sequence_ref_windows/"
    "counter_classification.csv"
)

frame = pd.read_csv(path)

print(frame["label"].value_counts().to_string())
PY
```

---

# List strongest scaling counters

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_analysis/"
    "latest_width_sequence_ref_windows/"
    "width_scaling_counters.csv"
)

frame = pd.read_csv(path)

columns = [
    "group",
    "counter",
    "label",
    "scaling_ratio_last_over_first",
    "corr_total_vs_width",
    "total_w128",
    "total_w2048",
]

print(
    frame
    .sort_values(
        ["label", "corr_total_vs_width"],
        ascending=[True, False],
    )[columns]
    .head(100)
    .to_string(index=False)
)
PY
```

---

# Find near-16× counters

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_analysis/"
    "latest_width_sequence_ref_windows/"
    "counter_classification.csv"
)

frame = pd.read_csv(path)

near = frame[
    frame["scaling_ratio_last_over_first"].between(15, 17)
    & (frame["corr_total_vs_width"] >= 0.95)
]

print(
    near[
        [
            "group",
            "counter",
            "scaling_ratio_last_over_first",
            "corr_total_vs_width",
        ]
    ]
    .sort_values(
        "scaling_ratio_last_over_first"
    )
    .to_string(index=False)
)
PY
```

---

# Compare padding experiments

```bash
python3 - <<'PY'
import pandas as pd

root = "results/perfcounter_analysis"

base = pd.read_csv(
    f"{root}/latest_width_sequence_ref_windows_sp/"
    "counter_classification.csv"
)

pad20 = pd.read_csv(
    f"{root}/latest_width_sequence_ref_windows_sp_pad20/"
    "counter_classification.csv"
)

keys = ["group", "counter"]

joined = base.merge(
    pad20,
    on=keys,
    suffixes=("_base", "_pad20"),
)

joined["ratio_change"] = (
    joined["scaling_ratio_last_over_first_pad20"]
    - joined["scaling_ratio_last_over_first_base"]
)

joined["label_changed"] = (
    joined["label_base"] != joined["label_pad20"]
)

print(
    joined[
        joined["label_changed"]
        | joined["ratio_change"].abs().gt(1)
    ][
        keys
        + [
            "label_base",
            "label_pad20",
            "scaling_ratio_last_over_first_base",
            "scaling_ratio_last_over_first_pad20",
        ]
    ]
    .to_string(index=False)
)
PY
```

---

# Compare reference and local repeated methods

```bash
python3 - <<'PY'
import pandas as pd

root = "results/perfcounter_analysis"

reference = pd.read_csv(
    f"{root}/repro_width_sequence_latest2/"
    "stable_width_scaling_counters.csv"
)

local = pd.read_csv(
    f"{root}/repro_width_sequence_latest2_local_windows/"
    "stable_width_scaling_counters.csv"
)

keys = ["group", "counter"]

both = (
    reference[keys]
    .drop_duplicates()
    .merge(
        local[keys].drop_duplicates(),
        on=keys,
        how="inner",
    )
    .sort_values(keys)
)

print(both.to_string(index=False))
PY
```

---

# Audit local detection quality

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_analysis/"
    "latest_width_sequence/"
    "region_detection_quality.csv"
)

quality = pd.read_csv(path)

print("Overall:")
print(quality["region_ok"].value_counts().to_string())

print("\nBy group:")
print(
    quality
    .groupby("group")["region_ok"]
    .agg(["count", "sum", "mean"])
    .sort_values("mean", ascending=False)
    .to_string()
)
PY
```

---

# Find stale absolute paths

```bash
grep -R   '/Users/jerryyun/adreno-gpu-profiler/results/perfcounter_sweeps/'   results/perfcounter_analysis   --include='*.csv'   -l
```

Do not manually rewrite historical `csv_path` columns unless creating a new
derived copy.

The path is provenance from the original analysis environment.

---

# Remove accidental platform files

The directory contains:

```text
.DS_Store
```

Remove it:

```bash
rm -f results/perfcounter_analysis/.DS_Store
```

Ensure `.gitignore` contains:

```gitignore
.DS_Store
```

Review whether `.Rhistory` is tracked:

```bash
git status --short --   results/perfcounter_analysis/repro_width_sequence_latest2/.Rhistory
```

Rename or remove it separately after confirming that no useful text exists only
there.

---

# Toolchain requirements

The analysis scripts require:

```text
Python 3
pandas
NumPy
```

Create a local environment:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

python3 -m venv .venv
source .venv/bin/activate

python3 -m pip install --upgrade pip
python3 -m pip install pandas numpy
```

Check versions:

```bash
python3 --version

python3 - <<'PY'
import numpy
import pandas

print("NumPy:", numpy.__version__)
print("pandas:", pandas.__version__)
PY
```

---

# Main findings

# 1. Independent local detection is not reliable across the full sweep

Both inspected local-analysis variants detect all five widths in only:

```text
4 of 141 chunk files
```

Most counter chunks are:

- zero;
- sparse;
- flat;
- active only during some widths;
- shifted relative to other chunks; or
- unsuitable for self-detecting all five bursts.

This is why local classification covers only 36–50 counters.

# 2. Reference windows provide complete single-run coverage

The reference method summarizes:

```text
1,200 counters
5 widths each
6,000 per-width rows
```

without requiring each chunk to detect its own activity.

# 3. The width sequence is visible in many counters

The reference analysis identifies:

```text
72 strong scaling counters
63 moderate scaling counters
45 near-16× high-correlation counters
```

This supports the correctness of the width-region assignment.

# 4. SP, TP, and UCHE dominate width-scaling results

The scaling subset includes many counters related to:

- SP instructions and wave activity;
- TP output/cache activity;
- UCHE reads, writes, bank requests, and busy cycles.

# 5. Scaling does not equal bottleneck

Some exact 16× counters count expected useful work.

A memory/cache bottleneck claim requires additional evidence from:

- runtime;
- stalls;
- starvation;
- latency;
- busy cycles;
- repeated-run stability; and
- benchmark design.

# 6. Repeated runs use consistent width timing

The two inspected reference-window runs contain five windows in the same order
with similar durations.

This is a necessary condition for reproducibility, but the full stability tables
must be inspected for per-counter consistency.

---

# What these results do not prove

The analyses do not prove:

- every selector is correctly mapped;
- every zero counter is unsupported;
- every strong counter is a bottleneck;
- all chunk timelines are perfectly aligned;
- the reference CSV is optimal;
- pad 20 is better than pad 1;
- the local detector is universally unusable;
- a correlation is causal;
- the fused-softmax kernel is only memory-bound;
- all widths used identical GPU frequency;
- all runs used identical thermal state; or
- “latest” always identifies the intended sweep directories.

---

# Known limitations

## Heuristic labels

The thresholds are fixed and not statistically learned.

## Ratio instability when the first value is small

A tiny width-128 total can create a very large ratio.

## Infinite ratio when the first total is zero

Such counters are normally classified as `active_mixed`.

## Correlation uses only five points

Pearson correlation can appear strong with a small number of widths.

## Shared-reference assumption

Reference times are applied to all chunks.

## Local-detection failure

Sparse and zero chunks cannot self-detect five bursts.

## Sampling jitter

Different chunk files can include different sample counts inside the same time
window.

## Window padding sensitivity

Totals can change with padding.

## Absolute path drift

Output CSVs retain old source paths.

## “Latest” is unstable

Directory modification time can change after copying or editing.

## No embedded analysis command

Most output directories do not contain the full CLI or tool versions.

## No input hashes

The outputs do not identify hashes of source sweep CSV files.

## Duplicate parameter experiments

Several `latest_*` directories differ only by reference or threshold settings.

## Accidental files

`.DS_Store` and the Markdown-like `.Rhistory` are unrelated to the actual
analysis pipeline.

---

# Recommended maintenance

1. Add this README.
2. Remove `.DS_Store`.
3. Rename or remove the accidental `.Rhistory`.
4. Preserve historical output directories unchanged.
5. Add `analysis_config.json` to every new output directory.
6. Record:
   - script path;
   - script Git commit;
   - complete CLI;
   - source sweep names;
   - source sweep hashes;
   - pandas/NumPy versions;
   - generation timestamp.
7. Replace ambiguous `latest_*` names with sweep IDs and parameter names.
8. Keep a small `latest` symlink or text pointer instead of copying results.
9. Prefer reference-window analysis for complete single-run results.
10. Use local-window analysis as a quality audit and independent cross-check.
11. Compare results across padding values before interpreting marginal counters.
12. Report both strong and moderate scaling separately.
13. Rename legacy `strong_width_scaling_counters.csv` to
    `width_scaling_counters.csv`.
14. Add explicit handling for first-width-zero counters.
15. Add confidence intervals or repeated-run statistics before making final
    claims.
16. Add runtime and frequency metadata.
17. Add a report explaining why specific counters imply workload scaling versus
    bottleneck behavior.
18. Update paths after repository reorganization only in new derived outputs.
19. Store selected final analyses under `evidence/` with a manifest.
20. Keep raw sweeper outputs separate from derived tables.

---

# Suggested future structure

```text
results/perfcounter_analysis/
├── README.md
├── single_run/
│   └── sweep_20260625_152944/
│       ├── local_thr008/
│       ├── local_thr003/
│       ├── ref_sp_pad001/
│       └── ref_sp_pad020/
├── repeated/
│   └── sweeps_20260625_152944_20260625_174559/
│       ├── reference_windows/
│       └── local_windows/
└── latest.txt
```

Each leaf directory should contain:

```text
analysis_config.json
input_hashes.txt
generation.log
output CSV files
human-readable report
```

---

# Quick summary

```text
legacy local detection:
    4 / 141 chunks detect all five widths
    36 counters classified
    no scaling counters in the default result

threshold-0.03 local detection:
    4 / 141 chunks detect all five widths
    50 counters classified
    3 moderate CP counters

single-run reference-window analysis:
    1,200 counters classified
    6,000 per-width rows
    72 strong scaling counters
    63 moderate scaling counters
    45 near-16× counters with correlation ≥ 0.95

repeated reference analysis:
    sweep_20260625_152944
    sweep_20260625_174559
    five consistent width windows per run
```

The primary conclusion is:

```text
Shared SP reference windows provide far more complete and interpretable
width-sequence analysis than independent local detection for every chunk.
```

The strongest next interpretation step is to use the repeated-run stability
tables and cross-check stable counters against the repeated local-window method.
