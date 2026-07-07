# Benchmark notes

## Why these variants match the project plan

The planning document asks for softmax with varying reduction widths, fixed total data size, KGSL counter collection, runtime logging, and disassembly. It also recommends RMSNorm as a second primitive after the softmax baseline works.

This package implements:

- width sweep: `16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192`
- fixed default total elements: `16,777,216` (2^24)
- validated counter set: `SP_BUSY_CYCLES`, `SP_ALU_WORKING_CYCLES`, `UCHE_BUSY_CYCLES`, `UCHE_RAM_READ_REQ`, `UCHE_VBIF_READ_BEATS_SP`, `SP_UCHE_READ_TRANS`
- runtime fields: width, rows, elements, repeats, elapsed time, elements/s, approximate effective bandwidth, verification status
- shader variants: three-pass softmax, fused local-memory softmax, online softmax, RMSNorm

## Interpreting the traffic estimates

These are approximate global-memory traffic models, not exact hardware measurements.

| Benchmark | Approximate global traffic per FP32 element | Why |
|---|---:|---|
| three-pass softmax | 16 B | read max + read sum + read normalize + write output |
| fused local-memory softmax | 8 B | read input once + write output once |
| online softmax | 12 B | read online pass + read output pass + write output |
| RMSNorm basic | 16 B | read sumsq + read input + read weight + write output |

Use these estimates only for first-order interpretation. The actual Adreno behavior can differ because of cache effects, compiler transformations, local memory behavior, and memory coalescing.

## Suggested first experiment

Start with only the controlled baseline:

```bash
OP=softmax VARIANT=three_pass ./scripts/run_width_sweep_with_counters.sh
python3 scripts/analyze_results.py
```

Then compare:

```text
runtime vs width
SP_ALU_WORKING_CYCLES / SP_BUSY_CYCLES vs width
UCHE_BUSY_CYCLES / SP_BUSY_CYCLES vs width
SP_UCHE_READ_TRANS / element_count vs width
UCHE_VBIF_READ_BEATS_SP / element_count vs width
```

After the three-pass result is stable, run fused local-memory softmax and online softmax.
