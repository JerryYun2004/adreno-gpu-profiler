# Daily Log — KGSL Busy Validation Against Sysfs Nodes

## What I worked on

Today I worked on validating the GPU busy values reported by my KGSL tracepoint/systrace profiler against the busy-related sysfs nodes exposed by KGSL on the Android phone.

The goal was to check whether the GPU busy percentage derived from KGSL tracepoints is consistent with the values exposed under:

```
/sys/class/kgsl/kgsl-3d0/
```

especially:

```
/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
/sys/class/kgsl/kgsl-3d0/gpubusy
/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load
```

This was intended as a sanity check for the correctness of the systrace/KGSL tracepoint profiler.

---

## Tasks completed

### 1. Found the missing trace parser

I first tried to locate `parse_focused_kgsl_trace.py` on CloudLab and on the phone, but it was not there. Eventually I found it on my MacBook under:

```
/Users/jerryyun/adreno_turnip/parse_focused_kgsl_trace.py
```

This parser is important because it defines how the existing tracepoint profiler computes:

```
kgsl_pwrstats busy_pct = busy / total * 100
kgsl_gpubusy busy_pct  = busy / elapsed * 100
ram_wait_pct           = ram_wait / ram_time * 100
```

The parser also extracts command-batch timing, frequency, bus-level events, context counts, and memory events.

---

### 2. Located the relevant KGSL sysfs nodes

I checked the KGSL sysfs directory on the phone. Initially, `find` did not show the contents because `/sys/class/kgsl/kgsl-3d0` is a symlink, so I reran the command with `find -L`.

The important exposed nodes found were:

```
/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
/sys/class/kgsl/kgsl-3d0/gpubusy
/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load
/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
/sys/class/kgsl/kgsl-3d0/gpuclk
/sys/class/kgsl/kgsl-3d0/gpu_clock_stats
```

The phone showed that `gpu_busy_percentage` outputs a percentage-like value such as:

```
5 %
```

and `gpubusy` outputs a busy/elapsed pair such as:

```
0       0
```

at idle.

---

### 3. Wrote a validation runner

I created a new host-side validation script:

```
tools/run_kgsl_busy_validation.sh
```

This script does the following:

```
1. Creates a device-side KGSL tracepoint setup script
2. Creates a device-side sysfs sampler
3. Enables KGSL tracepoints
4. Starts sysfs sampling
5. Starts KGSL tracing
6. Runs the Vulkan workload
7. Stops tracing and sampling
8. Pulls the raw trace, filtered trace, and sysfs CSV
9. Runs the analyzer
```

The tracepoints enabled are based on the previous focused KGSL scripts, including:

```
kgsl_pwrstats
kgsl_gpubusy
kgsl_pwrlevel
kgsl_buslevel
gpu_frequency
adreno_cmdbatch_queued
adreno_cmdbatch_submitted
adreno_cmdbatch_retired
adreno_cmdbatch_done
```

These were the same core tracepoints used in the earlier KGSL tracepoint profiler.

---

### 4. Wrote a validation analyzer

I also created:

```
tools/analyze_kgsl_busy_validation.py
```

The analyzer compares:

```
trace kgsl_pwrstats busy%
trace kgsl_gpubusy busy%
sysfs gpu_busy_percentage
sysfs gpubusy busy%
sysfs devfreq/gpu_load
```

It estimates the workload window from the Vulkan command-batch context, filters samples to that window, aligns nearby trace and sysfs samples, and reports:

```
average busy values
absolute error
relative error
correlation
sample counts
aligned CSV output
```

---

### 5. Fixed a shell quoting bug

The first run failed because the host shell expanded `$STOP` too early while generating the device-side sampler script:

```
STOP: unbound variable
```

I fixed this by generating the sampler script locally with a quoted heredoc, pushing it to the phone with `adb push`, and then running it on the device.

---

### 6. Ran a short compute validation test

I first ran:

```
OUT_ROOT=/Users/jerryyun/adreno_turnip/kgsl_busy_validation \
./tools/run_kgsl_busy_validation.sh compute
```

The workload completed correctly, but the GPU-active window was only about `0.22 s`. There were only:

```
trace kgsl_pwrstats samples: 3
sysfs samples: 2
```

The result showed that `gpu_busy_percentage` stayed at `0`, while trace `kgsl_pwrstats` showed around `67%` busy. This suggested that the workload was too short for the sysfs `gpu_busy_percentage` node to update meaningfully.

---

### 7. Added repeated workload support

To get a longer measurement window, I patched the runner to support:

```
WORKLOAD_REPEAT
POST_SAMPLE_S
```

Then I ran a longer compute validation:

```
OUT_ROOT=/Users/jerryyun/adreno_turnip/kgsl_busy_validation \
SAMPLE_INTERVAL_S=0.02 \
WORKLOAD_REPEAT=10 \
POST_SAMPLE_S=1.0 \
./tools/run_kgsl_busy_validation.sh compute
```

This ran `vk_compute_probe` 10 times. Each iteration passed verification.

The longer run produced a much better validation window:

```
duration_s:                  2.561693
trace kgsl_pwrstats samples: 42
trace kgsl_gpubusy samples:  2
sysfs samples:               34
```

The key results were:

```
trace pwrstats avg busy%:       61.22
trace gpubusy avg busy%:        64.47
sysfs gpu_busy_percentage avg:  46.76
sysfs gpubusy avg busy%:        64.09
sysfs devfreq gpu_load avg:     60.38
corr, pwrstats vs gpu_load:     0.9629
```

This was the most important result of the day.

---

## Main discoveries

### 1. The validation pipeline works

The new validation runner successfully collects both:

```
KGSL tracepoint data
sysfs KGSL busy/load samples
```

during the same workload window.

It also produces:

```
kgsl_trace_raw.log
kgsl_trace_filtered.log
kgsl_sysfs_busy_samples.csv
validation_summary.txt
aligned_trace_vs_sysfs.csv
```

---

### 2. `sysfs gpubusy` agrees very closely with trace `kgsl_gpubusy`

The strongest validation result was:

```
trace kgsl_gpubusy avg busy%: 64.47
sysfs gpubusy avg busy%:     64.09
```

This is a very close match. It suggests that the `gpubusy` sysfs node and the `kgsl_gpubusy` tracepoint are measuring a very similar busy/elapsed concept.

This is currently the best direct sanity check for the tracepoint profiler.

---

### 3. `devfreq/gpu_load` tracks `kgsl_pwrstats` very well

The correlation between trace `kgsl_pwrstats` busy% and sysfs `devfreq/gpu_load` was:

```
0.9629
```

This is a strong correlation. It suggests that `devfreq/gpu_load` is a useful sysfs-side signal for validating the trend of `kgsl_pwrstats`.

---

### 4. `gpu_busy_percentage` is less reliable for this validation

The node:

```
/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
```

reported a lower average:

```
46.76%
```

while trace `kgsl_pwrstats` reported:

```
61.22%
```

This does not necessarily mean the tracepoint profiler is wrong. More likely, `gpu_busy_percentage` uses a different update window, smoothing method, or cached governor-facing statistic.

Therefore, I should treat `gpu_busy_percentage` carefully and avoid using it as the only validation reference.

---

### 5. Different KGSL busy sources are not equivalent

The current interpretation is:

```
trace kgsl_gpubusy ↔ sysfs gpubusy
```

is the most apples-to-apples comparison.

```
trace kgsl_pwrstats ↔ sysfs devfreq/gpu_load
```

is a good trend/correlation comparison.

```
trace kgsl_pwrstats ↔ sysfs gpu_busy_percentage
```

may show larger mismatch because the sampling/counting method is probably different.

---

## How to reproduce

### 1. Run from the MacBook, not CloudLab

The phone is connected to the MacBook through ADB, so the validation script should be run from:

```
/Users/jerryyun/adreno_turnip
```

not from CloudLab.

Check ADB:

```
which adb
adb devices
```

If needed, add platform-tools to PATH:

```
exportPATH="$PATH:/Users/jerryyun/platform-tools"
```

---

### 2. Confirm KGSL sysfs nodes

```
adb shell'su -c "find -L /sys/class/kgsl/kgsl-3d0 -maxdepth 2 -type f | grep -Ei \"busy|pwr|freq|clock|stat|counter|devfreq|gpu\" | sort"'
```

Expected important nodes:

```
/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
/sys/class/kgsl/kgsl-3d0/gpubusy
/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load
/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
/sys/class/kgsl/kgsl-3d0/gpuclk
```

---

### 3. Run compute validation

```
cd /Users/jerryyun/adreno_turnip

OUT_ROOT=/Users/jerryyun/adreno_turnip/kgsl_busy_validation \
SAMPLE_INTERVAL_S=0.02 \
WORKLOAD_REPEAT=10 \
POST_SAMPLE_S=1.0 \
./tools/run_kgsl_busy_validation.sh compute
```

This runs:

```
/data/local/tmp/jerry_work/vk_compute_probe
```

which uses:

```
/data/local/tmp/jerry_work/alu.comp.spv
```

---

### 4. Run memory validation

```
cd /Users/jerryyun/adreno_turnip

OUT_ROOT=/Users/jerryyun/adreno_turnip/kgsl_busy_validation \
SAMPLE_INTERVAL_S=0.02 \
WORKLOAD_REPEAT=10 \
POST_SAMPLE_S=1.0 \
./tools/run_kgsl_busy_validation.sh mem
```

This runs:

```
/data/local/tmp/jerry_work/vk_mem_probe mem.comp.spv
```

---

### 5. Inspect the output

Each run creates a timestamped directory such as:

```
/Users/jerryyun/adreno_turnip/kgsl_busy_validation/20260601_164450_compute/
```

Important files:

```
kgsl_trace_raw.log
kgsl_trace_filtered.log
kgsl_sysfs_busy_samples.csv
validation_summary.txt
aligned_trace_vs_sysfs.csv
workload_stdout.log
```

To inspect the latest result:

```
cd /Users/jerryyun/adreno_turnip/kgsl_busy_validation
LATEST=$(ls -td */ | head -1)
cat"$LATEST/validation_summary.txt"
```

To inspect raw sysfs samples:

```
column-s,-t"$LATEST/kgsl_sysfs_busy_samples.csv" | head-30
column-s,-t"$LATEST/kgsl_sysfs_busy_samples.csv" | tail-30
```

---

## Suggested next steps

### 1. Run the same validation on memory workload

The compute workload already showed good agreement between trace `kgsl_gpubusy` and sysfs `gpubusy`. The next check is whether the memory-heavy workload shows similar agreement.

### 2. Compare vendor driver vs Mesa Turnip

Run the same compute and memory validations under both drivers.

Vendor:

```
adb shell'su -c "umount /vendor/lib64/hw/vulkan.adreno.so" 2>/dev/null || true'
adb shell'cd /data/local/tmp/jerry_work && ./vk_probe | grep -E "driverName|driverID"'
```

Turnip:

```
adb shell'su -c "mount --bind /data/local/tmp/jerry_work/turnip/vulkan.adreno.so /vendor/lib64/hw/vulkan.adreno.so"'
adb shell'cd /data/local/tmp/jerry_work && ./vk_probe | grep -E "driverName|driverID"'
```

Then run:

```
SAMPLE_INTERVAL_S=0.02WORKLOAD_REPEAT=10POST_SAMPLE_S=1.0 ./tools/run_kgsl_busy_validation.sh compute
SAMPLE_INTERVAL_S=0.02WORKLOAD_REPEAT=10POST_SAMPLE_S=1.0 ./tools/run_kgsl_busy_validation.sh mem
```

### 3. Treat `gpubusy` and `devfreq/gpu_load` as stronger references

For future validation, I should prioritize:

```
sysfs gpubusy vs trace kgsl_gpubusy
sysfs devfreq/gpu_load vs trace kgsl_pwrstats
```

and treat:

```
sysfs gpu_busy_percentage
```

as a weaker or secondary reference because it appears to use a different timing/update method.