# KGSL Tracepoint + Sysfs Profiler

**Status:** Work in progress  
**Project:** Adreno GPU Profiler  
**Target:** Qualcomm Adreno 830 GPU on Android/KGSL  
**Current role in project:** Primary working no-kernel-modification profiling path

## 1. Summary

This report documents the current work-in-progress profiler based on KGSL tracepoints, KGSL/sysfs sampling, and controlled Vulkan compute workloads.

The goal is to collect useful GPU activity information from user space on Android without modifying the kernel. Direct hardware performance-counter access has so far been blocked or unsupported through the tested KGSL, DRM/MSM, and Mesa Turnip paths. In contrast, KGSL tracepoints and sysfs/pwrstats nodes provide a practical profiling route that can distinguish broad workload behavior, especially ALU-heavy versus memory-heavy execution.

The current profiler is not a replacement for full hardware performance counters. It provides coarse-grained but useful evidence through:

- GPU busy percentage
- `kgsl_pwrstats`
- RAM-wait percentage trends
- GPU frequency
- bus-level related signals
- command-batch lifecycle tracepoints
- controlled workload comparisons

The strongest current finding is that RAM-wait related signals are directionally useful for identifying memory-heavy behavior. Memory-heavy workloads show much higher RAM-wait activity than ALU-heavy workloads, while both can keep the GPU busy.

## 2. Motivation

The original profiling goal was to collect low-level Adreno GPU performance counters from user space. Several paths were investigated:

1. direct KGSL performance-counter ioctls
2. DRM/MSM ioctls
3. Mesa Turnip performance-query experiments
4. KGSL tracepoints and KGSL/sysfs sampling

The first three paths did not produce reliable direct counter readback on the tested device. KGSL ioctl probing showed that some counter metadata/query/configuration operations could be reached, but actual reads failed. DRM/MSM probing did not provide a usable counter path. Mesa Turnip raw performance-query experiments helped expose the driver-side behavior but did not yet produce successful raw counter readback.

Therefore, the KGSL tracepoint + sysfs profiler became the most useful practical path. It does not provide detailed per-block hardware counters, but it can provide enough signal for workload characterization.

## 3. Research Question

The main research question for this profiler is:

> Can a useful Adreno GPU workload profiler be built using only user-space-accessible KGSL tracepoints and sysfs/pwrstats signals?

Sub-questions:

- Can ALU-heavy and memory-heavy workloads be distinguished without direct hardware counters?
- Which KGSL/sysfs signals are stable and useful enough for repeated experiments?
- How well do tracepoint timing and sysfs sampling agree?
- What are the limitations caused by sampling rate, timing mismatch, and coarse aggregation?
- Can this method become a reproducible profiling workflow for future experiments?

## 4. Device and Environment

Current target device:

```text
Device: OnePlus CPH2653
GPU: Qualcomm Adreno 830
Kernel interface: KGSL
Android version: Android 15
Main GPU device node: /dev/kgsl-3d0
Main sysfs path: /sys/class/kgsl/kgsl-3d0
Root access: Magisk su
Host: macOS with adb
Main repo: adreno-gpu-profiler
Device workspace: /data/local/tmp/jerry_work
```

The profiler is designed around Android/KGSL and assumes that the user can access relevant KGSL tracepoints and sysfs nodes, usually through root.

## 5. Method Overview

The profiler combines two sources of information:

1. **Tracepoint events**
   - command-batch lifecycle events
   - KGSL scheduling / submission / completion events where available
   - GPU work-period and frequency-related events where available

2. **Sysfs / pwrstats sampling**
   - busy percentage
   - RAM-wait percentage
   - frequency
   - bus level
   - other available KGSL nodes

The workflow is:

```text
Controlled Vulkan workload
        ↓
KGSL tracepoint capture + KGSL/sysfs sampling
        ↓
Python parsing and aggregation
        ↓
Comparison between workload classes
        ↓
Interpretation of ALU-heavy vs memory-heavy behavior
```

This design is intentionally benchmark-driven. Instead of trying to infer everything from arbitrary UI activity, the current experiments use controlled Vulkan compute workloads so that differences between ALU-heavy and memory-heavy cases can be attributed more clearly.

## 6. Controlled Workloads

The main controlled workload is a three-way Vulkan compute benchmark:

```text
workloads/vulkan_threeway_probe/
├── vk_threeway_probe.cpp
├── alu_heavy.comp
├── mem_heavy_clean.comp
└── copy_baseline.comp
```

The selected SPIR-V binaries used on device are stored under:

```text
workloads/shaders/
├── alu.comp.spv
├── alu_heavy.comp.spv
├── mem.comp.spv
├── mem_heavy_clean.comp.spv
└── copy_baseline.comp.spv
```

The three workload types are:

### ALU-heavy workload

This workload is designed to keep shader arithmetic units busy by performing many compute operations per element. It is intended to create a high-compute-intensity workload with relatively lower memory pressure.

### Memory-heavy workload

This workload is designed to increase memory access pressure and expose memory-wait behavior. It is intended to produce higher RAM-wait trends compared with the ALU-heavy workload.

### Copy-baseline workload

This workload acts as a simpler memory/copy baseline. It helps separate pure transfer-like behavior from more complex memory-heavy shader behavior.

## 7. Main Profiler Components

### Device-side samplers

```text
scripts/device/kgsl_live_sampler.sh
scripts/device/kgsl_fast_sampler.sh
scripts/device/kgsl_all_node_sampler.sh
```

These scripts sample KGSL/sysfs nodes on the Android device. They are intended to run under `/data/local/tmp/jerry_work` or a similar device workspace.

### Host-side capture scripts

```text
tools/capture/run_kgsl_fast_capture.sh
tools/capture/run_kgsl_threeway_capture.sh
tools/capture/run_kgsl_busy_validation.sh
tools/capture/run_focused_kgsl_trace_compute.sh
tools/capture/run_focused_kgsl_trace_mem.sh
tools/capture/run_focused_kgsl_trace_ui.sh
tools/capture/run_kgsl_trace_ui.sh
```

These scripts coordinate workload execution, tracepoint setup, sysfs sampling, and output collection.

### Analysis scripts

```text
tools/analysis/aggregate_threeway_kgsl.py
tools/analysis/analyze_kgsl_busy_validation.py
tools/analysis/compare_kgsl_runs.py
tools/analysis/parse_focused_kgsl_trace.py
tools/analysis/summarize_trials.py
```

These scripts parse raw traces and sampled CSV files, aggregate metrics, and compare workload runs.

### Live tools

```text
tools/live/live_kgsl_plot.py
tools/live/live_kgsl_pwrstats.py
```

These scripts support live observation of KGSL/sysfs and pwrstats behavior.

### Inventory scripts

```text
tools/inventory/inventory_kgsl_interfaces.sh
tools/inventory/gpu_counter_inventory.sh
```

These scripts document exposed KGSL/sysfs/tracepoint interfaces and help identify what can be sampled on the device.

## 8. Signals Currently Used

The profiler currently focuses on the following signals:

### GPU busy percentage

This is the main high-level activity signal. It indicates whether the GPU is active, but it does not identify the bottleneck by itself.

### `kgsl_pwrstats`

`kgsl_pwrstats` is one of the most useful available sources. It provides power/statistics information including RAM-wait related behavior.

### RAM-wait percentage

RAM-wait related signals are currently the most useful indicator for differentiating memory-heavy behavior from ALU-heavy behavior. Memory-heavy workloads show significantly higher RAM-wait trends.

### GPU frequency

Frequency is important for interpreting performance results. If ALU-heavy and memory-heavy workloads run at the same frequency, observed differences are less likely to be caused by DVFS frequency changes.

### Bus level / bandwidth-related signals

Bus-level signals help interpret memory pressure and system interconnect behavior. These are coarse, but they provide useful context.

### KGSL command-batch tracepoints

Command-batch lifecycle events help estimate GPU work timing and submission/completion behavior.

## 9. Current Findings

### 9.1 The tracepoint/sysfs path is usable

The KGSL tracepoint + sysfs route is currently the most reliable working method. It does not require kernel modification and can be used repeatedly with controlled workloads.

### 9.2 RAM-wait separates memory-heavy from ALU-heavy behavior

The clearest current result is that RAM-wait related signals are directionally useful. Memory-heavy workloads show much higher RAM-wait activity than ALU-heavy workloads.

This supports the use of `kgsl_pwrstats` as a practical signal for coarse memory-pressure detection.

### 9.3 GPU busy alone is insufficient

Both ALU-heavy and memory-heavy workloads can make the GPU appear highly busy. Therefore, GPU busy percentage alone cannot identify the bottleneck. It must be interpreted together with RAM-wait, frequency, bus-level signals, and workload context.

### 9.4 Frequency must be checked during comparison

Frequency and bus-level behavior must be recorded because workload comparisons can be misleading if DVFS behavior changes between runs.

In current controlled tests, frequency was often stable enough to allow meaningful ALU-heavy versus memory-heavy comparison, but this remains an important validation step.

### 9.5 Tracepoints and sysfs sampling may not align perfectly

Tracepoints and sysfs samples are collected through different mechanisms and timing paths. Some mismatch is expected because tracepoints record discrete events, while sysfs sampling observes state over sampling windows.

This means comparisons should be interpreted statistically over windows or repeated runs, not as exact cycle-level measurements.

## 10. Evidence Locations

Important evidence is stored under:

```text
evidence/raw_logs/phone_live_csv/
evidence/raw_logs/phone_cleanup_old/
evidence/summaries/vendor_turnip_comparison/
evidence/summaries/kgsl_tracepoints/
evidence/manifests/
```

Relevant profiler files include:

```text
tools/capture/
tools/analysis/
tools/live/
tools/inventory/
scripts/device/
workloads/vulkan_threeway_probe/
workloads/shaders/
```

The repository intentionally stores curated evidence rather than all raw captures. Larger raw capture folders should remain outside normal Git unless they are compressed, selected, and clearly documented.

## 11. Reproduction Workflow

The exact commands may change as the profiler is still work-in-progress. The current intended workflow is:

### 1. Clone repository

```bash
git clone https://github.com/JerryYun2004/adreno-gpu-profiler.git
cd adreno-gpu-profiler
```

### 2. Prepare Android workspace

```bash
adb shell 'su -c "mkdir -p /data/local/tmp/jerry_work"'
```

### 3. Push device-side samplers and shaders

```bash
adb push scripts/device/kgsl_live_sampler.sh /data/local/tmp/jerry_work/
adb push scripts/device/kgsl_fast_sampler.sh /data/local/tmp/jerry_work/
adb push scripts/device/kgsl_all_node_sampler.sh /data/local/tmp/jerry_work/

adb push workloads/shaders/*.spv /data/local/tmp/jerry_work/
adb shell 'su -c "chmod +x /data/local/tmp/jerry_work/*.sh"'
```

### 4. Run interface inventory

```bash
tools/inventory/inventory_kgsl_interfaces.sh
```

or:

```bash
tools/inventory/gpu_counter_inventory.sh
```

### 5. Run a controlled capture

Example capture scripts:

```bash
tools/capture/run_kgsl_fast_capture.sh
tools/capture/run_kgsl_threeway_capture.sh
tools/capture/run_kgsl_busy_validation.sh
```

The exact command depends on the workload and output directory being tested.

### 6. Analyze captured results

Example analysis scripts:

```bash
python3 tools/analysis/compare_kgsl_runs.py
python3 tools/analysis/aggregate_threeway_kgsl.py
python3 tools/analysis/analyze_kgsl_busy_validation.py
python3 tools/analysis/parse_focused_kgsl_trace.py
```

### 7. Inspect summaries and evidence

Generated summaries should be stored under a curated evidence or results folder. Important results should be copied into:

```text
evidence/summaries/
evidence/raw_logs/
evidence/manifests/
```

## 12. Current Limitations

### 12.1 No direct low-level hardware counters yet

This method does not currently expose detailed Adreno block-level counters such as per-shader-core ALU cycles, texture activity, cache misses, or exact memory-controller counters.

### 12.2 Coarse signal interpretation

RAM-wait and busy percentage are useful but coarse. They should not be overinterpreted as exact hardware utilization.

### 12.3 Sampling-window mismatch

Sysfs sampling windows may not align exactly with workload start/end or tracepoint events. Repeated runs and windowed statistics are necessary.

### 12.4 Workload sensitivity

The profiler is most meaningful when the workload is controlled. Arbitrary UI or app workloads are noisier and harder to interpret.

### 12.5 Device-specific behavior

The available KGSL nodes, tracepoints, and permissions may differ across devices, Android versions, kernels, and GPU generations.

## 13. Work-in-Progress Items

The following items still need improvement:

- Add more complete documentation for each capture script.
- Standardize output directory structure.
- Add a single top-level reproduction script for ALU/memory/copy experiments.
- Add generated plots for RAM-wait, busy percentage, frequency, and bus level.
- Improve synchronization between workload execution and sampling start/end.
- Add repeated-run statistics and confidence intervals.
- Clarify which tracepoints are essential and which are optional.
- Add example expected outputs.
- Compare vendor driver versus Turnip using the same capture pipeline.
- Investigate whether more KGSL/pwrstats fields can be interpreted safely.

## 14. Current Conclusion

The KGSL tracepoint + sysfs profiler is currently the most successful no-kernel-modification profiling route in this project. It cannot replace real hardware counters, but it provides enough signal to characterize broad workload behavior on the tested Adreno/KGSL device.

The most useful signal so far is RAM-wait behavior from KGSL/pwrstats-related data. When combined with GPU busy percentage, frequency, bus-level signals, and controlled Vulkan workloads, it provides a practical way to distinguish memory-heavy workloads from ALU-heavy workloads.

This method should remain the baseline working profiler while direct hardware counter access through KGSL, DRM/MSM, and Mesa Turnip continues to be investigated.
