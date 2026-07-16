# Vulkan Device Probe

This directory contains a small Vulkan utility for identifying the active Android Vulkan driver and reporting the capabilities needed by the Adreno GPU profiling workflow.

```text
tools/probes/runtime/vulkan_device_probe/
├── main.cpp
└── vk_probe
```

`main.cpp` is the source code.  
`vk_probe` is a prebuilt Android ARM64 executable.

## Purpose

The probe creates a minimal Vulkan instance and reports:

- Vulkan loader API version;
- number of physical devices;
- device name;
- vendor and device IDs;
- supported Vulkan API version;
- raw driver version;
- device type;
- timestamp support;
- timestamp period;
- driver name and information;
- Vulkan driver ID;
- conformance version; and
- queue-family capabilities.

It does not submit a compute shader or collect performance counters.

## Relationship to the profiler

The main profiling tools are:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

`vk_probe` supports them indirectly by confirming the Vulkan environment used by a benchmark.

```text
vk_probe
    ↓
identify active Vulkan driver and GPU capabilities
    ↓
verify vendor or Turnip environment
    ↓
run Vulkan benchmark
    ↓
capture KGSL counters with streamer or sweeper
```

The probe is useful for recording:

- whether the Qualcomm vendor driver or Mesa Turnip is active;
- which physical GPU Vulkan exposes;
- whether a compute-capable queue family exists;
- whether GPU timestamps are supported; and
- the `timestampPeriod` needed to convert timestamp ticks into nanoseconds.

The streamer and sweeper do not call or link this probe.

---

# Files

## `main.cpp`

Implements the Vulkan inspection program.

The source uses only standard C++ and Vulkan loader functions. It does not depend on other project source files.

## `vk_probe`

Prebuilt Android binary corresponding to `main.cpp`.

Current uploaded binary properties:

```text
Architecture:       ARM64 / AArch64
Format:             ELF 64-bit PIE
Android target:     API 35
NDK:                r28
Dynamic linker:     /system/bin/linker64
Stripped:           no
```

Its dynamic dependencies include:

```text
libvulkan.so
libc++_shared.so
libm.so
libdl.so
libc.so
```

Because the binary depends on `libc++_shared.so`, that library must be available through the runtime library path. Rebuilding with `-static-libstdc++` avoids this extra deployment dependency.

---

# What the probe reports

## Loader API version

The program calls:

```text
vkEnumerateInstanceVersion
```

and prints the Vulkan version supported by the Android Vulkan loader.

Example format:

```text
[vk_probe] Vulkan loader API version: 1.x.y
```

## Vulkan instance

The probe requests:

```text
Vulkan API 1.1
```

and creates an instance without enabling layers or instance extensions.

Failure at this stage usually indicates:

- an unavailable Vulkan loader;
- an incompatible driver;
- a broken driver deployment; or
- a linker or library-loading problem.

## Physical devices

The program enumerates every Vulkan physical device exposed by the loader.

Android phones normally expose one primary GPU, but the program supports more than one device.

## Device identity

For each device, it prints:

```text
name
vendorID
deviceID
apiVersion
driverVersion
deviceType
```

For Qualcomm hardware, the vendor ID is normally expected to identify Qualcomm, while the device name should identify an Adreno GPU.

## Timestamp properties

The probe prints:

```text
timestampComputeAndGraphics
timestampPeriod
```

`timestampComputeAndGraphics` indicates whether timestamp queries are supported for graphics and compute operations.

`timestampPeriod` gives the number of nanoseconds represented by one device timestamp tick:

```text
time_ns = timestamp_ticks × timestampPeriod
```

This value is important for Vulkan benchmark timing.

## Driver properties

The probe checks whether driver properties are available through:

```text
VK_KHR_driver_properties
```

or Vulkan 1.2 or newer.

When available, it prints:

```text
driverName
driverInfo
driverID
conformanceVersion
```

These fields are the main way to distinguish:

```text
Qualcomm vendor Vulkan driver
```

from:

```text
Mesa Turnip / Freedreno
```

## Queue families

The probe prints every queue family and marks support for:

```text
GRAPHICS
COMPUTE
TRANSFER
```

A Vulkan compute benchmark needs at least one queue family with:

```text
VK_QUEUE_COMPUTE_BIT
```

---

# Requirements

## Host

- macOS or Linux
- Android NDK
- Android Debug Bridge (`adb`)
- Bash

## Android device

- ARM64 Android device
- Vulkan loader at `libvulkan.so`
- Vulkan-capable GPU driver
- executable access under `/data/local/tmp`

Root access is not required for a normal vendor-driver probe.

Root is required by the optional Turnip bind-mount workflow.

---

# Build

## Recommended build

From the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

export ANDROID_NDK_HOME="$HOME/android-ndk-r27d"
export API=35
export HOST_TAG=darwin-x86_64

CXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android${API}-clang++"

"$CXX" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  -static-libstdc++ \
  tools/probes/runtime/vulkan_device_probe/main.cpp \
  -o tools/probes/runtime/vulkan_device_probe/vk_probe \
  -lvulkan
```

On Linux, use:

```bash
export HOST_TAG=linux-x86_64
```

### Why use `-static-libstdc++`

The current prebuilt binary depends on:

```text
libc++_shared.so
```

Using:

```text
-static-libstdc++
```

links the C++ runtime into the executable and simplifies deployment.

The Vulkan loader remains dynamically linked through:

```text
libvulkan.so
```

which is provided by Android.

## Dynamic C++ runtime build

To reproduce the current style of binary, omit:

```text
-static-libstdc++
```

Then also push the NDK C++ runtime:

```bash
LIBCXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
```

## Inspect the result

```bash
file tools/probes/runtime/vulkan_device_probe/vk_probe
```

Inspect dynamic dependencies:

```bash
readelf -d \
  tools/probes/runtime/vulkan_device_probe/vk_probe \
  | grep NEEDED
```

Optional checksum:

```bash
shasum -a 256 \
  tools/probes/runtime/vulkan_device_probe/vk_probe
```

---

# Push to the phone

Create the project work directory:

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work'
```

Push the probe:

```bash
adb push \
  tools/probes/runtime/vulkan_device_probe/vk_probe \
  /data/local/tmp/jerry_work/vk_probe

adb shell \
  'chmod 755 /data/local/tmp/jerry_work/vk_probe'
```

## When using the dynamic C++ runtime

Also push:

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/runtime'

adb push \
  "$LIBCXX" \
  /data/local/tmp/jerry_work/runtime/libc++_shared.so
```

Run with:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work &&
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/runtime:$LD_LIBRARY_PATH \
   ./vk_probe'
```

A statically linked C++ runtime build does not need this step.

---

# Run

## Vendor Vulkan driver

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_probe'
```

Save the output on the host:

```bash
mkdir -p results/vulkan_device_probe

adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_probe' \
  | tee results/vulkan_device_probe/vendor_vk_probe.txt
```

## Expected output structure

```text
[vk_probe] Starting Vulkan probe...
[vk_probe] Vulkan loader API version: ...
[vk_probe] Physical device count: ...

[vk_probe] Device 0
  name: ...
  vendorID: ...
  deviceID: ...
  apiVersion: ...
  driverVersion: ...
  deviceType: ...
  timestampComputeAndGraphics: ...
  timestampPeriod: ... ns
  VK_KHR_driver_properties supported: yes
  driverName: ...
  driverInfo: ...
  driverID: ...
  conformanceVersion: ...
  queue families: ...
    queue[0]: count=... flags=... GRAPHICS COMPUTE TRANSFER

[vk_probe] Done.
```

---

# Turnip validation

The probe is used by:

```text
tools/capture/run_vk_probe_with_turnip.sh
```

That script:

1. runs `vk_probe` with the vendor driver;
2. bind-mounts a Turnip Vulkan HAL over the vendor HAL;
3. runs `vk_probe` again;
4. saves probe and logcat output;
5. removes the bind mount; and
6. verifies that the vendor driver is restored.

Expected phone files:

```text
/data/local/tmp/jerry_work/vk_probe
/data/local/tmp/jerry_work/turnip/vulkan.adreno.so
```

Run:

```bash
./tools/capture/run_vk_probe_with_turnip.sh
```

Expected host logs:

```text
$HOME/adreno_turnip/turnip_probe_latest.log
$HOME/adreno_turnip/turnip_logcat_latest.txt
```

Driver properties should change between the vendor and Turnip runs.

The exact names and versions depend on the installed driver build.

---

# What to record for experiments

Save the probe output with each important benchmark campaign.

Useful fields include:

```text
device name
vendor ID
device ID
Vulkan API version
driver version
driver name
driver information
driver ID
conformance version
timestamp period
queue-family flags
```

Also record:

```text
Android build fingerprint
kernel version
phone model
benchmark commit
shader commit or hash
profiler commit
```

This helps distinguish performance differences caused by:

- workload changes;
- profiler changes;
- vendor-driver changes;
- Turnip changes;
- Android updates; or
- kernel updates.

---

# Connection to benchmark timing

A Vulkan runner may use timestamp queries to measure GPU execution.

The raw difference is expressed in timestamp ticks:

```text
delta_ticks = end_timestamp - start_timestamp
```

Convert it using the value reported by this probe:

```text
elapsed_ns = delta_ticks × timestampPeriod
elapsed_ms = elapsed_ns / 1,000,000
```

Do not assume the timestamp period is exactly one nanosecond.

---

# Limitations

- The probe does not create a Vulkan logical device.
- It does not submit command buffers.
- It does not test shader execution.
- It does not enumerate all features, limits, extensions, memory heaps, or formats.
- It does not verify that every listed queue can successfully execute a workload.
- It does not collect KGSL hardware counters.
- It does not directly measure performance.
- It requests Vulkan API 1.1 for the instance even when the loader supports a newer version.
- The set of recognized `VkResult` names is intentionally limited; unlisted results print as `UNKNOWN_VK_RESULT`.
- Driver-property output depends on driver and API support.

A successful probe confirms basic enumeration and driver identity, not complete benchmark correctness.

---

# Troubleshooting

## `Permission denied`

Check executable permissions:

```bash
adb shell \
  'ls -l /data/local/tmp/jerry_work/vk_probe'
```

Fix them:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/vk_probe'
```

## `libc++_shared.so` not found

The current prebuilt binary uses the shared NDK C++ runtime.

Either:

- push `libc++_shared.so` and set `LD_LIBRARY_PATH`; or
- rebuild with `-static-libstdc++`.

Check with:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_probe'
```

A linker error naming `libc++_shared.so` confirms this problem.

## `vkCreateInstance` fails

Possible causes include:

- Vulkan loader unavailable;
- incompatible driver;
- broken Turnip deployment;
- incorrect HAL bind mount;
- missing dependent libraries; or
- linker/SELinux restrictions.

Capture logcat:

```bash
adb logcat -c

adb shell \
  'cd /data/local/tmp/jerry_work && ./vk_probe'

adb logcat -d \
  | grep -Ei \
    'vulkan|adreno|turnip|freedreno|mesa|linker|dlopen|avc|denied'
```

## No physical devices

Confirm that Android has a Vulkan HAL:

```bash
adb shell \
  'ls -l /vendor/lib64/hw/vulkan*.so'
```

Check system properties:

```bash
adb shell \
  'getprop | grep -i vulkan'
```

## Driver properties are blank

The driver may not expose `VK_KHR_driver_properties`, or its reported API version may be older than Vulkan 1.2.

The rest of the device information can still be valid.

## No compute queue

Inspect every printed queue family.

A benchmark requiring Vulkan compute cannot run unless at least one family includes:

```text
COMPUTE
```

## Turnip test leaves the vendor HAL mounted over

Check:

```bash
adb shell \
  'su -c "mount | grep vulkan.adreno.so"'
```

Restore the vendor path:

```bash
adb shell \
  'su -c "umount /vendor/lib64/hw/vulkan.adreno.so"'
```

Then rerun the normal vendor probe.

---

# Recommended validation order

1. Build or obtain `vk_probe`.
2. Run it with the vendor Vulkan driver.
3. Confirm a physical device and compute queue.
4. Record driver properties and timestamp period.
5. Run a known Vulkan benchmark.
6. Build and deploy Turnip only when driver comparison is needed.
7. Run `run_vk_probe_with_turnip.sh`.
8. Confirm that the vendor driver is restored.
9. Run the profiler with the intended driver and benchmark.

---

# Notes

- This probe is read-only with respect to Vulkan device state.
- Root is not needed for the normal vendor-driver run.
- Turnip HAL replacement is a separate privileged experiment.
- Keep vendor and Turnip probe logs with their corresponding benchmark results.
- Rebuild the probe when changing the Android API, NDK, or C++ runtime strategy.
