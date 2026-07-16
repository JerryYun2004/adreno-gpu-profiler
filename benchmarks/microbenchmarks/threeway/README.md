# Three-Way GPU Microbenchmark

This directory contains a controlled Vulkan compute benchmark with three workload
classes:

1. **Copy baseline** — one input read and one output write per element.
2. **ALU-heavy** — one input read, a long integer arithmetic loop, and one output
   write per element.
3. **Memory-heavy** — four input-buffer reads per loop iteration, a small amount
   of integer accumulation, and one output write per element.

The three workloads share the same host runner, descriptor layout, buffer sizes,
workgroup size, dispatch structure, and verification workflow. This makes them
useful for comparing GPU behavior while keeping most host-side and launch-side
variables controlled.

The benchmark was originally created for the project’s KGSL/tracefs/sysfs
validation work. It can also be run beside the perf-counter streamer or used as
the benchmark command for the perf-counter sweeper.

The benchmark is not compiled into the streamer or sweeper. Instead:

```text
three-way Vulkan workload
          ↓
controlled copy / ALU / memory activity
          ↓
streamer, sweeper, or KGSL capture tools observe the GPU
          ↓
offline analysis compares the three workload classes
```

---

# Directory layout

The current repository layout is:

```text
threeway/
├── runner/
│   ├── alu_heavy.comp
│   ├── copy_baseline.comp
│   ├── mem_heavy_clean.comp
│   └── vk_threeway_probe.cpp
└── shaders/
    ├── alu_heavy.comp.spv
    ├── alu.comp.spv
    ├── copy_baseline.comp.spv
    ├── mem_heavy_clean.comp.spv
    └── mem.comp.spv
```

The three current source/SPIR-V pairs are:

```text
runner/alu_heavy.comp
    → shaders/alu_heavy.comp.spv

runner/copy_baseline.comp
    → shaders/copy_baseline.comp.spv

runner/mem_heavy_clean.comp
    → shaders/mem_heavy_clean.comp.spv
```

The additional files:

```text
shaders/alu.comp.spv
shaders/mem.comp.spv
```

are older binary-only workload artifacts retained from the earlier two-way
ALU-versus-memory workflow. Their source files are not stored in this directory,
so they should be treated as legacy artifacts unless their provenance is
re-established through hashes or SPIR-V disassembly.

---

# Status and intended use

| Component | Status | Intended use |
|---|---|---|
| `runner/vk_threeway_probe.cpp` | Current | Shared runner for all three controlled workloads |
| `runner/copy_baseline.comp` | Current | Lowest-work structural baseline |
| `runner/alu_heavy.comp` | Current | Compute/ALU-heavy workload |
| `runner/mem_heavy_clean.comp` | Current | Read-heavy multi-stream memory workload |
| `shaders/*_heavy*.spv` and `copy_baseline.comp.spv` | Current generated artifacts | Runtime shader modules |
| `shaders/alu.comp.spv` | Legacy binary-only artifact | Historical two-way comparison |
| `shaders/mem.comp.spv` | Legacy binary-only artifact | Historical two-way comparison |

This directory is a supported auxiliary benchmark, not a production profiler
component.

---

# Relationship to the main profiler tools

The main hardware-counter tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

## Connection to the streamer

The streamer can record selected Adreno performance counters while one of the
three workloads runs.

Example:

```text
start streamer for SP_ALU_WORKING_CYCLES
run copy workload
save CSV

start streamer for SP_ALU_WORKING_CYCLES
run ALU workload
save CSV

start streamer for SP_ALU_WORKING_CYCLES
run memory workload
save CSV
```

This provides a controlled comparison of how one counter responds to the three
workload classes.

## Connection to the sweeper

The sweeper can run one of the three benchmark commands repeatedly while
collecting every supported counter group and chunk.

Conceptually:

```text
streamer_sweeper
    benchmark command:
        vk_threeway_probe alu alu_heavy.comp.spv ...
```

The three-way directory does not currently include a dedicated sweeper wrapper.
Use the current sweeper interface documented in:

```text
tools/profiling/perfcounter_sweeper/README.md
```

## Connection to KGSL trace/sysfs analysis

This benchmark is especially closely connected to:

```text
tools/capture/run_kgsl_threeway_capture.sh
analysis/kgsl_trace_analysis/aggregate_threeway_kgsl.py
analysis/kgsl_trace_analysis/compare_kgsl_runs.py
analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py
```

The three-way benchmark supplies controlled workload classes, while the KGSL
tools examine:

- command-batch activity;
- GPU busy time;
- RAM wait;
- GPU frequency;
- bus activity;
- memory allocation events; and
- driver/runtime behavior.

The intended broad behavior is:

```text
copy:
    lowest total work

alu:
    high GPU/SP activity
    relatively low RAM wait

mem:
    high GPU activity
    higher memory-system activity
    potentially higher RAM wait and bus demand
```

---

# Why a three-way benchmark is needed

A two-way ALU-versus-memory comparison shows that workloads differ, but it does
not provide a low-work reference.

The copy baseline adds that reference.

This allows three useful comparisons:

```text
copy → ALU
copy → memory
ALU  → memory
```

## Copy versus ALU

Shows the effect of adding substantial arithmetic while keeping:

- one input buffer;
- one output buffer;
- one invocation per element;
- the same workgroup size;
- the same number of dispatches; and
- the same command submission structure.

## Copy versus memory

Shows the effect of replacing one simple read/write operation with many
input-buffer reads per invocation.

## ALU versus memory

Helps identify whether a signal responds more strongly to arithmetic pressure or
memory-system pressure.

## Structural controls

All three modes use the same:

- Vulkan runner;
- physical-device selection;
- queue selection;
- buffer allocation;
- host memory type;
- descriptor bindings;
- push-constant layout;
- command pool;
- command buffer;
- dispatch dimensions;
- dispatch repeat loop;
- queue submission count;
- completion wait;
- result verification count; and
- resource cleanup path.

This makes command-batch counts, allocation totals, and setup behavior useful
controls.

---

# File reference

# `runner/vk_threeway_probe.cpp`

## Purpose

A shared Vulkan compute runner supporting three modes:

```text
alu
mem
copy
```

Command-line syntax:

```text
vk_threeway_probe alu|mem|copy shader.spv [n] [iters] [dispatch_repeats]
```

Examples:

```text
vk_threeway_probe copy copy_baseline.comp.spv
vk_threeway_probe alu  alu_heavy.comp.spv
vk_threeway_probe mem  mem_heavy_clean.comp.spv
```

## Default values

The default element count is:

```text
n = 262144 = 2^18 elements
```

Each input/output buffer therefore contains:

```text
262144 × 4 bytes = 1048576 bytes = 1 MiB
```

Default iteration count depends on mode:

```text
copy: 1
alu:  2048
mem:  512
```

Default repeated dispatch count:

```text
64
```

These values are intentionally different because each shader performs a
different amount of work per iteration.

They do **not** make the three workloads equal in runtime or total instruction
count.

## Mode selection

The runner parses the first argument into:

```cpp
enum class Mode {
    ALU,
    MEM,
    COPY
};
```

Unknown modes terminate with a usage message.

## Shader-path selection

The runner does not automatically choose a shader based on mode.

The user must supply both:

```text
mode
shader path
```

Correct pairings are:

```text
copy → copy_baseline.comp.spv
alu  → alu_heavy.comp.spv
mem  → mem_heavy_clean.comp.spv
```

Supplying a mismatched mode and shader can produce verification failure.

## Input initialization

The input buffer is initialized as:

```cpp
in[i] = i * 17u + 123u;
```

The output buffer is initialized to zero.

This deterministic initialization makes CPU verification reproducible.

## Vulkan initialization

The runner:

1. creates a Vulkan 1.1 instance;
2. enumerates physical devices;
3. chooses the first physical device;
4. searches for a compute-only queue;
5. falls back to any compute-capable queue;
6. creates a logical device;
7. obtains one queue;
8. allocates input/output buffers;
9. creates a shader module;
10. creates descriptor and pipeline layouts;
11. creates one compute pipeline;
12. allocates one descriptor set;
13. records one command buffer;
14. submits the command buffer once;
15. waits for the queue to become idle; and
16. verifies output.

## Buffer allocation

The runner creates two Vulkan storage buffers:

```text
input
output
```

Both use:

```text
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
```

This allows the CPU to initialize and verify buffers without explicit staging
copies or cache flush/invalidate operations.

The tradeoff is that host-visible memory may not behave exactly like a
production device-local allocation.

## Descriptor layout

All shaders share:

```text
descriptor set 0
binding 0 → readonly input storage buffer
binding 1 → writeonly output storage buffer
```

## Push constants

All shaders share:

```cpp
struct PushConstants {
    uint32_t n;
    uint32_t iters;
};
```

Total push-constant size:

```text
8 bytes
```

The copy shader receives `iters` but does not use it.

## Workgroup and dispatch geometry

All shaders use:

```glsl
layout(local_size_x = 256) in;
```

The runner computes:

```cpp
groups = (n + 255u) / 256u;
```

Each shader checks:

```glsl
if (idx >= pc.n) {
    return;
}
```

Therefore, `n` does not need to be divisible by 256.

## Repeated dispatches

The runner records:

```cpp
for (uint32_t r = 0; r < dispatch_repeats; r++) {
    vkCmdDispatch(cmd, groups, 1, 1);
}
```

All dispatches are placed in one command buffer and submitted with one
`vkQueueSubmit()` call.

This creates a long measurable GPU interval while keeping queue-submission
structure fixed.

The shaders only read the input buffer and overwrite the output buffer. They do
not read the output of a previous dispatch, so each repeat performs the same
logical calculation.

## Memory barrier

After the repeated dispatches, the runner inserts:

```text
compute-shader write
    → host read
```

using a Vulkan memory barrier.

This makes the final output visible to CPU verification after
`vkQueueWaitIdle()`.

## CPU references

The runner contains one reference function per mode:

```text
ref_copy()
ref_alu()
ref_mem_clean()
```

The selected mode determines which reference is used.

## Verification

The runner checks:

```text
min(n, 1024)
```

output elements.

A successful run prints:

```text
Verification PASSED
```

A mismatch prints:

```text
index
GPU result
expected CPU result
```

and returns a nonzero process exit code.

## Power-of-two restriction

The runner checks:

```cpp
if ((n & (n - 1u)) != 0u) {
    ...
}
```

This is required by memory mode because it uses:

```text
index & (n - 1)
```

for wraparound.

However, the current check applies to **all** modes.

Therefore, copy and ALU mode are also unnecessarily restricted to power-of-two
element counts.

For consistent three-way experiments, using the same power-of-two `n` for all
three modes is appropriate.

## Status

Current and reusable.

It is the central file in this directory.

---

# `runner/copy_baseline.comp`

## Purpose

The lowest-work reference workload.

Each invocation:

1. reads one `uint` from the input buffer;
2. writes that value to the output buffer.

Core operation:

```glsl
out_data[idx] = in_data[idx];
```

## Work per invocation

Approximately:

```text
one global/storage-buffer read
one global/storage-buffer write
bounds check
index calculation
```

There is no loop and no use of `pc.iters`.

## Why it exists

The copy workload provides a baseline for:

- Vulkan dispatch overhead;
- descriptor/buffer setup;
- one read plus one write;
- command-batch structure;
- allocation structure; and
- low-work GPU activity.

Without this baseline, ALU and memory workloads can only be compared against
each other.

## Expected behavior

Relative to the other modes, copy should usually have:

- the shortest active interval;
- the lowest SP busy total;
- the lowest ALU-working total;
- fewer input reads than memory mode;
- lower RAM wait than the memory-heavy workload; and
- the same command-batch and buffer-allocation structure.

## Limitations

This is not a pure measure of launch overhead because it still performs real
buffer reads and writes.

It can also be strongly affected by:

- cache state;
- memory placement;
- write-combining behavior;
- dispatch repeat count; and
- GPU frequency scaling.

---

# `runner/alu_heavy.comp`

## Purpose

A loop-based integer arithmetic workload.

Each invocation:

1. reads one input element;
2. mixes the input with the invocation index;
3. executes `pc.iters` rounds of integer arithmetic;
4. writes one final result.

The loop contains:

```text
integer multiply
integer add
XOR
right shift
integer multiply
XOR
right shift
integer multiply
XOR
right shift
integer add/XOR mix
```

The arithmetic is identical to `ref_alu()` in the host runner.

## Data dependency

Each operation updates the same variable:

```glsl
uint x
```

The next operation depends on the previous result.

This creates a serial dependency chain and prevents the arithmetic from being
removed as dead code.

## Memory traffic

Per invocation, the shader performs approximately:

```text
one input read
one output write
```

The amount of external buffer traffic does not increase with `iters`.

This makes the workload suitable for creating high arithmetic intensity.

## Default workload size

Default:

```text
n = 262144
iters = 2048
dispatch_repeats = 64
```

This is intentionally long-running so profiler sampling and trace capture see a
clear active interval.

## Why it exists

Use this workload to examine:

- SP busy cycles;
- ALU working cycles;
- integer instruction activity;
- compute-heavy GPU load;
- frequency scaling under arithmetic load;
- command-batch active duration; and
- differences from memory-heavy behavior.

## Relation to ALU calibration

This shader is not the most precise ALU calibration workload.

The dedicated directory:

```text
benchmarks/microbenchmarks/alu_calibration/
```

contains fixed unrolled 64/128/256/512 ALU-chain variants designed to test
counter linearity.

The three-way ALU workload serves a different purpose:

```text
three-way:
    broad workload classification

ALU calibration:
    controlled scaling and counter validation
```

## Limitations

The loop may be transformed by the driver compiler through:

- unrolling;
- scheduling;
- strength reduction;
- instruction fusion; or
- other optimization.

The `iters` count is therefore not an exact Adreno machine-instruction count.

---

# `runner/mem_heavy_clean.comp`

## Purpose

A controlled multi-stream read-heavy workload.

Each invocation:

1. starts from its own global index;
2. loops `pc.iters` times;
3. calculates a base position `p`;
4. performs four input-buffer reads;
5. combines the values with XOR and addition; and
6. writes one final result.

The four streams are:

```glsl
in_data[p]
in_data[(p + 1024) & mask]
in_data[(p + 8192) & mask]
in_data[(p + 65536) & mask]
```

## Byte separations

Because each element is a 32-bit `uint`, the stream offsets correspond to:

```text
1024 elements  = 4 KiB
8192 elements  = 32 KiB
65536 elements = 256 KiB
```

For the default 1 MiB input buffer, these streams sample widely separated
regions of the working set.

## Work per invocation

For `iters = I`:

```text
4 × I input-buffer reads
1 output-buffer write
small integer address/accumulation work
```

With the default `iters = 512`:

```text
2048 input reads per invocation
1 output write per invocation
```

## Address behavior

At loop iteration `i`:

```glsl
p = (idx + i) & mask;
```

Adjacent invocations still access adjacent positions for each stream at the same
loop iteration.

Therefore, this workload is:

```text
multi-stream and read-heavy
```

but not:

```text
random pointer chasing
classic lane-stride uncoalesced access
pure dependent-load latency test
```

The fixed offsets primarily create multiple simultaneously active regions and
increase the effective read working set.

## Why it exists

Use this workload to examine:

- UCHE activity;
- SP-to-UCHE read transactions;
- RAM-read requests;
- external-memory beats;
- RAM wait;
- bus demand;
- memory-related stalls;
- command-batch active time; and
- behavior relative to the copy and ALU modes.

## “Clean” meaning

The name `mem_heavy_clean` indicates that the workload is intended to be a
clearer controlled memory comparison than earlier memory probes.

It uses:

- fixed source code;
- deterministic streams;
- a matching CPU reference;
- the same runner as the other modes; and
- no optional verification bypass.

## Power-of-two requirement

The shader uses:

```glsl
mask = pc.n - 1
index & mask
```

This implements modulo wraparound correctly only when `n` is a power of two.

The runner enforces that requirement.

## Limitations

The workload does not guarantee DRAM access for every read.

Requests may be served by:

- L1/texture cache;
- UCHE;
- system cache;
- or external memory

depending on device state and working-set behavior.

---

# Current SPIR-V files

# `shaders/copy_baseline.comp.spv`

Compiled runtime module for:

```text
runner/copy_baseline.comp
```

Use with:

```text
mode = copy
```

# `shaders/alu_heavy.comp.spv`

Compiled runtime module for:

```text
runner/alu_heavy.comp
```

Use with:

```text
mode = alu
```

# `shaders/mem_heavy_clean.comp.spv`

Compiled runtime module for:

```text
runner/mem_heavy_clean.comp
```

Use with:

```text
mode = mem
```

# `shaders/alu.comp.spv`

Legacy binary-only ALU workload.

Its editable GLSL source is not stored in this directory.

Do not assume it is identical to `alu_heavy.comp.spv`.

Audit it with:

```bash
spirv-dis \
  benchmarks/microbenchmarks/threeway/shaders/alu.comp.spv \
  -o /tmp/threeway_legacy_alu.spvasm
```

# `shaders/mem.comp.spv`

Legacy binary-only memory workload.

Its editable GLSL source is not stored in this directory.

Do not assume it is identical to `mem_heavy_clean.comp.spv`.

Audit it with:

```bash
spirv-dis \
  benchmarks/microbenchmarks/threeway/shaders/mem.comp.spv \
  -o /tmp/threeway_legacy_mem.spvasm
```

## Recommended handling of legacy SPIR-V

Before deleting or replacing them:

```bash
shasum -a 256 \
  benchmarks/microbenchmarks/threeway/shaders/alu.comp.spv \
  benchmarks/microbenchmarks/threeway/shaders/mem.comp.spv
```

Store the hashes in historical experiment documentation.

If no active script references these binaries, move them to a legacy directory:

```text
threeway/legacy_shaders/
```

or remove them after preserving provenance in Git history.

---

# Shader interface shared by all current workloads

All three current shaders use the same interface.

## GLSL version

```glsl
#version 450
```

## Local workgroup size

```glsl
layout(local_size_x = 256) in;
```

## Input buffer

```glsl
layout(set = 0, binding = 0, std430) readonly buffer InBuf {
    uint in_data[];
};
```

## Output buffer

```glsl
layout(set = 0, binding = 1, std430) writeonly buffer OutBuf {
    uint out_data[];
};
```

## Push constants

```glsl
layout(push_constant) uniform PushConstants {
    uint n;
    uint iters;
} pc;
```

## Invocation index

```glsl
uint idx = gl_GlobalInvocationID.x;
```

## Bounds check

```glsl
if (idx >= pc.n) {
    return;
}
```

## Why the shared interface matters

Using the same interface lets one runner load all three SPIR-V modules without
changing:

- descriptor layouts;
- pipeline layouts;
- buffer allocations;
- command recording;
- dispatch dimensions; or
- push-constant size.

Only the SPIR-V module and verification mode change.

This is the central mechanism behind the benchmark’s controlled comparison.

---

# What we expect from the three workloads

There is no single exact numerical ratio that all devices must reproduce.

The expected results are qualitative and comparative.

## Copy baseline

Expected:

```text
lowest total GPU work
lowest active duration
low SP ALU activity
one input read and one output write per invocation
```

## ALU-heavy

Expected:

```text
high SP busy activity
high SP ALU working activity
low buffer traffic relative to arithmetic count
lower RAM-wait fraction than memory mode
```

## Memory-heavy

Expected:

```text
high input-read activity
high UCHE/SP-to-UCHE traffic
higher RAM-read activity
potentially higher RAM wait
potentially higher bus demand
long active interval
```

## Useful structural checks

The three modes should have similar or identical:

```text
buffer allocation count
allocated bytes
descriptor structure
dispatch count
command-buffer count
queue-submission count
```

When these controls match, differences in runtime signals are more plausibly due
to the shader workload.

---

# Counter families of interest

## Copy baseline

Useful counters:

```text
SP_BUSY_CYCLES
UCHE_BUSY_CYCLES
SP_UCHE_READ_TRANS
write-transaction counters
RBBM busy counters
```

## ALU-heavy

Primary candidates:

```text
SP_ALU_WORKING_CYCLES
SP_BUSY_CYCLES
SP_FULL_ALU_INSTRUCTIONS
SP_FULL_ALU_MUL_INSTRUCTIONS
```

Names vary by counter table.

## Memory-heavy

Primary candidates:

```text
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
UCHE latency counters
TP/UCHE starvation counters
SP stall counters
```

## Interpretation caution

A larger counter value means more counted events during the capture.

It does not by itself prove that the counter identifies the performance
bottleneck.

A stronger conclusion combines:

- counter totals;
- execution time;
- active-window duration;
- busy cycles;
- RAM-wait metrics;
- frequency;
- repeated-run stability; and
- controlled benchmark configuration.

---

# Toolchain requirements

## Host operating environment

Recommended:

```text
macOS or Linux
Bash or Zsh
```

## GLSL compiler

Required:

```text
glslangValidator
```

Check:

```bash
glslangValidator --version
```

## SPIR-V Tools

Recommended:

```text
spirv-val
spirv-dis
```

Check:

```bash
spirv-val --version
spirv-dis --version
```

## Android NDK

Required to cross-compile the runner for ARM64 Android.

The project has used Android NDK r27d/r28-era toolchains and Android API 35.

Set:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

## ADB

Required for device transfer and execution.

Check:

```bash
adb version
adb devices
```

## Vulkan-capable Android device

The device must expose a compute-capable Vulkan queue.

## Profiler requirements

For hardware-counter collection:

- rooted or otherwise authorized KGSL counter access;
- streamer or sweeper built for the device;
- correct counter table for the Adreno generation.

For KGSL trace/sysfs collection:

- enabled tracefs/ftrace access;
- available KGSL tracepoints;
- capture scripts under `tools/capture/`.

---

# Build workflow

Run from the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

THREEWAY_DIR="benchmarks/microbenchmarks/threeway"
```

---

# Compile current shaders

Create the output directory:

```bash
mkdir -p "$THREEWAY_DIR/shaders"
```

Compile all three source shaders:

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$THREEWAY_DIR/runner/copy_baseline.comp" \
  -o "$THREEWAY_DIR/shaders/copy_baseline.comp.spv"

glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$THREEWAY_DIR/runner/alu_heavy.comp" \
  -o "$THREEWAY_DIR/shaders/alu_heavy.comp.spv"

glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$THREEWAY_DIR/runner/mem_heavy_clean.comp" \
  -o "$THREEWAY_DIR/shaders/mem_heavy_clean.comp.spv"
```

Equivalent loop:

```bash
for SHADER in copy_baseline alu_heavy mem_heavy_clean; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "$THREEWAY_DIR/runner/${SHADER}.comp" \
    -o "$THREEWAY_DIR/shaders/${SHADER}.comp.spv"
done
```

---

# Validate current SPIR-V

```bash
for SHADER in copy_baseline alu_heavy mem_heavy_clean; do
  spirv-val \
    --target-env vulkan1.1 \
    "$THREEWAY_DIR/shaders/${SHADER}.comp.spv"
done
```

Successful validation normally prints no error.

---

# Generate disassembly for inspection

```bash
mkdir -p "$THREEWAY_DIR/shaders/disassembly"

for SHADER in copy_baseline alu_heavy mem_heavy_clean; do
  spirv-dis \
    "$THREEWAY_DIR/shaders/${SHADER}.comp.spv" \
    -o "$THREEWAY_DIR/shaders/disassembly/${SHADER}.comp.spvasm"
done
```

Useful checks:

```bash
grep -nE \
  'OpLoopMerge|OpLoad|OpStore|OpIMul|OpIAdd|OpBitwiseXor' \
  "$THREEWAY_DIR/shaders/disassembly/"*.spvasm \
  | less
```

Expected broad structure:

```text
copy:
    no loop
    one input-buffer load
    one output-buffer store

alu:
    loop
    integer arithmetic operations
    one input-buffer load
    one output-buffer store

mem:
    loop
    multiple input-buffer loads
    one output-buffer store
```

---

# Build the Android runner

Set the NDK:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

On an Intel macOS NDK package:

```bash
NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin"
```

Build for ARM64 Android API 35:

```bash
"$NDK_BIN/aarch64-linux-android35-clang++" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -static-libstdc++ \
  "$THREEWAY_DIR/runner/vk_threeway_probe.cpp" \
  -o "$THREEWAY_DIR/runner/vk_threeway_probe" \
  -lvulkan
```

Check:

```bash
file "$THREEWAY_DIR/runner/vk_threeway_probe"
```

Expected:

```text
ELF 64-bit
ARM aarch64
Android executable
```

If the compiler path differs:

```bash
find "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt" \
  -type f \
  -name 'aarch64-linux-android35-clang++'
```

---

# Push workflow

Create a device directory:

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/threeway'
```

Push the runner:

```bash
adb push \
  "$THREEWAY_DIR/runner/vk_threeway_probe" \
  /data/local/tmp/jerry_work/threeway/
```

Push the current shaders:

```bash
adb push \
  "$THREEWAY_DIR/shaders/copy_baseline.comp.spv" \
  "$THREEWAY_DIR/shaders/alu_heavy.comp.spv" \
  "$THREEWAY_DIR/shaders/mem_heavy_clean.comp.spv" \
  /data/local/tmp/jerry_work/threeway/
```

Set execute permission:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/threeway/vk_threeway_probe'
```

Confirm:

```bash
adb shell \
  'ls -lh /data/local/tmp/jerry_work/threeway'
```

---

# Run workflow

# Run the copy baseline

Using explicit values:

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   copy \
   /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv \
   262144 \
   1 \
   64'
```

Using mode defaults:

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   copy \
   /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv'
```

# Run the ALU-heavy workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   alu \
   /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
   262144 \
   2048 \
   64'
```

# Run the memory-heavy workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   mem \
   /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
   262144 \
   512 \
   64'
```

# Run all three sequentially

```bash
THREEWAY_REMOTE="/data/local/tmp/jerry_work/threeway"

adb shell \
  "$THREEWAY_REMOTE/vk_threeway_probe \
   copy \
   $THREEWAY_REMOTE/copy_baseline.comp.spv \
   262144 1 64"

adb shell \
  "$THREEWAY_REMOTE/vk_threeway_probe \
   alu \
   $THREEWAY_REMOTE/alu_heavy.comp.spv \
   262144 2048 64"

adb shell \
  "$THREEWAY_REMOTE/vk_threeway_probe \
   mem \
   $THREEWAY_REMOTE/mem_heavy_clean.comp.spv \
   262144 512 64"
```

Every successful command should print:

```text
Workload complete.
Verification PASSED
Done.
```

---

# Capture exit codes

For automated experiments, print the process exit code:

```bash
adb shell '
  /data/local/tmp/jerry_work/threeway/vk_threeway_probe \
    mem \
    /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
    262144 \
    512 \
    64

  rc=$?
  echo "exit_code=$rc"
  exit $rc
'
```

Expected:

```text
exit_code=0
```

---

# Collect with the perf-counter streamer

Use two terminals for controlled manual collection.

## Enable counter access

```bash
adb shell \
  'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

The path is device-specific.

## Create output directory

```bash
mkdir -p results/microbenchmarks/threeway/streamer
```

## Terminal 1: stream one counter

Example:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n SP_ALU_WORKING_CYCLES \
    --csv"' \
  | tee results/microbenchmarks/threeway/streamer/alu_sp_alu_working.csv
```

The installed streamer path may differ.

Check:

```bash
adb shell \
  'ls -l /data/local/tmp/adreno_perf_stream \
         /data/local/tmp/jerry_work/adreno_perf_stream 2>/dev/null'
```

Check the current CLI:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream --help"'
```

## Terminal 2: run one workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   alu \
   /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
   262144 \
   2048 \
   64'
```

Stop the streamer after the workload with:

```text
Ctrl+C
```

Repeat with copy and memory.

## Suggested counter/workload pairings

Copy:

```text
SP_BUSY_CYCLES
UCHE_BUSY_CYCLES
SP_UCHE_READ_TRANS
```

ALU:

```text
SP_ALU_WORKING_CYCLES
SP_BUSY_CYCLES
SP_FULL_ALU_MUL_INSTRUCTIONS
```

Memory:

```text
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
UCHE_BUSY_CYCLES
SP_BUSY_CYCLES
```

---

# Collect with the perf-counter sweeper

The benchmark command for ALU mode is:

```bash
/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
  alu \
  /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
  262144 \
  2048 \
  64
```

Memory mode:

```bash
/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
  mem \
  /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
  262144 \
  512 \
  64
```

Copy mode:

```bash
/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
  copy \
  /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv \
  262144 \
  1 \
  64
```

Use these as benchmark commands according to the current sweeper README.

For a fair comparison, keep constant:

```text
n
dispatch_repeats
sampling interval
counter group/chunk
driver
GPU thermal state
GPU frequency policy
buffer initialization
runner build
shader build
```

The `iters` value intentionally differs by workload.

---

# Collect with KGSL trace/sysfs tools

The most direct project capture script is:

```text
tools/capture/run_kgsl_threeway_capture.sh
```

Review its current CLI before running:

```bash
sed -n '1,320p' \
  tools/capture/run_kgsl_threeway_capture.sh
```

The resulting run directories can be analyzed with:

```bash
python3 analysis/kgsl_trace_analysis/aggregate_threeway_kgsl.py \
  --copy /path/to/copy_run \
  --alu /path/to/alu_run \
  --mem /path/to/mem_run \
  --out-dir results/kgsl_analysis/threeway
```

Pairwise analysis:

```bash
python3 analysis/kgsl_trace_analysis/compare_kgsl_runs.py \
  --a /path/to/alu_run \
  --b /path/to/mem_run \
  --a-label alu \
  --b-label mem \
  --out-dir results/kgsl_analysis/alu_vs_mem
```

---

# Vendor and Turnip comparison

The same SPIR-V modules can be run through:

- the device’s vendor Vulkan driver; and
- Turnip, when the project’s Android Turnip setup is available.

Keep all benchmark arguments identical across drivers.

The project contains:

```text
tools/capture/run_vk_probe_with_turnip.sh
```

Review it before use:

```bash
sed -n '1,280p' \
  tools/capture/run_vk_probe_with_turnip.sh
```

A driver comparison is useful for examining:

- command-batch count;
- active duration;
- RAM wait;
- frequency behavior;
- compiler/runtime differences;
- cache behavior; and
- workload verification.

Do not compare results unless all three modes pass verification under both
drivers.

---

# Basic analysis examples

# Compare verification and runtime logs

Save outputs:

```bash
mkdir -p results/microbenchmarks/threeway/logs
```

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   copy \
   /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv \
   262144 1 64' \
  | tee results/microbenchmarks/threeway/logs/copy.log
```

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   alu \
   /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
   262144 2048 64' \
  | tee results/microbenchmarks/threeway/logs/alu.log
```

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   mem \
   /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
   262144 512 64' \
  | tee results/microbenchmarks/threeway/logs/mem.log
```

Check:

```bash
grep -H \
  'Verification PASSED\|Verification FAILED\|Vulkan error' \
  results/microbenchmarks/threeway/logs/*.log
```

# Compare one streamer counter

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

root = Path("results/microbenchmarks/threeway/streamer")
counter = "SP_ALU_WORKING_CYCLES"

for mode in ["copy", "alu", "mem"]:
    path = root / f"{mode}_{counter}.csv"

    if not path.exists():
        print(f"{mode}: missing {path}")
        continue

    df = pd.read_csv(path)

    if counter not in df.columns:
        print(f"{mode}: missing column {counter}")
        continue

    print(f"{mode:5s} total={df[counter].sum():.0f}")
PY
```

Adapt filenames to the actual capture names.

---

# Fair-comparison guidance

## Keep `n` constant

All three modes should operate on the same number of elements.

Recommended:

```text
262144
```

## Keep dispatch repeats constant

Recommended:

```text
64
```

## Understand that `iters` is not normalized

Default:

```text
copy = 1
alu  = 2048
mem  = 512
```

These values create clear workload classes.

They do not represent equal work, equal execution time, or equal energy.

## Warm-up

Run one unmeasured workload before collecting formal data:

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   copy \
   /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv \
   262144 1 4'
```

Then allow the device to settle before formal capture.

## Repeat runs

Use multiple repetitions and report:

```text
mean
standard deviation
coefficient of variation
```

## Control thermal state

Long ALU and memory workloads can heat the device and change frequency.

Record:

```text
GPU frequency
thermal pwrlevel
throttling
device temperature
run order
```

## Randomize or rotate run order

Always running:

```text
copy → ALU → memory
```

can bias later workloads through thermal buildup.

For repeated studies, rotate order, for example:

```text
run 1: copy → ALU → memory
run 2: memory → copy → ALU
run 3: ALU → memory → copy
```

---

# Expected interpretations

# Copy → ALU

Expected increases in:

```text
SP_ALU_WORKING_CYCLES
SP_BUSY_CYCLES
integer instruction counters
GPU active duration
```

Memory read/write transaction counts may remain closer because both perform one
main input read and one output write per invocation.

# Copy → memory

Expected increases in:

```text
input read transactions
UCHE activity
RAM-read requests
external-memory beats
RAM wait
GPU active duration
```

# ALU → memory

Expected:

```text
ALU:
    stronger arithmetic counters

memory:
    stronger memory/cache/request counters
    potentially stronger RAM wait
```

The exact `SP_BUSY_CYCLES` relationship depends on runtime, frequency, scheduling,
and counter semantics.

---

# Results that should trigger investigation

- Any mode fails verification.
- Copy produces more ALU-working cycles than ALU mode.
- Memory mode shows no additional read transactions over copy.
- All memory-related counters remain zero.
- ALU and memory are accidentally run with the wrong SPIR-V modules.
- Different modes use different `n` or dispatch repeat counts without being
  documented.
- The element count is not a power of two.
- Legacy `alu.comp.spv` or `mem.comp.spv` is substituted without auditing.
- Captures include large unrelated UI/GPU activity.
- Results vary dramatically across identical repetitions.
- Driver comparisons use different shader binaries.
- Counter totals are compared without accounting for different active durations.
- A single counter is treated as conclusive proof of a bottleneck.

---

# Key mechanisms and libraries

## Vulkan C API

The runner uses raw Vulkan C API calls for:

- instance creation;
- device enumeration;
- queue-family selection;
- device creation;
- queue retrieval;
- buffer creation;
- memory allocation and mapping;
- shader-module creation;
- descriptor layout and updates;
- pipeline-layout creation;
- compute-pipeline creation;
- command-pool creation;
- command-buffer recording;
- dispatch;
- pipeline barriers;
- queue submission; and
- synchronization.

## GLSL compute

The shaders use:

- GLSL 4.50;
- storage buffers;
- push constants;
- global invocation IDs;
- fixed local workgroup size;
- bounds checking;
- integer arithmetic;
- loops; and
- observable output writes.

## SPIR-V

```text
.comp     → editable GLSL source
.comp.spv → Vulkan runtime binary
.spvasm   → optional human-readable disassembly
```

## CPU reference verification

Each mode has an exact host-side reference function.

This is stronger than merely checking that the Vulkan command returned
successfully.

## Deterministic input

All modes use identical deterministic input values.

## One runner, three shaders

Using one runner removes many host-side differences from the comparison.

---

# Known limitations

## No prebuilt runner in the current directory tree

The source file is present, but a tracked `vk_threeway_probe` binary is not
listed in the current tree.

The runner must be built before use.

## Legacy binary-only SPIR-V files

`alu.comp.spv` and `mem.comp.spv` do not have source files in this directory.

## No build script

The directory lacks:

```text
build_shaders.sh
build_runner_android.sh
build_and_push.sh
```

## No run wrapper

The directory lacks:

```text
run_threeway.sh
run_threeway_with_streamer.sh
run_threeway_sweep.sh
```

## No Vulkan timestamps

The runner does not use timestamp queries to report GPU execution duration.

## First physical device

The runner selects the first enumerated device.

## Host-visible buffers

The allocation strategy prioritizes simplicity over production memory placement.

## Partial verification

Only the first 1024 elements are verified.

## Global power-of-two restriction

The runner rejects non-power-of-two `n` even for copy and ALU mode.

## Unequal iteration counts

The three modes are workload classes, not equal-work benchmarks.

## Repeated dispatches in one submission

The command structure represents many dispatch commands in one command buffer
and one queue submission.

It does not model workloads that submit each dispatch separately.

## Compiler transformations

Driver compilers may optimize the shaders differently.

## Memory workload is not pointer chasing

The memory addresses do not depend on loaded data.

## Memory workload can be cache-served

Not every read must reach external DRAM.

## No automatic metadata

The runner does not record:

- shader hash;
- runner hash;
- driver version;
- GPU frequency;
- thermal state;
- timestamp;
- device build fingerprint; or
- profiler configuration.

---

# Recommended maintenance

1. Add a `CMakeLists.txt` or Makefile.
2. Add `build_shaders.sh`.
3. Add `build_runner_android.sh`.
4. Add `push_to_device.sh`.
5. Add `run_threeway.sh`.
6. Add a streamer wrapper that records one CSV per mode.
7. Add a sweeper wrapper for one mode at a time.
8. Add a metadata file containing tool versions and hashes.
9. Add Vulkan timestamp queries.
10. Add optional full-buffer verification.
11. Restrict the power-of-two check to memory mode.
12. Add `--help` handling.
13. Add mode-to-shader filename validation.
14. Move binary-only legacy shaders under `legacy_shaders/`.
15. Add SPIR-V disassembly to the repository when compiler-audit reproducibility
    is important.
16. Record repeated-run mean and standard deviation.
17. Add a simple host script to compare streamer CSVs.
18. Keep the specialized ALU calibration and memory-stride benchmarks separate;
    this benchmark should remain the broad three-class comparison.

---

# Quick-reference commands

# Compile shaders

```bash
cd /Users/jerryyun/adreno-gpu-profiler

THREEWAY_DIR="benchmarks/microbenchmarks/threeway"

for SHADER in copy_baseline alu_heavy mem_heavy_clean; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "$THREEWAY_DIR/runner/${SHADER}.comp" \
    -o "$THREEWAY_DIR/shaders/${SHADER}.comp.spv"
done
```

# Build runner

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"

NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin"

"$NDK_BIN/aarch64-linux-android35-clang++" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -static-libstdc++ \
  benchmarks/microbenchmarks/threeway/runner/vk_threeway_probe.cpp \
  -o benchmarks/microbenchmarks/threeway/runner/vk_threeway_probe \
  -lvulkan
```

# Push

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/threeway'

adb push \
  benchmarks/microbenchmarks/threeway/runner/vk_threeway_probe \
  benchmarks/microbenchmarks/threeway/shaders/copy_baseline.comp.spv \
  benchmarks/microbenchmarks/threeway/shaders/alu_heavy.comp.spv \
  benchmarks/microbenchmarks/threeway/shaders/mem_heavy_clean.comp.spv \
  /data/local/tmp/jerry_work/threeway/

adb shell \
  'chmod 755 /data/local/tmp/jerry_work/threeway/vk_threeway_probe'
```

# Run copy

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   copy \
   /data/local/tmp/jerry_work/threeway/copy_baseline.comp.spv \
   262144 1 64'
```

# Run ALU

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   alu \
   /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
   262144 2048 64'
```

# Run memory

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   mem \
   /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
   262144 512 64'
```

# Expected mode classification

```text
copy → lowest-work baseline
alu  → arithmetic-heavy
mem  → multi-stream read-heavy
```
