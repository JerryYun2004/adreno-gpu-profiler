# Adreno ML Primitive Benchmarks

This package implements the benchmark code requested in the planning document: row-wise softmax variants with reduction-width sweeps, plus RMSNorm. The intended workflow is to run each Vulkan compute benchmark on the phone while the KGSL performance-counter streamer records counters.

## Included benchmarks

### Softmax variants

1. `softmax_three_pass.comp`
   - Stable row-wise softmax.
   - One workgroup per row.
   - Reads input three times and writes output once.
   - Approximate FP32 traffic model: `3 reads + 1 write = 16 bytes/element`.
   - Best first baseline because it is easy to reason about.

2. `softmax_fused_lmem.comp`
   - Loads each row into shared/local memory.
   - Reuses local memory for max, exp/sum, and output.
   - Approximate global traffic model: `1 read + 1 write = 8 bytes/element`.
   - Useful for seeing whether reduced global memory traffic is offset by local-memory and synchronization pressure.

3. `softmax_online.comp`
   - Online-softmax-style max/normalizer update.
   - Streams each lane's chunk as `(m, l)` pairs, reduces pairs, then writes final softmax.
   - Approximate global traffic model: `2 reads + 1 write = 12 bytes/element`.
   - Useful because online softmax is conceptually related to FlashAttention-style kernels.

### RMSNorm variant

4. `rmsnorm_basic.comp`
   - Row-wise RMSNorm: `y_i = x_i * weight_i * inversesqrt(mean(x^2) + epsilon)`.
   - One workgroup per row.
   - Approximate traffic model: read `x` for reduction, reread `x`, read `weight`, write `y`, roughly `16 bytes/element` for FP32.

## Directory layout

```text
shaders/ml_primitives/softmax/
    softmax_three_pass.comp
    softmax_fused_lmem.comp
    softmax_online.comp

shaders/ml_primitives/rmsnorm/
    rmsnorm_basic.comp

support_sw/vulkan_ml_primitive_bench/
    ml_primitive_bench.cpp
    CMakeLists.txt

scripts/
    build_shaders.sh
    build_android.sh
    push_to_phone.sh
    run_width_sweep_with_counters.sh
    disassemble_spirv.sh
    analyze_results.py
```

## Build shaders

Requires `glslc` from the Vulkan SDK or Android shader tools.

```bash
cd ml_primitive_benchmarks
./scripts/build_shaders.sh
```

Outputs:

```text
build/spv/softmax_three_pass.spv
build/spv/softmax_fused_lmem.spv
build/spv/softmax_online.spv
build/spv/rmsnorm_basic.spv
```

## Build Android benchmark binary

Set your Android NDK path first.

```bash
export ANDROID_NDK_HOME="$HOME/Library/Android/sdk/ndk/27.2.12479018"
export ANDROID_API=35
./scripts/build_android.sh
```

Output:

```text
build/android/ml_primitive_bench
```

## Push to phone

```bash
./scripts/push_to_phone.sh
```

Default phone location:

```text
/data/local/tmp/jerry_work/ml_primitives
```

## Run one benchmark manually

Example: three-pass softmax, width 256, fixed total elements 16M.

```bash
adb shell 'su -c "/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
  --op softmax \
  --variant three_pass \
  --spv /data/local/tmp/jerry_work/ml_primitives/spv/softmax_three_pass.spv \
  --width 256 \
  --elements 16777216 \
  --repeats 128 \
  --verify"'
```

Example: RMSNorm.

```bash
adb shell 'su -c "/data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
  --op rmsnorm \
  --variant basic \
  --spv /data/local/tmp/jerry_work/ml_primitives/spv/rmsnorm_basic.spv \
  --width 256 \
  --elements 16777216 \
  --repeats 128 \
  --verify"'
```

## Run width sweep with counters

The script uses the validated counter set from the planning document:

```text
SP_BUSY_CYCLES
SP_ALU_WORKING_CYCLES
UCHE_BUSY_CYCLES
UCHE_RAM_READ_REQ
UCHE_VBIF_READ_BEATS_SP
SP_UCHE_READ_TRANS
```

Run three-pass softmax sweep:

```bash
OP=softmax VARIANT=three_pass ./scripts/run_width_sweep_with_counters.sh
```

Run fused local-memory softmax sweep:

```bash
OP=softmax VARIANT=fused_lmem ./scripts/run_width_sweep_with_counters.sh
```

Run online softmax sweep:

```bash
OP=softmax VARIANT=online ./scripts/run_width_sweep_with_counters.sh
```

Run RMSNorm sweep:

```bash
OP=rmsnorm VARIANT=rmsnorm_basic ./scripts/run_width_sweep_with_counters.sh
```

Default sweep widths:

```text
16 32 64 128 256 512 1024 2048 4096 8192
```

Default total elements:

```text
16777216
```

The number of rows is computed as:

```text
rows = total_elements / width
```

This keeps total element count approximately fixed while varying the reduction width.

## Analyze results

```bash
python3 -m pip install pandas
python3 scripts/analyze_results.py
```

This writes:

```text
results/ml_primitive_width_sweep/summary.csv
```

## Disassemble SPIR-V

```bash
./scripts/disassemble_spirv.sh
```

This gives a portable SPIR-V disassembly. For driver ISA disassembly, keep using your existing Adreno/Turnip disassembly workflow and save the output beside these files.

## Suggested report questions

For each width, compare runtime and counters against the expected traffic and arithmetic pattern:

- Does `SP_ALU_WORKING_CYCLES / SP_BUSY_CYCLES` increase for softmax because of `exp()`?
- Does `UCHE_BUSY_CYCLES / SP_BUSY_CYCLES` increase when memory traffic dominates?
- Does fused local memory reduce external traffic?
- Does local memory introduce enough barriers/occupancy pressure to hurt large widths?
- Does online softmax reduce traffic but increase arithmetic dependency cost?
- Is RMSNorm less special-function-heavy than softmax?
