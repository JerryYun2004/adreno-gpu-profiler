# Memory-Stride Microbenchmarks

This directory contains Vulkan compute microbenchmarks used to study Adreno
memory-system behavior and validate memory-related hardware performance
counters.

The workloads are designed to create controlled GPU reads while varying either:

- the number and spacing of memory streams; or
- the stride used to move through an input buffer.

They are run **alongside** the project’s perf-counter streamer or sweeper. They
are not linked into either profiler binary.

The directory currently contains three related but different paths:

1. `runner/mem.comp` + `runner/main.cpp` + `runner/vk_mem_probe` form a
   self-contained, verified four-stream memory-heavy workload.
2. `legacy_shaders/mem_stride_legacy.comp` produces compile-time stride variants
   that are compatible with the current runner’s two-word push-constant layout,
   but require `--no-verify`.
3. `shaders/mem_stride.comp` is a newer runtime-stride design with a four-word
   push-constant layout. The current runner does not provide those extra values,
   so this path is not presently self-contained.

The interface mismatch between the current runtime-stride shader and the current
runner is important. Do not assume that every `.spv` file in this directory is
valid with `runner/vk_mem_probe`.

---

# Directory layout

```text
memory_stride/
├── legacy_shaders/
│   ├── mem_stride_legacy.comp
│   └── spv/
│       ├── mem_stride_1_legacy.comp.spv
│       ├── mem_stride_4_legacy.comp.spv
│       ├── mem_stride_16_legacy.comp.spv
│       └── mem_stride_64_legacy.comp.spv
├── runner/
│   ├── alu.comp
│   ├── alu.comp.spv
│   ├── main.cpp
│   ├── mem.comp
│   ├── mem.comp.spv
│   ├── mem.comp.spvasm
│   ├── vk_compute_probe
│   └── vk_mem_probe
└── shaders/
    ├── mem_stride.comp
    └── spv/
        ├── mem_stride_1.comp.spv
        ├── mem_stride_4.comp.spv
        ├── mem_stride_16.comp.spv
        └── mem_stride_64.comp.spv
```

---

# Relationship to the main profiler tools

The main profiler tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

The memory microbenchmark workflow is:

```text
GLSL memory workload
        ↓
compiled SPIR-V
        ↓
Vulkan runner executes on the Adreno GPU
        ↓
streamer or sweeper collects hardware counter deltas
        ↓
host-side CSV analysis compares memory behavior
```

The microbenchmark files do **not**:

- implement KGSL perf-counter access;
- contain the Adreno counter table;
- become part of the streamer or sweeper executable;
- replace the profiler; or
- automatically start counter collection.

They do:

- create repeatable GPU memory traffic;
- help verify that memory-related counters respond to controlled workloads;
- compare cache-friendly and less cache-friendly access patterns;
- provide an independent memory-heavy workload beside the ALU calibration;
- help distinguish ALU pressure from cache, UCHE, TP, and external-memory
  pressure; and
- exercise the complete path from Vulkan shader execution to profiler CSV
  output.

## Connection to the streamer

The streamer samples one or more selected counters while the Vulkan workload
runs.

Typical workflow:

```text
start streamer
run one stride variant
stop streamer
save CSV
repeat for the other strides
compare totals and active regions
```

The existing calibration captures are stored under:

```text
results/calibration/memory_stride/raw_streams/
```

with names such as:

```text
mem_stride_1_perf_stream.csv
mem_stride_4_perf_stream.csv
mem_stride_16_perf_stream.csv
mem_stride_64_perf_stream.csv
```

## Connection to the sweeper

The sweeper can collect all available counter groups and chunks while repeatedly
running an external benchmark command.

The memory workloads can be used as the sweeper’s benchmark command, but this
directory currently contains no dedicated wrapper such as:

```text
run_memory_stride_sweep.sh
```

The sweeper does not load `.comp` or `.spv` files directly. It invokes a Vulkan
runner, and the runner loads the selected `.spv` module.

---

# Status summary

| Component | Status | Recommended use |
|---|---|---|
| `runner/mem.comp` | Current, verified workload | General memory-heavy baseline |
| `runner/main.cpp` | Current source for `vk_mem_probe` | Build and run `mem.comp`; run legacy stride shaders with `--no-verify` |
| `runner/vk_mem_probe` | Current generated Android binary | Main runner currently used |
| `runner/alu.comp` | Auxiliary ALU baseline copied into this runner directory | Historical comparison only |
| `runner/vk_compute_probe` | Historical prebuilt binary; not reproduced by current `main.cpp` | Legacy comparison only |
| `legacy_shaders/mem_stride_legacy.comp` | Operational legacy stride design | Current practical stride sweep with `--no-verify` |
| `legacy_shaders/spv/*_legacy.comp.spv` | Generated compile-time stride variants | Runtime inputs for current runner |
| `shaders/mem_stride.comp` | Newer design, runner support incomplete | Retain for future runtime-stride runner |
| `shaders/spv/mem_stride_*.comp.spv` | Generated artifacts requiring audit | Do not assume compatibility with current runner |

---

# File reference

## `runner/main.cpp`

## Purpose

A minimal Vulkan host program that loads a compute shader, creates input and
output storage buffers, dispatches the shader repeatedly, and optionally verifies
the output.

Its command-line interface is:

```text
vk_mem_probe [spv_path] [elements] [iters] [dispatch_repeats] [--no-verify]
```

Defaults:

```text
spv_path:         /data/local/tmp/jerry_work/mem.comp.spv
elements:         262144
iters:            512
dispatch_repeats: 64
verification:     enabled
```

The runner prints:

- selected SPIR-V path;
- element count;
- iteration count;
- dispatch repeat count;
- verification state;
- selected Vulkan device;
- compute queue family;
- Vulkan errors;
- completion status; and
- output-verification result.

## Vulkan operations performed

The runner:

1. reads SPIR-V from disk;
2. creates a Vulkan 1.1 instance;
3. enumerates physical devices;
4. selects the first physical device;
5. prefers a compute-only queue family;
6. falls back to any compute-capable queue;
7. creates a logical device and queue;
8. creates two storage buffers;
9. allocates host-visible, host-coherent memory;
10. initializes the input buffer;
11. creates the shader module;
12. creates a two-binding descriptor-set layout;
13. creates an 8-byte push-constant range;
14. creates a compute pipeline;
15. records repeated dispatches;
16. inserts a shader-write-to-host-read memory barrier;
17. submits and waits for the queue;
18. verifies up to the first 1024 outputs; and
19. releases Vulkan resources.

## Descriptor layout

```text
set 0, binding 0 → input storage buffer
set 0, binding 1 → output storage buffer
```

## Push-constant layout

The runner sends exactly two 32-bit values:

```cpp
struct PushConstants {
    uint32_t n;
    uint32_t iters;
};
```

Total size:

```text
8 bytes
```

This is compatible with:

- `runner/mem.comp`; and
- `legacy_shaders/mem_stride_legacy.comp`.

It is **not compatible** with the current `shaders/mem_stride.comp`, which
expects four 32-bit values.

## Dispatch structure

The runner calculates:

```cpp
groups = (n + 255) / 256
```

because every shader in this directory uses:

```glsl
layout(local_size_x = 256) in;
```

All provided source shaders include a bounds check, so `n` does not need to be
divisible by 256.

## Verification behavior

The current runner always uses the four-stream memory CPU reference:

```text
cpu_mem_reference()
```

when verification is enabled.

The source also contains an ALU CPU reference function, but that function is not
used by the current `main()` implementation.

Therefore:

- `mem.comp` can be verified correctly;
- `alu.comp` cannot be verified correctly by this source;
- `mem_stride_legacy.comp` cannot be verified correctly by this source; and
- `mem_stride.comp` cannot be used correctly because of the push-constant
  mismatch.

Use `--no-verify` only when intentionally running a shader whose output formula
does not match `cpu_mem_reference()`.

## Important power-of-two requirement

`mem.comp` and `mem_stride_legacy.comp` use:

```glsl
index & (n - 1)
```

as a fast wraparound operation.

For the intended access pattern, use a power-of-two element count:

```text
262144
524288
1048576
...
```

The runner does not enforce this requirement.

A non-power-of-two size remains memory-safe because the bitwise result is no
larger than `n - 1`, but the resulting address distribution is not equivalent to
modulo `n` and can be highly uneven.

## Status

Current source for the memory runner.

The source corresponds to the `[vk_mem_probe]` logging interface and
`--no-verify` option.

---

## `runner/vk_mem_probe`

## Purpose

Prebuilt ARM64 Android executable generated from the memory runner.

It loads a user-selected SPIR-V module and executes it using Vulkan.

## Observed binary format

The uploaded binary is:

```text
ELF 64-bit LSB PIE executable
ARM aarch64
Android API 35
built with Android NDK r27d
not stripped
```

## Status

Generated runtime artifact.

The source of truth is `runner/main.cpp`.

Rebuild it when:

- `main.cpp` changes;
- the NDK changes;
- Android API level changes;
- the ABI changes; or
- a reproducible release is needed.

---

## `runner/mem.comp`

## Purpose

A verified memory-heavy GLSL compute shader.

Each invocation:

1. reads its own initial element;
2. enters an iteration loop;
3. computes four different input indices;
4. performs four storage-buffer reads;
5. combines the values using XOR and addition; and
6. writes one output element.

The four index streams are:

```glsl
j0 = idx + i * 17   + 1
j1 = idx + i * 67   + 13
j2 = idx + i * 257  + 29
j3 = idx + i * 1021 + 53
```

Each result is wrapped using:

```glsl
& (n - 1)
```

## Work per invocation

For `iters = I`, each invocation performs approximately:

```text
1 initial input read
4 × I loop input reads
1 output write
```

For `iters = 512`:

```text
2049 input reads per invocation
1 output write per invocation
```

This excludes any compiler-generated or cache-line-level effects.

## What kind of workload is this?

This is a multi-stream, memory-heavy workload.

It is not:

- true random pointer chasing;
- dependent-load latency chasing;
- a pure bandwidth copy;
- a single-stride sweep; or
- a guarantee of uncached DRAM traffic.

At a fixed iteration, adjacent invocations still tend to access adjacent
addresses for each of the four streams. The workload can therefore retain some
wave-level coalescing while exercising multiple address streams and a large
working set.

## Why it exists

Use `mem.comp` as:

- the verified memory-heavy baseline;
- a contrast to `runner/alu.comp`;
- a sanity check for RAM-wait and memory-traffic counters;
- a long-running workload for profiler testing; and
- a test of the runner’s CPU verification.

## Expected profiler behavior

Compared with a compute-heavy ALU shader, this workload should generally show
more activity in memory-related metrics, such as:

```text
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
```

and potentially:

```text
KGSL ram_wait
bus bandwidth
GPU active duration
```

Exact counter names and behavior depend on the Adreno generation and counter
table.

---

## `runner/mem.comp.spv`

## Purpose

Compiled SPIR-V binary generated from `runner/mem.comp`.

This is the default intended shader for `runner/vk_mem_probe`.

## Status

Generated runtime artifact.

Rebuild it whenever `mem.comp` changes.

---

## `runner/mem.comp.spvasm`

## Purpose

Human-readable SPIR-V disassembly of `runner/mem.comp.spv`.

It is retained for:

- checking descriptor bindings;
- checking the push-constant offsets;
- inspecting index arithmetic;
- confirming the loop structure;
- auditing the generated integer and load operations; and
- documenting compiler output.

It is not loaded by Vulkan.

---

## `runner/alu.comp`

## Purpose

An ALU-heavy comparison shader retained inside the memory-runner directory.

Each invocation:

1. reads one input element;
2. mixes it with the invocation index;
3. repeatedly performs integer multiply, add, XOR, and shifts; and
4. writes one result.

## Status

Historical auxiliary workload.

This is effectively an ALU baseline copied into the older `vulkan_mem_probe`
workspace. The canonical ALU calibration now lives under:

```text
benchmarks/microbenchmarks/alu_calibration/
```

## Compatibility warning

Its descriptor and two-word push-constant layout match `runner/main.cpp`.

However, the current `main.cpp` verifies all shaders with
`cpu_mem_reference()`, not the ALU reference.

Running `alu.comp.spv` through `vk_mem_probe` with verification enabled will
normally fail verification.

Use the dedicated ALU runner or `--no-verify` for historical experiments.

---

## `runner/alu.comp.spv`

## Purpose

Compiled SPIR-V generated from `runner/alu.comp`.

## Status

Generated historical artifact.

Prefer the canonical ALU calibration directory for new experiments.

---

## `runner/vk_compute_probe`

## Purpose

Historical prebuilt ALU compute executable kept in the memory-runner workspace.

## Observed binary format

The uploaded binary is:

```text
ELF 64-bit LSB PIE executable
ARM aarch64
Android API 35
built with Android NDK r28
not stripped
```

Its embedded log strings use the prefix:

```text
[vk_compute_probe]
```

## Status

Legacy binary.

The current `runner/main.cpp` uses `[vk_mem_probe]` logging and does not reproduce
this binary exactly. Its corresponding source is therefore not clearly present
in this directory.

The canonical ALU runner source is documented separately under:

```text
benchmarks/microbenchmarks/alu_calibration/
```

---

## `legacy_shaders/mem_stride_legacy.comp`

## Purpose

A compile-time stride shader used to generate fixed stride variants.

The source defines:

```glsl
#ifndef STRIDE
#define STRIDE 1u
#endif
```

The stride is compiled into each SPIR-V module with a preprocessor definition.

Each invocation:

1. gets `gl_GlobalInvocationID.x`;
2. exits when `gid >= n`;
3. creates a wrap mask `n - 1`;
4. starts at its own invocation index;
5. advances by the compile-time `STRIDE` every iteration;
6. reads one input element per iteration;
7. accumulates the values; and
8. writes one output element.

## Push constants

```text
n
iters
```

This matches the current runner’s 8-byte push-constant range.

## Why compile-time stride was used

A compile-time constant:

- requires no runner change;
- can let the shader compiler simplify address arithmetic;
- produces one explicit SPIR-V module per stride; and
- makes the workload variant visible in the filename.

## Why it is called legacy

The newer source attempted to make `stride` and `mask` runtime push constants,
which avoids compiling four shader modules.

However, the current runner still supports only `n` and `iters`. Therefore, the
legacy compile-time path is currently the practical runnable stride path.

## Verification warning

The current `vk_mem_probe` CPU reference implements the four-stream `mem.comp`
algorithm, not this single-stride accumulation.

Run the legacy stride variants with:

```text
--no-verify
```

until a matching CPU reference is added.

---

## `legacy_shaders/spv/mem_stride_1_legacy.comp.spv`

Compiled legacy shader with:

```text
STRIDE = 1
```

This has the strongest overlap between accesses from adjacent loop iterations.

---

## `legacy_shaders/spv/mem_stride_4_legacy.comp.spv`

Compiled legacy shader with:

```text
STRIDE = 4
```

---

## `legacy_shaders/spv/mem_stride_16_legacy.comp.spv`

Compiled legacy shader with:

```text
STRIDE = 16
```

---

## `legacy_shaders/spv/mem_stride_64_legacy.comp.spv`

Compiled legacy shader with:

```text
STRIDE = 64
```

This advances more quickly through the working set and may reduce reuse across
successive iterations.

---

## `shaders/mem_stride.comp`

## Purpose

A newer runtime-configurable stride shader.

Its push constants are:

```glsl
uint n;
uint iters;
uint stride;
uint mask;
```

Total push-constant size:

```text
16 bytes
```

Each invocation performs one load per loop iteration:

```glsl
idx = (idx + stride) & mask;
acc += in_data[idx];
```

## Intended improvement

This design allows one SPIR-V module to execute any stride value.

Conceptually, the runner would pass:

```text
n      = number of elements
iters  = reads per invocation
stride = 1, 4, 16, 64, ...
mask   = n - 1
```

That is cleaner than compiling four nearly identical modules.

## Current incompatibility

The present `runner/main.cpp` creates only an 8-byte push-constant range and
writes only:

```text
n
iters
```

It does not provide:

```text
stride
mask
```

Therefore, the current runner is not valid for this shader.

Possible consequences include:

- pipeline creation failure on strict implementations;
- undefined values for `stride` and `mask`;
- incorrect indexing;
- meaningless results; or
- validation-layer errors.

Do not use this shader with the current runner until the runner is updated.

## Required runner change

A compatible runner needs:

```cpp
struct PushConstants {
    uint32_t n;
    uint32_t iters;
    uint32_t stride;
    uint32_t mask;
};
```

and a 16-byte Vulkan push-constant range.

It should also implement a matching CPU reference when verification is enabled.

---

## `shaders/spv/mem_stride_1.comp.spv`

## `shaders/spv/mem_stride_4.comp.spv`

## `shaders/spv/mem_stride_16.comp.spv`

## `shaders/spv/mem_stride_64.comp.spv`

## Purpose

These are generated SPIR-V artifacts stored under the newer runtime-stride
directory.

## Audit warning

Based on the current `shaders/mem_stride.comp` source, stride is a runtime push
constant. Compiling the source four times without modifying the source should
produce the same SPIR-V module each time.

The filenames alone do not prove that stride 1, 4, 16, and 64 are encoded in
these binaries.

Before using them, compare their hashes:

```bash
shasum -a 256 \
  benchmarks/microbenchmarks/memory_stride/shaders/spv/*.spv
```

If all four hashes match, they are the same runtime-stride module under four
different names.

If the hashes differ, they may have been built from an earlier source revision
or with an undocumented compile-time transformation. Preserve them for
historical reproducibility, but regenerate and document them before new work.

## Recommended cleanup

For the runtime-stride design, prefer one clearly named file:

```text
mem_stride_runtime.comp.spv
```

and pass the stride through the runner.

---

# Shader structure and rationale

All source shaders use:

```glsl
#version 450
layout(local_size_x = 256) in;
```

## One-dimensional workgroups

The workloads process one-dimensional `uint` arrays.

A 256-thread local size:

- matches the runner’s dispatch calculation;
- produces many parallel memory requests;
- keeps launch geometry fixed across variants; and
- is commonly supported by Vulkan compute devices.

The purpose is to keep workgroup size controlled, not to claim that 256 is the
optimal local size.

## Two storage buffers

All shaders use:

```text
set 0, binding 0 → input
set 0, binding 1 → output
```

The output write ensures that the computation remains observable and discourages
dead-code elimination.

## Bounds checking

Every source shader checks:

```glsl
if (gid >= n) {
    return;
}
```

This makes ceiling-divided dispatch safe.

## Bitmask wrapping

The stride shaders use:

```glsl
index & (n - 1)
```

instead of:

```glsl
index % n
```

For power-of-two `n`, these are equivalent.

The bitmask form is generally simpler for integer hardware and removes integer
division/modulo from the inner loop.

## Dependent loop-carried index

The stride shader updates:

```glsl
idx = (idx + stride) & mask;
```

each iteration.

The next address depends on the preceding index value, but not on the loaded
data.

This is an address-generation dependency, not true pointer chasing.

## Accumulator

```glsl
acc += in_data[idx];
```

makes every load contribute to the final output.

This helps prevent the compiler from deleting the reads.

---

# What changing stride actually tests

It is tempting to describe the variants as directly changing memory coalescing,
but the current shader pattern is more subtle.

At a fixed loop iteration:

```text
thread 0 accesses base + 0
thread 1 accesses base + 1
thread 2 accesses base + 2
...
```

because every thread begins at its own `gid` and adds the same stride-dependent
offset.

Therefore, adjacent threads still access adjacent elements at each iteration,
even for stride 64.

Changing stride mainly changes:

- how far the wave moves between iterations;
- overlap between successive iteration footprints;
- temporal reuse;
- cache-line reuse across iterations;
- how quickly the address sequence wraps around the buffer; and
- pressure on the effective working set.

It does **not** create the classic pattern where lane `k` accesses:

```text
base + k × stride
```

at the same instruction.

For a classic lane-stride/coalescing test, the address would instead depend on:

```glsl
gid * stride
```

or a related lane-scaled expression.

This distinction matters when interpreting the counters.

---

# Why these files are needed

## 1. Validate memory-related counter activity

The workload should activate counters associated with:

- SP-to-UCHE reads;
- UCHE requests;
- cache activity;
- external memory beats;
- memory latency;
- memory stalls; and
- shader-core busy time.

This helps distinguish working counters from:

- unsupported countables;
- incorrectly mapped selectors;
- counters stuck at zero;
- stale CSV data; or
- collection-window errors.

## 2. Compare memory-heavy and ALU-heavy behavior

The verified `mem.comp` workload can be compared with the ALU calibration.

Expected broad distinction:

```text
ALU workload:
    more arithmetic work
    relatively lower RAM wait

memory workload:
    more read requests and transactions
    potentially higher RAM wait and bus activity
```

## 3. Study cache locality

The stride variants change iteration-to-iteration locality while keeping:

- element count;
- workgroup size;
- loop iteration count;
- dispatch count; and
- one load per stride-loop iteration

constant.

This provides a controlled way to study whether selected counters respond to
changes in effective locality.

## 4. Verify the end-to-end profiler path

The workflow tests:

```text
GLSL
→ SPIR-V compiler
→ Vulkan runner
→ Android Vulkan driver
→ Adreno memory system
→ KGSL counter access
→ streamer/sweeper
→ CSV output
→ host-side comparison
```

## 5. Establish a memory baseline before ML kernels

Softmax, RMSNorm, and attention combine:

- global reads and writes;
- local/shared memory;
- arithmetic;
- reductions;
- barriers; and
- cache behavior.

A simpler memory workload helps determine whether surprising ML-kernel counter
results come from the profiler or from the kernel.

---

# Expected results

Unlike the ALU-chain calibration, the stride sweep does not have a simple ideal
ratio such as 1:2:4:8.

## Expected qualitative trends

Compared with the ALU-heavy workload, `mem.comp` should generally show more:

```text
UCHE activity
SP-to-UCHE read transactions
read requests
external memory beats
RAM wait
bus demand
```

For the stride variants, increasing stride may cause:

- lower temporal reuse;
- more cache-line turnover;
- increased UCHE or external-memory requests;
- increased read beats;
- increased latency or stall signals; and
- longer runtime.

However, monotonic growth is not guaranteed.

## Why monotonic scaling is not guaranteed

Results depend on:

- cache-line size;
- wave/subgroup width;
- cache capacity;
- replacement behavior;
- buffer size;
- iteration count;
- address wrap period;
- compiler optimization;
- memory coalescing;
- GPU frequency;
- thermal state;
- sampling interval; and
- counter semantics.

A stride of 64 can also create periodic patterns that interact with cache sets or
the power-of-two mask.

## Counters of interest

Primary candidates include:

```text
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
SP_BUSY_CYCLES
SP_ALU_WORKING_CYCLES
```

Additional useful families can include:

```text
UCHE read/write requests
UCHE miss or latency counters
TP/UCHE starvation counters
VBIF read/write beat counters
HLSQ stalls
SP stalls
RBBM busy counters
```

Counter availability and naming vary by Adreno generation.

## Results that should trigger investigation

- Every memory-related counter is zero.
- The workload finishes but verification fails for `mem.comp`.
- A legacy stride shader is run without `--no-verify` and is interpreted as a
  workload failure.
- The runtime-stride shader is used with the 8-byte runner.
- The four runtime-stride SPIR-V files have identical hashes but are assumed to
  encode different strides.
- The element count is not a power of two.
- Result files contain long idle regions or data from another workload.
- Repeated identical runs vary dramatically.
- Higher stride is assumed to mean uncoalesced lane accesses without inspecting
  the shader’s actual indexing.

---

# Toolchain requirements

## Host tools

### `glslangValidator`

Compiles Vulkan GLSL compute shaders to SPIR-V.

Check:

```bash
glslangValidator --version
```

### SPIR-V Tools

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

### Android NDK

Required to build the ARM64 Android Vulkan runner.

The observed `vk_mem_probe` binary was built with NDK r27d for Android API 35.

Set:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

### ADB

Required to push and run files on the phone.

```bash
adb version
adb devices
```

### Vulkan-capable Android device

The device must provide a compute-capable Vulkan queue.

## Profiler requirements

The streamer or sweeper must already be built and pushed separately.

On the rooted project phone, counter access is enabled with:

```bash
adb shell \
  'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

The exact path and permissions are device-specific.

---

# Build workflow

Run from the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

MEM_DIR="benchmarks/microbenchmarks/memory_stride"
```

---

## 1. Compile the verified memory-heavy shader

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$MEM_DIR/runner/mem.comp" \
  -o "$MEM_DIR/runner/mem.comp.spv"
```

Validate:

```bash
spirv-val \
  --target-env vulkan1.1 \
  "$MEM_DIR/runner/mem.comp.spv"
```

Disassemble:

```bash
spirv-dis \
  "$MEM_DIR/runner/mem.comp.spv" \
  -o "$MEM_DIR/runner/mem.comp.spvasm"
```

---

## 2. Compile the auxiliary ALU shader

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$MEM_DIR/runner/alu.comp" \
  -o "$MEM_DIR/runner/alu.comp.spv"
```

Validate:

```bash
spirv-val \
  --target-env vulkan1.1 \
  "$MEM_DIR/runner/alu.comp.spv"
```

Prefer the dedicated ALU calibration directory for new ALU experiments.

---

## 3. Compile the legacy fixed-stride variants

Create the output directory:

```bash
mkdir -p "$MEM_DIR/legacy_shaders/spv"
```

Compile:

```bash
for STRIDE in 1 4 16 64; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "-DSTRIDE=${STRIDE}u" \
    "$MEM_DIR/legacy_shaders/mem_stride_legacy.comp" \
    -o "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"
done
```

Validate:

```bash
for STRIDE in 1 4 16 64; do
  spirv-val \
    --target-env vulkan1.1 \
    "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"
done
```

Check that the generated modules are distinct:

```bash
shasum -a 256 \
  "$MEM_DIR/legacy_shaders/spv/"*.spv
```

They should normally have different hashes because `STRIDE` is compiled into the
shader.

Inspect the constant in disassembly:

```bash
for STRIDE in 1 4 16 64; do
  spirv-dis \
    "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv" \
    -o "/tmp/mem_stride_${STRIDE}_legacy.spvasm"

  echo "=== STRIDE ${STRIDE} ==="
  grep -n "OpConstant" "/tmp/mem_stride_${STRIDE}_legacy.spvasm" | head
done
```

---

## 4. Compile the runtime-stride shader

Compile one clearly named module:

```bash
mkdir -p "$MEM_DIR/shaders/spv"

glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$MEM_DIR/shaders/mem_stride.comp" \
  -o "$MEM_DIR/shaders/spv/mem_stride_runtime.comp.spv"
```

Validate:

```bash
spirv-val \
  --target-env vulkan1.1 \
  "$MEM_DIR/shaders/spv/mem_stride_runtime.comp.spv"
```

Do not run this module through the current `vk_mem_probe` until the runner’s
push-constant structure is updated.

---

## 5. Build `vk_mem_probe`

Set the NDK path:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

On macOS:

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
  "$MEM_DIR/runner/main.cpp" \
  -o "$MEM_DIR/runner/vk_mem_probe" \
  -lvulkan
```

Check:

```bash
file "$MEM_DIR/runner/vk_mem_probe"
```

Expected architecture:

```text
ARM aarch64
```

The compiler may report that `cpu_reference()` is unused. That is expected in
the current source because verification uses `cpu_mem_reference()`.

---

# Push workflow

## Create a device directory

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/memory_stride'
```

## Push the runner

```bash
adb push \
  "$MEM_DIR/runner/vk_mem_probe" \
  /data/local/tmp/jerry_work/memory_stride/
```

Set permission:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/memory_stride/vk_mem_probe'
```

## Push the verified memory shader

```bash
adb push \
  "$MEM_DIR/runner/mem.comp.spv" \
  /data/local/tmp/jerry_work/memory_stride/
```

## Push legacy stride variants

```bash
adb push \
  "$MEM_DIR/legacy_shaders/spv/"*_legacy.comp.spv \
  /data/local/tmp/jerry_work/memory_stride/
```

## Optional: push runtime-stride shader for future runner work

```bash
adb push \
  "$MEM_DIR/shaders/spv/mem_stride_runtime.comp.spv" \
  /data/local/tmp/jerry_work/memory_stride/
```

Confirm:

```bash
adb shell \
  'ls -lh /data/local/tmp/jerry_work/memory_stride'
```

---

# Run workflow

## Run the verified four-stream memory workload

Recommended controlled configuration:

```text
elements:         1048576
iters:            512
dispatch_repeats: 128
```

Command:

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem.comp.spv \
   1048576 \
   512 \
   128'
```

Expected output includes:

```text
Workload complete.
Verification PASSED
```

## Skip verification for long profiler captures

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

Verification runs on the CPU after GPU completion, so it does not affect GPU
counter values during dispatch. Skipping it is useful when reducing host-side
delay or repeatedly launching the workload.

## Run one legacy stride variant

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_1_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

## Run all legacy stride variants

```bash
for STRIDE in 1 4 16 64; do
  echo "=== memory stride ${STRIDE} ==="

  adb shell \
    "/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
     /data/local/tmp/jerry_work/memory_stride/mem_stride_${STRIDE}_legacy.comp.spv \
     1048576 \
     512 \
     128 \
     --no-verify"
done
```

## Do not run the runtime-stride shader yet

This is not a valid current command:

```text
vk_mem_probe mem_stride_runtime.comp.spv n iters repeats
```

because the runner does not pass `stride` or `mask`.

Add a compatible runner first.

---

# Collect with the perf-counter streamer

Use two terminals for controlled manual collection.

## Terminal 1: enable counter access

```bash
adb shell \
  'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

## Terminal 1: start one counter stream

Create output directory:

```bash
mkdir -p results/calibration/memory_stride/raw_streams
```

Example for stride 1:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n UCHE_VBIF_READ_BEATS_SP \
    --csv"' \
  | tee \
    results/calibration/memory_stride/raw_streams/mem_stride_1_perf_stream.csv
```

The profiler binary may instead be under:

```text
/data/local/tmp/jerry_work/adreno_perf_stream
```

Check:

```bash
adb shell \
  'ls -l /data/local/tmp/adreno_perf_stream \
         /data/local/tmp/jerry_work/adreno_perf_stream 2>/dev/null'
```

Check the current CLI before a new experiment:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream --help"'
```

## Terminal 2: run the workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_1_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

## Terminal 1: stop collection

Press:

```text
Ctrl+C
```

Repeat for strides 4, 16, and 64.

## Suggested streamer counters

Run one counter at a time or according to the current streamer’s supported
multi-counter CLI.

Useful candidates:

```text
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
UCHE_RAM_READ_REQ
UCHE_BUSY_CYCLES
SP_BUSY_CYCLES
SP_ALU_WORKING_CYCLES
```

---

# Collect with the perf-counter sweeper

The sweeper can collect every group/chunk against the same benchmark command.

The exact CLI belongs to:

```text
tools/profiling/perfcounter_sweeper/
```

Review:

```bash
sed -n '1,260p' \
  tools/profiling/perfcounter_sweeper/README.md
```

The benchmark command for one legacy variant is conceptually:

```bash
/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
  /data/local/tmp/jerry_work/memory_stride/mem_stride_16_legacy.comp.spv \
  1048576 \
  512 \
  128 \
  --no-verify
```

A reproducible sweeper wrapper should record:

- stride;
- element count;
- iterations;
- dispatch repeats;
- counter sampling interval;
- driver;
- GPU frequency/thermal state;
- shader hash;
- runner hash; and
- result directory.

---

# Basic host-side analysis

## Sum one counter column

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/calibration/memory_stride/raw_streams/"
    "mem_stride_1_perf_stream.csv"
)

counter = "UCHE_VBIF_READ_BEATS_SP"

df = pd.read_csv(path)
print(df[counter].sum())
PY
```

## Compare all four captures

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

root = Path("results/calibration/memory_stride/raw_streams")
counter = "UCHE_VBIF_READ_BEATS_SP"

print(f"{'stride':>8} {'total':>20}")

for stride in [1, 4, 16, 64]:
    path = root / f"mem_stride_{stride}_perf_stream.csv"
    df = pd.read_csv(path)
    total = float(df[counter].sum())
    print(f"{stride:8d} {total:20.0f}")
PY
```

## Compare several counters

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

root = Path("results/calibration/memory_stride/raw_streams")

counters = [
    "UCHE_VBIF_READ_BEATS_SP",
    "SP_UCHE_READ_TRANS",
    "UCHE_RAM_READ_REQ",
    "UCHE_BUSY_CYCLES",
    "SP_BUSY_CYCLES",
    "SP_ALU_WORKING_CYCLES",
]

rows = []

for stride in [1, 4, 16, 64]:
    path = root / f"mem_stride_{stride}_perf_stream.csv"
    df = pd.read_csv(path)

    row = {"stride": stride}
    for counter in counters:
        row[counter] = float(df[counter].sum()) if counter in df.columns else None
    rows.append(row)

print(pd.DataFrame(rows).to_string(index=False))
PY
```

## Analysis caution

Do not blindly sum:

- long idle periods;
- unrelated UI activity;
- profiler startup/shutdown transients; or
- different capture durations.

Prefer:

- controlled start and stop;
- active-window detection;
- identical collection duration;
- repeated runs;
- idle-baseline checks; and
- mean and standard deviation across repetitions.

---

# Reproducible end-to-end example

## Host: build legacy stride shaders

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
    -o "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"

  spirv-val \
    --target-env vulkan1.1 \
    "$MEM_DIR/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv"
done
```

## Host: build runner

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"

NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin"

"$NDK_BIN/aarch64-linux-android35-clang++" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -static-libstdc++ \
  "$MEM_DIR/runner/main.cpp" \
  -o "$MEM_DIR/runner/vk_mem_probe" \
  -lvulkan
```

## Host: push

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

## Host: enable counter access

```bash
adb shell \
  'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

## Terminal 1: start streamer

```bash
mkdir -p results/calibration/memory_stride/raw_streams

adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n UCHE_VBIF_READ_BEATS_SP \
    --csv"' \
  | tee \
    results/calibration/memory_stride/raw_streams/mem_stride_16_perf_stream.csv
```

## Terminal 2: run stride 16

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_16_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

## Terminal 1: stop

Press:

```text
Ctrl+C
```

---

# Key mechanisms and libraries

## GLSL compute shaders

The source shaders use:

- Vulkan GLSL 4.50;
- `gl_GlobalInvocationID`;
- fixed 256-thread workgroups;
- storage buffers;
- push constants;
- unsigned integer arithmetic;
- loop-based address generation;
- bitmask wrapping; and
- observable output accumulation.

## SPIR-V

```text
.comp     → editable GLSL source
.comp.spv → Vulkan runtime module
.spvasm   → human-readable disassembly
```

## Vulkan C API

The runner uses raw Vulkan objects including:

- `VkInstance`
- `VkPhysicalDevice`
- `VkDevice`
- `VkQueue`
- `VkBuffer`
- `VkDeviceMemory`
- `VkShaderModule`
- `VkDescriptorSetLayout`
- `VkPipelineLayout`
- `VkPipeline`
- `VkDescriptorPool`
- `VkDescriptorSet`
- `VkCommandPool`
- `VkCommandBuffer`

## Host-visible coherent memory

The runner requests:

```text
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
```

This simplifies initialization and verification.

It does not guarantee the same memory placement or behavior as a production
device-local allocation strategy.

## Repeated dispatches

The runner records multiple dispatches into one command buffer before one queue
submission.

This creates a longer measurable GPU interval but can differ from submitting
each dispatch separately.

## CPU verification

`mem.comp` has a matching CPU reference.

The stride shaders currently do not.

## KGSL perf counters

The streamer/sweeper reads Adreno hardware counters through the project’s KGSL
counter-access path while the Vulkan workload runs.

---

# Known limitations

## Runtime-stride shader/runner mismatch

The newer shader expects 16 bytes of push constants; the runner supplies 8.

This is the highest-priority correctness issue in the directory.

## Misleading runtime SPIR-V filenames

The current runtime shader does not compile stride into SPIR-V, yet four
stride-named binaries exist.

Audit their hashes and provenance.

## Legacy variants require `--no-verify`

The runner’s CPU reference does not match the legacy stride algorithm.

## `vk_compute_probe` source mismatch

The historical `vk_compute_probe` binary is not reproduced by the current
`main.cpp`.

## Power-of-two element assumption

The fast bitmask wrap is intended for power-of-two `n`.

## Not a classic uncoalesced lane-stride benchmark

Adjacent threads access adjacent elements at each loop iteration.

The variants primarily change temporal locality and iteration-to-iteration
footprint movement.

## No Vulkan timestamp query

The runner does not report precise GPU execution duration from timestamp queries.

## Host-visible buffers

Memory placement and caching can vary by driver.

## First physical device selection

The runner selects the first enumerated Vulkan device.

## Partial verification

Only the first 1024 elements are checked.

## No dedicated build or run scripts

The directory currently lacks:

```text
build_shaders.sh
build_runner_android.sh
push_to_device.sh
run_stride_calibration.sh
analyze_stride_results.py
```

## Thermal/frequency variation

Long memory workloads can change device temperature and GPU frequency.

## Counter interpretation

A growing counter indicates more counted events, not automatically a bottleneck.

---

# Recommended maintenance

1. Decide whether compile-time or runtime stride is the supported design.
2. If runtime stride is preferred, update `main.cpp` to pass `stride` and `mask`.
3. Add a matching CPU reference for the runtime and legacy stride algorithms.
4. Replace the four runtime-stride filenames with one
   `mem_stride_runtime.comp.spv`.
5. Record how the existing `shaders/spv/mem_stride_*.comp.spv` files were built.
6. Move or remove the historical `runner/vk_compute_probe` after preserving its
   provenance.
7. Keep the canonical ALU workload only in `alu_calibration/`.
8. Add a `CMakeLists.txt` or Makefile for the runner.
9. Add a shader build script.
10. Add a push/run script.
11. Add a host analysis script that isolates active windows.
12. Add repeated-run statistics.
13. Record compiler versions and SHA-256 hashes.
14. Add a classic lane-stride shader if coalescing behavior is the intended
    research question.
15. Add a pointer-chasing shader if dependent memory latency is the intended
    research question.
16. Add Vulkan timestamp queries for runtime measurements.
17. Add command-line validation that `n` is a power of two.

---

# Quick-reference commands

## Compile verified memory shader

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  benchmarks/microbenchmarks/memory_stride/runner/mem.comp \
  -o benchmarks/microbenchmarks/memory_stride/runner/mem.comp.spv
```

## Compile legacy stride variants

```bash
for STRIDE in 1 4 16 64; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "-DSTRIDE=${STRIDE}u" \
    benchmarks/microbenchmarks/memory_stride/legacy_shaders/mem_stride_legacy.comp \
    -o \
    benchmarks/microbenchmarks/memory_stride/legacy_shaders/spv/mem_stride_${STRIDE}_legacy.comp.spv
done
```

## Push

```bash
adb push \
  benchmarks/microbenchmarks/memory_stride/runner/vk_mem_probe \
  benchmarks/microbenchmarks/memory_stride/legacy_shaders/spv/*_legacy.comp.spv \
  /data/local/tmp/jerry_work/memory_stride/
```

## Run verified memory workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem.comp.spv \
   1048576 \
   512 \
   128'
```

## Run legacy stride workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/memory_stride/vk_mem_probe \
   /data/local/tmp/jerry_work/memory_stride/mem_stride_64_legacy.comp.spv \
   1048576 \
   512 \
   128 \
   --no-verify'
```

## Primary candidate counters

```text
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
UCHE_RAM_READ_REQ
UCHE_BUSY_CYCLES
SP_BUSY_CYCLES
SP_ALU_WORKING_CYCLES
```
