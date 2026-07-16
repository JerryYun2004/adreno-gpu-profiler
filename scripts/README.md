# Utility Scripts

Helper scripts for building the Turnip Vulkan driver, sampling Qualcomm KGSL GPU statistics on an Android device, and pulling profiling results to the host.

## Requirements

### Host

- macOS or Linux
- Bash
- Android Debug Bridge (`adb`)
- Meson and Ninja for Mesa builds
- Android NDK for cross-compilation

### Android device

- Qualcomm Adreno GPU using KGSL
- Root access through `su`
- KGSL sysfs nodes under `/sys/class/kgsl/kgsl-3d0`
- Android shell utilities used by the scripts, including `find`, `sed`, and `timeout`

Make the scripts executable before use:

```bash
chmod +x scripts/*.sh
```

## Scripts

| Script | Runs on | Purpose |
|---|---|---|
| `build_turnip_android_kgsl.sh` | Host | Cross-compiles Mesa's Turnip Vulkan driver for Android AArch64 with the KGSL backend. |
| `kgsl_live_sampler.sh` | Device | Continuously prints selected KGSL statistics as CSV. |
| `kgsl_fast_sampler.sh` | Device | Samples selected KGSL statistics for a fixed duration and saves them to a CSV file. |
| `kgsl_all_node_sampler.sh` | Device | Repeatedly scans a broad set of KGSL sysfs nodes and records all readable values. |
| `pull_latest_sweep.sh` | Host | Finds and pulls the newest perfcounter sweep directory from the phone. |

## Turnip Android build

Run `build_turnip_android_kgsl.sh` from the root of the Mesa source tree:

```bash
cd third_party/mesa
../../scripts/build_turnip_android_kgsl.sh
```

The script:

- Locates the Android NDK from `ANDROID_NDK_HOME` or common installation paths.
- Targets Android API 29 and AArch64.
- Creates `android-aarch64.cross`.
- recreates the `build-android` directory.
- Builds the Freedreno Turnip Vulkan driver with the KGSL backend.

Output:

```text
build-android/.../libvulkan_freedreno.so
```

To select a specific NDK:

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r27d
```

> **Note:** The script deletes any existing `build-android` directory before configuring the build.

## KGSL samplers

Copy the device-side scripts to the phone:

```bash
adb push scripts/kgsl_live_sampler.sh /data/local/tmp/
adb push scripts/kgsl_fast_sampler.sh /data/local/tmp/
adb push scripts/kgsl_all_node_sampler.sh /data/local/tmp/

adb shell 'chmod +x /data/local/tmp/kgsl_*_sampler.sh'
```

The selected samplers record these fields when available:

```text
timestamp_ms,gpu_load,gpu_busy_percentage,kernel_gpu_busy,clock_mhz,cur_freq,gpuclk,bus_split,gpubusy
```

Unavailable nodes are reported as `NA`. Values are taken directly from the kernel interfaces and may differ between devices or kernel versions.

### Live sampler

```bash
adb shell '/data/local/tmp/kgsl_live_sampler.sh [interval_seconds]'
```

Example—sample approximately every 50 ms and save the host-side output:

```bash
adb shell '/data/local/tmp/kgsl_live_sampler.sh 0.05' \
  > results/kgsl_live_samples.csv
```

The sampler runs until interrupted with `Ctrl+C`.

### Fixed-duration fast sampler

```bash
adb shell '/data/local/tmp/kgsl_fast_sampler.sh [output] [interval_seconds] [duration_seconds]'
```

Defaults:

```text
output:   /data/local/tmp/kgsl_fast_samples.csv
interval: 0.05 seconds
duration: 10 seconds
```

Example:

```bash
adb shell '/data/local/tmp/kgsl_fast_sampler.sh \
  /data/local/tmp/kgsl_fast_samples.csv 0.02 15'

adb pull /data/local/tmp/kgsl_fast_samples.csv results/
```

### All-node sampler

This script scans a broad set of KGSL, devfreq, power, bus-monitor, device-tree, and CoreSight nodes.

Start it in one terminal:

```bash
adb shell 'su -c "/data/local/tmp/kgsl_all_node_sampler.sh 0.02"'
```

Stop it from another terminal:

```bash
adb shell 'su -c "touch /data/local/tmp/kgsl_all_node_sampler.stop"'
```

Pull the result:

```bash
adb pull /data/local/tmp/kgsl_all_node_samples.csv results/
```

The CSV format is:

```text
timestamp_ns,path,value
```

Because many files are read during each pass, the effective sampling period can be much longer than the requested interval.

## Pull the latest perfcounter sweep

```bash
./scripts/pull_latest_sweep.sh
```

The script finds the newest directory matching:

```text
/data/local/tmp/jerry_work/perfcounter_sweeps/sweep_*
```

It pulls the directory into:

```text
results/perfcounter_sweeps/
```

The destination is currently stored as an absolute path in the script. Update `DEST_ROOT` when using the repository from another location.

## Notes

- Keep the phone connected and visible through `adb devices`.
- Root permissions are required to read many KGSL nodes.
- Very short requested intervals may not be achievable because each sample performs several shell and sysfs operations.
- KGSL node availability and value formats are device- and kernel-dependent.