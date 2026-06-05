# Daily Log — Mobile GPU Profiling Work

## Date

2026-06-02

## Objective

The goal today was to determine how much useful GPU profiling information can be collected on the Android Adreno 830 phone **without modifying Linux kernel permissions** or directly accessing hardware performance counters. The focus was on KGSL/sysfs nodes, tracefs tracepoints, Vulkan microbenchmarks, and building a repeatable profiling workflow.

---

## Tasks Completed

### 1. Built a full KGSL/sysfs + tracefs capture workflow

I created and tested a workflow that enables KGSL/GPU tracepoints, runs Vulkan microbenchmarks, samples KGSL/sysfs nodes, and saves all outputs into structured run folders.

The capture includes:

- Raw tracefs log: `trace_raw.log`
- Fast sysfs samples: `sysfs_fast_samples.csv`
- Workload stdout/stderr
- Tracepoint enabled-state metadata
- Benchmark configuration metadata

The workflow was tested on controlled Vulkan workloads rather than noisy UI activity.

---

### 2. Captured repeated compute-heavy and memory-heavy datasets

I first ran repeat10 and then repeat30 captures for the original two workloads:

- `vk_compute_probe`
- `vk_mem_probe`

Both workloads completed cleanly with verification passing. For the stronger repeat30 runs, both had:

- 30 verified iterations
- 107 KGSL/GPU tracepoints enabled
- Similar command-batch counts
- Similar KGSL memory allocation structure

This gave a controlled basis for comparing compute-heavy and memory-heavy behavior.

---

### 3. Built a reusable comparison parser

I created a reusable Python parser:

```bash
tools/compare_kgsl_runs.py
```

The parser compares two run folders and extracts:

- Verification count
- Tracepoint count
- Sysfs fast-sample statistics
- Command-batch counts
- KGSL memory allocation totals
- `kgsl_buslevel` values
- `gpu_work_period` active time
- `kgsl_pwrstats` values
- `ram_wait` and busy percentages

The parser outputs:

```
compare_summary.txt
compare_summary.csv
profile_summary_<label>.txt
```

---

### 4. Added `kgsl_pwrstats` parsing

I inspected the `kgsl_pwrstats` tracepoint format:

```
kgsl_pwrstats: d_name=kgsl-3d0 total=... busy=... ram_time=... ram_wait=... context_count=...
```

Then I extended the parser to calculate:

```
pwrstats_overall_busy_pct = busy_sum / total_sum
pwrstats_overall_ram_wait_pct = ram_wait_sum / ram_time_sum
pwrstats_ram_wait_pct_avg
pwrstats_ram_wait_sum
```

This became the strongest metric for distinguishing memory-heavy behavior.

---

### 5. Repeated validation of compute vs memory result

I ran a second independent compute/memory repeat30 validation pair. The result repeated strongly:

- Compute workload had low RAM-wait, around 3.9%.
- Memory workload had high RAM-wait, around 50%.
- Command-batch counts and KGSL allocation totals remained nearly identical.

This confirmed that the observed memory-pressure signal was reproducible, not a one-off run.

---

### 6. Verified benchmark shader behavior

I pulled the deployed SPIR-V shader binaries from the phone:

```bash
adb pull /data/local/tmp/jerry_work/alu.comp.spv kgsl_full_capture/deployed_artifacts/alu.comp.spv
adb pull /data/local/tmp/jerry_work/mem.comp.spv kgsl_full_capture/deployed_artifacts/mem.comp.spv
```

I installed/used SPIR-V tools and disassembled the shaders with:

```bash
spirv-dis kgsl_full_capture/deployed_artifacts/alu.comp.spv \
  -o kgsl_full_capture/deployed_artifacts/alu.comp.spvasm

spirv-dis kgsl_full_capture/deployed_artifacts/mem.comp.spv \
  -o kgsl_full_capture/deployed_artifacts/mem.comp.spvasm
```

I confirmed:

- `alu.comp` contains a real arithmetic loop with repeated integer multiply/add/xor/shift operations.
- `mem.comp` contains a real loop with four indexed global loads per iteration.
- The memory shader also has significant address-generation arithmetic, so the original memory benchmark was both memory-heavy and somewhat ALU-heavy.

---

### 7. Built a cleaner three-way benchmark suite

To separate workload behavior more clearly, I created a new three-way benchmark suite:

```
copy_baseline.comp
alu_heavy.comp
mem_heavy_clean.comp
vk_threeway_probe.cpp
```

The intended workload types are:

```
copy_baseline:
  one load + one store, almost no arithmetic

alu_heavy:
  one input load, long dependent integer ALU chain, one output store

mem_heavy_clean:
  repeated global loads with simpler address generation
```

I compiled the shaders:

```bash
cd /Users/jerryyun/adreno_turnip/clean_benchmarks

glslangValidator -V alu_heavy.comp -o alu_heavy.comp.spv
glslangValidator -V mem_heavy_clean.comp -o mem_heavy_clean.comp.spv
glslangValidator -V copy_baseline.comp -o copy_baseline.comp.spv
```

I downloaded/used Android NDK r27d locally and built the Android binary:

```bash
cd /Users/jerryyun/adreno_turnip/clean_benchmarks

export NDK=/Users/jerryyun/android-ndk-r27d
export API=29
export HOST_TAG=darwin-x86_64

"$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android${API}-clang++" \
  -std=c++17 -O2 \
  vk_threeway_probe.cpp \
  -o vk_threeway_probe \
  -lvulkan
```

The binary was confirmed as:

```
ELF 64-bit ARM aarch64 Android executable
```

I pushed the files to the phone:

```bash
adb push vk_threeway_probe /data/local/tmp/jerry_work/vk_threeway_probe
adb push alu_heavy.comp.spv /data/local/tmp/jerry_work/alu_heavy.comp.spv
adb push mem_heavy_clean.comp.spv /data/local/tmp/jerry_work/mem_heavy_clean.comp.spv
adb push copy_baseline.comp.spv /data/local/tmp/jerry_work/copy_baseline.comp.spv
adb shell 'chmod +x /data/local/tmp/jerry_work/vk_threeway_probe'
```

All three correctness tests passed:

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe copy copy_baseline.comp.spv 262144 1 8'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe alu alu_heavy.comp.spv 262144 2048 8'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe mem mem_heavy_clean.comp.spv 262144 512 8'
```

Each printed:

```
Verification PASSED
```

---

### 8. Captured clean three-way benchmark datasets

I created a new capture script:

```bash
tools/run_kgsl_threeway_capture.sh
```

Then captured all three workloads:

```bash
cd /Users/jerryyun/adreno_turnip

./tools/run_kgsl_threeway_capture.sh copy 30 0.005
./tools/run_kgsl_threeway_capture.sh alu 30 0.005
./tools/run_kgsl_threeway_capture.sh mem 30 0.005
```

Generated datasets:

```
copy:
/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162806_vendor_threeway_copy_repeat30_fast_full_clean

alu:
/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162813_vendor_threeway_alu_repeat30_fast_full_clean

mem:
/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162841_vendor_threeway_mem_repeat30_fast_full_clean
```

All three completed:

- 30 verified iterations
- 107 tracepoints enabled
- No workload stderr
- No sampler stderr

---

### 9. Compared three-way benchmark behavior

The three-way benchmark produced clear separation:

```
copy:
  very low GPU activity

alu:
  high GPU busy/load
  relatively low RAM wait

mem:
  high GPU busy
  very high RAM wait
```

Key metrics:

```
copy:
  pwrstats_overall_busy_pct:      2.61%
  pwrstats_overall_ram_wait_pct:  0.00%
  gpu_load_avg:                   1.45

alu:
  pwrstats_overall_busy_pct:      94.79%
  pwrstats_overall_ram_wait_pct:  8.67%
  gpu_load_avg:                   76.81
  gpu_busy_percentage_avg:        47.56

mem:
  pwrstats_overall_busy_pct:      92.49%
  pwrstats_overall_ram_wait_pct:  72.85%
  gpu_load_avg:                   69.56
  gpu_busy_percentage_avg:        30.04
```

The ALU and memory workloads had identical KGSL allocation totals and identical command-batch counts, so the difference is due to runtime behavior rather than setup structure.

---

### 10. Started live real-time plotting prototype

I created a live KGSL/sysfs sampler and host plotter:

```
/data/local/tmp/jerry_work/kgsl_live_sampler.sh
tools/live_kgsl_plot.py
```

The live plotter is intended to show real-time signals such as:

- `gpu_load`
- `gpu_busy_percentage`
- `clock_mhz`
- `cur_ab`

The command used was:

```bash
cd /Users/jerryyun/adreno_turnip

python3 tools/live_kgsl_plot.py \
  --interval 0.02 \
  --window 10 \
  --out kgsl_full_capture/live_kgsl_test.csv
```

I also created compatibility symlinks so older scripts still work after moving active files under `jerry_work`:

```bash
/data/local/tmp/kgsl_fast_sampler.sh -> /data/local/tmp/jerry_work/kgsl_fast_sampler.sh
/data/local/tmp/kgsl_live_sampler.sh -> /data/local/tmp/jerry_work/kgsl_live_sampler.sh
```

I verified the benchmark path still works:

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe copy copy_baseline.comp.spv 262144 1 1'
```

This passed verification.

---

## Key Discoveries / Findings

1. **KGSL/sysfs/tracefs signals can distinguish memory-heavy and compute-heavy workloads without direct hardware performance counters.**
2. **`kgsl_pwrstats ram_wait` is the strongest memory-pressure proxy found so far.**
    - Original memory workload repeatedly showed around 50% overall RAM wait.
    - Clean memory workload showed around 72.85% overall RAM wait.
    - ALU workload showed much lower RAM wait.
3. **Simple tracepoint event counts are not enough.**
    - Command-batch counts and KGSL allocation totals were often identical across workloads.
    - Useful information comes from values inside tracepoints and sysfs nodes, especially `ram_wait`, `gpu_load`, `busy`, and bus-related signals.
4. **The original memory benchmark was valid but not perfectly orthogonal.**
    - It was memory-heavy, but also had significant address-generation arithmetic.
    - This motivated the cleaner three-way benchmark suite.
5. **The cleaner three-way suite gives better separation.**
    - Copy baseline: low activity.
    - ALU-heavy: high busy/load, low RAM wait.
    - Memory-heavy: high busy, very high RAM wait.
6. **Current no-kernel-modification profiling is better at detecting memory pressure than true ALU occupancy.**
    - Direct ALU utilization likely still requires hardware performance counters.
    - However, high busy plus low RAM wait is a useful proxy for compute-heavy behavior.

---

## Reproduction Steps

### A. Prepare device-side working directory

```bash
adb shell 'mkdir -p /data/local/tmp/jerry_work'
```

### B. Build and push three-way benchmark

```bash
cd /Users/jerryyun/adreno_turnip/clean_benchmarks

glslangValidator -V alu_heavy.comp -o alu_heavy.comp.spv
glslangValidator -V mem_heavy_clean.comp -o mem_heavy_clean.comp.spv
glslangValidator -V copy_baseline.comp -o copy_baseline.comp.spv

export NDK=/Users/jerryyun/android-ndk-r27d
export API=29
export HOST_TAG=darwin-x86_64

"$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android${API}-clang++" \
  -std=c++17 -O2 \
  vk_threeway_probe.cpp \
  -o vk_threeway_probe \
  -lvulkan

adb push vk_threeway_probe /data/local/tmp/jerry_work/vk_threeway_probe
adb push alu_heavy.comp.spv /data/local/tmp/jerry_work/alu_heavy.comp.spv
adb push mem_heavy_clean.comp.spv /data/local/tmp/jerry_work/mem_heavy_clean.comp.spv
adb push copy_baseline.comp.spv /data/local/tmp/jerry_work/copy_baseline.comp.spv
adb shell 'chmod +x /data/local/tmp/jerry_work/vk_threeway_probe'
```

### C. Verify benchmarks

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe copy copy_baseline.comp.spv 262144 1 8'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe alu alu_heavy.comp.spv 262144 2048 8'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe mem mem_heavy_clean.comp.spv 262144 512 8'
```

Expected:

```
Verification PASSED
```

### D. Run three-way KGSL capture

```bash
cd /Users/jerryyun/adreno_turnip

./tools/run_kgsl_threeway_capture.sh copy 30 0.005
./tools/run_kgsl_threeway_capture.sh alu 30 0.005
./tools/run_kgsl_threeway_capture.sh mem 30 0.005
```

### E. Run pairwise comparison

```bash
COPY=/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162806_vendor_threeway_copy_repeat30_fast_full_clean
ALU=/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162813_vendor_threeway_alu_repeat30_fast_full_clean
MEM=/Users/jerryyun/adreno_turnip/kgsl_full_capture/20260602_162841_vendor_threeway_mem_repeat30_fast_full_clean

python3 tools/compare_kgsl_runs.py \
  --a "$COPY" \
  --b "$ALU" \
  --a-label copy \
  --b-label alu \
  --out-dir /Users/jerryyun/adreno_turnip/kgsl_full_capture/compare_threeway_copy_vs_alu

python3 tools/compare_kgsl_runs.py \
  --a "$COPY" \
  --b "$MEM" \
  --a-label copy \
  --b-label mem \
  --out-dir /Users/jerryyun/adreno_turnip/kgsl_full_capture/compare_threeway_copy_vs_mem

python3 tools/compare_kgsl_runs.py \
  --a "$ALU" \
  --b "$MEM" \
  --a-label alu \
  --b-label mem \
  --out-dir /Users/jerryyun/adreno_turnip/kgsl_full_capture/compare_threeway_alu_vs_mem
```

### F. Run real-time live plotter prototype

Terminal 1:

```bash
cd /Users/jerryyun/adreno_turnip

python3 tools/live_kgsl_plot.py \
  --interval 0.02 \
  --window 10 \
  --out kgsl_full_capture/live_kgsl_test.csv
```

Terminal 2:

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe alu alu_heavy.comp.spv 262144 2048 64'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe mem mem_heavy_clean.comp.spv 262144 512 64'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_threeway_probe copy copy_baseline.comp.spv 262144 1 64'
```

Important: the plotter must remain running while the benchmarks execute.

---

## Issues / Limitations

1. **No direct ALU occupancy is available through current no-kernel-modification signals.**
    - The profiler can infer compute-heavy behavior from high busy/load and low RAM wait, but not directly measure ALU pipeline occupancy.
2. **Tracepoint counts alone are weak features.**
    - Event values are much more useful than event counts.
3. **Live plotting prototype needs further testing.**
    - Initially I stopped the plotter before running the benchmarks, so no graph update was visible.
    - The plotter must stay running in one terminal while benchmarks run in another.
    - If no matplotlib window appears, the issue may be the matplotlib backend.
4. **Some scripts may still assume old device paths.**
    - Active files were organized under `/data/local/tmp/jerry_work`.
    - Compatibility symlinks were created for samplers to reduce path breakage.
5. **`kgsl_buslevel avg_bw` may not be a time-weighted bandwidth average.**
    - It should be treated as an event-value feature rather than a perfect continuous bandwidth measurement.
    - `kgsl_pwrstats ram_wait` appears more reliable for memory-pressure detection.

---

## Most Likely Next Step

The next practical step is to finish and validate the **real-time reporting workflow**:

1. Keep `tools/live_kgsl_plot.py` running in one terminal.
2. Run the three-way benchmarks in another terminal.
3. Confirm that `gpu_load`, `gpu_busy_percentage`, `clock_mhz`, and `cur_ab` update live.
4. If the matplotlib GUI does not show, fix the plotting backend or switch to a CSV/live terminal dashboard first.
5. Later, extend live reporting to parse `trace_pipe` so that `kgsl_pwrstats ram_wait_pct` can also be plotted in real time.

This would move the project from offline trace analysis toward a CPU-profiler-like live GPU profiler.