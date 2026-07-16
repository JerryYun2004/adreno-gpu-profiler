# Memory-Stride Perf-Counter Calibration Evidence

This directory contains the final perf-counter streamer captures from the
memory-stride calibration performed on 2026-06-11.

```text
evidence/perfcounter_calibration/2026-06-11_mem_stride/
├── mem_stride_1_perf_stream_final.csv
├── mem_stride_4_perf_stream_final.csv
├── mem_stride_16_perf_stream_final.csv
└── mem_stride_64_perf_stream_final.csv
```

Each file records one fixed-stride workload while the phone-side Adreno
perf-counter streamer sampled six SP and UCHE counters.

These files are historical evidence. They are not source code, executable
benchmarks, or inputs required to build the streamer or sweeper.

Their role is:

```text
memory-stride shader + Vulkan runner
                ↓
        controlled GPU workload
                ↓
   perf-counter streamer samples counters
                ↓
     these four evidence captures
                ↓
   host-side comparison and validation
```

The corresponding workload source and runner are documented under:

```text
benchmarks/microbenchmarks/memory_stride/
```

---

# Status and relationship to the main tools

| File | Status | Workload represented | Relationship to streamer/sweeper |
|---|---|---|---|
| `mem_stride_1_perf_stream_final.csv` | Final historical evidence | Compile-time stride 1 | Direct output from the streamer |
| `mem_stride_4_perf_stream_final.csv` | Final historical evidence | Compile-time stride 4 | Direct output from the streamer |
| `mem_stride_16_perf_stream_final.csv` | Final historical evidence | Compile-time stride 16 | Direct output from the streamer |
| `mem_stride_64_perf_stream_final.csv` | Final historical evidence | Compile-time stride 64 | Direct output from the streamer |

The files are most directly connected to:

```text
tools/profiling/perfcounter_streamer/
```

They are not normal output from:

```text
tools/profiling/perfcounter_sweeper/
```

because each file contains one manually selected six-counter set rather than the
sweeper’s multi-directory group/chunk layout.

The same memory-stride workload can be used with the sweeper in later
experiments, but these four files should be interpreted as streamer captures.

---

# Important format note

Despite the `.csv` extension, these files are not strict CSV files from the
first line.

Each file contains:

1. a human-readable streamer preamble;
2. selected counter descriptions;
3. KGSL `GET` results and counter register addresses;
4. streamer status messages; and
5. a valid CSV table beginning at line 18.

Example structure:

```text
[selected] 6 counters, interval 0.005 s
...
[GET] SP_BUSY_CYCLES ...
...
[stream] Press Ctrl+C to stop. Values are deltas since previous sample.
[csv] logging elapsed_s and counter deltas to ...
elapsed_s,SP_BUSY_CYCLES,...
0.006663,0,0,...
...
```

A direct command such as:

```python
pandas.read_csv("mem_stride_1_perf_stream_final.csv")
```

fails because the preamble and CSV table have different numbers of fields.

Use the header-aware parser documented later in this README.

---

# Capture configuration encoded in the files

All four files record:

```text
selected counters: 6
requested interval: 0.005 seconds
data type: counter deltas since the previous sample
```

The files select the same six counters and the same KGSL group/selector/register
mapping.

| Counter | Group | Group ID | Selector | Low register | High register |
|---|---:|---:|---:|---:|---:|
| `SP_BUSY_CYCLES` | SP | `0x0a` | 1 | `0x29c` | `0x29d` |
| `SP_ALU_WORKING_CYCLES` | SP | `0x0a` | 2 | `0x29a` | `0x29b` |
| `UCHE_BUSY_CYCLES` | UCHE | `0x08` | 1 | `0x254` | `0x255` |
| `UCHE_RAM_READ_REQ` | UCHE | `0x08` | 52 | `0x252` | `0x253` |
| `UCHE_VBIF_READ_BEATS_SP` | UCHE | `0x08` | 25 | `0x250` | `0x251` |
| `SP_UCHE_READ_TRANS` | SP | `0x0a` | 46 | `0x296` | `0x297` |

The selector/register information is valuable evidence that the same counter
configuration was requested for every stride.

---

# What the files do and do not contain

## The files contain

- selected counter names;
- Adreno counter group IDs;
- selector numbers;
- returned low/high counter registers;
- requested sampling interval;
- elapsed time for every sample;
- counter deltas for every sample;
- idle samples before and after the workload;
- the active workload burst; and
- enough information to compare the four captures.

## The files do not contain

- the exact benchmark command;
- shader hash;
- runner hash;
- runner exit code;
- output verification result;
- element count;
- iteration count;
- dispatch repeat count;
- phone model;
- Android build fingerprint;
- kernel version;
- Vulkan driver identity;
- GPU frequency;
- device temperature; or
- streamer binary hash.

The experiment directory and project history provide some of this context, but
it is not embedded in the captures themselves.

The project’s recorded workflow for the memory-stride calibration used a
power-of-two element count and the legacy compile-time stride shaders. The
historical benchmark configuration associated with this series was:

```text
elements:         1048576
iterations:       512
dispatch repeats: 128
verification:     disabled with --no-verify
strides:          1, 4, 16, 64
```

Because these settings are not encoded inside the files, future formal
experiments should save a separate metadata file containing the exact command.

---

# File inventory and integrity

The currently inspected files have the following properties.

| File | Size | Data rows | SHA-256 |
|---|---:|---:|---|
| `mem_stride_1_perf_stream_final.csv` | 9,975 bytes | 233 | `ca31a1942ad7201dcde4f36de164dda910619dd5e808bb38781081dde11efd29` |
| `mem_stride_4_perf_stream_final.csv` | 9,203 bytes | 209 | `2ed31a37fe51f999a409795da8dab725ef6cb3d0770c48879e828ad7a56aac77` |
| `mem_stride_16_perf_stream_final.csv` | 9,561 bytes | 228 | `753aabd4b7aecbdb93cbf3c30431124447aaa6723ca9c0336985f70da1666af5` |
| `mem_stride_64_perf_stream_final.csv` | 9,450 bytes | 224 | `08c8ff5541d5da5acac84c61afaca52d96a3d3c12fbe5dff0bfd510e3704e12f` |

These hashes describe the exact uploaded byte content, including the streamer
preamble.

Verify them from the repository root:

```bash
shasum -a 256 \
  evidence/perfcounter_calibration/2026-06-11_mem_stride/*.csv
```

Do not rewrite, sort, reformat, or strip the preamble from the historical files.
A cleaned table should be generated as a new derived file.

---

# File reference

# `mem_stride_1_perf_stream_final.csv`

## Purpose

Records the counter stream for the stride-1 memory workload.

Stride 1 advances the per-invocation address by one `uint` element per loop
iteration.

For the legacy shader:

```glsl
idx = (idx + 1) & (n - 1);
acc += in_data[idx];
```

This produces the strongest iteration-to-iteration spatial locality among the
four variants.

## Capture properties

```text
CSV data rows:       233
capture end time:    1.348891 s
median sample step:  0.005277 s
nonzero rows:        140
main active region:  0.248486 s to 0.985422 s
main active samples: 139
```

There is also one isolated nonzero sample at:

```text
1.172201 s
```

The isolated sample is outside the main continuous workload region and should
not automatically be included in an active-window comparison.

It may represent:

- delayed GPU activity;
- unrelated background activity;
- residual work;
- another short command; or
- a collection artifact.

The file alone cannot determine the cause.

## Primary-region totals

Using the longest contiguous nonzero region:

```text
SP_BUSY_CYCLES:              8,663,257,167
SP_ALU_WORKING_CYCLES:       5,381,565,070
UCHE_BUSY_CYCLES:              280,459,342
UCHE_RAM_READ_REQ:              34,900,424
UCHE_VBIF_READ_BEATS_SP:              7,068
SP_UCHE_READ_TRANS:                   4,800
```

## Interpretation

Stride 1 provides the locality-friendly reference for comparing larger strides.

Its RAM-read-request total is the lowest of the four primary regions.

---

# `mem_stride_4_perf_stream_final.csv`

## Purpose

Records the counter stream for the stride-4 memory workload.

Each loop iteration advances by four `uint` elements:

```text
4 elements × 4 bytes = 16 bytes
```

## Capture properties

```text
CSV data rows:       209
capture end time:    1.164687 s
median sample step:  0.005283 s
nonzero rows:        127
main active region:  0.229333 s to 0.905830 s
main active samples: 127
```

The active samples form one continuous region.

## Primary-region totals

```text
SP_BUSY_CYCLES:              8,716,333,912
SP_ALU_WORKING_CYCLES:       5,381,565,070
UCHE_BUSY_CYCLES:              389,984,495
UCHE_RAM_READ_REQ:              38,403,772
UCHE_VBIF_READ_BEATS_SP:              7,068
SP_UCHE_READ_TRANS:                   4,800
```

## Relative to stride 1

```text
SP_BUSY_CYCLES:         1.006×
SP_ALU_WORKING_CYCLES:  1.000×
UCHE_BUSY_CYCLES:       1.391×
UCHE_RAM_READ_REQ:      1.100×
```

The ALU-working total is byte-for-byte numerically identical to the stride-1
primary-region total.

The UCHE activity and RAM-read requests are higher.

---

# `mem_stride_16_perf_stream_final.csv`

## Purpose

Records the counter stream for the stride-16 memory workload.

Each loop iteration advances by:

```text
16 elements × 4 bytes = 64 bytes
```

A 64-byte step may interact with cache-line behavior, but the actual cache-line
size and request coalescing must not be inferred from the filename alone.

## Capture properties

```text
CSV data rows:       228
capture end time:    1.270685 s
median sample step:  0.005273 s
nonzero rows:        125
main active region:  0.293869 s to 0.965020 s
main active samples: 125
```

The active samples form one continuous region.

## Primary-region totals

```text
SP_BUSY_CYCLES:              8,780,704,191
SP_ALU_WORKING_CYCLES:       5,381,292,032
UCHE_BUSY_CYCLES:              619,375,381
UCHE_RAM_READ_REQ:              50,431,114
UCHE_VBIF_READ_BEATS_SP:                  0
SP_UCHE_READ_TRANS:                       0
```

## Relative to stride 1

```text
SP_BUSY_CYCLES:         1.014×
SP_ALU_WORKING_CYCLES:  1.000×
UCHE_BUSY_CYCLES:       2.208×
UCHE_RAM_READ_REQ:      1.445×
```

This capture has the largest `UCHE_BUSY_CYCLES` total of the four primary
regions.

---

# `mem_stride_64_perf_stream_final.csv`

## Purpose

Records the counter stream for the stride-64 memory workload.

Each loop iteration advances by:

```text
64 elements × 4 bytes = 256 bytes
```

This moves through the working set more quickly and reduces iteration-to-
iteration locality relative to smaller strides.

## Capture properties

```text
CSV data rows:       224
capture end time:    1.263661 s
median sample step:  0.005288 s
nonzero rows:        124
main active region:  0.238918 s to 0.913702 s
main active samples: 124
```

The active samples form one continuous region.

## Primary-region totals

```text
SP_BUSY_CYCLES:              8,820,830,026
SP_ALU_WORKING_CYCLES:       5,381,292,032
UCHE_BUSY_CYCLES:              552,913,593
UCHE_RAM_READ_REQ:              67,121,254
UCHE_VBIF_READ_BEATS_SP:                  0
SP_UCHE_READ_TRANS:                       0
```

## Relative to stride 1

```text
SP_BUSY_CYCLES:         1.018×
SP_ALU_WORKING_CYCLES:  1.000×
UCHE_BUSY_CYCLES:       1.971×
UCHE_RAM_READ_REQ:      1.923×
```

Stride 64 has the highest `UCHE_RAM_READ_REQ` total of the four captures.

---

# Counter schema

The valid CSV table has seven columns:

```text
elapsed_s
SP_BUSY_CYCLES
SP_ALU_WORKING_CYCLES
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
```

# `elapsed_s`

Time in seconds since the streamer began sampling.

This is not a GPU timestamp.

The difference between adjacent rows is the observed host-side sampling step.

Although the requested interval was 5 ms, the actual step varies due to:

- process scheduling;
- system-call latency;
- counter-read overhead;
- ADB/shell execution;
- CPU load; and
- operating-system timing.

# Counter columns

Every counter value is a delta since the previous sample.

A row does not contain the absolute hardware counter register value.

For an isolated workload window, summing deltas approximates the number of
counted events accumulated during that window.

---

# Counter interpretation

The following descriptions are based on counter names and intended use. Exact
hardware definitions depend on the Adreno counter table and generation.

# `SP_BUSY_CYCLES`

Cycles during which the shader processor block reports busy activity.

Useful for:

- broad SP activity;
- workload duration in SP-cycle terms;
- comparison with ALU-working cycles; and
- identifying whether larger stride changes overall shader-core activity.

# `SP_ALU_WORKING_CYCLES`

Cycles during which SP arithmetic hardware reports working activity.

In this experiment, the primary-region totals are nearly identical across all
four strides.

That is a useful control result because the stride shader performs the same loop
and accumulator arithmetic for every stride.

# `UCHE_BUSY_CYCLES`

Cycles during which UCHE reports busy activity.

UCHE is involved in GPU memory/cache traffic.

The totals increase strongly from stride 1 to 16, then decrease somewhat at
stride 64 while remaining well above stride 1.

This is a non-monotonic response and should not be simplified to “larger stride
always means more UCHE busy cycles.”

# `UCHE_RAM_READ_REQ`

RAM-read requests attributed through UCHE.

The primary-region total increases monotonically:

```text
stride 1:   34,900,424
stride 4:   38,403,772
stride 16:  50,431,114
stride 64:  67,121,254
```

Normalized to stride 1:

```text
stride 1:   1.000×
stride 4:   1.100×
stride 16:  1.445×
stride 64:  1.923×
```

This is the clearest stride-dependent trend in these four files.

# `UCHE_VBIF_READ_BEATS_SP`

Intended to represent UCHE/VBIF read-beat activity associated with SP.

Observed values:

```text
stride 1:  one nonzero sample, total 7,068
stride 4:  one nonzero sample, total 7,068
stride 16: zero
stride 64: zero
```

This pattern is not credible as a robust quantitative stride trend by itself.

Possible explanations include:

- the counter does not behave as expected on this generation;
- the selector mapping is incomplete;
- the event is extremely sparse;
- counter programming or reading is unreliable;
- the workload path is not attributed to this countable; or
- the one-shot values are setup/background events.

Treat this counter as inconclusive in this evidence set.

# `SP_UCHE_READ_TRANS`

Intended to represent SP-to-UCHE read transactions.

Observed values:

```text
stride 1:  one nonzero sample, total 4,800
stride 4:  one nonzero sample, total 4,800
stride 16: zero
stride 64: zero
```

As with `UCHE_VBIF_READ_BEATS_SP`, the result is too sparse and inconsistent to
support a quantitative stride conclusion.

---

# Observed comparison

Using the longest contiguous nonzero region in each file:

| Stride | SP busy | SP ALU working | UCHE busy | UCHE RAM reads | VBIF read beats | SP→UCHE reads |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 8,663,257,167 | 5,381,565,070 | 280,459,342 | 34,900,424 | 7,068 | 4,800 |
| 4 | 8,716,333,912 | 5,381,565,070 | 389,984,495 | 38,403,772 | 7,068 | 4,800 |
| 16 | 8,780,704,191 | 5,381,292,032 | 619,375,381 | 50,431,114 | 0 | 0 |
| 64 | 8,820,830,026 | 5,381,292,032 | 552,913,593 | 67,121,254 | 0 | 0 |

Normalized to stride 1:

| Stride | SP busy | SP ALU working | UCHE busy | UCHE RAM reads |
|---:|---:|---:|---:|---:|
| 1 | 1.000× | 1.000× | 1.000× | 1.000× |
| 4 | 1.006× | 1.000× | 1.391× | 1.100× |
| 16 | 1.014× | 1.000× | 2.208× | 1.445× |
| 64 | 1.018× | 1.000× | 1.971× | 1.923× |

---

# Main findings supported by the files

# 1. ALU work is controlled well

`SP_ALU_WORKING_CYCLES` changes by less than approximately 0.01% across the
four primary regions.

This supports the intended experimental control:

```text
same loop arithmetic
same element count
same iteration count
same dispatch count
different memory stride
```

# 2. SP busy increases only slightly

`SP_BUSY_CYCLES` increases by approximately 1.8% from stride 1 to stride 64.

This suggests that changing stride affects the workload without dramatically
changing total SP busy-cycle accumulation.

# 3. RAM-read requests increase clearly

`UCHE_RAM_READ_REQ` rises monotonically and reaches approximately 1.92× the
stride-1 total at stride 64.

This is the strongest evidence that reduced iteration-to-iteration locality
caused more lower-level read demand.

# 4. UCHE busy behavior is strong but non-monotonic

`UCHE_BUSY_CYCLES` rises to approximately 2.21× at stride 16, then is
approximately 1.97× at stride 64.

Possible reasons include:

- request batching;
- cache behavior;
- latency versus throughput balance;
- sampling-window differences;
- GPU frequency behavior;
- counter semantics; or
- normal run-to-run variation.

One run per stride is not enough to separate these explanations.

# 5. Two transaction counters are inconclusive

`UCHE_VBIF_READ_BEATS_SP` and `SP_UCHE_READ_TRANS` are zero in most samples and
entirely zero in the stride-16 and stride-64 captures.

These counters should not be used as the primary evidence from this dataset.

---

# What the files do not prove

The files do not prove that:

- stride 64 is universally the slowest access pattern;
- all additional RAM-read requests reached external DRAM;
- UCHE was the final performance bottleneck;
- cache-line size is 64 or 256 bytes;
- the sparse transaction counters are invalid on every workload;
- the four runs had identical GPU frequency;
- the four runs had identical temperature;
- the active windows represent exactly the same GPU duration; or
- the results are statistically reproducible.

A stronger experiment would repeat every stride several times and record:

```text
runtime
GPU frequency
thermal state
driver identity
benchmark command
runner/shader hashes
counter totals
active-window boundaries
mean and standard deviation
```

---

# Why these evidence files are needed

# 1. Validate memory-related counters

The dataset shows that at least some memory-related counters respond to changing
access locality.

`UCHE_RAM_READ_REQ` is particularly useful because it exhibits a clear,
monotonic response.

# 2. Validate experimental controls

The nearly constant `SP_ALU_WORKING_CYCLES` provides evidence that the arithmetic
portion remained controlled.

# 3. Preserve the original raw stream

The files retain:

- idle samples;
- active samples;
- preamble metadata;
- selector IDs;
- register mappings;
- sampling timing; and
- sparse unexpected counter behavior.

A summarized table alone would lose this information.

# 4. Support future parser testing

These files are useful fixtures for testing:

- hybrid log/CSV parsing;
- active-window detection;
- zero/sparse counter handling;
- counter-total calculation; and
- capture comparison.

# 5. Document profiler limitations

The two sparse transaction counters demonstrate why counter names and successful
`GET` operations are not sufficient to establish that a counter is useful.

---

# Read the files correctly

# Python loader

Use a parser that locates the actual CSV header:

```python
from io import StringIO
from pathlib import Path

import pandas as pd


def read_streamer_capture(path: str | Path) -> tuple[pd.DataFrame, list[str]]:
    path = Path(path)
    lines = path.read_text(errors="replace").splitlines()

    try:
        header_index = next(
            i for i, line in enumerate(lines)
            if line.startswith("elapsed_s,")
        )
    except StopIteration as exc:
        raise ValueError(f"No elapsed_s CSV header found in {path}") from exc

    preamble = lines[:header_index]
    csv_text = "\n".join(lines[header_index:])

    frame = pd.read_csv(StringIO(csv_text))

    if "elapsed_s" not in frame.columns:
        raise ValueError(f"Parsed table has no elapsed_s column: {path}")

    return frame, preamble
```

Use it:

```python
df, preamble = read_streamer_capture(
    "evidence/perfcounter_calibration/"
    "2026-06-11_mem_stride/"
    "mem_stride_1_perf_stream_final.csv"
)

print("\n".join(preamble))
print(df.head())
```

---

# Bash extraction of the strict CSV section

Create a derived clean CSV without modifying the original:

```bash
INPUT="evidence/perfcounter_calibration/2026-06-11_mem_stride/mem_stride_1_perf_stream_final.csv"
OUTPUT="/tmp/mem_stride_1_clean.csv"

awk '
  /^elapsed_s,/ {
    emit = 1
  }

  emit {
    print
  }
' "$INPUT" > "$OUTPUT"
```

Check:

```bash
head "$OUTPUT"
```

The derived file omits the counter-selection and register metadata.

---

# Detect the main active region

The following script selects the longest contiguous run of rows where at least
one counter is nonzero.

```bash
python3 - <<'PY'
from io import StringIO
from pathlib import Path

import numpy as np
import pandas as pd


def load(path):
    path = Path(path)
    lines = path.read_text(errors="replace").splitlines()
    header = next(
        i for i, line in enumerate(lines)
        if line.startswith("elapsed_s,")
    )
    return pd.read_csv(StringIO("\n".join(lines[header:])))


path = Path(
    "evidence/perfcounter_calibration/"
    "2026-06-11_mem_stride/"
    "mem_stride_1_perf_stream_final.csv"
)

df = load(path)
counter_columns = [
    column for column in df.columns
    if column != "elapsed_s"
]

active = df[counter_columns].abs().sum(axis=1) != 0
indices = list(np.flatnonzero(active.to_numpy()))

regions = []

if indices:
    start = previous = indices[0]

    for index in indices[1:]:
        if index == previous + 1:
            previous = index
        else:
            regions.append((start, previous))
            start = previous = index

    regions.append((start, previous))

if not regions:
    raise SystemExit("No nonzero region detected")

start, end = max(
    regions,
    key=lambda pair: pair[1] - pair[0] + 1,
)

region = df.iloc[start:end + 1]

print(f"start_s={region['elapsed_s'].iloc[0]}")
print(f"end_s={region['elapsed_s'].iloc[-1]}")
print(f"samples={len(region)}")
print()
print(region[counter_columns].sum().to_string())
PY
```

---

# Compare all four files

```bash
python3 - <<'PY'
from io import StringIO
from pathlib import Path

import numpy as np
import pandas as pd


root = Path(
    "evidence/perfcounter_calibration/"
    "2026-06-11_mem_stride"
)


def load(path):
    lines = path.read_text(errors="replace").splitlines()
    header = next(
        i for i, line in enumerate(lines)
        if line.startswith("elapsed_s,")
    )
    return pd.read_csv(StringIO("\n".join(lines[header:])))


def longest_nonzero_region(df):
    counters = [
        column for column in df.columns
        if column != "elapsed_s"
    ]

    active = df[counters].abs().sum(axis=1) != 0
    indices = list(np.flatnonzero(active.to_numpy()))

    if not indices:
        return df.iloc[0:0]

    regions = []
    start = previous = indices[0]

    for index in indices[1:]:
        if index == previous + 1:
            previous = index
        else:
            regions.append((start, previous))
            start = previous = index

    regions.append((start, previous))

    start, end = max(
        regions,
        key=lambda pair: pair[1] - pair[0] + 1,
    )

    return df.iloc[start:end + 1]


rows = []

for stride in [1, 4, 16, 64]:
    path = root / f"mem_stride_{stride}_perf_stream_final.csv"
    df = load(path)
    region = longest_nonzero_region(df)

    counters = [
        column for column in df.columns
        if column != "elapsed_s"
    ]

    row = {
        "stride": stride,
        "start_s": region["elapsed_s"].iloc[0],
        "end_s": region["elapsed_s"].iloc[-1],
        "samples": len(region),
    }

    for counter in counters:
        row[counter] = float(region[counter].sum())

    rows.append(row)

summary = pd.DataFrame(rows).sort_values("stride")

print("=== Primary-region totals ===")
print(summary.to_string(index=False))

numeric_counters = [
    column for column in summary.columns
    if column not in {"stride", "start_s", "end_s", "samples"}
]

normalized = (
    summary
    .set_index("stride")[numeric_counters]
    .div(summary.set_index("stride").loc[1, numeric_counters])
)

print()
print("=== Normalized to stride 1 ===")
print(normalized.round(3).to_string())
PY
```

---

# Inspect sparse counters

```bash
python3 - <<'PY'
from io import StringIO
from pathlib import Path

import pandas as pd


root = Path(
    "evidence/perfcounter_calibration/"
    "2026-06-11_mem_stride"
)

sparse = [
    "UCHE_VBIF_READ_BEATS_SP",
    "SP_UCHE_READ_TRANS",
]


def load(path):
    lines = path.read_text(errors="replace").splitlines()
    header = next(
        i for i, line in enumerate(lines)
        if line.startswith("elapsed_s,")
    )
    return pd.read_csv(StringIO("\n".join(lines[header:])))


for stride in [1, 4, 16, 64]:
    path = root / f"mem_stride_{stride}_perf_stream_final.csv"
    df = load(path)

    print(f"\n=== stride {stride} ===")

    for counter in sparse:
        values = df.loc[df[counter] != 0, ["elapsed_s", counter]]

        print(f"\n{counter}")

        if values.empty:
            print("(no nonzero samples)")
        else:
            print(values.to_string(index=False))
PY
```

---

# Plot the captures

Plotting is optional and requires matplotlib:

```bash
python3 -m pip install pandas matplotlib
```

Example:

```bash
python3 - <<'PY'
from io import StringIO
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


path = Path(
    "evidence/perfcounter_calibration/"
    "2026-06-11_mem_stride/"
    "mem_stride_64_perf_stream_final.csv"
)

lines = path.read_text(errors="replace").splitlines()
header = next(
    i for i, line in enumerate(lines)
    if line.startswith("elapsed_s,")
)

df = pd.read_csv(StringIO("\n".join(lines[header:])))

plt.figure(figsize=(10, 5))
plt.plot(
    df["elapsed_s"],
    df["UCHE_RAM_READ_REQ"],
)
plt.xlabel("Elapsed time (s)")
plt.ylabel("UCHE_RAM_READ_REQ delta")
plt.title("Memory stride 64")
plt.tight_layout()
plt.show()
PY
```

Use separate plots or normalized values when comparing counters with very
different magnitudes.

---

# Reproduce the workload

The historical files are evidence, not scripts.

The current workload implementation is under:

```text
benchmarks/microbenchmarks/memory_stride/
```

The practical historical stride path uses:

```text
legacy_shaders/mem_stride_legacy.comp
runner/vk_mem_probe
```

Because the runner’s CPU reference does not match the legacy stride shader, use:

```text
--no-verify
```

for the current historical workflow.

## Compile legacy stride shaders

```bash
cd /Users/jerryyun/adreno-gpu-profiler

MEM_DIR="benchmarks/microbenchmarks/memory_stride"

mkdir -p "$MEM_DIR/legacy_shaders/spv"

for STRIDE in 1 4 16 64; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "-DSTRIDE=${STRIDE}u" \
    "$MEM_DIR/legacy_shaders/mem_stride_legacy.comp" \
    -o \
    "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"
done
```

## Validate

```bash
for STRIDE in 1 4 16 64; do
  spirv-val \
    --target-env vulkan1.1 \
    "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"
done
```

## Push

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/memory_stride'

adb push \
  "$MEM_DIR/runner/vk_mem_probe" \
  /data/local/tmp/jerry_work/memory_stride/

adb push \
  "$MEM_DIR/legacy_shaders/spv/"*_legacy.comp.spv \
  /data/local/tmp/jerry_work/memory_stride/

adb shell \
  'chmod 755 /data/local/tmp/jerry_work/memory_stride/vk_mem_probe'
```

## Run one workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_16_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

---

# Reproduce the streamer capture

The exact current streamer CLI should be confirmed from:

```text
tools/profiling/perfcounter_streamer/README.md
```

The historical files show:

```text
interval = 0.005 seconds
six counters selected
CSV deltas enabled
```

Use two terminals.

## Terminal 1: streamer

Conceptually:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.005 \
    --csv \
    -n SP_BUSY_CYCLES \
    -n SP_ALU_WORKING_CYCLES \
    -n UCHE_BUSY_CYCLES \
    -n UCHE_RAM_READ_REQ \
    -n UCHE_VBIF_READ_BEATS_SP \
    -n SP_UCHE_READ_TRANS"'
```

The current binary may use a different multi-counter option format.

Check:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream --help"'
```

## Terminal 2: workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_16_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

Stop the streamer after the workload finishes.

---

# Recommended future capture layout

A more complete evidence directory should contain:

```text
2026-06-11_mem_stride/
├── README.md
├── experiment_config.txt
├── hashes.txt
├── benchmark_logs/
│   ├── stride_1.log
│   ├── stride_4.log
│   ├── stride_16.log
│   └── stride_64.log
├── raw/
│   ├── mem_stride_1_perf_stream_final.csv
│   ├── mem_stride_4_perf_stream_final.csv
│   ├── mem_stride_16_perf_stream_final.csv
│   └── mem_stride_64_perf_stream_final.csv
└── derived/
    ├── primary_region_totals.csv
    ├── normalized_to_stride_1.csv
    └── analysis_summary.md
```

The historical files should not be moved unless repository organization requires
it and the move is recorded in Git.

---

# Recommended experiment metadata

A future `experiment_config.txt` should record:

```text
date
device model
Android build fingerprint
kernel version
GPU model
Vulkan driver
streamer Git commit
streamer SHA-256
runner Git commit
runner SHA-256
shader SHA-256
shader source path
stride
elements
iterations
dispatch repeats
sampling interval
counter names
counter selectors
counter registers
warm-up procedure
run order
GPU frequency policy
thermal state
benchmark exit code
verification status
```

---

# Known limitations of this evidence set

## One run per stride

There are no repeated captures for calculating variance.

## Different active sample counts

The primary regions contain:

```text
stride 1:  139 samples
stride 4:  127 samples
stride 16: 125 samples
stride 64: 124 samples
```

Counter totals can be affected by region duration and sampling alignment.

## Stride-1 isolated sample

The stride-1 file contains one additional nonzero sample outside the main active
region.

## Sparse transaction counters

Two selected counters do not provide a reliable trend.

## No runtime measurements

The files contain host elapsed time but no Vulkan timestamp-query results.

## No frequency or thermal data

Differences may partly reflect dynamic frequency scaling or temperature.

## No benchmark log

The files do not prove that the intended stride shader was loaded.

## No driver identity

Vendor Vulkan and Turnip can compile or schedule the same shader differently.

## Hybrid file format

Standard CSV readers require preprocessing.

## Counter semantics are generation-specific

A successful selector request does not prove that a counter is meaningful for
the workload.

## Counter multiplexing context is not documented

The files show six counters selected at once, but do not include a separate
explanation of hardware slot limits or whether any counters shared resources.

---

# Recommended maintenance

1. Keep the four raw files immutable.
2. Add this README beside them.
3. Add a hash manifest for this evidence directory.
4. Add a small reusable hybrid-stream parser.
5. Save benchmark stdout/stderr in future experiments.
6. Record exact runner and shader hashes.
7. Repeat every stride at least three times.
8. Rotate stride order to reduce thermal/order bias.
9. Record GPU frequency and thermal state.
10. Store primary-region totals as derived data, not replacements for raw files.
11. Treat the two sparse transaction counters as inconclusive.
12. Use `UCHE_RAM_READ_REQ` as the strongest observed stride-sensitive signal in
    this dataset.
13. Compare with runtime and KGSL RAM-wait before claiming a bottleneck.
14. Add a classic lane-stride workload if coalescing is the intended question.
15. Add pointer chasing if dependent memory latency is the intended question.
16. Confirm the exact shader source corresponding to these 2026-06-11 captures.
17. Record whether the vendor or Turnip Vulkan driver was used.
18. Add a machine-readable metadata format such as JSON or TOML.

---

# Quick summary

```text
mem_stride_1_perf_stream_final.csv
    locality-friendly reference
    lowest UCHE_RAM_READ_REQ
    one isolated nonzero sample after the main region

mem_stride_4_perf_stream_final.csv
    modest increase in UCHE activity and RAM reads

mem_stride_16_perf_stream_final.csv
    highest UCHE_BUSY_CYCLES
    1.445× stride-1 RAM-read requests

mem_stride_64_perf_stream_final.csv
    highest UCHE_RAM_READ_REQ
    1.923× stride-1 RAM-read requests
```

The main supported conclusion is:

```text
As stride increases, SP ALU work remains essentially constant while
UCHE RAM-read requests increase substantially.
```

The evidence supports increased memory-system pressure with larger stride, but
does not by itself prove the final performance bottleneck or explain the
non-monotonic UCHE busy-cycle behavior.
