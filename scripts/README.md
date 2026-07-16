# Utility Scripts

This directory contains helper scripts for building Turnip, sampling KGSL sysfs statistics, and copying perf-counter sweep results from the Android device.

```text
scripts/
├── build_turnip_android_kgsl.sh
├── kgsl_all_node_sampler.sh
├── kgsl_fast_sampler.sh
├── kgsl_live_sampler.sh
└── pull_latest_sweep.sh
```

These files support the profiling workflow but are not compiled into the final streamer or sweeper.

## Main profiling workflow

```text
GLSL compute shader (.comp)
        ↓ glslangValidator
SPIR-V module (.comp.spv)
        ↓ loaded by a Vulkan runner
GPU workload on the Adreno device
        ↓ measured by
adreno_perf_stream or streamer_sweeper
        ↓
CSV and metadata on the phone
        ↓ pull_latest_sweep.sh
host-side results and analysis
```

The production profiler tools are:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

## Relationship to the final product

| Script | Role | Used by final streamer/sweeper? |
|---|---|---|
| `build_turnip_android_kgsl.sh` | Builds Mesa Turnip for Android/KGSL driver experiments. | No. Optional driver infrastructure. |
| `kgsl_live_sampler.sh` | Continuously prints selected KGSL sysfs values. | No. Auxiliary validation tool. |
| `kgsl_fast_sampler.sh` | Saves selected KGSL sysfs values for a fixed duration. | No. Auxiliary validation tool. |
| `kgsl_all_node_sampler.sh` | Scans many KGSL-related sysfs files to discover useful signals. | No. Exploratory tool. |
| `pull_latest_sweep.sh` | Pulls the newest sweeper result from the phone. | Yes, as a post-capture helper. |

---

# File reference

## `build_turnip_android_kgsl.sh`

### What it does

Cross-compiles Mesa's Freedreno Turnip Vulkan driver for ARM64 Android using the KGSL kernel backend.

It:

1. assumes the current directory is the Mesa source root;
2. finds the Android NDK;
3. creates `android-aarch64.cross`;
4. deletes and recreates `build-android/`;
5. configures Mesa for Android API 29;
6. enables the Freedreno Vulkan driver with `freedreno-kmds=kgsl`; and
7. builds `libvulkan_freedreno.so`.

### Why it exists

It supports experiments comparing the Qualcomm vendor Vulkan driver with Mesa Turnip. It is not needed to build the perf-counter streamer or sweeper.

### Requirements

- Mesa source tree
- Android NDK
- Meson
- Ninja
- Python 3
- macOS or Linux

### Build

```bash
cd /Users/jerryyun/adreno-gpu-profiler/third_party/mesa

export ANDROID_NDK_HOME="$HOME/android-ndk-r27d"

../../scripts/build_turnip_android_kgsl.sh
```

### Expected output

```text
third_party/mesa/build-android/.../libvulkan_freedreno.so
```

The script builds the driver only. Driver deployment and selection are separate steps.

> The existing `build-android/` directory is deleted before each build.

---

## `kgsl_live_sampler.sh`

### What it does

Continuously reads a focused set of KGSL and kernel GPU nodes and prints CSV rows to standard output.

Collected fields:

```text
timestamp_ms
gpu_load
gpu_busy_percentage
kernel_gpu_busy
clock_mhz
cur_freq
gpuclk
bus_split
gpubusy
```

Unavailable values are reported as `NA`.

### Why it exists

It provides a quick check that the GPU becomes active and that busy/frequency nodes respond during a workload. It does not read raw Adreno hardware performance counters and does not replace `adreno_perf_stream`.

### Push

```bash
adb push \
  scripts/kgsl_live_sampler.sh \
  /data/local/tmp/kgsl_live_sampler.sh

adb shell \
  'chmod 755 /data/local/tmp/kgsl_live_sampler.sh'
```

### Run

```bash
adb shell \
  '/data/local/tmp/kgsl_live_sampler.sh 0.05'
```

Save the output on the host:

```bash
adb shell \
  '/data/local/tmp/kgsl_live_sampler.sh 0.05' \
  > results/kgsl_live_samples.csv
```

Stop it with `Ctrl+C`.

---

## `kgsl_fast_sampler.sh`

### What it does

Reads the same focused KGSL nodes as the live sampler, but runs for a fixed duration and writes the CSV on the phone.

Syntax:

```text
kgsl_fast_sampler.sh [output_csv] [interval_seconds] [duration_seconds]
```

Defaults:

```text
output:   /data/local/tmp/kgsl_fast_samples.csv
interval: 0.05 seconds
duration: 10 seconds
```

### Why it exists

It provides a repeatable, automatically bounded sysfs capture around a benchmark. It is useful for workload comparisons but is not called by the final profiler.

### Push and run

```bash
adb push \
  scripts/kgsl_fast_sampler.sh \
  /data/local/tmp/kgsl_fast_sampler.sh

adb shell \
  'chmod 755 /data/local/tmp/kgsl_fast_sampler.sh'

adb shell \
  '/data/local/tmp/kgsl_fast_sampler.sh \
   /data/local/tmp/kgsl_fast_samples.csv \
   0.02 \
   15'
```

### Pull the output

```bash
adb pull \
  /data/local/tmp/kgsl_fast_samples.csv \
  results/
```

---

## `kgsl_all_node_sampler.sh`

### What it does

Builds a list of readable files under the KGSL sysfs tree and repeatedly records their values.

It searches areas including:

```text
/sys/class/kgsl/kgsl-3d0
devfreq
power
device/devfreq
kgsl-busmon
device tree
CoreSight GPU nodes
```

Output format:

```text
timestamp_ns,path,value
```

### Why it exists

This is an interface-discovery tool. It helps identify device-specific busy, power, frequency, bus, and debugging nodes that may deserve focused collection later.

It is not suitable for precise high-rate profiling because one scan reads many files and can take much longer than the requested sleep interval.

### Push

```bash
adb push \
  scripts/kgsl_all_node_sampler.sh \
  /data/local/tmp/kgsl_all_node_sampler.sh

adb shell \
  'chmod 755 /data/local/tmp/kgsl_all_node_sampler.sh'
```

### Start

```bash
adb shell \
  'su -c "/data/local/tmp/kgsl_all_node_sampler.sh 0.02"'
```

### Stop

From another terminal:

```bash
adb shell \
  'su -c "touch /data/local/tmp/kgsl_all_node_sampler.stop"'
```

### Pull the output

```bash
adb pull \
  /data/local/tmp/kgsl_all_node_samples.csv \
  results/
```

---

## `pull_latest_sweep.sh`

### What it does

Finds the newest directory matching:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps/sweep_*
```

and pulls it to the host.

### Why it exists

A sweeper run creates a timestamped directory containing many counter groups, chunks, CSV files, logs, and metadata. This script avoids manually locating and typing the latest device path.

This is the only script in this directory directly tied to the normal final sweeper workflow.

### Run

```bash
cd /Users/jerryyun/adreno-gpu-profiler

./scripts/pull_latest_sweep.sh
```

### Expected output

A directory containing some or all of:

```text
sweep_<timestamp>/
├── 01_CP/
├── 02_RBBM/
├── ...
├── run_config.txt
└── summary.csv
```

### Path note

The current script uses:

```text
/Users/jerryyun/adreno-gpu-profiler/results/perfcounter_sweeps
```

The reorganized repository stores full sweeps under:

```text
results/perfcounter_sweeps/full_sweeps/
```

To use that layout, change the script to:

```bash
DEST_ROOT="/Users/jerryyun/adreno-gpu-profiler/results/perfcounter_sweeps/full_sweeps"
```

---

# Compute shader files

There are no `.comp` files directly in `scripts/`. Compute shaders are benchmark inputs stored elsewhere, such as:

```text
benchmarks/microbenchmarks/
benchmarks/ml_primitives/shaders/
```

They are relevant because the streamer and sweeper measure workloads created by these shaders.

## File pipeline

```text
shader.comp
    Editable Vulkan GLSL compute source.

shader.comp.spv
    Compiled SPIR-V module loaded by the Vulkan runner.

shader.comp.spvasm
    Optional human-readable SPIR-V disassembly for inspection.
```

The `.comp` file is the source of truth. The `.spv` file is the runtime artifact. The `.spvasm` file is not loaded by Vulkan.

## Typical `.comp` structure

### GLSL version

```glsl
#version 450
```

### Workgroup size

```glsl
layout(
    local_size_x = 256,
    local_size_y = 1,
    local_size_z = 1
) in;
```

A one-dimensional workgroup matches the one-dimensional arrays used by most project benchmarks. Keeping the workgroup size fixed also controls dispatch geometry across variants.

### Storage-buffer bindings

```glsl
layout(set = 0, binding = 0, std430)
readonly buffer InputBuffer {
    uint in_data[];
};

layout(set = 0, binding = 1, std430)
writeonly buffer OutputBuffer {
    uint out_data[];
};
```

The Vulkan runner must create descriptor bindings with the same set and binding numbers.

### Push constants

```glsl
layout(push_constant) uniform PushConstants {
    uint n;
    uint iters;
} pc;
```

Push constants provide small runtime parameters. Their field order and total size must exactly match the C or C++ runner.

### Invocation index and bounds check

```glsl
uint gid = gl_GlobalInvocationID.x;

if (gid >= pc.n) {
    return;
}
```

The bounds check prevents a partially filled final workgroup from accessing beyond the buffers.

### Controlled workload

The main body creates the behavior under study:

```text
ALU shader      dependent arithmetic
memory shader   repeated or strided memory reads
copy shader     minimal load/store baseline
softmax shader  reductions and normalization
RMSNorm shader  reduction, scaling, and normalization
```

### Observable output

```glsl
out_data[gid] = result;
```

The output write prevents unused work from being removed and allows CPU-side verification.

## Why these shader files are needed

The streamer and sweeper measure GPU activity but do not create a workload.

Shaders and their Vulkan runners are needed to:

- generate repeatable GPU work;
- calibrate counter behavior;
- compare ALU-heavy and memory-heavy activity;
- keep dispatch and input variables controlled;
- verify the complete profiling path; and
- profile realistic ML primitives after calibration.

The sweeper does not directly read `.comp` or `.spv` files. It launches a benchmark executable, and the benchmark executable loads the `.spv` module.

---

# Required tools

## Host

| Tool | Purpose |
|---|---|
| Bash | Runs the helper scripts. |
| `adb` | Pushes, executes, and pulls phone-side files. |
| Android NDK | Builds ARM64 Android profiler and benchmark binaries. |
| `make` | Builds the streamer and sweeper. |
| `glslangValidator` | Compiles `.comp` into `.spv`. |
| `spirv-val` | Validates SPIR-V. |
| `spirv-dis` | Generates `.spvasm` disassembly. |
| Meson and Ninja | Build Turnip. |
| Python 3 | Supports generators, builds, and analysis. |
| Git | Tracks the files. |

Check the environment:

```bash
adb version
adb devices
glslangValidator --version
spirv-val --version
spirv-dis --version
python3 --version
meson --version
ninja --version
```

## Android device

- ARM64 Android
- Qualcomm Adreno GPU
- Vulkan compute support
- KGSL at `/sys/class/kgsl/kgsl-3d0`
- root access through `su` for protected interfaces

---

# Build the final profiler tools

These scripts are maintained with the tools they build.

## Streamer

```bash
cd /Users/jerryyun/adreno-gpu-profiler/tools/profiling/perfcounter_streamer

NDK="$HOME/android-ndk-r27d" \
API=35 \
HOST_TAG=darwin-x86_64 \
./build_and_push.sh
```

Expected phone path:

```text
/data/local/tmp/adreno_perf_stream
```

Check the interface:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream --help"'
```

## Sweeper

```bash
cd /Users/jerryyun/adreno-gpu-profiler/tools/profiling/perfcounter_sweeper

NDK="$HOME/android-ndk-r27d" \
API=35 \
HOST_TAG=darwin-x86_64 \
./build_and_push.sh
```

Expected phone path:

```text
/data/local/tmp/streamer_sweeper
```

Check the current interface before running:

```bash
adb shell \
  'su -c "/data/local/tmp/streamer_sweeper --help"'
```

The tool-specific `README.md` and current `--help` output are the source of truth for command-line arguments.

---

# Build and push a compute workload

## Compile

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  path/to/shader.comp \
  -o path/to/shader.comp.spv
```

## Validate

```bash
spirv-val \
  --target-env vulkan1.1 \
  path/to/shader.comp.spv
```

## Disassemble

```bash
spirv-dis \
  path/to/shader.comp.spv \
  -o path/to/shader.comp.spvasm
```

Use the disassembly to inspect workgroup size, bindings, push constants, loops, and generated operations.

## Build a Vulkan runner manually

Prefer a benchmark-specific build script or CMake file when available. A typical manual NDK build is:

```bash
export ANDROID_NDK_HOME="$HOME/android-ndk-r27d"

NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin"

"$NDK_BIN/aarch64-linux-android35-clang++" \
  -std=c++17 \
  -O2 \
  -static-libstdc++ \
  path/to/runner.cpp \
  -o path/to/runner \
  -lvulkan
```

## Push

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/example_workload'

adb push \
  path/to/runner \
  path/to/shader.comp.spv \
  /data/local/tmp/jerry_work/example_workload/

adb shell \
  'chmod 755 /data/local/tmp/jerry_work/example_workload/runner'
```

Test the benchmark directly before profiling. Confirm:

```text
exit status = 0
verification = PASS
correct shader is loaded
runtime is long enough to observe
```

---

# Typical use

## Focused streamer capture

Terminal 1:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
   --csv \
   -i 0.001 \
   -n SP_ALU_WORKING_CYCLES"' \
  | tee results/example_stream.csv
```

Terminal 2:

```bash
adb shell \
  '/data/local/tmp/jerry_work/example_workload/runner \
   /data/local/tmp/jerry_work/example_workload/shader.comp.spv'
```

Stop the streamer with `Ctrl+C`.

## Full sweeper capture

1. Build and push `streamer_sweeper`.
2. Build and push the benchmark runner.
3. Compile and push the required `.spv`.
4. Test the benchmark directly.
5. Run the sweeper using its current documented interface.
6. Confirm that the result contains chunk CSVs, logs, metadata, and `summary.csv`.
7. Pull the newest sweep:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

./scripts/pull_latest_sweep.sh
```

---

# Expected results

## KGSL samplers

Expect CSV time series containing GPU busy/load, frequency, bus-related values, and `NA` for unavailable nodes.

These are supporting kernel signals, not raw hardware performance counters.

## Streamer

Expect sampled deltas for selected Adreno counters and a focused CSV suitable for workload-window analysis.

## Sweeper

Expect a timestamped directory containing:

- group and chunk directories;
- one CSV per counter chunk;
- active counter names;
- benchmark logs;
- metadata;
- benchmark exit status;
- optional verification output; and
- `summary.csv`.

## Turnip build

Expect:

```text
libvulkan_freedreno.so
```

This is a driver artifact, not profiler output.

---

# Limitations

- KGSL nodes vary across devices and kernel versions.
- Root access is usually required for protected KGSL interfaces.
- Shell sleep intervals are requested intervals, not guaranteed effective sample rates.
- `kgsl_all_node_sampler.sh` is too expensive for precise high-rate sampling.
- Sysfs statistics and raw Adreno performance counters are different measurement sources.
- A growing counter indicates more events, not automatically a bottleneck.
- Shader bindings and push constants must match the Vulkan runner exactly.
- Repeated experiments should control workload arguments, ordering, temperature, and frequency behavior.
- Record shader, runner, profiler, device, driver, and kernel versions for reproducibility.

---

# Initial setup

```bash
cd /Users/jerryyun/adreno-gpu-profiler

chmod +x scripts/*.sh

adb devices
```

Push all phone-side samplers:

```bash
adb push \
  scripts/kgsl_live_sampler.sh \
  scripts/kgsl_fast_sampler.sh \
  scripts/kgsl_all_node_sampler.sh \
  /data/local/tmp/

adb shell \
  'chmod 755 \
   /data/local/tmp/kgsl_live_sampler.sh \
   /data/local/tmp/kgsl_fast_sampler.sh \
   /data/local/tmp/kgsl_all_node_sampler.sh'
```