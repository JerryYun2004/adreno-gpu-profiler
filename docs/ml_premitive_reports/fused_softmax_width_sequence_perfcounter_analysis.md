# Fused Softmax Width-Sequence Perfcounter Analysis

## Topic

This document summarizes the first clean Adreno A8xx KGSL performance-counter analysis of a fused softmax width sweep. The goal was to understand how GPU performance counters change as the softmax reduction width increases, and to identify counters that suggest shader utilization, memory/cache pressure, underutilization, or bottlenecks.

## Experiment Summary

The benchmark used a fused local-memory softmax workload with a fixed row count and repeat count, while sweeping the softmax width inside one recorded counter window.

The width sequence was:

```text
width = 128
sleep
width = 256
sleep
width = 512
sleep
width = 1024
sleep
width = 2048
```

This was implemented using the internal width-sequence mode in `streamer_sweeper`, so each counter chunk recorded the same width sequence. This made it possible to compare counter values across widths within each counter CSV trace.

Example command:

```bash
adb shell "su -c \"/data/local/tmp/streamer_sweeper \
  --time 4 \
  --widths 128,256,512,1024,2048 \
  --width-sleep 0.1 \
  --bench-args '--rows 128 --repeats 16 --csv'\""
```

The analysis used `SP_chunk001.csv` as a reference trace to detect the five width windows. A padded reference-window method was then applied to all counter CSV files.

Final reference windows used for analysis:

```text
width 128:   0.0499–0.1120 s
width 256:   0.2487–0.3074 s
width 512:   0.4418–0.5006 s
width 1024:  0.6343–0.6935 s
width 2048:  0.8443–0.9026 s
```

Each window contained about 41–42 samples, which reduced the risk of missing activity due to small timing shifts between counter chunks.

## Analysis Method

The raw sweep generated many CSV files, one per counter chunk. Since many counter groups are inactive or mostly irrelevant for compute workloads, automatic region detection inside every CSV was unreliable.

The final analysis used the following approach:

1. Select a reliable reference trace from the shader processor group:

   ```text
   11_SP/SP_chunk001.csv
   ```

2. Detect the five activity regions corresponding to the five softmax widths.

3. Pad each detected region to make the time windows robust across chunks.

4. Apply the same reference time windows to every counter CSV.

5. For each counter and width, compute:

   * total counter delta
   * maximum sample value
   * mean sample value
   * active-sample mean
   * number of active samples

6. Classify counters based on scaling behavior:

   * zero across all widths
   * strong width scaling
   * moderate width scaling
   * flat or saturated
   * mixed active behavior

The final analysis folder was:

```text
results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20
```

Important output files:

```text
reference_width_windows.csv
counter_width_summary.csv
counter_classification.csv
zero_counters.csv
width_scaling_counters.csv
flat_or_saturated_counters.csv
```

## Key Sanity Check: Width Scaling Is Visible

The strongest sanity check is that many counters scale almost exactly with the width ratio.

The largest width is 2048 and the smallest width is 128:

```text
2048 / 128 = 16
```

Several counters also scale almost exactly 16×:

```text
SP_FULL_ALU_MUL_INSTRUCTIONS:  32,768 → 524,288
SP_GM_STORE_INSTRUCTIONS:      16,384 → 262,144
UCHE_WRITE_REQUESTS_SP:        16,384 → 262,144
UCHE_RAM_READ_REQ:             16,176 → 262,152
TP_TP_SP_TRANS:                32,768 → 524,288
TP_L1_CACHELINE_REQUESTS:       8,192 → 131,072
```

This confirms that the selected analysis windows are aligned with the intended softmax width sequence. The counter data is not random noise; it is clearly tracking the width-dependent workload.

## Shader Processor Findings

Some shader processor counters increase with width, but not all of them scale linearly with element count.

Examples of SP cycle/utilization counters:

```text
SP_BUSY_CYCLES:         1.28M → 3.53M, ratio ≈ 2.77
SP_ALU_WORKING_CYCLES:  0.20M → 0.55M, ratio ≈ 2.80
SP_WAVE_CONTEXTS:       18.0M → 50.5M, ratio ≈ 2.80
```

These counters increase as width increases, but they do not scale 16×. This is expected because cycle-style counters reflect time/utilization, not simply the number of elements processed. The GPU can exploit parallelism, overlap execution, and amortize fixed overheads.

In contrast, some SP instruction and transaction counters scale almost exactly with width:

```text
SP_FULL_ALU_MUL_INSTRUCTIONS: 16×
SP_FS_STAGE_TEX_INSTRUCTIONS: 16×
SP_GM_STORE_INSTRUCTIONS:     16×
SP_UCHE_WRITE_TRANS:          16×
```

This suggests that the amount of shader work and data movement increases nearly linearly with softmax width, while SP cycle counters reflect parallel execution and scheduling effects.

## Memory, Cache, TP, and UCHE Findings

The strongest bottleneck-related signals appear in the TP, UCHE, and VBIF/cache-memory path.

Important counters that scale strongly with width include:

```text
UCHE_VBIF_LATENCY_CYCLES:        250K → 7.38M, ratio ≈ 29.5
UCHE_STARVED_CYCLES_VBIF_DECMP:  4.9K → 244K, ratio ≈ 50.0
UCHE_RAM_WRITE_REQ:              10K → 262K, ratio ≈ 25.6
SP_TEXTURE_FETCH_LATENCY_CYCLES: 49.9K → 1.19M, ratio ≈ 23.9
TP_L1_5_MISS_LATENCY_CYCLES:     50.6K → 800K, ratio ≈ 15.8
TP_STARVE_CYCLES_UCHE:           68.4K → 1.00M, ratio ≈ 14.7
SP_LOW_EFFICIENCY_STARVED_BY_TP: 73.8K → 1.15M, ratio ≈ 15.5
SP_STALL_CYCLES_TP:              644 → 9,088, ratio ≈ 14.1
```

These counters suggest that larger softmax widths increasingly stress the memory/cache path. In particular, the following counter families are important:

```text
UCHE read/write requests
UCHE RAM read/write requests
UCHE/VBIF latency counters
TP cacheline request counters
TP starvation counters
SP stall/starvation counters related to TP/UCHE
```

The memory/cache counters scale more aggressively than the basic SP busy and ALU working cycle counters. This suggests that larger fused-softmax widths are not limited only by ALU execution. Instead, the TP/UCHE/VBIF path becomes increasingly important and may become the main pressure point at larger widths.

## Interpretation of Zero Counters

Many counters remain zero throughout the sweep. This is expected.

Common reasons include:

1. The counter belongs to a graphics pipeline block that is not used by a compute workload.
2. The counter measures a shader stage or hardware path that fused softmax does not trigger.
3. The counter exists in the XML table but is not meaningful for this GPU/kernel path.
4. The event being counted is valid but irrelevant to this benchmark.

For a compute softmax workload, many counters from these groups may be zero or mostly irrelevant:

```text
VFD
VPC
TSE
RAS
RB
LRZ
some PC graphics-stage counters
some texture/fragment/render-backend counters
```

Zero counters should not automatically be interpreted as errors. They usually mean the corresponding hardware path was inactive during the compute workload.

## Important Note on Counter Names

Some counters have names that sound graphics-specific, such as:

```text
TP_OUTPUT_PIXELS
TP_QUADS_RECEIVED
SP_FS_STAGE_TEX_INSTRUCTIONS
SP_TEXTURE_FETCH_LATENCY_SAMPLES
```

However, several of these counters scale cleanly with the compute softmax width sequence. Therefore, they should not be discarded solely because their names sound graphics-specific.

On Adreno, some performance-counter names appear to be reused across shared internal datapaths, or the XML naming does not map cleanly to high-level Vulkan shader stages. The safest approach is to interpret counters empirically:

```text
If a counter scales cleanly with a controlled compute workload, it is measuring a real workload-related event.
```

## Underutilization vs Bottleneck Interpretation

The current data does not suggest a simple “ALU-only” bottleneck.

A pure ALU bottleneck would likely show SP ALU and SP busy counters dominating the trend, with memory/cache counters growing less strongly. Instead, this sweep shows that:

```text
SP instruction and transaction counters scale with width.
SP busy and ALU working cycles increase moderately.
TP/UCHE/VBIF latency and request counters increase strongly.
Starvation/stall counters related to TP/UCHE become more significant at larger widths.
```

This suggests the larger fused-softmax widths increasingly pressure the memory/cache path.

Likely interpretation:

```text
At small widths, fixed overhead and limited work size matter more.
As width increases, the amount of shader work and memory traffic increases nearly linearly.
At larger widths, TP/UCHE/VBIF latency and starvation counters rise strongly, suggesting increasing cache/memory pressure.
```

## Current Working Hypothesis

The fused softmax width sweep produces consistent counter scaling. Many instruction, transaction, and cache request counters scale almost exactly 16× from width 128 to width 2048, matching the 16× increase in softmax width.

SP busy and ALU working cycles increase by about 2.8×, which indicates that these cycle counters are not directly proportional to element count. This is expected because the GPU executes work in parallel and can overlap some operations.

Memory/cache-related counters in TP, UCHE, and VBIF scale much more strongly. UCHE read/write requests, TP cacheline requests, SP store/UCHE transactions, VBIF latency counters, and TP/UCHE starvation counters all increase significantly at larger widths.

Overall, the larger fused-softmax widths appear to put increasing pressure on the memory/cache path, especially TP/UCHE/VBIF, rather than being limited only by SP ALU execution.

## Recommended Next Steps

1. Generate plots for the key SP, TP, UCHE, and VBIF counters.
2. Compare fused softmax against other softmax variants, such as online and three-pass.
3. Add targeted group selection to the sweeper, for example:

   ```bash
   --groups SP,UCHE,TP,CP,HLSQ
   ```

   This would reduce runtime and avoid sweeping many irrelevant graphics counters.
4. Add marker logging directly inside the sweeper:

   ```text
   width,start_elapsed_s,end_elapsed_s,benchmark_exit_status
   ```

   This would remove the need to infer width windows from counter traces.
5. Repeat the width-sequence sweep with different row counts to separate width scaling from occupancy/batch-size effects.
6. Compare per-width runtime with counter totals to determine whether larger widths are actually slower due to memory/cache pressure or simply doing proportionally more work.

## Useful Analysis Commands

Show strongest width-scaling counters:

```bash
python3 - <<'PY'
import pandas as pd

df = pd.read_csv("results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20/counter_classification.csv")

cols = [
    "group", "counter", "label",
    "scaling_ratio_last_over_first",
    "corr_total_vs_width",
    "total_w128", "total_w256", "total_w512", "total_w1024", "total_w2048",
]

print(
    df[df["label"].isin(["strong_width_scaling", "moderate_width_scaling"])]
    .sort_values("scaling_ratio_last_over_first", ascending=False)
    [cols]
    .head(80)
    .to_string(index=False)
)
PY
```

Show memory/cache/bottleneck-looking counters:

```bash
python3 - <<'PY'
import pandas as pd

df = pd.read_csv("results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20/counter_classification.csv")

keywords = "BUSY|STALL|STARVE|LATENCY|READ|WRITE|REQ|TRANS|MISS|VBIF|UCHE|CACHE"

mask = (
    df["counter"].str.contains(keywords, case=False, na=False) &
    (df["total_all_widths"] > 0)
)

cols = [
    "group", "counter", "label",
    "scaling_ratio_last_over_first",
    "corr_total_vs_width",
    "total_w128", "total_w256", "total_w512", "total_w1024", "total_w2048",
]

print(df[mask].sort_values("total_w2048", ascending=False)[cols].head(100).to_string(index=False))
PY
```

Find counters with near-perfect 16× scaling:

```bash
python3 - <<'PY'
import pandas as pd

df = pd.read_csv("results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20/counter_classification.csv")

near16 = df[
    (df["scaling_ratio_last_over_first"] > 15.5) &
    (df["scaling_ratio_last_over_first"] < 16.5) &
    (df["corr_total_vs_width"] > 0.99)
].copy()

cols = [
    "group", "counter",
    "scaling_ratio_last_over_first",
    "corr_total_vs_width",
    "total_w128", "total_w2048",
]

print(near16.sort_values("total_w2048", ascending=False)[cols].to_string(index=False))
PY
```

Find possible superlinear bottleneck counters:

```bash
python3 - <<'PY'
import pandas as pd

df = pd.read_csv("results/perfcounter_analysis/latest_width_sequence_ref_windows_sp_pad20/counter_classification.csv")

superlinear = df[
    (df["scaling_ratio_last_over_first"] > 16.5) &
    (df["total_all_widths"] > 0)
].copy()

cols = [
    "group", "counter", "label",
    "scaling_ratio_last_over_first",
    "corr_total_vs_width",
    "total_w128", "total_w256", "total_w512", "total_w1024", "total_w2048",
]

print(superlinear.sort_values("scaling_ratio_last_over_first", ascending=False)[cols].head(80).to_string(index=False))
PY
```
