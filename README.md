# Adreno GPU Profiler

This repository contains my ongoing research work on profiling a mobile Adreno GPU from user space on Android.

The current target device is a Qualcomm Adreno 830 GPU exposed through KGSL on Android. The project investigates whether useful GPU performance information can be collected without kernel modification, and compares several possible profiling paths including KGSL sysfs nodes, KGSL tracepoints, direct KGSL performance-counter ioctls, DRM/MSM ioctls, and Mesa Turnip performance-query experiments.

## Current Status

The most reliable working path at the moment is:

```text
Controlled Vulkan workload
        ↓
KGSL tracepoints + KGSL/sysfs sampling
        ↓
Python parsing / aggregation / comparison
        ↓
ALU-heavy vs memory-heavy workload characterization

The project can currently collect and analyze:

GPU busy percentage from KGSL/sysfs
kgsl_pwrstats information
RAM-wait percentage trends
GPU frequency and bus-level related signals
KGSL tracepoint events such as command batch lifecycle events
Controlled workload results for ALU-heavy, memory-heavy, and copy-baseline Vulkan compute shaders

The key practical finding so far is that kgsl_pwrstats / RAM-wait related signals are directionally useful for distinguishing memory-heavy workloads from ALU-heavy workloads, even when direct hardware performance counters are blocked.

Major Findings So Far
1. KGSL tracepoint/sysfs profiling works

The KGSL tracepoint and sysfs route is currently the best no-kernel-modification profiling path.

It provides coarse but useful visibility into:

GPU busy behavior
active/idle timing
memory pressure through RAM-wait related signals
command batch timing
frequency and bus-level behavior

This path is implemented through scripts under:

tools/capture/
tools/analysis/
tools/live/
scripts/device/
2. Direct KGSL performance-counter read is blocked

Direct KGSL performance-counter access was tested using custom ioctl probes.

The probes showed that counter metadata/query paths can work, and counters can sometimes be reserved/configured, but actual counter reads fail with permission errors on the tested Android/KGSL stack.

Relevant files:

probes/kgsl_counter_probe/
evidence/raw_logs/direct_counter_access/
3. DRM/MSM performance-counter path is not usable on this device

The DRM render node was inspected and probed, but the tested MSM DRM performance-counter ioctls did not provide a usable counter path on this phone.

Relevant files:

probes/drm_msm_probe/
evidence/raw_logs/direct_counter_access/01_drm_msm_probe/
4. Mesa Turnip raw performance-query path was explored

Mesa Turnip was built and modified experimentally to expose / debug A8XX raw performance-query behavior on KGSL.

The Mesa experiment is kept as a submodule:

third_party/mesa/

The submodule points to my Mesa fork and the branch:

experiment/kgsl-a8xx-perfcraw

The current Mesa experiment commit is:

4027f06090eec398fab0d4facaa431c4104ec367

Relevant evidence is stored under:

evidence/raw_logs/20260605_turnip_perf_query/
configs/mesa_android_cross/
scripts/setup/build_turnip_android_kgsl.sh

The current conclusion is that the Turnip path is useful for understanding and testing the driver-side performance-query path, but it has not yet produced successful user-space raw counter readback on the tested KGSL stack.

Repository Layout
adreno-gpu-profiler/
├── README.md
├── tools/
│   ├── capture/              # host-side capture orchestration scripts
│   ├── analysis/             # parsers, aggregators, comparison scripts
│   ├── live/                 # live plotting / live pwrstats tools
│   └── inventory/            # scripts for discovering KGSL/sysfs/tracepoint interfaces
├── scripts/
│   ├── device/               # scripts intended to run on Android device
│   └── setup/                # setup/build helper scripts
├── probes/
│   ├── kgsl_counter_probe/   # KGSL ioctl counter-access probes
│   ├── drm_msm_probe/        # DRM/MSM ioctl probes
│   ├── vk_ext_probe/         # Vulkan extension probing
│   ├── vk_perf_enum_probe/   # Vulkan performance-query enumeration probe
│   ├── vk_perf_read_probe/   # Vulkan performance-query read probe
│   └── include/              # local UAPI headers used by probes
├── workloads/
│   ├── vulkan_threeway_probe/ # ALU / memory / copy Vulkan benchmark source
│   └── shaders/              # selected SPIR-V shader binaries deployed to device
├── configs/
│   ├── device/               # device-side configs such as Turnip ICD JSON
│   └── mesa_android_cross/   # Mesa Android cross-compilation config
├── evidence/
│   ├── raw_logs/             # curated raw terminal/log evidence
│   ├── summaries/            # summary outputs and trial comparison summaries
│   └── manifests/            # SHA256 manifests for evidence folders
├── glslang/                  # submodule for shader tooling
└── third_party/
    └── mesa/                 # Mesa fork/submodule with experimental Turnip patches
Important Evidence Folders
evidence/raw_logs/direct_counter_access/

Raw evidence for direct KGSL ioctl and DRM/MSM counter-access attempts.

evidence/raw_logs/20260605_turnip_perf_query/

Mesa Turnip raw performance-query debug logs and selected counter experiments.

evidence/raw_logs/phone_live_csv/

Curated phone-side live KGSL/sysfs CSV samples.

evidence/summaries/vendor_turnip_comparison/

Summary outputs comparing vendor driver and Turnip behavior.

evidence/summaries/kgsl_tracepoints/

Tracepoint/event-level summaries for vendor and Turnip workloads.

Cloning

A normal clone without submodules is small:

git clone https://github.com/JerryYun2004/adreno-gpu-profiler.git
cd adreno-gpu-profiler

To initialize only Mesa:

git submodule update --init third_party/mesa

To initialize all submodules:

git submodule update --init --recursive

Note: the Mesa submodule is large. Use submodules only when needed.

Reproducing the Current Tool Setup
1. Push device scripts and assets

The expected Android workspace is:

/data/local/tmp/jerry_work

Device-side scripts are under:

scripts/device/

Selected shader binaries are under:

workloads/shaders/
2. Run KGSL inventory
tools/inventory/inventory_kgsl_interfaces.sh

or:

tools/inventory/gpu_counter_inventory.sh

These scripts are used to inspect available KGSL nodes, sysfs entries, tracepoints, and related GPU interfaces.

3. Run live profiling
python3 tools/live/live_kgsl_pwrstats.py

or:

python3 tools/live/live_kgsl_plot.py

These scripts sample live KGSL/sysfs data and help observe GPU busy and memory-wait behavior.

4. Run capture scripts

Useful capture scripts include:

tools/capture/run_kgsl_fast_capture.sh
tools/capture/run_kgsl_threeway_capture.sh
tools/capture/run_kgsl_busy_validation.sh
tools/capture/run_focused_kgsl_trace_compute.sh
tools/capture/run_focused_kgsl_trace_mem.sh
tools/capture/run_focused_kgsl_trace_ui.sh
5. Analyze captured data

Useful analysis scripts include:

tools/analysis/compare_kgsl_runs.py
tools/analysis/aggregate_threeway_kgsl.py
tools/analysis/analyze_kgsl_busy_validation.py
tools/analysis/parse_focused_kgsl_trace.py
tools/analysis/summarize_trials.py
Mesa / Turnip Experiment

The Mesa submodule points to my experimental fork:

https://github.com/JerryYun2004/mesa.git

Current branch:

experiment/kgsl-a8xx-perfcraw

Current checked-out commit:

4027f06090eec398fab0d4facaa431c4104ec367

The Mesa modifications are experimental and are used to investigate whether Turnip can expose or read A8XX raw performance counters through KGSL.

Build-related files are kept in the main repo:

configs/mesa_android_cross/android-aarch64.cross
scripts/setup/build_turnip_android_kgsl.sh
Data / Evidence Policy

This repo intentionally commits curated evidence, not full raw data dumps.

Committed:

small .txt summaries
small .log files
compressed selected CSV examples
command-output excerpts
manifests with SHA256 hashes
source code and scripts

Avoided:

full raw capture directories
large tracefs dumps
large CSV datasets
Android executable binaries
Mesa build outputs
.so files

This keeps the repo usable while preserving enough evidence to justify the research conclusions.

Current Practical Conclusion

At this stage, direct access to low-level Adreno hardware performance counters appears blocked or unsupported from normal user-space routes on the tested phone, even with root. However, KGSL tracepoints, KGSL sysfs nodes, and kgsl_pwrstats provide enough signal to build a useful no-kernel-modification profiler for distinguishing broad workload behavior such as ALU-heavy versus memory-heavy execution.

Next Steps

Planned follow-up work:

Improve documentation for each attempted profiling method.
Add cleaner reproduction scripts for each benchmark/capture path.
Build a real-time visualization workflow for KGSL/sysfs/pwrstats signals.
Continue investigating whether Mesa Turnip or Vulkan performance-query paths can expose reliable counters.
Prepare method reports and figures for supervisor review / conference poster use.
