# Perf-Counter Sweep Results

This directory contains the raw multi-group output produced by the Adreno
perf-counter sweeper.

```text
results/perfcounter_sweeps/
├── README.md
├── full_sweeps/
│   ├── sweep_<timestamp>/
│   └── ...
└── width_sweeps/
    └── by_width/
        ├── width_128/
        ├── width_256/
        ├── width_512/
        ├── width_1024/
        └── width_2048/
```

The directory is the main raw-data boundary between capture and analysis:

```text
Vulkan benchmark
        ↓
tools/profiling/perfcounter_sweeper/streamer_sweeper
        ↓
results/perfcounter_sweeps/
        ↓
analysis/perfcounter_width_sequence/
        ↓
results/perfcounter_analysis/
```

The files here are not source code and are not required to build the streamer or
sweeper. They are the measurements produced by those tools.

---

# Purpose

The sweeper exists because an Adreno counter group can expose more countables
than the hardware can activate simultaneously.

For one group:

```text
all countables
      ↓
split into chunks based on physical counter-slot capacity
      ↓
activate one chunk
      ↓
run the same benchmark
      ↓
record counter deltas
      ↓
release counters
      ↓
repeat for the next chunk
```

A complete sweep repeats the benchmark many times so that every selected
counter is measured at least once.

The output preserves:

- group and chunk identity;
- selector ranges;
- active counter names;
- physical low/high register assignments;
- requested sampling duration and interval;
- raw per-sample counter deltas;
- benchmark command;
- benchmark stdout;
- verification status;
- benchmark exit status; and
- one sweep-level summary.

---

# Relationship to the main profiler tools

The main runtime tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

## Streamer

The streamer samples a selected set of counters continuously.

Use it for:

- manual counter inspection;
- calibration;
- focused experiments;
- live CSV capture; and
- validating one or several known counters.

## Sweeper

The sweeper automates:

- counter-group enumeration;
- chunking based on hardware capacity;
- repeated `PERFCOUNTER_GET`;
- benchmark execution;
- CSV collection;
- per-chunk metadata;
- benchmark-log capture;
- `PERFCOUNTER_PUT`; and
- sweep-level indexing.

The files in this directory are primarily sweeper outputs.

## Analysis

The scripts under:

```text
analysis/perfcounter_width_sequence/
```

consume chunk CSV files after collection has completed.

They do not access KGSL directly.

---

# Reorganized layout

Before repository cleanup, sweep directories were nested under duplicated names
such as:

```text
results/perfcounter_sweeps/perfcounter_sweeps/
results/perfcounter_sweeps/perfcounter_sweeps_widths/
```

The reorganized layout is:

```text
results/perfcounter_sweeps/full_sweeps/
results/perfcounter_sweeps/width_sweeps/by_width/
```

The file contents were moved without intentionally changing their bytes.

Historical paths inside metadata and summary files still refer to the original
device-side location:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps/
```

Those paths are provenance, not current host paths.

---

# Top-level collections

# `full_sweeps/`

Contains timestamped sweeps that can include several or all supported counter
groups.

Known historical examples include:

```text
sweep_20260624_124835
sweep_20260624_145250
sweep_20260624_145305
sweep_20260625_130734
sweep_20260625_152944
sweep_20260625_174559
```

A later full sweep commonly has this structure:

```text
sweep_<timestamp>/
├── 01_CP/
├── 02_RBBM/
├── 03_PC/
├── 04_VFD/
├── 05_HLSQ/
├── 06_VPC/
├── 07_TSE/
├── 08_RAS/
├── 09_UCHE/
├── 10_TP/
├── 11_SP/
├── 12_RB/
├── 13_LRZ/
├── 14_ALWAYSON/
├── run_config.txt
└── summary.csv
```

The exact group set varies by sweep version and configuration.

The inspected historical sweep:

```text
sweep_20260624_124835
```

contains only groups CP through UCHE in its summary. It should therefore be
treated as an earlier partial sweep rather than assumed to represent the later
fourteen-group layout.

# `width_sweeps/by_width/`

Contains complete sweeps collected separately for one fixed benchmark width.

Typical structure:

```text
width_sweeps/by_width/
├── width_128/
│   └── sweep_<timestamp>/
├── width_256/
│   └── sweep_<timestamp>/
├── width_512/
│   └── sweep_<timestamp>/
├── width_1024/
│   └── sweep_<timestamp>/
└── width_2048/
    └── sweep_<timestamp>/
```

This layout is useful when each sweep runs only one width.

It differs from a width-sequence sweep, where one benchmark command produces
several bursts for widths 128–2048 inside every chunk capture.

---

# One sweep directory

A sweep normally contains:

```text
sweep_<timestamp>/
├── <number>_<group>/
│   ├── <GROUP>_chunk001.csv
│   ├── <GROUP>_chunk001_meta.txt
│   ├── <GROUP>_chunk001_benchmark.log
│   ├── <GROUP>_chunk002.csv
│   ├── <GROUP>_chunk002_meta.txt
│   ├── <GROUP>_chunk002_benchmark.log
│   └── ...
├── run_config.txt
└── summary.csv
```

Every chunk should have a trio:

```text
counter CSV
metadata text
benchmark log
```

The sweep-level `summary.csv` provides an index across those trios.

---

# Group numbering

The numeric directory prefix keeps group order stable.

Typical mapping:

| Directory | Group | Broad role |
|---|---|---|
| `01_CP` | CP | Command processor |
| `02_RBBM` | RBBM | Register/bus/block management |
| `03_PC` | PC | Primitive control |
| `04_VFD` | VFD | Vertex fetch/decode |
| `05_HLSQ` | HLSQ | High-level sequencer |
| `06_VPC` | VPC | Vertex/primitive control |
| `07_TSE` | TSE | Triangle setup |
| `08_RAS` | RAS | Rasterization |
| `09_UCHE` | UCHE | Unified cache/memory interface |
| `10_TP` | TP | Texture processor |
| `11_SP` | SP | Shader processor |
| `12_RB` | RB | Render backend |
| `13_LRZ` | LRZ | Low-resolution Z |
| `14_ALWAYSON` | ALWAYSON | Always-on/timebase counters |

Not every workload activates every group.

Compute-only workloads can leave many graphics-pipeline counters at zero.

---

# Hardware capacity and chunking

Each group has a limited number of physical counter slots.

If:

```text
group has N countables
group capacity is C slots
```

the number of chunks is approximately:

```text
ceil(N / C)
```

The final chunk can contain fewer counters.

The inspected `sweep_20260624_124835/summary.csv` reports:

| Group | Chunks | Countables attempted | Full-chunk capacity | Final chunk |
|---|---:|---:|---:|---:|
| `CP` | 7 | 91 | 14 | 7 |
| `RBBM` | 5 | 17 | 4 | 1 |
| `PC` | 11 | 87 | 8 | 7 |
| `VFD` | 4 | 28 | 8 | 4 |
| `HLSQ` | 23 | 137 | 6 | 5 |
| `VPC` | 12 | 71 | 6 | 5 |
| `TSE` | 30 | 119 | 4 | 3 |
| `RAS` | 10 | 39 | 4 | 3 |
| `UCHE` | 5 | 120 | 24 | 24 |

Totals for this inspected summary:

```text
groups:                 9
chunk runs:           107
active counter slots: 709
benchmark failures:     0
```

The `active counter slots` total counts one counter activation in one chunk. It
does not imply that all 709 counters produced nonzero values.

---

# `summary.csv`

## Purpose

Provides one row per chunk.

Inspected columns:

```text
group
chunk
counter_start_index
counter_end_index
active_counters
csv_path
benchmark_log
benchmark_exit_status
```

## Column reference

### `group`

Counter-group name.

### `chunk`

One-based chunk number inside the group.

### `counter_start_index`

Inclusive start index in the group’s countable table.

### `counter_end_index`

Exclusive end index.

The number attempted should normally equal:

```text
counter_end_index - counter_start_index
```

### `active_counters`

Number of counters successfully activated for this chunk.

A value smaller than the attempted range can indicate activation failures.

In the inspected summary, active count matches the range size for every row.

### `csv_path`

Device-side CSV path at capture time.

Example:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps/
sweep_20260624_124835/06_VPC/VPC_chunk005.csv
```

### `benchmark_log`

Device-side benchmark-log path.

### `benchmark_exit_status`

Exit status returned by the benchmark command.

```text
0:
    benchmark process exited successfully

nonzero:
    benchmark failed, crashed, or returned an error
```

Exit status zero does not prove that:

- every counter was valid;
- every counter changed;
- the benchmark used the intended driver;
- the benchmark produced stable timing; or
- the CSV captured the complete workload window.

---

# Chunk CSV files

Example:

```text
VPC_chunk005.csv
```

Schema:

```text
elapsed_s,<counter_1>,<counter_2>,...
```

Example columns:

```text
elapsed_s
VPC_FE_POSRAM_FULL_CYCLES
VPC_FE_GMEM_NOP_FULL_CYCLES
VPC_FE_GMEM_POS_FULL_CYCLES
VPC_FE_BOTTLENECK
VPC_US_BUSY_CYCLES
VPC_US_WORKING_CYCLES
```

## `elapsed_s`

Time since the chunk capture started.

It is streamer process time, not a Vulkan GPU timestamp.

## Counter columns

Each value is a delta since the previous sample.

The CSV does not contain absolute hardware register values.

## Strict CSV format

Unlike some manually captured streamer files under `results/calibration/`, the
inspected sweeper chunk files are strict CSV from the first line.

They can be loaded directly:

```python
import pandas as pd

frame = pd.read_csv("VPC_chunk005.csv")
```

---

# Metadata files

Example:

```text
VPC_chunk005_meta.txt
```

The metadata is a CSV-like text file containing several sections.

Typical header fields:

```text
group,VPC
capacity,6
duration_s,10.000000
interval_s,0.001000
benchmark_cmd,<command>
```

Then:

```text
activation_status,counter,group,selector,reg_low,reg_high_or_error
GET_OK,...
GET_OK,...
...
```

Then result fields:

```text
result,ok
active_counters,6
attempted_counters,6
csv,<device path>
benchmark_log,<device path>
rows,5727
benchmark_exit_status,0
```

## Why the file is not ordinary two-column CSV

The activation section has six columns, while header and result rows have two.

Use a purpose-built parser or line-based parsing rather than:

```python
pandas.read_csv(meta_path)
```

---

# Activation rows

An activation row records:

```text
status
counter name
numeric group ID
selector
low register
high register or error
```

Example:

```text
GET_OK,VPC_US_BUSY_CYCLES,0x5,28,0x202,0x203
```

A successful row means KGSL accepted the request and allocated a physical slot.

It does not prove that the counter is useful for the current workload.

---

# Physical slot reuse

For VPC capacity 6, chunk 5 returned:

```text
selector 24 → 0x20a / 0x20b
selector 25 → 0x208 / 0x209
selector 26 → 0x206 / 0x207
selector 27 → 0x204 / 0x205
selector 28 → 0x202 / 0x203
selector 29 → 0x200 / 0x201
```

Chunk 4 and chunk 12 reuse those physical register pairs for different
selectors.

This is expected:

```text
selector:
    semantic event to count

physical slot/register pair:
    hardware storage assigned for this chunk
```

When a chunk ends, the counters are released. The same slots can be reused by
the next chunk.

---

# Benchmark logs

A benchmark log is normally a one-row CSV.

Inspected columns:

```text
op
variant
width
rows
elements
repeats
elapsed_ms
elements_per_s
estimated_bytes_per_element
estimated_bandwidth_GBps
verify
device
```

Example workload:

```text
operation:  softmax
variant:    fused_lmem
width:      256
rows:       256
elements:   65536
repeats:    32
verification: PASS
device:     Adreno (TM) 830
```

A benchmark log proves that the runner completed and reported verification.

It does not prove that every selected counter responded correctly.

---

# `run_config.txt`

A later sweep should contain a sweep-level configuration file.

It should be treated as the primary description of:

- sweep timestamp;
- group selection;
- benchmark command;
- capture duration;
- requested interval;
- tool paths;
- device output directory; and
- optional width-sequence settings.

The inspected upload did not include `run_config.txt`, so this README does not
claim its exact historical schema.

Inspect a sweep directly:

```bash
cat \
  results/perfcounter_sweeps/full_sweeps/<sweep>/run_config.txt
```

---

# Worked example: `sweep_20260624_124835/06_VPC`

The inspected files belong to:

```text
sweep_20260624_124835
group 06_VPC
```

## VPC group properties

```text
numeric group ID: 0x05
capacity:          6
countables:       71
chunks:           12
last chunk:        5 counters
```

## Capture configuration

Metadata records:

```text
requested duration: 10.0 seconds
requested interval: 1.0 ms
```

Benchmark command:

```bash
/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
  --op softmax \
  --variant fused_lmem \
  --spv \
    /data/local/tmp/jerry_work/ml_primitives/spv/softmax_fused_lmem.spv \
  --width 256 \
  --rows 256 \
  --repeats 32 \
  --csv
```

---

# VPC chunk selector examples

## Chunk 4

Selectors 18–23:

```text
VPC_FE_TSE_FE_TRANSACTIONS
VPC_FE_STALL_CYCLES_CCU
VPC_FE_NUM_WM_HIT
VPC_FE_STALL_DQ_WACK
VPC_FE_STALL_CYCLES_PRG_END_FE
VPC_FE_STALL_CYCLES_PRG_END_VPCVS
```

All six `GET` operations succeeded.

The benchmark exited zero and verification passed.

## Chunk 5

Selectors 24–29:

```text
VPC_FE_POSRAM_FULL_CYCLES
VPC_FE_GMEM_NOP_FULL_CYCLES
VPC_FE_GMEM_POS_FULL_CYCLES
VPC_FE_BOTTLENECK
VPC_US_BUSY_CYCLES
VPC_US_WORKING_CYCLES
```

All six activated successfully.

## Chunk 12

Selectors 66–70:

```text
VPC_BE_CCHE_NUM_POS_REQ
VPC_BE_STALL_CYCLES_LM_ACK
VPC_BE_STALL_CYCLES_PRG_END_VPCPS
VPC_BE_POS_OVERFETCH_ATTR
VPC_BE_BOTTLENECK
```

The final chunk contains five counters because the group ends at selector index
71.

---

# Observed VPC sampling behavior

The uploaded inspection set contains chunk CSVs:

```text
1, 2, 3, 4, 5, 6, 7, 10, 11, 12
```

Chunk CSVs 8 and 9 were not included in the inspection set.

| Chunk | Counters | Rows | Median observed step (ms) | Nonzero rows | Nonzero regions by row | Nonzero counters |
|---:|---:|---:|---:|---:|---|---|
| 1 | 6 | 5,694 | 1.785 | 5 | 61; 80–83 | `VPC_FE_BUSY_CYCLES`, `VPC_FE_WORKING_CYCLES` |
| 2 | 6 | 5,710 | 1.784 | 5 | 65; 83–86 | `VPC_FE_VS_BUSY_CYCLES`, `VPC_FE_VS_WORKING_CYCLES` |
| 3 | 6 | 5,670 | 1.799 | 0 | none | none |
| 4 | 6 | 5,700 | 1.786 | 0 | none | none |
| 5 | 6 | 5,727 | 1.777 | 5 | 58; 77–80 | `VPC_FE_BOTTLENECK`, `VPC_US_BUSY_CYCLES`, `VPC_US_WORKING_CYCLES` |
| 6 | 6 | 5,699 | 1.783 | 5 | 62; 80–83 | `VPC_US_STARVE_CYCLES_TSE_FE` |
| 7 | 6 | 6,228 | 1.499 | 6,228 | 0–6227 | `VPC_US_STARVE_CYCLES_REORDER` |
| 10 | 6 | 6,681 | 1.436 | 0 | none | none |
| 11 | 6 | 7,589 | 1.214 | 7,589 | 0–7588 | `VPC_BE_PS_BUSY_CYCLES`, `VPC_BE_PS_WORKING_CYCLES`, `VPC_BE_STARVE_CYCLES_CCHE` |
| 12 | 5 | 6,756 | 1.432 | 0 | none | none |

---

# Requested interval versus observed interval

The metadata requests:

```text
interval = 1.000 ms
```

The actual median interval in the inspected files ranges approximately:

```text
1.214–1.799 ms
```

A ten-second capture therefore contains:

```text
approximately 5,670–7,589 rows
```

rather than exactly:

```text
10,000 rows
```

This difference is expected from:

- userspace scheduling;
- ioctl/read overhead;
- CSV formatting;
- storage writes;
- Android CPU scheduling;
- varying number of active counters;
- device load; and
- sleep/timer precision.

The requested interval is a target, not a guarantee.

Always calculate the actual interval from `elapsed_s`.

---

# Benchmark duration versus capture duration

The inspected benchmark logs report:

| Chunk | Benchmark time (ms) | Elements/s | Estimated bandwidth (GB/s) | Verify |
|---:|---:|---:|---:|---|
| 4 | 4.6414 | 451,835,500 | 3.615 | `PASS` |
| 7 | 4.5466 | 461,255,783 | 3.690 | `PASS` |
| 9 | 3.6117 | 580,660,399 | 4.645 | `PASS` |
| 10 | 3.6323 | 577,363,274 | 4.619 | `PASS` |

The capture lasts ten seconds, while the benchmark lasts approximately four
milliseconds.

Consequences:

```text
most rows are idle/background samples
the benchmark appears in only a few samples
small timing shifts strongly affect local active-window detection
counter totals should not be calculated over the full ten seconds blindly
```

The four inspected benchmark times have substantial variation relative to their
mean. Formal comparisons should use repeated runs and report variability.

---

# Why some chunks contain only a few nonzero rows

At an observed sample interval near 1.5–1.8 ms, a four-millisecond benchmark can
overlap only several samples.

This is consistent with chunks 1, 2, 5, and 6 showing approximately five
nonzero rows.

One isolated sample before the main four-row region can reflect:

- benchmark setup;
- pipeline creation;
- a warm-up dispatch;
- driver activity;
- unrelated GPU work; or
- timing jitter.

The CSV alone cannot determine the exact cause.

---

# Why some chunks are all zero

Chunks 3, 4, 10, and 12 are all zero in the inspected CSVs.

Possible explanations include:

- the workload does not activate those graphics events;
- the counter is stage-specific;
- the selector is valid but irrelevant to compute softmax;
- the event is too rare for this workload;
- the counter mapping or semantics require validation; or
- the benchmark window was missed.

A zero CSV is still a valid measurement result when:

```text
GET succeeded
benchmark exited zero
CSV duration completed
counters were released
```

Do not delete zero chunks; they document unsupported or inactive behavior.

---

# Continuously active/suspicious counters

Two inspected counters are nonzero throughout nearly the entire ten-second
capture:

```text
VPC_US_STARVE_CYCLES_REORDER
VPC_BE_STARVE_CYCLES_CCHE
```

This is very different from the four-millisecond benchmark window.

Possible explanations include:

- the event counts background/display activity;
- the counter increments while the relevant block is idle/starved;
- the semantic name is easy to misinterpret;
- the selector/register mapping is imperfect;
- counter deltas include activity unrelated to the benchmark;
- the event is effectively always active under the device state; or
- the read path needs additional validation.

These counters should not be treated as benchmark-specific without:

- an idle baseline;
- workload/no-work comparison;
- repeated runs;
- a benchmark marker;
- correlation with another trusted counter; and
- validation against counter documentation.

---

# Benchmark repeatability warning

The benchmark runs once per chunk.

Therefore, a full sweep compares counters measured during different benchmark
invocations.

Variation can arise from:

- thermal state;
- GPU frequency;
- cache state;
- scheduler activity;
- background rendering;
- process startup;
- allocation state;
- driver warm-up; and
- normal benchmark noise.

The sweeper provides broad coverage, but it is not equivalent to measuring all
counters simultaneously during one identical benchmark invocation.

---

# Integrity inventory for inspected files

| File | Bytes | Rows/lines | Columns | SHA-256 |
|---|---:|---:|---:|---|
| `summary.csv` | 22,704 | 107 | 8 | `ed074f978a56e24b586009d9e2ac1080d04107436db760016f05b97294dacac2` |
| `VPC_chunk001.csv` | 119,746 | 5,694 | 7 | `302c4d998d58b24679ab9ca1f96920251fc48e1d789bce27eeb0cb1af3afe6c3` |
| `VPC_chunk002.csv` | 120,098 | 5,710 | 7 | `465e60c94276725c33158c272382ec017ea2f4957f8c1c6de6abf6b667997815` |
| `VPC_chunk003.csv` | 119,231 | 5,670 | 7 | `3bbdc60dcb5970a4177aa11cef575f335829df4dc0ed1fbce4bab58a8d15a7b4` |
| `VPC_chunk004.csv` | 119,866 | 5,700 | 7 | `75305bd8ad90921d18ca3c0c02d4fd620ea6e39e70035fa4c8c929e43ed22d0c` |
| `VPC_chunk005.csv` | 120,447 | 5,727 | 7 | `dc7f870aa2f2a381745af0f7a104191ec99ed64560b6960060f4795226832c8e` |
| `VPC_chunk006.csv` | 119,848 | 5,699 | 7 | `309d9f735c6d9df6d5aa7eab8ce6401ceacb50ba996c80ea8e39a029263b9d2d` |
| `VPC_chunk007.csv` | 162,132 | 6,228 | 7 | `c6281190835f4eb665b46f4ecef8470322b6baf929f85d55dc90de06d603ce27` |
| `VPC_chunk010.csv` | 140,459 | 6,681 | 7 | `a96ce9968eb4057b115669e61be3ceaf75840207430691447f25a75a7c71fce5` |
| `VPC_chunk011.csv` | 204,306 | 7,589 | 7 | `8a92e1fcf3bd1ca8d7952c438998303b1551573bd2dc6d8ab6870a1c633f775c` |
| `VPC_chunk012.csv` | 128,504 | 6,756 | 6 | `e29af087fa1966933d3bd0389a903657eac93e16c3ff5950506d87219748d0fe` |
| `VPC_chunk004_meta.txt` | 959 | 18 |  | `8614ff451dc711b79824345d8cd76b2b54154cb3bef2e196d22dbeef44812c27` |
| `VPC_chunk005_meta.txt` | 945 | 18 |  | `2c9076608fcf91f54b2e443de27d4ca629d83b72f648caf36aa1395b75681166` |
| `VPC_chunk012_meta.txt` | 907 | 17 |  | `939a6f0e2b92fdc4ea5ca18adf611d85afcb149d8be64cf031831d9c61749b56` |
| `VPC_chunk004_benchmark.log` | 224 | 2 |  | `98ed53823d420be033ad4043d0bca833473d491e95fb957db3b04d8f1eda9c2d` |
| `VPC_chunk007_benchmark.log` | 224 | 2 |  | `5e864e7e31aff03a4ba154d134ec9e5d5ca3880aaaca41e08f748d4b8e1a1291` |
| `VPC_chunk009_benchmark.log` | 224 | 2 |  | `a4d8068f0944a7cc8d9887e417b99da3e9ce1930c33c1a2f39b3e9b68c0606fa` |
| `VPC_chunk010_benchmark.log` | 224 | 2 |  | `891597fc2bf7d2ad80e2d32bcce01e670234a44935ff418b21af28368aaf38a7` |

This table covers only the uploaded inspection set.

It is not a manifest for the complete sweep.

---

# Audit one sweep

Run from the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

SWEEP="results/perfcounter_sweeps/full_sweeps/sweep_20260624_124835"
```

## List groups

```bash
find "$SWEEP" \
  -mindepth 1 \
  -maxdepth 1 \
  -type d \
  -print \
  | sort
```

## Count files

```bash
find "$SWEEP" -type f | wc -l
```

## Count chunk trios

```bash
find "$SWEEP" -type f -name '*_chunk*.csv' | wc -l
find "$SWEEP" -type f -name '*_chunk*_meta.txt' | wc -l
find "$SWEEP" -type f -name '*_chunk*_benchmark.log' | wc -l
```

For a complete sweep, these counts should normally match.

---

# Validate `summary.csv`

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

sweep = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835"
)

summary = pd.read_csv(sweep / "summary.csv")

errors = []

for row in summary.itertuples(index=False):
    attempted = (
        int(row.counter_end_index)
        - int(row.counter_start_index)
    )

    if int(row.active_counters) > attempted:
        errors.append(
            f"{row.group} chunk {row.chunk}: "
            f"active={row.active_counters} > attempted={attempted}"
        )

    group_dirs = list(
        sweep.glob(f"*_{row.group}")
    )

    if len(group_dirs) != 1:
        errors.append(
            f"{row.group} chunk {row.chunk}: "
            f"expected one group directory, found {len(group_dirs)}"
        )
        continue

    group_dir = group_dirs[0]
    stem = f"{row.group}_chunk{int(row.chunk):03d}"

    for suffix in [
        ".csv",
        "_meta.txt",
        "_benchmark.log",
    ]:
        path = group_dir / f"{stem}{suffix}"

        if not path.is_file():
            errors.append(f"missing {path}")

if errors:
    print("\n".join(errors))
    raise SystemExit(1)

print(f"Validated {len(summary)} summary rows.")
PY
```

---

# Verify benchmark exit statuses

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835/summary.csv"
)

summary = pd.read_csv(path)

print(
    summary["benchmark_exit_status"]
    .value_counts(dropna=False)
    .sort_index()
    .to_string()
)

failed = summary[
    summary["benchmark_exit_status"] != 0
]

if not failed.empty:
    print("\nFailed chunks:")
    print(failed.to_string(index=False))
PY
```

---

# Summarize groups and capacities

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835/summary.csv"
)

summary = pd.read_csv(path)

table = (
    summary
    .groupby("group", sort=False)
    .agg(
        chunks=("chunk", "count"),
        countables=("active_counters", "sum"),
        full_chunk_capacity=("active_counters", "max"),
        failed_benchmarks=(
            "benchmark_exit_status",
            lambda series: int((series != 0).sum()),
        ),
    )
)

print(table.to_string())
PY
```

---

# Validate metadata row counts

```bash
python3 - <<'PY'
from pathlib import Path
import re

import pandas as pd


def read_meta(path):
    values = {}

    for line in path.read_text(errors="replace").splitlines():
        parts = line.split(",", maxsplit=1)

        if len(parts) == 2:
            key, value = parts

            if key in {
                "group",
                "capacity",
                "duration_s",
                "interval_s",
                "result",
                "active_counters",
                "attempted_counters",
                "csv",
                "benchmark_log",
                "rows",
                "benchmark_exit_status",
            }:
                values[key] = value

    return values


sweep = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835"
)

failures = []

for meta in sweep.rglob("*_chunk*_meta.txt"):
    values = read_meta(meta)

    match = re.search(
        r"([A-Z0-9]+)_chunk(\d+)_meta\.txt$",
        meta.name,
    )

    if not match:
        continue

    group, chunk = match.groups()
    csv_path = meta.with_name(
        f"{group}_chunk{int(chunk):03d}.csv"
    )

    if not csv_path.is_file():
        failures.append(f"missing {csv_path}")
        continue

    rows = len(pd.read_csv(csv_path))
    expected = int(values["rows"])

    if rows != expected:
        failures.append(
            f"{csv_path}: rows={rows}, metadata={expected}"
        )

if failures:
    print("\n".join(failures))
    raise SystemExit(1)

print("All inspected metadata row counts match their CSV files.")
PY
```

---

# Find zero and active counters

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

group_dir = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835/06_VPC"
)

for path in sorted(group_dir.glob("VPC_chunk*.csv")):
    frame = pd.read_csv(path)
    counters = [
        column
        for column in frame.columns
        if column != "elapsed_s"
    ]

    active = [
        counter
        for counter in counters
        if (frame[counter] != 0).any()
    ]

    zero = [
        counter
        for counter in counters
        if not (frame[counter] != 0).any()
    ]

    print(f"\n{path.name}")
    print(f"  rows:   {len(frame)}")
    print(f"  active: {len(active)}")
    print(f"  zero:   {len(zero)}")

    for counter in active:
        print(
            f"    {counter}: "
            f"sum={frame[counter].sum():.0f}, "
            f"nonzero_samples={(frame[counter] != 0).sum()}"
        )
PY
```

---

# Calculate actual sampling intervals

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

group_dir = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835/06_VPC"
)

print(
    f"{'file':24s} "
    f"{'rows':>8s} "
    f"{'duration_s':>12s} "
    f"{'median_ms':>12s} "
    f"{'mean_ms':>12s}"
)

for path in sorted(group_dir.glob("VPC_chunk*.csv")):
    frame = pd.read_csv(path)
    delta = frame["elapsed_s"].diff().dropna()

    print(
        f"{path.name:24s} "
        f"{len(frame):8d} "
        f"{frame['elapsed_s'].iloc[-1]:12.6f} "
        f"{delta.median() * 1000:12.3f} "
        f"{delta.mean() * 1000:12.3f}"
    )
PY
```

---

# Flag continuously active counters

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

group_dir = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835/06_VPC"
)

for path in sorted(group_dir.glob("VPC_chunk*.csv")):
    frame = pd.read_csv(path)

    for counter in frame.columns:
        if counter == "elapsed_s":
            continue

        fraction = float((frame[counter] != 0).mean())

        if fraction >= 0.90:
            print(
                f"{path.name}: {counter}: "
                f"{fraction:.1%} nonzero samples"
            )
PY
```

A continuously active counter is not automatically invalid.

It requires an idle baseline and semantic validation.

---

# Summarize benchmark logs

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

sweep = Path(
    "results/perfcounter_sweeps/full_sweeps/"
    "sweep_20260624_124835"
)

rows = []

for path in sorted(sweep.rglob("*_benchmark.log")):
    try:
        frame = pd.read_csv(path)
    except Exception as exc:
        print(f"SKIP {path}: {exc}")
        continue

    if frame.empty:
        print(f"EMPTY {path}")
        continue

    row = frame.iloc[-1].to_dict()
    row["benchmark_log"] = str(path)
    rows.append(row)

result = pd.DataFrame(rows)

print(f"logs parsed: {len(result)}")

if "verify" in result.columns:
    print("\nVerification:")
    print(result["verify"].value_counts(dropna=False).to_string())

if "elapsed_ms" in result.columns:
    print("\nElapsed time:")
    print(result["elapsed_ms"].describe().to_string())

failed = result[
    result.get("verify", "").astype(str) != "PASS"
]

if not failed.empty:
    print("\nNon-PASS logs:")
    print(failed.to_string(index=False))
PY
```

---

# Detect incomplete sweeps

```bash
python3 - <<'PY'
from pathlib import Path

root = Path(
    "results/perfcounter_sweeps/full_sweeps"
)

expected_groups = {
    "CP",
    "RBBM",
    "PC",
    "VFD",
    "HLSQ",
    "VPC",
    "TSE",
    "RAS",
    "UCHE",
    "TP",
    "SP",
    "RB",
    "LRZ",
    "ALWAYSON",
}

for sweep in sorted(root.glob("sweep_*")):
    actual = {
        path.name.split("_", 1)[1]
        for path in sweep.iterdir()
        if path.is_dir() and "_" in path.name
    }

    missing = sorted(expected_groups - actual)

    print(sweep.name)

    if missing:
        print(f"  missing: {','.join(missing)}")
    else:
        print("  all expected groups present")
PY
```

A sweep can be intentionally partial.

Record the group selection in `run_config.txt` rather than treating every
missing group as an error.

---

# Compare fixed-width sweep inventories

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

root = Path(
    "results/perfcounter_sweeps/width_sweeps/by_width"
)

for width_dir in sorted(root.glob("width_*")):
    print(f"\n=== {width_dir.name} ===")

    for sweep in sorted(width_dir.glob("sweep_*")):
        summary_path = sweep / "summary.csv"

        if not summary_path.is_file():
            print(f"{sweep.name}: missing summary.csv")
            continue

        summary = pd.read_csv(summary_path)

        print(
            f"{sweep.name}: "
            f"chunks={len(summary)}, "
            f"groups={summary['group'].nunique()}, "
            f"active_counters={summary['active_counters'].sum()}, "
            f"benchmark_failures="
            f"{(summary['benchmark_exit_status'] != 0).sum()}"
        )
PY
```

---

# Pull sweeps from the phone

The repository contains:

```text
scripts/sync/pull_latest_sweep.sh
```

Review its current paths and interface:

```bash
sed -n '1,280p' \
  scripts/sync/pull_latest_sweep.sh
```

A manual workflow is:

```bash
REMOTE_ROOT="/data/local/tmp/jerry_work/perfcounter_sweeps"

LATEST="$(
  adb shell "ls -dt $REMOTE_ROOT/sweep_* 2>/dev/null | head -n 1" \
  | tr -d '\r'
)"

printf 'Latest remote sweep: %s\n' "$LATEST"
```

Pull it:

```bash
mkdir -p results/perfcounter_sweeps/full_sweeps

adb pull \
  "$LATEST" \
  results/perfcounter_sweeps/full_sweeps/
```

Verify after pulling:

```bash
SWEEP_NAME="$(basename "$LATEST")"

test -f \
  "results/perfcounter_sweeps/full_sweeps/$SWEEP_NAME/summary.csv"
```

Do not pull into a directory that already contains a sweep with the same name
unless byte identity is checked first.

---

# Run a new sweep

The capture tool is:

```text
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

Build and push using:

```text
tools/profiling/perfcounter_sweeper/build_and_push.sh
```

Inspect current usage before running:

```bash
adb shell \
  'su -c "/data/local/tmp/streamer_sweeper --help"'
```

The exact CLI can change as the sweeper evolves.

Do not copy a historical invocation without comparing it with current `--help`
and:

```bash
sed -n '1,320p' \
  tools/profiling/perfcounter_sweeper/README.md
```

A reproducible run must explicitly record:

- selected groups;
- capture duration;
- requested interval;
- benchmark command;
- output root;
- counter table version;
- sweeper binary hash;
- benchmark binary hash;
- shader hash;
- Vulkan driver;
- device build;
- kernel version;
- run order;
- thermal state; and
- frequency policy.

---

# Example benchmark command

The inspected VPC metadata uses:

```bash
/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
  --op softmax \
  --variant fused_lmem \
  --spv \
    /data/local/tmp/jerry_work/ml_primitives/spv/softmax_fused_lmem.spv \
  --width 256 \
  --rows 256 \
  --repeats 32 \
  --csv
```

Run this command directly before a long sweep:

```bash
adb shell '
  /data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
    --op softmax \
    --variant fused_lmem \
    --spv \
      /data/local/tmp/jerry_work/ml_primitives/spv/softmax_fused_lmem.spv \
    --width 256 \
    --rows 256 \
    --repeats 32 \
    --csv
'
```

Requirements before sweeping:

```text
exit status = 0
verify = PASS
runtime long enough to be observed
device/driver identity confirmed
```

---

# Recommended duration design

The inspected historical configuration uses:

```text
capture duration: 10 seconds
benchmark time:   approximately 4 milliseconds
```

This is inefficient for benchmark-window analysis.

Better options include:

## Repeat the benchmark inside the capture

Run several benchmark bursts with controlled gaps.

This produces visible active regions and allows width-sequence analysis.

## Increase benchmark repeats

Make one benchmark invocation long enough to span many samples.

## Shorten capture duration

Start capture shortly before the benchmark and stop shortly after it.

## Add explicit markers

Record benchmark start/end times in:

- the counter CSV;
- a sidecar file;
- trace markers; or
- metadata.

A useful target is enough samples to distinguish:

```text
idle before
steady benchmark activity
idle after
```

---

# Width-sequence sweeps

A width-sequence benchmark can run:

```text
128 → 256 → 512 → 1024 → 2048
```

inside one chunk capture.

The analysis scripts assign widths by burst order.

Recommended analysis:

```bash
python3 \
  analysis/perfcounter_width_sequence/analyze_repeated_width_sweeps.py \
  --sweeps-root results/perfcounter_sweeps/full_sweeps \
  --latest-n 2 \
  --widths 128,256,512,1024,2048 \
  --reference-relpath 11_SP/SP_chunk001.csv \
  --out-dir \
    results/perfcounter_analysis/repro_width_sequence_latest2 \
  --threshold-frac 0.08 \
  --min-active-samples 1 \
  --pad-samples 20
```

See:

```text
analysis/perfcounter_width_sequence/README.md
results/perfcounter_analysis/README.md
```

---

# Storage and repository considerations

A complete sweep can contain hundreds of files.

Every chunk can produce:

```text
CSV
metadata
benchmark log
```

and every CSV can contain thousands of rows.

Before committing large sweeps, consider:

- repository growth;
- Git clone size;
- repeated zero data;
- whether all raw data belongs in Git;
- whether selected evidence should be preserved separately;
- compression;
- Git LFS;
- external archival storage; and
- manifest-based retrieval.

Do not delete raw sweeps merely because derived analysis exists.

Derived files cannot recreate:

- precise samples;
- timing jitter;
- active-window boundaries;
- sparse values;
- unexpected background activity; or
- metadata details.

---

# Integrity workflow

Create hashes for one sweep:

```bash
SWEEP="results/perfcounter_sweeps/full_sweeps/sweep_20260624_124835"

find "$SWEEP" \
  -type f \
  -print0 \
  | sort -z \
  | xargs -0 shasum -a 256 \
  > "$SWEEP/input_hashes.txt"
```

Adding `input_hashes.txt` changes the directory after hashing. Exclude the hash
file itself on regeneration:

```bash
find "$SWEEP" \
  -type f \
  ! -name input_hashes.txt \
  -print0 \
  | sort -z \
  | xargs -0 shasum -a 256 \
  > "$SWEEP/input_hashes.txt"
```

For immutable long-term evidence, copy selected sweeps to an evidence area and
create a manifest rather than silently editing historical files.

---

# What sweep results support

A well-controlled sweep can show:

- which counters are accepted by KGSL;
- which counters remain zero;
- which counters respond to a workload;
- which counters scale with width;
- which groups show activity;
- whether counter behavior is reproducible;
- whether benchmark verification succeeds; and
- which counters deserve focused follow-up.

---

# What sweep results do not prove

A sweep does not prove:

- every selector name is correct;
- a nonzero counter is a bottleneck;
- a zero counter is unsupported;
- counters from different chunks occurred in the same exact execution;
- benchmark timing was identical across chunks;
- the requested interval was achieved;
- a continuously active counter belongs to the benchmark;
- the intended Vulkan driver was used;
- thermal/frequency state was constant; or
- results generalize to another Adreno generation.

---

# Known limitations

## Counters are multiplexed across repeated benchmark runs

Different chunks are measured during different executions.

## Sampling interval is approximate

The inspected 1 ms target produced median steps of roughly 1.2–1.8 ms.

## Short benchmark window

A roughly 4 ms workload appears in only a few samples.

## Large idle fraction

Ten-second captures contain mostly idle/background time for the inspected
workload.

## No benchmark marker in chunk CSV

The exact benchmark start/end samples are inferred from counter activity.

## Benchmark variability

The inspected logs range from approximately 3.61 to 4.64 ms.

## Zero-counter ambiguity

Zero can mean valid-but-inactive, stage mismatch, unsupported semantics, or
collection problems.

## Continuously active counters

Some counters can remain nonzero through the entire capture.

## Historical absolute paths

Summary and metadata files retain device paths.

## Partial sweeps

Early directories under `full_sweeps/` may not contain all fourteen groups.

## Tool-version provenance

Historical sweeps do not always embed source commits or binary hashes.

## Driver identity

Benchmark logs report the device name but not always the driver name.

## Thermal and frequency state

Not embedded in the inspected files.

## Repository size

Raw sweep storage grows quickly.

---

# Recommended maintenance

1. Add this README.
2. Preserve raw sweep files unchanged.
3. Add `run_config.txt` to every sweep.
4. Record complete tool and benchmark hashes.
5. Record Git commit IDs.
6. Record device, kernel, Android, and driver identity.
7. Record actual interval statistics after capture.
8. Add benchmark start/end markers.
9. Save frequency and thermal telemetry.
10. Run a warm-up before formal collection.
11. Use repeated benchmark invocations or longer workloads.
12. Keep verification enabled.
13. Check benchmark exit status for every chunk.
14. Validate metadata row counts.
15. Validate chunk trio completeness.
16. Flag continuously active counters.
17. Keep zero counters as evidence.
18. Use repeated sweeps before making conclusions.
19. Use reference windows for width-sequence analysis.
20. Preserve selected final sweeps under `evidence/` with manifests.
21. Compress or archive old sweeps instead of deleting them.
22. Avoid ambiguous `latest` selection based only on modification time.
23. Document intentionally partial group selections.
24. Keep raw and derived data in separate directories.

---

# Suggested future structure

```text
results/perfcounter_sweeps/
├── README.md
├── full_sweeps/
│   └── sweep_<timestamp>/
│       ├── run_config.json
│       ├── summary.csv
│       ├── input_hashes.txt
│       ├── generation.log
│       ├── device_metadata.json
│       ├── benchmark_metadata.json
│       ├── telemetry/
│       │   ├── frequency.csv
│       │   └── thermal.csv
│       └── groups/
│           ├── 01_CP/
│           └── ...
└── width_sweeps/
    └── by_width/
        └── width_<N>/
            └── sweep_<timestamp>/
```

Changing the existing historical layout is not required. The suggested
structure is for future sweep generations.

---

# Quick reference

```text
full_sweeps/
    multi-group timestamped raw captures

width_sweeps/by_width/
    one fixed-width sweep collection per width

*_chunkNNN.csv
    elapsed time plus raw counter deltas

*_chunkNNN_meta.txt
    counter selectors, physical slots, configuration, and status

*_chunkNNN_benchmark.log
    one benchmark result row

run_config.txt
    sweep-level configuration

summary.csv
    one row per chunk
```

---

# Main interpretation from the inspected example

```text
The sweeper successfully activated 709 counter slots across 107 chunk runs for
nine groups, and every benchmark returned exit status zero.

However, the requested 1 ms interval was not achieved exactly, the approximately
4 ms benchmark occupied only a few samples in most chunks, several VPC chunks
were entirely zero, and two counters remained nonzero throughout the ten-second
capture.

The data is therefore valuable raw evidence, but counter interpretation requires
active-window isolation, idle baselines, repeated runs, and counter-specific
validation.
```
