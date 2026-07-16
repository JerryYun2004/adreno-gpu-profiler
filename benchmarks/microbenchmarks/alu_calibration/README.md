# ALU Calibration Microbenchmarks

This directory contains controlled Vulkan compute workloads used to validate and
calibrate the Adreno hardware performance-counter collection path.

The calibration asks a simple question:

> If the amount of integer ALU work is increased by a known factor while input
> size and memory traffic remain approximately fixed, do ALU-related hardware
> counters increase by the expected factor?

This directory contains two generations of the workload:

1. `legacy_runner/` contains an older, self-contained Vulkan runner and a
   loop-based shader whose iteration count is supplied at runtime.
2. `shaders/` contains the later fixed, explicitly unrolled ALU-chain variants
   used for controlled 64/128/256/512-operation calibration.

These files are not compiled into the perf-counter streamer or sweeper. They are
test workloads run **alongside** those tools to verify that the counters behave
correctly.

---

# Directory layout

```text
alu_calibration/
├── legacy_runner/
│   ├── alu.comp
│   ├── alu.comp.spv
│   ├── main.cpp
│   └── vk_compute_probe
└── shaders/
    ├── alu_chain_64.comp
    ├── alu_chain_64.comp.spv
    ├── alu_chain_64.comp.spvasm
    ├── alu_chain_128.comp
    ├── alu_chain_128.comp.spv
    ├── alu_chain_128.comp.spvasm
    ├── alu_chain_256.comp
    ├── alu_chain_256.comp.spv
    ├── alu_chain_256.comp.spvasm
    ├── alu_chain_512.comp
    ├── alu_chain_512.comp.spv
    ├── alu_chain_512.comp.spvasm
    └── generate_alu_chain_variants.py
```

---

# Relationship to the main profiler tools

The main tools are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

The relationship is:

```text
ALU calibration shader + Vulkan runner
                 ↓
       controlled GPU workload
                 ↓
perf-counter streamer or sweeper records counters
                 ↓
compare counter totals across workload variants
```

The calibration files do **not**:

- provide counter definitions;
- perform KGSL perf-counter `ioctl` operations;
- replace the streamer or sweeper;
- become linked into either profiler binary; or
- need to be present on the phone after calibration is complete.

They do:

- generate repeatable ALU-heavy GPU activity;
- test whether `SP_ALU_WORKING_CYCLES` responds linearly;
- help distinguish a valid counter from a stuck, noisy, or incorrectly mapped
  counter;
- verify the end-to-end path from shader execution to counter CSV output; and
- provide a known baseline before profiling more complicated workloads such as
  softmax or RMSNorm.

## Connection to the streamer

The streamer records selected counters continuously while one calibration
variant runs.

This is the most direct calibration workflow:

```text
start streamer
run one ALU variant
stop streamer
save CSV
repeat for all variants
normalize and compare totals
```

## Connection to the sweeper

The sweeper can collect many counter groups and chunks while an external
benchmark command is executed.

The calibration shaders can therefore be used as the benchmark workload for a
sweep. However, this directory does not currently contain a wrapper script that
connects `vk_alu_varying_probe` to `streamer_sweeper`.

The sweeper does not read `.comp`, `.spv`, or `.spvasm` files itself. It runs a
Vulkan benchmark executable, and that executable loads the `.spv` shader.

---

# File reference

## `legacy_runner/alu.comp`

### Purpose

A loop-based GLSL compute shader that performs a configurable amount of integer
arithmetic per invocation.

The shader receives two push constants:

```glsl
uint n;
uint iters;
```

- `n` is the valid number of elements.
- `iters` controls the number of ALU-loop iterations.

Each invocation:

1. obtains `gl_GlobalInvocationID.x`;
2. exits when the invocation index is outside `n`;
3. reads one `uint` from the input storage buffer;
4. mixes the value with the invocation index;
5. executes a loop containing integer multiply, add, XOR, and shift operations;
6. writes one `uint` to the output storage buffer.

### Status

Legacy but functional.

This shader remains useful because it is paired with the self-contained
`legacy_runner/main.cpp` and supports runtime adjustment of ALU intensity.

It was superseded for precise calibration by the fixed unrolled chain shaders,
which make the generated instruction sequence easier to inspect.

### Why it exists

The runtime loop makes it convenient to quickly create longer or shorter
compute workloads without recompiling the shader.

It is useful for:

- checking that Vulkan compute runs correctly;
- producing a long ALU-heavy workload;
- testing counter collection duration;
- verifying runner and shader interfaces; and
- coarse ALU-versus-memory comparisons.

### Limitation

Loop-based shaders are less suitable for strict instruction-count calibration.

The driver compiler may:

- optimize the loop;
- unroll it partially or fully;
- schedule loop-control instructions;
- introduce branch overhead; or
- transform the arithmetic sequence.

Therefore, `iters=512` does not mean exactly 512 hardware ALU instructions.

---

## `legacy_runner/alu.comp.spv`

### Purpose

Compiled SPIR-V binary generated from `legacy_runner/alu.comp`.

This is the file loaded by Vulkan at runtime.

### Status

Generated runtime artifact.

The `.comp` file is the human-editable source of truth. Rebuild the `.spv` file
after changing the shader source.

### Use

The legacy runner defaults to:

```text
/data/local/tmp/jerry_work/alu.comp.spv
```

but a different path can be supplied as the first command-line argument.

---

## `legacy_runner/main.cpp`

### Purpose

A minimal Vulkan compute runner for `legacy_runner/alu.comp`.

It performs the complete host-side Vulkan workflow:

1. reads a SPIR-V file;
2. creates a Vulkan instance;
3. selects the first physical device;
4. selects a compute-capable queue family;
5. creates a logical device and queue;
6. allocates host-visible input and output storage buffers;
7. creates a shader module;
8. creates descriptor-set and pipeline layouts;
9. creates a compute pipeline;
10. uploads push constants;
11. records repeated dispatches into a command buffer;
12. submits the workload;
13. waits for completion;
14. checks the first 1024 results against a CPU reference; and
15. destroys all Vulkan resources.

### Command-line interface

```text
vk_compute_probe [spv_path] [elements] [alu_iters] [dispatch_repeats]
```

Defaults:

```text
spv_path:         /data/local/tmp/jerry_work/alu.comp.spv
elements:         262144
alu_iters:        512
dispatch_repeats: 64
```

### Important implementation details

The runner uses:

```cpp
uint32_t groups = (n + 255u) / 256u;
```

because the shader uses:

```glsl
layout(local_size_x = 256) in;
```

The shader has an `idx >= n` guard, so `n` does not need to be divisible by 256.

The runner uses two host-visible, host-coherent Vulkan storage buffers:

```text
binding 0 → input
binding 1 → output
```

The CPU reference implements the same arithmetic loop as the shader. This makes
the legacy pair self-checking.

### Status

Legacy runner.

It is tightly coupled to `legacy_runner/alu.comp`. It is not a correct
verification runner for the fixed `alu_chain_*.comp` shaders because those
shaders perform a different arithmetic sequence and do not use the `iters` push
constant.

The fixed-chain SPIR-V modules may still be accepted by the Vulkan pipeline
layout, but the legacy CPU verification would report a mismatch.

### Why it exists

This file demonstrates the complete Vulkan execution path in one source file
and is useful for debugging:

- shader loading;
- Vulkan initialization;
- queue selection;
- descriptor binding;
- push constants;
- command submission;
- storage-buffer access; and
- output correctness.

---

## `legacy_runner/vk_compute_probe`

### Purpose

Prebuilt Android executable compiled from `legacy_runner/main.cpp`.

### Status

Generated binary.

It should be rebuilt when:

- `main.cpp` changes;
- the Android API level changes;
- the NDK version changes;
- the target ABI changes; or
- reproducible builds are required.

Do not treat the binary as the source of truth.

### Expected target

The project uses an ARM64 Android phone, so this binary should be an:

```text
ELF 64-bit LSB executable, ARM aarch64
```

Check it with:

```bash
file benchmarks/microbenchmarks/alu_calibration/legacy_runner/vk_compute_probe
```

---

## `shaders/generate_alu_chain_variants.py`

### Purpose

Generates the four fixed, unrolled GLSL ALU-chain source files:

```text
alu_chain_64.comp
alu_chain_128.comp
alu_chain_256.comp
alu_chain_512.comp
```

The generator defines:

```python
variants = {
    64: 16,
    128: 32,
    256: 64,
    512: 128,
}
```

The first number is the intended count of high-level ALU update statements.
The second number is the number of `ALU_STEP` macro invocations.

Each `ALU_STEP` contains four high-level assignment statements.

### Constants

The generator uses a fixed list of pseudo-random 32-bit constants.

Fixed constants are useful because they:

- make generation deterministic;
- prevent every step from being textually identical;
- avoid adding runtime constant-buffer traffic; and
- keep every workload variant reproducible.

For the 512-operation shader, the generator needs 128 macro blocks. After the
first 64 constants, it perturbs repeated constants so the second half is not a
verbatim copy of the first half.

### Status

Current source generator.

Run it whenever the generated `.comp` variants need to be recreated.

### Important behavior

The script writes files into its **current working directory**:

```python
out_dir = Path(".")
```

Therefore, run it from the `shaders/` directory. Running it from the repository
root would generate the shader files in the wrong location.

---

## `shaders/alu_chain_64.comp`

### Purpose

Smallest fixed-chain calibration shader.

It contains:

```text
16 ALU_STEP blocks
64 high-level ALU update statements
112 relevant integer SPIR-V operations
```

Use it as the lowest-ALU-work baseline.

---

## `shaders/alu_chain_128.comp`

### Purpose

Second fixed-chain calibration shader.

It contains:

```text
32 ALU_STEP blocks
128 high-level ALU update statements
224 relevant integer SPIR-V operations
```

It should produce approximately twice the controlled ALU work of the 64 variant.

---

## `shaders/alu_chain_256.comp`

### Purpose

Reference or middle calibration shader.

It contains:

```text
64 ALU_STEP blocks
256 high-level ALU update statements
448 relevant integer SPIR-V operations
```

This variant is convenient as the normalization baseline:

```text
normalized total for 256 variant = 1.0
```

---

## `shaders/alu_chain_512.comp`

### Purpose

Largest fixed-chain calibration shader.

It contains:

```text
128 ALU_STEP blocks
512 high-level ALU update statements
896 relevant integer SPIR-V operations
```

It should produce approximately twice the controlled ALU work of the 256
variant and eight times that of the 64 variant.

---

## `shaders/alu_chain_*.comp.spv`

### Purpose

Compiled SPIR-V binaries loaded by the fixed-chain Vulkan runner.

These files are the runtime shader inputs pushed to the Android device.

### Status

Generated runtime artifacts.

Rebuild them after changing or regenerating the `.comp` sources.

### Validation

Use `spirv-val` to confirm that each module is structurally valid before pushing
it to the phone.

---

## `shaders/alu_chain_*.comp.spvasm`

### Purpose

Human-readable SPIR-V disassembly generated from each `.spv` module.

These files are not loaded by Vulkan.

They are retained for:

- verifying the compiled instruction structure;
- counting integer operations;
- checking descriptor bindings;
- confirming the local workgroup size;
- detecting unexpected loops or branches; and
- documenting what the GLSL compiler produced.

### Status

Generated inspection artifacts.

The `.comp` file remains the source of truth, and `.spv` remains the runtime
binary.

---

# Fixed-chain shader structure

Every fixed-chain shader has the same overall structure.

## 1. GLSL version

```glsl
#version 450
```

The shaders use Vulkan-compatible GLSL 4.50.

## 2. One-dimensional 256-thread workgroup

```glsl
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
```

A one-dimensional workgroup matches the one-dimensional input array.

The fixed workgroup size keeps this variable controlled across all four shader
variants.

A size of 256 was selected because:

- it is commonly supported for Vulkan compute;
- it provides many parallel invocations per workgroup;
- it matches the project’s runner dispatch logic; and
- it keeps occupancy and launch structure identical across variants.

The purpose is not to prove that 256 is optimal. The purpose is to hold
workgroup size constant while ALU-chain length changes.

## 3. Two storage buffers

```glsl
layout(set = 0, binding = 0, std430) readonly buffer InputBuffer {
    uint in_data[];
};

layout(set = 0, binding = 1, std430) writeonly buffer OutputBuffer {
    uint out_data[];
};
```

Every invocation performs:

```text
one input load
one output store
```

The storage-buffer structure is identical across all variants. This keeps the
main input/output memory traffic approximately constant while ALU work changes.

## 4. Dependent ALU macro

```glsl
#define ALU_STEP(x, c) \
    x = x + c;         \
    x = x ^ (x << 13); \
    x = x + (x >> 17); \
    x = x ^ (x << 5);
```

Every statement reads the result of the preceding statement.

This dependency chain is intentional.

It:

- prevents independent operations from being completely reordered;
- creates a long serial arithmetic dependency;
- makes the result depend on every ALU step;
- discourages dead-code elimination; and
- makes the effect of increasing chain length easier to observe in cycle-based
  counters.

The arithmetic uses unsigned 32-bit operations. Overflow wraps modulo `2^32`,
which is defined behavior for GLSL unsigned integers.

## 5. Global invocation index

```glsl
uint gid = gl_GlobalInvocationID.x;
```

Each invocation processes one element.

## 6. One input read

```glsl
uint x = in_data[gid];
```

All controlled ALU work operates on a local scalar after this load.

## 7. Explicitly unrolled chain

The source contains repeated macro calls rather than a runtime loop:

```glsl
ALU_STEP(x, 0x9e3779b9u)
ALU_STEP(x, 0x85ebca6bu)
...
```

Explicit unrolling was chosen to:

- remove dynamic loop-control overhead;
- make variant size visible in source;
- make SPIR-V instruction counting straightforward;
- avoid uncertainty about compiler loop unrolling;
- keep the operation sequence fixed; and
- produce predictable ratios between variants.

## 8. One output write

```glsl
out_data[gid] = x;
```

Writing the final value makes the full chain observable. Without this store, a
compiler could remove computations whose result is never used.

---

# What the variant names mean

The names:

```text
64
128
256
512
```

refer to the number of **high-level ALU update statements** generated by the
macro sequence.

They do not mean:

- exactly that many Adreno machine instructions;
- exactly that many shader-core cycles;
- exactly that many SPIR-V instructions; or
- exactly that many Vulkan operations.

Each macro block contains four high-level assignments, but several assignments
contain both a shift and an add/XOR.

One macro block disassembles to seven relevant integer SPIR-V operations:

```text
x = x + c             → OpIAdd
x = x ^ (x << 13)     → OpShiftLeftLogical + OpBitwiseXor
x = x + (x >> 17)     → OpShiftRightLogical + OpIAdd
x = x ^ (x << 5)      → OpShiftLeftLogical + OpBitwiseXor
```

Therefore:

| Variant | ALU_STEP blocks | High-level updates | Relevant SPIR-V integer operations |
|---:|---:|---:|---:|
| 64 | 16 | 64 | 112 |
| 128 | 32 | 128 | 224 |
| 256 | 64 | 256 | 448 |
| 512 | 128 | 512 | 896 |

The Adreno compiler may further translate, combine, or schedule these operations.
The SPIR-V counts are a controlled intermediate representation, not a final ISA
count.

---

# Why these files are needed

## 1. Validate counter identity

A counter believed to represent ALU work should respond to controlled ALU-chain
length.

For example, `SP_ALU_WORKING_CYCLES` should rise strongly as the chain changes
from 64 to 512.

A counter that remains zero or flat may be:

- incorrectly mapped;
- unsupported;
- not enabled correctly;
- unrelated to ALU work;
- sampled incorrectly; or
- affected by a collection bug.

## 2. Validate counter linearity

The controlled workload ratios are:

```text
64 : 128 : 256 : 512
 1 :   2 :   4 :   8
```

When normalized to the 256 variant, the ideal relationship is:

```text
64  → 0.25
128 → 0.50
256 → 1.00
512 → 2.00
```

The previously observed project calibration was approximately:

```text
64  → 0.253
128 → 0.500
256 → 1.000
512 → 1.996
```

This close agreement provided strong evidence that
`SP_ALU_WORKING_CYCLES` was being collected correctly for the controlled
workload.

## 3. Separate ALU scaling from memory traffic

All fixed-chain variants perform approximately the same:

- number of input loads;
- number of output stores;
- number of invocations;
- dispatch count; and
- buffer size.

Only the in-register arithmetic chain length changes.

This helps isolate ALU-related counter behavior from basic memory traffic.

## 4. Verify the complete profiler path

A successful calibration checks more than the shader.

It exercises:

```text
GLSL source
→ SPIR-V compiler
→ Vulkan runner
→ Android Vulkan driver
→ Adreno GPU
→ KGSL perf-counter access
→ streamer/sweeper
→ CSV output
→ host-side analysis
```

## 5. Establish confidence before complex benchmarks

Softmax, RMSNorm, attention, and other ML kernels mix:

- ALU operations;
- local/shared memory;
- global memory;
- reductions;
- synchronization; and
- cache behavior.

A simple calibration workload should be understood first so that unexpected
counter behavior in larger kernels is less likely to be caused by the profiler
itself.

---

# Expected results

## Primary expected result

For a fixed element count and dispatch repeat count:

```text
SP_ALU_WORKING_CYCLES
```

should increase approximately with chain length.

Expected normalized behavior:

```text
64  ≈ 0.25 × 256
128 ≈ 0.50 × 256
256 = 1.00 × 256
512 ≈ 2.00 × 256
```

## Other likely behavior

`SP_BUSY_CYCLES` should generally increase, but it may not scale as cleanly as
`SP_ALU_WORKING_CYCLES`.

Possible reasons include:

- fixed dispatch overhead;
- shader scheduling;
- occupancy;
- instruction issue width;
- clock/frequency changes;
- pipeline effects; and
- counter semantics.

Instruction-related SP counters may scale strongly when they correspond to the
generated integer operations.

Basic input/output memory counters should remain much flatter because every
variant performs one input read and one output write per invocation.

## Results that should trigger investigation

- All variants produce nearly identical `SP_ALU_WORKING_CYCLES`.
- The 64 variant produces more ALU cycles than the 512 variant.
- Every SP counter is zero.
- Counter totals vary dramatically between repeated identical runs.
- The benchmark verification fails.
- The detected workload window excludes part of the dispatch.
- Memory-transaction totals rise by 8× even though buffer traffic is fixed.
- The shader runner reads beyond the buffer boundary.

---

# Important runner compatibility

## Legacy shader

Use:

```text
legacy_runner/main.cpp
legacy_runner/vk_compute_probe
```

with:

```text
legacy_runner/alu.comp.spv
```

This pair shares:

- push-constant layout;
- descriptor bindings;
- workgroup size; and
- CPU verification algorithm.

## Fixed-chain shaders

The fixed-chain shaders were used with a separate runner referred to in the
project as:

```text
vk_alu_varying_probe
```

That runner is not currently stored in this directory.

The directory is therefore **not fully self-contained** for rebuilding and
running the fixed-chain calibration.

A complete long-term layout should include:

```text
alu_calibration/
├── runner/
│   ├── vk_alu_varying_probe.cpp
│   ├── CMakeLists.txt or Makefile
│   └── build_and_push.sh
└── shaders/
    └── ...
```

Until that source is added, the fixed-chain shaders require an existing
`vk_alu_varying_probe` binary or another compatible Vulkan runner.

## Requirements for a compatible fixed-chain runner

The runner must:

- load a user-selected SPIR-V file;
- bind an input storage buffer at set 0, binding 0;
- bind an output storage buffer at set 0, binding 1;
- dispatch one invocation per element;
- use a workgroup size consistent with 256 shader invocations;
- use an element count divisible by 256, unless the shader gains a bounds check;
- repeat dispatches enough times to create a measurable interval; and
- either implement the correct chain verification or provide a controlled
  no-verification mode.

The current fixed-chain shaders do not include an `n` bounds check.

Therefore:

```text
element_count % 256 must equal 0
```

for a standard ceiling-divided dispatch. Otherwise, the final workgroup can
access beyond the storage buffers.

---

# Toolchain requirements

## Required host tools

### Python 3

Used to generate the fixed-chain GLSL files.

Check:

```bash
python3 --version
```

### `glslangValidator`

Compiles Vulkan GLSL `.comp` source into `.spv`.

Check:

```bash
glslangValidator --version
```

### SPIR-V Tools

Recommended tools:

```text
spirv-val
spirv-dis
```

- `spirv-val` validates the binary module.
- `spirv-dis` generates the `.spvasm` inspection file.

Check:

```bash
spirv-val --version
spirv-dis --version
```

### Android NDK

Required to rebuild the Android ARM64 Vulkan runner.

The project previously used Android NDK r27d, but another compatible NDK may
work.

Set:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

### Android Debug Bridge

Used to push and run binaries and shaders on the phone.

Check:

```bash
adb version
adb devices
```

### Vulkan support on the device

The Android device must expose a Vulkan compute queue.

The existing runner uses Vulkan 1.1 API declarations.

## Required profiler-side setup

To collect hardware counters, the phone must have the permissions required by
the project’s KGSL perf-counter tools.

The existing rooted-device setup uses:

```bash
adb shell 'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

The exact sysfs path and permission requirement are device-specific.

The streamer or sweeper binaries must already be built and pushed separately.

---

# Build workflow

Run the following commands from the repository root unless a section says
otherwise.

Set a convenience variable:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

ALU_DIR="benchmarks/microbenchmarks/alu_calibration"
```

---

## 1. Regenerate the fixed-chain GLSL sources

The generator writes to the current directory, so enter `shaders/` first:

```bash
cd "$ALU_DIR/shaders"

python3 generate_alu_chain_variants.py

cd -
```

Expected output:

```text
wrote alu_chain_64.comp with 16 ALU_STEP blocks = 64 high-level ALU ops
wrote alu_chain_128.comp with 32 ALU_STEP blocks = 128 high-level ALU ops
wrote alu_chain_256.comp with 64 ALU_STEP blocks = 256 high-level ALU ops
wrote alu_chain_512.comp with 128 ALU_STEP blocks = 512 high-level ALU ops
```

Review generated changes:

```bash
git diff -- "$ALU_DIR/shaders/"'*.comp'
```

---

## 2. Compile the fixed-chain shaders

```bash
cd "$ALU_DIR/shaders"

for OPS in 64 128 256 512; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp" \
    -o "alu_chain_${OPS}.comp.spv"
done

cd -
```

---

## 3. Validate the SPIR-V modules

```bash
cd "$ALU_DIR/shaders"

for OPS in 64 128 256 512; do
  spirv-val \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp.spv"
done

cd -
```

Successful validation normally prints no error.

---

## 4. Regenerate the SPIR-V disassembly

```bash
cd "$ALU_DIR/shaders"

for OPS in 64 128 256 512; do
  spirv-dis \
    "alu_chain_${OPS}.comp.spv" \
    -o "alu_chain_${OPS}.comp.spvasm"
done

cd -
```

---

## 5. Verify the relevant SPIR-V operation counts

```bash
cd "$ALU_DIR/shaders"

for OPS in 64 128 256 512; do
  COUNT="$(
    grep -Ec \
      'Op(IAdd|ShiftLeftLogical|ShiftRightLogical|BitwiseXor)' \
      "alu_chain_${OPS}.comp.spvasm"
  )"

  printf "%-24s %s\n" "alu_chain_${OPS}" "$COUNT"
done

cd -
```

Expected counts:

```text
alu_chain_64             112
alu_chain_128            224
alu_chain_256            448
alu_chain_512            896
```

Confirm that there is no runtime loop in the fixed-chain SPIR-V:

```bash
grep -nE 'OpLoopMerge|OpBranchConditional' \
  "$ALU_DIR/shaders"/alu_chain_*.comp.spvasm
```

The fixed-chain arithmetic should not require an `OpLoopMerge`.

---

## 6. Compile the legacy shader

```bash
glslangValidator \
  -V \
  --target-env vulkan1.1 \
  "$ALU_DIR/legacy_runner/alu.comp" \
  -o "$ALU_DIR/legacy_runner/alu.comp.spv"
```

Validate it:

```bash
spirv-val \
  --target-env vulkan1.1 \
  "$ALU_DIR/legacy_runner/alu.comp.spv"
```

---

## 7. Build the legacy Android runner

Set the NDK:

```bash
export ANDROID_NDK_HOME="/path/to/android-ndk-r27d"
```

On macOS, the NDK toolchain directory is normally:

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
  "$ALU_DIR/legacy_runner/main.cpp" \
  -o "$ALU_DIR/legacy_runner/vk_compute_probe" \
  -lvulkan
```

Check the result:

```bash
file "$ALU_DIR/legacy_runner/vk_compute_probe"
```

Expected architecture:

```text
ARM aarch64
```

If the NDK is installed under another host tag, locate the compiler with:

```bash
find "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt" \
  -type f \
  -name 'aarch64-linux-android35-clang++'
```

---

# Push workflow

## Create a device directory

```bash
adb shell 'mkdir -p /data/local/tmp/jerry_work/alu_calibration'
```

## Push the legacy runner and shader

```bash
adb push \
  "$ALU_DIR/legacy_runner/vk_compute_probe" \
  /data/local/tmp/jerry_work/alu_calibration/

adb push \
  "$ALU_DIR/legacy_runner/alu.comp.spv" \
  /data/local/tmp/jerry_work/alu_calibration/
```

## Push all fixed-chain shaders

```bash
adb push \
  "$ALU_DIR/shaders/alu_chain_64.comp.spv" \
  "$ALU_DIR/shaders/alu_chain_128.comp.spv" \
  "$ALU_DIR/shaders/alu_chain_256.comp.spv" \
  "$ALU_DIR/shaders/alu_chain_512.comp.spv" \
  /data/local/tmp/jerry_work/alu_calibration/
```

## Set executable permission

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/alu_calibration/vk_compute_probe'
```

If the fixed-chain runner is stored elsewhere:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe'
```

## Confirm files

```bash
adb shell \
  'ls -lh /data/local/tmp/jerry_work/alu_calibration'
```

---

# Run workflow

## Run the legacy probe

```bash
adb shell \
  '/data/local/tmp/jerry_work/alu_calibration/vk_compute_probe \
   /data/local/tmp/jerry_work/alu_calibration/alu.comp.spv \
   262144 \
   512 \
   64'
```

Arguments:

```text
262144 → elements
512    → loop iterations per invocation
64     → repeated dispatches
```

Expected output should include:

```text
Workload complete.
Verification PASSED
```

## Run the fixed-chain variants

The following commands assume the existing project binary is available at:

```text
/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe
```

Run one variant:

```bash
adb shell \
  '/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe \
   /data/local/tmp/jerry_work/alu_calibration/alu_chain_64.comp.spv'
```

Run all four variants:

```bash
for OPS in 64 128 256 512; do
  echo "=== ALU chain ${OPS} ==="

  adb shell \
    "/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe \
     /data/local/tmp/jerry_work/alu_calibration/alu_chain_${OPS}.comp.spv"
done
```

The exact CLI belongs to `vk_alu_varying_probe`, whose source is not currently
in this directory. Check the runner’s source or known invocation before changing
arguments.

The current binary has historically used:

```text
elements = 262144
dispatch_repeats = 64
```

These settings keep the element count and dispatch count fixed across all four
variants.

---

# Collect with the perf-counter streamer

The cleanest workflow uses two terminals.

## Terminal 1: start counter collection

From the repository root:

```bash
mkdir -p results/calibration/alu_chain/raw_streams
```

For the 64 variant:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n SP_ALU_WORKING_CYCLES \
    --csv"' \
  | tee results/calibration/alu_chain/raw_streams/alu_chain_64_perf_stream.csv
```

Keep this running while the workload executes. Stop it with `Ctrl+C` after the
benchmark finishes.

The exact binary path may instead be:

```text
/data/local/tmp/jerry_work/adreno_perf_stream
```

Confirm it with:

```bash
adb shell \
  'ls -l /data/local/tmp/adreno_perf_stream \
         /data/local/tmp/jerry_work/adreno_perf_stream 2>/dev/null'
```

Also confirm the current CLI:

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream --help"'
```

## Terminal 2: run the workload

```bash
adb shell \
  '/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe \
   /data/local/tmp/jerry_work/alu_calibration/alu_chain_64.comp.spv'
```

Repeat the workflow for 128, 256, and 512, changing the output filename and
shader path.

## Suggested counters

Primary:

```text
SP_ALU_WORKING_CYCLES
```

Useful supporting counters, when available:

```text
SP_BUSY_CYCLES
SP_FULL_ALU_INSTRUCTIONS
SP_FULL_ALU_MUL_INSTRUCTIONS
SP_EFU_WORKING_CYCLES
```

Counter names vary by generation and table. Use the streamer’s list or fuzzy
search rather than assuming every name exists.

---

# Collect with the perf-counter sweeper

The sweeper collects groups/chunks and invokes a benchmark command repeatedly.

The ALU calibration can be connected to it by supplying a benchmark command that
runs one fixed-chain SPIR-V module.

Conceptually:

```text
streamer_sweeper
    benchmark command:
        vk_alu_varying_probe alu_chain_256.comp.spv
```

However, the exact sweeper CLI and wrapper script are maintained in:

```text
tools/profiling/perfcounter_sweeper/
```

Check:

```bash
sed -n '1,240p' \
  tools/profiling/perfcounter_sweeper/README.md
```

This calibration directory currently has no dedicated:

```text
run_alu_calibration_sweep.sh
```

A reproducible sweeper wrapper should record:

- shader variant;
- element count;
- dispatch repeats;
- sampling interval;
- counter group/chunk;
- device;
- driver;
- frequency/thermal state; and
- output directory.

Do not run different shader variants within one sweeper result unless the
benchmark log and timing boundaries clearly identify each variant.

---

# Analyze the collected results

## Sum one streamer counter column

Example:

```bash
python3 - <<'PY'
import pandas as pd

path = (
    "results/calibration/alu_chain/raw_streams/"
    "alu_chain_64_perf_stream.csv"
)

df = pd.read_csv(path)

counter = "SP_ALU_WORKING_CYCLES"
print(df[counter].sum())
PY
```

## Compare all variants

```bash
python3 - <<'PY'
from pathlib import Path
import pandas as pd

root = Path("results/calibration/alu_chain/raw_streams")
counter = "SP_ALU_WORKING_CYCLES"

totals = {}

for ops in [64, 128, 256, 512]:
    path = root / f"alu_chain_{ops}_perf_stream.csv"
    df = pd.read_csv(path)
    totals[ops] = float(df[counter].sum())

reference = totals[256]

print(f"{'variant':>8} {'total':>18} {'normalized_to_256':>20}")
for ops in [64, 128, 256, 512]:
    norm = totals[ops] / reference if reference else float("nan")
    print(f"{ops:8d} {totals[ops]:18.0f} {norm:20.3f}")
PY
```

Expected normalized values should be close to:

```text
64   0.25
128  0.50
256  1.00
512  2.00
```

## Important analysis requirement

Do not blindly sum a long capture containing extensive idle time or unrelated
GPU activity.

Prefer:

- controlled start/stop timing;
- workload-window detection;
- subtraction of idle baseline where appropriate;
- repeated runs;
- identical sampling settings; and
- normalization to one reference variant.

---

# Reproducible end-to-end example

## Host terminal 1: build shaders

```bash
cd /Users/jerryyun/adreno-gpu-profiler

ALU_DIR="benchmarks/microbenchmarks/alu_calibration"

cd "$ALU_DIR/shaders"

python3 generate_alu_chain_variants.py

for OPS in 64 128 256 512; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp" \
    -o "alu_chain_${OPS}.comp.spv"

  spirv-val \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp.spv"

  spirv-dis \
    "alu_chain_${OPS}.comp.spv" \
    -o "alu_chain_${OPS}.comp.spvasm"
done

cd -
```

## Host terminal 1: push shaders

```bash
adb shell 'mkdir -p /data/local/tmp/jerry_work/alu_calibration'

adb push \
  "$ALU_DIR/shaders/"alu_chain_*.comp.spv \
  /data/local/tmp/jerry_work/alu_calibration/
```

## Host terminal 1: enable counters

```bash
adb shell \
  'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'
```

## Host terminal 1: start streamer

```bash
mkdir -p results/calibration/alu_chain/raw_streams

adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n SP_ALU_WORKING_CYCLES \
    --csv"' \
  | tee results/calibration/alu_chain/raw_streams/alu_chain_256_perf_stream.csv
```

## Host terminal 2: run shader

```bash
adb shell \
  '/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe \
   /data/local/tmp/jerry_work/alu_calibration/alu_chain_256.comp.spv'
```

## Host terminal 1: stop collection

Press:

```text
Ctrl+C
```

Repeat for all four variants.

---

# Key mechanisms and libraries

## GLSL compute shaders

The `.comp` files use Vulkan GLSL compute-shader features:

- `gl_GlobalInvocationID`
- fixed local workgroup size
- storage buffers
- `std430` layout
- unsigned 32-bit arithmetic
- macros
- shifts, XOR, and addition

## SPIR-V

SPIR-V is the intermediate binary consumed by Vulkan.

This directory uses:

```text
.comp     → source
.comp.spv → Vulkan runtime binary
.spvasm   → human-readable disassembly
```

## Vulkan C API

The legacy runner uses raw Vulkan C API objects, including:

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

## Storage-buffer descriptors

Both workload generations use:

```text
descriptor set 0
binding 0: readonly input storage buffer
binding 1: writeonly output storage buffer
```

## Push constants

Only the legacy shader uses push constants:

```text
n
iters
```

The fixed-chain shaders do not use push constants.

## Host-visible coherent memory

The legacy runner requests:

```text
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
```

This simplifies input initialization and output verification but does not
necessarily represent the highest-performance device-local buffer strategy.

For calibration, simplicity and determinism are more important than maximizing
buffer-transfer performance.

## Dependent integer arithmetic

The fixed-chain workload uses serial dependencies to create controlled arithmetic
pressure.

This differs from an instruction-throughput benchmark containing many
independent operations. It is better described as a dependent ALU-chain
calibration.

## Counter collection

The profiler side uses the project’s KGSL perf-counter access path to read Adreno
counter deltas while the Vulkan workload runs.

---

# Known limitations

## Fixed-chain runner source is missing here

The largest structural limitation is that `vk_alu_varying_probe` is not included
in this directory.

The fixed-chain calibration cannot currently be rebuilt from this directory
alone.

## No build-and-push script

The directory contains no:

```text
build.sh
build_android.sh
build_and_push.sh
run_calibration.sh
```

The manual commands in this README provide the current workflow.

## No bounds check in fixed-chain shaders

The fixed-chain shaders directly access:

```glsl
in_data[gid]
out_data[gid]
```

They do not check `gid < n`.

Use an element count divisible by 256 or add a push constant and bounds check.

## Variant name is not exact machine instruction count

The 64/128/256/512 labels count high-level update statements, not final Adreno
instructions.

## Compiler behavior can still vary

Explicit unrolling removes loop uncertainty, but the driver compiler can still:

- combine operations;
- choose different instruction forms;
- schedule operations differently;
- spill registers;
- alter occupancy; or
- produce generation-specific code.

## Memory is not completely absent

Each invocation still performs one input load and one output store.

The benchmark minimizes changing memory traffic; it is not a zero-memory
benchmark.

## No timing query

The legacy runner waits for completion but does not use Vulkan timestamp queries
to report precise GPU execution time.

## First-device selection

The legacy runner chooses the first enumerated physical device. This is
sufficient on the phone but is not a robust multi-GPU selection policy.

## Partial verification

The legacy runner verifies only the first 1024 elements.

## Host-visible buffers

Host-visible coherent buffers may have different placement and caching behavior
across drivers.

## Thermal and frequency effects

Long repeated dispatches can change GPU frequency and temperature. Calibration
runs should be repeated under comparable conditions.

---

# Recommended maintenance

1. Add the `vk_alu_varying_probe` source to this directory.
2. Add a `CMakeLists.txt` or Makefile for the fixed-chain runner.
3. Add `build_shaders.sh`.
4. Add `build_runner_android.sh`.
5. Add `push_to_device.sh`.
6. Add `run_streamer_calibration.sh`.
7. Add a host-side script that automatically calculates normalized totals.
8. Add a bounds check to fixed-chain shaders, or enforce divisible element
   counts in the runner.
9. Record compiler versions and SPIR-V hashes with every calibration.
10. Add a small metadata file describing the validated counter results.
11. Consider moving generated Android binaries to a release or build-output
    directory instead of tracking them beside source.
12. Keep `.comp.spvasm` files when instruction-audit reproducibility is
    important.
13. Add a no-optimization and optimized compilation comparison if compiler
    transformations become a concern.
14. Run several repetitions and report mean and standard deviation rather than
    relying on one capture.

---

# Quick-reference commands

## Generate

```bash
cd benchmarks/microbenchmarks/alu_calibration/shaders
python3 generate_alu_chain_variants.py
```

## Compile

```bash
for OPS in 64 128 256 512; do
  glslangValidator \
    -V \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp" \
    -o "alu_chain_${OPS}.comp.spv"
done
```

## Validate

```bash
for OPS in 64 128 256 512; do
  spirv-val \
    --target-env vulkan1.1 \
    "alu_chain_${OPS}.comp.spv"
done
```

## Disassemble

```bash
for OPS in 64 128 256 512; do
  spirv-dis \
    "alu_chain_${OPS}.comp.spv" \
    -o "alu_chain_${OPS}.comp.spvasm"
done
```

## Push

```bash
adb push \
  alu_chain_*.comp.spv \
  /data/local/tmp/jerry_work/alu_calibration/
```

## Run

```bash
adb shell \
  '/data/local/tmp/jerry_work/02_probe_binaries/vk_alu_varying_probe \
   /data/local/tmp/jerry_work/alu_calibration/alu_chain_256.comp.spv'
```

## Primary calibration counter

```text
SP_ALU_WORKING_CYCLES
```

## Expected normalized relationship

```text
64 : 128 : 256 : 512
0.25 : 0.50 : 1.00 : 2.00
```
