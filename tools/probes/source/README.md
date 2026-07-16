# GPU Performance-Counter Probe Sources

This directory contains early experiments used to determine which performance-monitoring interfaces are available on the target Android/Adreno platform.

```text
tools/probes/source/
├── adreno_perfcntr_test.c
├── drm_msm_probe.c
├── drm_perfcntr_config_probe.c
├── drm_perfcntr_config_probe_v2.c
├── kgsl_ioctl_probe.c
├── kgsl_open_probe.c
├── msm_drm.h
├── msm_kgsl.h
├── perfcounter_test
├── vk_ext_probe.cpp
├── vk_perf_enum_probe.cpp
└── vk_perf_read_probe.cpp
```

The probes investigate three possible counter-access paths:

```text
1. Direct KGSL ioctls through /dev/kgsl-3d0
2. MSM DRM ioctls through /dev/dri/renderD128 or /dev/dri/card0
3. VK_KHR_performance_query through Vulkan
```

The final profiler uses the **direct KGSL ioctl path**.

---

# Relationship to the final profiler

The production tools are:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

These source probes are not linked into or executed by the final tools.

They were used to answer interface-discovery questions before the final implementation:

```text
probe available interfaces
        ↓
compare KGSL, DRM, and Vulkan access paths
        ↓
confirm direct KGSL GET / QUERY / READ / PUT behavior
        ↓
implement the streamer
        ↓
add generated countable tables and chunking
        ↓
implement the sweeper
```

The final KGSL path was selected because it exposes raw Adreno group/countable access directly on the target vendor kernel.

The DRM and Vulkan probes remain useful for:

- testing other kernels and drivers;
- comparing vendor Vulkan with Mesa Turnip;
- checking whether a more portable counter API is available; and
- documenting why an alternative interface was not used.

---

# File summary

| File | Interface tested | Purpose |
|---|---|---|
| `kgsl_open_probe.c` | KGSL device node | Tests whether `/dev/kgsl-3d0` can be opened with different access modes. |
| `kgsl_ioctl_probe.c` | KGSL perf-counter ioctls | Runs `QUERY`, then `GET → READ → PUT` for one group/countable pair. |
| `adreno_perfcntr_test.c` | KGSL perf-counter ioctls | Historical multi-counter SP sampling test. |
| `perfcounter_test` | KGSL perf-counter ioctls | Prebuilt Android binary corresponding to the historical C test. |
| `drm_msm_probe.c` | Generic DRM | Identifies the driver behind `/dev/dri` nodes and checks syncobj support. |
| `drm_perfcntr_config_probe.c` | MSM DRM perf counters | Tests `DRM_IOCTL_MSM_PERFCNTR_CONFIG` in update and stream modes. |
| `drm_perfcntr_config_probe_v2.c` | MSM DRM perf counters | Refined stream tests for SP, ALWAYSON, and an empty configuration. |
| `vk_ext_probe.cpp` | Vulkan extensions | Enumerates device extensions and checks for `VK_KHR_performance_query`. |
| `vk_perf_enum_probe.cpp` | Vulkan performance queries | Enumerates Vulkan performance counters for every queue family. |
| `vk_perf_read_probe.cpp` | Vulkan performance queries | Creates a performance query pool, submits work, and attempts to read one counter. |
| `msm_kgsl.h` | KGSL UAPI | Local kernel userspace API header used by the KGSL ioctl experiments. |
| `msm_drm.h` | MSM DRM UAPI | Local userspace API header containing the MSM perf-counter stream interface. |

---

# Access-path overview

## Direct KGSL

```text
/dev/kgsl-3d0
    ├── PERFCOUNTER_GET
    ├── PERFCOUNTER_PUT
    ├── PERFCOUNTER_QUERY
    └── PERFCOUNTER_READ
```

This is the path used by the final streamer and sweeper.

## MSM DRM

```text
/dev/dri/renderD128 or /dev/dri/card0
    └── DRM_IOCTL_MSM_PERFCNTR_CONFIG
```

This interface belongs to the MSM DRM UAPI. A device exposing `/dev/dri` nodes does not automatically guarantee that the nodes support this ioctl.

Vendor Android systems commonly use KGSL for the active Adreno userspace path, so the DRM experiments may legitimately return unsupported-ioctl or permission errors.

## Vulkan performance query

```text
VK_KHR_performance_query
    ├── enumerate counters
    ├── acquire profiling lock
    ├── create performance query pool
    ├── submit Vulkan commands
    └── read query result
```

This is the most portable API in principle, but it only works when the active Vulkan driver exposes and correctly implements the extension.

---

# KGSL probes

## `kgsl_open_probe.c`

### Purpose

Tests whether the following device can be opened:

```text
/dev/kgsl-3d0
```

It tries:

```text
O_RDWR
O_RDONLY
O_WRONLY
```

### Why it exists

This is the smallest possible KGSL access test. It separates a device-node permission problem from an ioctl or counter-selection problem.

### Expected output

```text
=== KGSL open probe ===
target: /dev/kgsl-3d0
open O_RDWR success: fd=...
open O_RDONLY success: fd=...
open O_WRONLY failed: errno=... (...)
```

The exact access modes depend on the vendor kernel and security policy.

### Run

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/kgsl_open_probe"'
```

---

## `kgsl_ioctl_probe.c`

### Purpose

Tests the complete direct KGSL counter path for one selected group and countable.

Default selection:

```text
group     = ALWAYSON, 0x1B
countable = 0
```

The probe:

1. opens `/dev/kgsl-3d0`;
2. calls `PERFCOUNTER_QUERY`;
3. calls `PERFCOUNTER_GET`;
4. calls `PERFCOUNTER_READ`;
5. calls `PERFCOUNTER_PUT`; and
6. closes the device.

### Syntax

```text
kgsl_ioctl_probe [group_id] [countable]
```

### Examples

ALWAYSON countable 0:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/kgsl_ioctl_probe 0x1b 0"'
```

SP ALU working cycles:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/kgsl_ioctl_probe 0x0a 2"'
```

### Expected output

```text
[OK]   open(/dev/kgsl-3d0)
[OK]   PERFCOUNTER_QUERY max_counters=...
[OK]   PERFCOUNTER_GET offset=... offset_hi=...
[OK]   PERFCOUNTER_READ value=...
[OK]   PERFCOUNTER_PUT
```

### Connection to the final tools

This probe contains the essential ioctl lifecycle later generalized by `adreno_perf_stream`.

The final tools add:

- generated A8xx countable names;
- multiple simultaneous counters;
- configurable intervals;
- continuous delta sampling;
- CSV output;
- counter cleanup;
- chunking; and
- automated benchmark reruns.

### Header-path note

The source includes:

```c
#include "linux/msm_kgsl.h"
```

If the repository stores `msm_kgsl.h` directly beside the source, either:

- place a copy or symlink at `linux/msm_kgsl.h`; or
- change the include to:
  ```c
  #include "msm_kgsl.h"
  ```

Keep the include arrangement consistent with the build instructions used by the repository.

---

## `adreno_perfcntr_test.c`

### Purpose

Historical proof-of-concept that allocates and reads three SP counters:

```text
SP_BUSY_CYCLES             countable 1
SP_ALU_WORKING_CYCLES      countable 2
SP_FS_INSTRUCTIONS         countable 44
```

It:

1. opens `/dev/kgsl-3d0`;
2. calls `GET` for all three countables;
3. reads all three in one `READ` request;
4. prints deltas once per second for 100 iterations;
5. calls `PUT` for all three countables; and
6. closes the device.

### Historical value

This source demonstrates that a single `PERFCOUNTER_READ` request can contain multiple group/countable entries.

That behavior is used by the final streamer to collect multiple active counters in one sample.

### Important output-label issue

The current print statement says:

```text
busy-cycles=..., gm-atomics=..., fs-instructions=...
```

However, the second sampled countable is:

```text
SP_ALU_WORKING_CYCLES
```

not `SP_GM_ATOMICS`.

Therefore, the second printed field should be interpreted as:

```text
alu-working-cycles
```

The label should be corrected before using this program for new measurements.

### Other limitations

- It does not exit immediately after an `open()` failure.
- It does not check whether every `GET` succeeded before sampling.
- It samples for a fixed 100 seconds.
- It uses hardcoded counters.
- It has no CSV output.
- It does not handle counter wraparound explicitly.
- `pack2xi32()` is unused.

Use the final streamer rather than this test for normal profiling.

---

## `perfcounter_test`

This is a historical prebuilt Android binary associated with:

```text
adreno_perfcntr_test.c
```

Current binary metadata:

```text
Architecture:       ARM64 / AArch64
Format:             ELF 64-bit PIE
Android target:     API 29
NDK:                r27d
Dynamic linker:     /system/bin/linker64
Stripped:           no
Dependencies:       libc.so, libdl.so
```

The embedded source filename is:

```text
adreno_perfcntr_test.c
```

The binary is retained for reproducibility. Rebuild it from source when changing the code, NDK, or Android API target.

---

# DRM probes

## `drm_msm_probe.c`

### Purpose

Checks the identity and basic capabilities of:

```text
/dev/dri/renderD128
/dev/dri/card0
```

For each node, it runs:

```text
DRM_IOCTL_VERSION
DRM_IOCTL_GET_CAP(DRM_CAP_SYNCOBJ)
```

### What it tells you

`DRM_IOCTL_VERSION` reports the DRM driver name, date, and description.

This should be checked before attempting MSM-specific ioctls. The existence of a render node alone does not prove that it is backed by the expected MSM DRM driver.

### Expected output

```text
=== Probe /dev/dri/renderD128 ===
DRM_IOCTL_VERSION ok
  name: ...
  date: ...
  desc: ...
DRM_CAP_SYNCOBJ: ...
```

### Build dependency

This source includes:

```c
#include <drm/drm.h>
```

A suitable `drm.h` from libdrm or Mesa's DRM UAPI headers must be available at build time.

---

## `drm_perfcntr_config_probe.c`

### Purpose

Tests:

```text
DRM_IOCTL_MSM_PERFCNTR_CONFIG
```

for:

```text
group     = SP
countable = 2
```

Countable 2 is intended to represent:

```text
A8XX_PERF_SP_ALU_WORKING_CYCLES
```

It tests both:

```text
MSM_PERFCNTR_UPDATE
MSM_PERFCNTR_STREAM | MSM_PERFCNTR_UPDATE
```

on:

```text
/dev/dri/renderD128
/dev/dri/card0
```

### Stream behavior

In stream mode, the ioctl may return a new file descriptor. The program attempts to read the first bytes from it and print them in hexadecimal.

### Important limitation

The stream `read()` is blocking. If the ioctl succeeds but no sample becomes available, the probe may wait indefinitely.

Use it only in a controlled test environment and stop it if it blocks.

---

## `drm_perfcntr_config_probe_v2.c`

### Purpose

Refines the DRM experiment by testing:

```text
SP countable 2
ALWAYSON countable 0
empty group configuration
```

The v2 script calculates a small power-of-two stream-buffer size based on the requested sample layout.

It tests `MSM_PERFCNTR_STREAM` without automatically combining it with `UPDATE`.

### Why the empty configuration is tested

An empty configuration can be useful for checking whether the ioctl exists and whether the driver accepts a reservation-style or capability-probing request.

### Expected outcomes

Possible results include:

```text
[OK] ioctl ret=<stream fd>
[FAIL] errno=ENOTTY
[FAIL] errno=EINVAL
[FAIL] errno=EACCES
[FAIL] errno=E2BIG
```

Interpretation depends on the driver and requested configuration.

### Relationship to the final profiler

These DRM tests are exploratory and are not used by the final KGSL streamer or sweeper.

---

# Vulkan probes

## `vk_ext_probe.cpp`

### Purpose

Creates a Vulkan instance, enumerates physical devices and device extensions, and checks specifically for:

```text
VK_KHR_performance_query
```

### Run this first

This should be the first Vulkan performance-query probe because the later probes depend on the extension.

### Expected output

```text
[vk_ext_probe] Device 0
  name: ...
  device extensions: ...
    VK_KHR_performance_query specVersion=...
  VK_KHR_performance_query: YES
```

If the result is `NO`, the counter enumeration and read probes are expected to fail or be unusable with the current driver.

---

## `vk_perf_enum_probe.cpp`

### Purpose

Enumerates Vulkan performance counters for every queue family.

For every counter, it prints:

```text
name
category
description
unit
scope
storage
UUID
```

It also asks the driver how many passes are needed to collect all counters for a queue family.

### Required Vulkan functions

```text
vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR
```

The program loads these functions with:

```text
vkGetInstanceProcAddr
```

### Expected output

```text
[queue family 0]
  performance counters: ...
  passes needed for all counters: ...

counter[...]
  name: ...
  category: ...
  description: ...
  unit: ...
  scope: ...
  storage: ...
  uuid: ...
```

### Why it matters

Counter indices passed to `vk_perf_read_probe` are driver-defined. Always enumerate them for the exact driver build before selecting an index.

Do not assume that index 3 has the same meaning across vendor and Turnip drivers.

---

## `vk_perf_read_probe.cpp`

### Purpose

Tests an actual Vulkan performance-query capture.

It:

1. creates a Vulkan instance;
2. selects the first graphics- or compute-capable queue family;
3. checks performance-query features;
4. enables `VK_KHR_performance_query`;
5. creates a logical device;
6. acquires the profiling lock;
7. creates a performance query pool;
8. allocates a device buffer;
9. records repeated `vkCmdFillBuffer` operations;
10. submits and waits for the work;
11. reads the performance result in three modes;
12. releases the profiling lock; and
13. destroys Vulkan resources.

### Syntax

```text
vk_perf_read_probe [counter_index] [fill_repeats] [buffer_mb]
```

Defaults:

```text
counter_index = 3
fill_repeats  = 512
buffer_mb     = 64
```

### Example

```bash
adb shell \
  'cd /data/local/tmp/jerry_work/source_probes &&
   ./vk_perf_read_probe 3 512 64'
```

### Query read modes

The program tests:

```text
VK_QUERY_RESULT_WAIT_BIT
VK_QUERY_RESULT_PARTIAL_BIT
VK_QUERY_RESULT_PARTIAL_BIT |
VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
```

### Important interpretation rule

The result union is currently printed as:

```c
result.uint64
```

That is valid only when the selected counter's storage type is:

```text
VK_PERFORMANCE_COUNTER_STORAGE_UINT64_KHR
```

Use `vk_perf_enum_probe` first and check the selected counter's `storage` field.

For counters stored as:

```text
INT32
INT64
UINT32
FLOAT32
FLOAT64
```

the corresponding union member must be used.

### Other limitations

- The default index is driver-specific.
- The extension is enabled directly; the extension probe should be run first.
- The program does not query the required pass count for the selected counter.
- `counterPassIndex` is fixed to zero.
- It selects the first physical device.
- It uses buffer fills rather than a compute shader.
- Error paths do not consistently destroy every object already created.
- It assumes one counter and one query.

This is an interface experiment, not the final profiling path.

---

# UAPI headers

## `msm_kgsl.h`

A local copy of the Qualcomm KGSL userspace API header.

Relevant definitions include:

```text
KGSL performance-counter group IDs
kgsl_perfcounter_get
kgsl_perfcounter_put
kgsl_perfcounter_query
kgsl_perfcounter_read
IOCTL_KGSL_PERFCOUNTER_GET
IOCTL_KGSL_PERFCOUNTER_PUT
IOCTL_KGSL_PERFCOUNTER_QUERY
IOCTL_KGSL_PERFCOUNTER_READ
```

The file also contains many unrelated KGSL memory, context, submission, synchronization, and property interfaces because it is a full UAPI header.

The source header carries:

```text
SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
```

Preserve its license notice.

## `msm_drm.h`

A local MSM DRM userspace API header containing:

```text
drm_msm_perfcntr_group
drm_msm_perfcntr_config
MSM_PERFCNTR_STREAM
MSM_PERFCNTR_UPDATE
DRM_IOCTL_MSM_PERFCNTR_CONFIG
```

It also defines the wider MSM DRM GEM, submit, synchronization, and VM-bind interfaces.

Preserve its upstream copyright and permission notice.

## Header maintenance

These headers describe kernel ABIs. Do not casually modify structure layouts, field types, alignment, or ioctl numbers.

When updating them:

1. identify the exact upstream source and revision;
2. replace the header as a unit where possible;
3. preserve licensing;
4. rebuild every dependent probe; and
5. verify structure sizes on the target architecture.

---

# Build requirements

## Common host tools

- Android NDK
- Bash
- `adb`
- `file`
- optional `readelf`

## Vulkan sources

Require Android Vulkan headers and:

```text
libvulkan.so
```

The Android NDK provides both.

## DRM sources

Require:

```text
drm.h
msm_drm.h
```

The NDK does not necessarily provide the complete libdrm/MSM UAPI include layout expected by these sources.

Use a controlled local include directory containing:

```text
include/drm/drm.h
include/drm/msm_drm.h
```

## KGSL sources

`kgsl_open_probe.c` has no KGSL header dependency.

`kgsl_ioctl_probe.c` expects:

```text
include/linux/msm_kgsl.h
```

---

# Example Android build

From the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

export ANDROID_NDK_HOME="$HOME/android-ndk-r27d"
export API=29
export HOST_TAG=darwin-x86_64

TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG"
CC="$TOOLCHAIN/bin/aarch64-linux-android${API}-clang"
CXX="$TOOLCHAIN/bin/aarch64-linux-android${API}-clang++"

mkdir -p build/source_probes
mkdir -p build/source_probes/include/linux
mkdir -p build/source_probes/include/drm
```

On Linux:

```bash
export HOST_TAG=linux-x86_64
```

## Prepare local headers

KGSL:

```bash
cp tools/probes/source/msm_kgsl.h \
  build/source_probes/include/linux/msm_kgsl.h
```

MSM DRM:

```bash
cp tools/probes/source/msm_drm.h \
  build/source_probes/include/drm/msm_drm.h
```

Also provide a compatible `drm.h`:

```bash
cp third_party/mesa/include/drm-uapi/drm.h \
  build/source_probes/include/drm/drm.h
```

Confirm that the selected Mesa revision actually contains that path before copying.

## Build KGSL probes

```bash
"$CC" \
  -O2 -Wall -Wextra \
  tools/probes/source/kgsl_open_probe.c \
  -o build/source_probes/kgsl_open_probe

"$CC" \
  -O2 -Wall -Wextra \
  -Ibuild/source_probes/include \
  tools/probes/source/kgsl_ioctl_probe.c \
  -o build/source_probes/kgsl_ioctl_probe

"$CC" \
  -O2 -Wall -Wextra \
  tools/probes/source/adreno_perfcntr_test.c \
  -o build/source_probes/perfcounter_test
```

## Build DRM probes

```bash
"$CC" \
  -O2 -Wall -Wextra \
  -Ibuild/source_probes/include \
  tools/probes/source/drm_msm_probe.c \
  -o build/source_probes/drm_msm_probe

"$CC" \
  -O2 -Wall -Wextra \
  -Ibuild/source_probes/include/drm \
  tools/probes/source/drm_perfcntr_config_probe.c \
  -o build/source_probes/drm_perfcntr_config_probe

"$CC" \
  -O2 -Wall -Wextra \
  -Ibuild/source_probes/include/drm \
  tools/probes/source/drm_perfcntr_config_probe_v2.c \
  -o build/source_probes/drm_perfcntr_config_probe_v2
```

## Build Vulkan probes

```bash
for src in \
  vk_ext_probe.cpp \
  vk_perf_enum_probe.cpp \
  vk_perf_read_probe.cpp
do
  "$CXX" \
    -std=c++17 \
    -O2 -Wall -Wextra \
    -static-libstdc++ \
    "tools/probes/source/$src" \
    -o "build/source_probes/${src%.cpp}" \
    -lvulkan
done
```

## Inspect outputs

```bash
file build/source_probes/*
```

---

# Push to the phone

Create a device directory:

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/source_probes'
```

Push all newly built executables:

```bash
adb push \
  build/source_probes/kgsl_open_probe \
  build/source_probes/kgsl_ioctl_probe \
  build/source_probes/perfcounter_test \
  build/source_probes/drm_msm_probe \
  build/source_probes/drm_perfcntr_config_probe \
  build/source_probes/drm_perfcntr_config_probe_v2 \
  build/source_probes/vk_ext_probe \
  build/source_probes/vk_perf_enum_probe \
  build/source_probes/vk_perf_read_probe \
  /data/local/tmp/jerry_work/source_probes/
```

Set permissions:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/source_probes/*'
```

---

# Recommended test order

## 1. Identify device nodes

```bash
adb shell \
  'ls -lZ /dev/kgsl-3d0 /dev/dri/card0 /dev/dri/renderD128 2>/dev/null'
```

## 2. Test basic KGSL access

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/kgsl_open_probe"'
```

## 3. Test direct KGSL counters

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/kgsl_ioctl_probe 0x1b 0"'
```

## 4. Identify DRM nodes

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/source_probes/drm_msm_probe"'
```

Only continue with MSM-specific DRM probes if the node identity and kernel support make that path plausible.

## 5. Check Vulkan extension support

```bash
adb shell \
  'cd /data/local/tmp/jerry_work/source_probes && ./vk_ext_probe'
```

## 6. Enumerate Vulkan counters

Run only when `VK_KHR_performance_query` is present:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work/source_probes && ./vk_perf_enum_probe'
```

## 7. Read one Vulkan counter

Choose the index and storage type from the enumeration output:

```bash
adb shell \
  'cd /data/local/tmp/jerry_work/source_probes &&
   ./vk_perf_read_probe <counter_index> 512 64'
```

---

# Expected conclusions

## KGSL succeeds

This confirms the access path used by the final profiler:

```text
/dev/kgsl-3d0
GET / QUERY / READ / PUT
```

## DRM nodes exist but perf-counter ioctl fails

This means the device exposes DRM nodes but does not necessarily implement the tested MSM perf-counter streaming ABI on those nodes.

This is a valid reason to retain the KGSL backend.

## Vulkan extension is absent

The active Vulkan driver does not expose the standardized performance-query interface.

Raw KGSL access may still work.

## Vulkan extension exists but query read fails

Possible causes include:

- unsupported selected counter;
- wrong counter storage interpretation;
- missing profiling-lock support;
- incorrect pass index;
- driver bug; or
- incomplete extension implementation.

---

# Safety and reproducibility

- Run KGSL and DRM ioctl probes only on the intended development device.
- Use root only where required.
- Do not run these probes concurrently with the streamer or sweeper.
- KGSL counter slots are limited and concurrent allocation can interfere with results.
- Stop a DRM stream probe if its blocking `read()` does not return.
- Preserve all UAPI header license notices.
- Record the phone model, Android build, kernel, driver, NDK, API target, and Git commit with saved logs.
- Prefer the final streamer and sweeper for normal measurements.
