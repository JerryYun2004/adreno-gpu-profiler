# A8XX Performance Counter Access Log

## Project Goal

The goal of this project is to directly investigate whether the Qualcomm Adreno A8XX hardware performance counter `A8XX_PERF_SP_ALU_WORKING_CYCLES` can be accessed from userspace.

The target is direct hardware performance-counter access. Systrace, KGSL pwrstats, tracepoints, and sysfs signals may be useful for comparison, but they are not the main target of this investigation.

A valid project result is to prove that this counter cannot be accessed on the current Android build without modifying permissions, SELinux policy, kernel configuration, or reflashing a modified kernel.

---

## Target Counter

From the Mesa A8XX performance-counter files:

```text
Counter name:  A8XX_PERF_SP_ALU_WORKING_CYCLES
Group:         SP
Selector:      2
Select family: SP_PERFCTR_SP_SEL
Counter:       RBBM_PERFCTR_SP
```

The SP group is the Shader Processor counter group. This is the group relevant to ALU activity.

---

## Device / Environment

```text
Host:
  macOS Darwin arm64

Phone:
  OnePlus CPH2653
  Android 15 / SDK 35
  Kernel: 6.6.30-android15-8-gb5f0c188ea2a-ab12656338-4k
  Build: OnePlus/CPH2653EEA/OP5D55L1:15/AP3A.240617.008/V.R4T3.1c0bb8c-55cf-20dbb:user/release-keys

ADB shell:
  uid=2000(shell)
  context=u:r:shell:s0
  SELinux: Enforcing
```

Phone-side test binaries and temporary files should only be placed under:

```text
/data/local/tmp/jerry_work
```

---

## Directory Structure

```text
logs/perfcounter_access/
├── README.md
├── 00_inventory/
├── 01_drm_msm_probe/
├── 02_kgsl_open_probe/
├── 03_kgsl_ioctl_probe/
├── 04_vendor_driver/
└── 05_turnip_driver/
```

Raw command outputs are saved in timestamped folders. This README records the interpretation and reproduction steps.

---

# 2026-06-04 — Initial GPU Interface Inventory

## Goal

Collect the exposed GPU-related device interfaces and determine which kernel paths are available for direct counter access.

## Command

```bash
cd /Users/jerryyun/adreno_turnip

OUT_ROOT=logs/perfcounter_access/00_inventory \
./tools/gpu_counter_inventory.sh
```

## Key Results

The following GPU-related device nodes exist:

```text
/dev/dri/card0
/dev/dri/renderD128
/dev/kgsl-3d0
```

Permissions and SELinux labels:

```text
crw-rw-rw- root   graphics u:object_r:graphics_device:s0 /dev/dri/card0
crw-rw-rw- root   graphics u:object_r:graphics_device:s0 /dev/dri/renderD128
crw-rw-rw- system system   u:object_r:gpu_device:s0      /dev/kgsl-3d0
```

The DRM nodes are connected to the MSM DRM driver:

```text
/sys/class/drm/card0      -> /sys/bus/platform/drivers/msm_drm
/sys/class/drm/renderD128 -> /sys/bus/platform/drivers/msm_drm
```

Tracefs is mounted:

```text
tracefs on /sys/kernel/tracing
```

## Interpretation

The phone exposes both DRM/MSM and KGSL-related GPU device nodes. However, DRM and KGSL have different SELinux labels. Normal Unix file permissions alone are not enough to determine whether userspace access is allowed.

---

# 2026-06-04 — DRM/MSM Runtime Probe

## Goal

Test whether the adb shell context can open the exposed DRM/MSM nodes.

## Build Command

```bash
cd /Users/jerryyun/adreno_turnip

export NDK=/Users/jerryyun/android-ndk-r27d
export API=35
export CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang"

mkdir -p build
"$CC" tools/drm_msm_probe.c -o build/drm_msm_probe
```

## Run Command

```bash
adb shell 'mkdir -p /data/local/tmp/jerry_work'
adb push build/drm_msm_probe /data/local/tmp/jerry_work/drm_msm_probe
adb shell 'chmod +x /data/local/tmp/jerry_work/drm_msm_probe'

OUT_DIR=logs/perfcounter_access/01_drm_msm_probe/$(date +%Y%m%d_%H%M%S)
mkdir -p "$OUT_DIR"

adb shell /data/local/tmp/jerry_work/drm_msm_probe 2>&1 | tee "$OUT_DIR/drm_msm_probe.txt"
```

## Result

```text
=== Probe /dev/dri/renderD128 ===
open failed: errno=13 (Permission denied)

=== Probe /dev/dri/card0 ===
open failed: errno=13 (Permission denied)
```

## Interpretation

The DRM/MSM nodes exist and are associated with the `msm_drm` platform driver. However, the adb shell context cannot open either `/dev/dri/renderD128` or `/dev/dri/card0`.

Since the Unix file mode is `crw-rw-rw-`, the failure is likely caused by SELinux or Android GPU access policy rather than normal Unix permissions.

## Current Conclusion

The DRM/MSM path is not currently usable from adb shell for direct access to `A8XX_PERF_SP_ALU_WORKING_CYCLES`.

---

# 2026-06-04 — KGSL Open Probe

## Goal

Test whether the adb shell context can open `/dev/kgsl-3d0`.

## Build and Run Command

```bash
cd /Users/jerryyun/adreno_turnip

export NDK=/Users/jerryyun/android-ndk-r27d
export API=35
export CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang"

mkdir -p build
"$CC" tools/kgsl_open_probe.c -o build/kgsl_open_probe

adb shell 'mkdir -p /data/local/tmp/jerry_work'
adb push build/kgsl_open_probe /data/local/tmp/jerry_work/kgsl_open_probe
adb shell 'chmod +x /data/local/tmp/jerry_work/kgsl_open_probe'

OUT_DIR=logs/perfcounter_access/02_kgsl_open_probe/$(date +%Y%m%d_%H%M%S)
mkdir -p "$OUT_DIR"

adb shell /data/local/tmp/jerry_work/kgsl_open_probe 2>&1 | tee "$OUT_DIR/kgsl_open_probe.txt"

adb shell 'ls -lZ /dev/kgsl-3d0 2>/dev/null || true' \
  | tee "$OUT_DIR/kgsl_permissions.txt"

adb shell 'id; getenforce' \
  | tee "$OUT_DIR/shell_context.txt"
```

## Result

```text
=== KGSL open probe ===
target: /dev/kgsl-3d0
open O_RDWR success: fd=3
open O_RDONLY success: fd=3
open O_WRONLY success: fd=3
```

KGSL node:

```text
crw-rw-rw- 1 system system u:object_r:gpu_device:s0 457, 0 /dev/kgsl-3d0
```

ADB shell context:

```text
uid=2000(shell)
context=u:r:shell:s0
SELinux: Enforcing
```

## Interpretation

Unlike the DRM/MSM nodes, `/dev/kgsl-3d0` can be opened successfully from the adb shell context. This means KGSL is not blocked at the basic file-open level.

The next question is whether KGSL performance-counter ioctls are allowed.

## Current Conclusion

The KGSL path is the most realistic path for direct access to `A8XX_PERF_SP_ALU_WORKING_CYCLES`.

---

# 2026-06-04 — KGSL Header Search

## Goal

Find a KGSL UAPI header that defines the performance-counter ioctl structures and ioctl numbers.

## Initial Search Commands

```bash
cd /Users/jerryyun/adreno_turnip

find . -iname "msm_kgsl.h" -o -iname "*kgsl*.h" | sort

grep -R "IOCTL_KGSL_PERFCOUNTER" -n . 2>/dev/null | head -50
grep -R "KGSL_PERFCOUNTER" -n . 2>/dev/null | head -100
```

## Initial Result

No useful KGSL ioctl header was found in the current working directory.

## Targeted Search Command

```bash
cd /Users/jerryyun

find ~/adreno_turnip ~/Downloads ~/github ~/Documents ~/Desktop \
  -type f \( -name "msm_kgsl.h" -o -name "tu_knl_kgsl.cc" \) \
  2>/dev/null | sort
```

## Targeted Search Result

```text
/Users/jerryyun/adreno_turnip/tools/include/linux/msm_kgsl.h
/Users/jerryyun/Desktop/adreno-gpu-profiler/third_party/mesa/src/freedreno/vulkan/msm_kgsl.h
/Users/jerryyun/Desktop/adreno-gpu-profiler/third_party/mesa/src/freedreno/vulkan/tu_knl_kgsl.cc
/Users/jerryyun/Downloads/mesa-main/src/freedreno/vulkan/msm_kgsl.h
/Users/jerryyun/Downloads/mesa-main/src/freedreno/vulkan/tu_knl_kgsl.cc
```

## Interpretation

The current working directory `/Users/jerryyun/adreno_turnip` does not contain the full Mesa tree, but previous Mesa copies exist elsewhere. The best header to use for the current standalone KGSL ioctl probes is:

```text
/Users/jerryyun/Desktop/adreno-gpu-profiler/third_party/mesa/src/freedreno/vulkan/msm_kgsl.h
```

This is better than the old Android 7 fallback header because it comes from the Mesa/Turnip tree previously used in this project.

## Header Copy Command

```bash
cd /Users/jerryyun/adreno_turnip

mkdir -p tools/include/linux

cp /Users/jerryyun/Desktop/adreno-gpu-profiler/third_party/mesa/src/freedreno/vulkan/msm_kgsl.h \
   tools/include/linux/msm_kgsl.h

grep -n "IOCTL_KGSL_PERFCOUNTER" tools/include/linux/msm_kgsl.h
grep -n "KGSL_PERFCOUNTER_GROUP_SP" tools/include/linux/msm_kgsl.h
grep -n "KGSL_PERFCOUNTER_GROUP_ALWAYSON" tools/include/linux/msm_kgsl.h
grep -n "struct kgsl_perfcounter" tools/include/linux/msm_kgsl.h
```

## Header Verification Result

```text
1005: * struct kgsl_perfcounter_get - argument to IOCTL_KGSL_PERFCOUNTER_GET
1029:#define IOCTL_KGSL_PERFCOUNTER_GET \
1033: * struct kgsl_perfcounter_put - argument to IOCTL_KGSL_PERFCOUNTER_PUT
1049:#define IOCTL_KGSL_PERFCOUNTER_PUT \
1053: * struct kgsl_perfcounter_query - argument to IOCTL_KGSL_PERFCOUNTER_QUERY
1079:#define IOCTL_KGSL_PERFCOUNTER_QUERY \
1083: * struct kgsl_perfcounter_read_group - argument to IOCTL_KGSL_PERFCOUNTER_QUERY
1106:#define IOCTL_KGSL_PERFCOUNTER_READ \
466:#define KGSL_PERFCOUNTER_GROUP_SP 0xA
484:#define KGSL_PERFCOUNTER_GROUP_SP_PWR 0x1C
483:#define KGSL_PERFCOUNTER_GROUP_ALWAYSON 0x1B
491:#define KGSL_PERFCOUNTER_GROUP_ALWAYSON_PWR 0x23
1020:struct kgsl_perfcounter_get {
1042:struct kgsl_perfcounter_put {
1069:struct kgsl_perfcounter_query {
1093:struct kgsl_perfcounter_read_group {
1099:struct kgsl_perfcounter_read {
```

## Current Header Choice

Use:

```text
tools/include/linux/msm_kgsl.h
```

copied from Mesa/Turnip, not the Android 7 fallback header.

---

# Historical Logs Integrated

Previous logs were reviewed to guide the systematic redo. The previous results are treated as historical evidence only. The current goal is to reproduce them more carefully with cleaner scripts and structured logs.

Useful recovered findings:

- The phone's real GPU path is KGSL, and the GPU model was previously confirmed as `Adreno830v2`.
- The visible DRM nodes appeared associated with `msm_drm` / display-KMS paths.
- Mesa/Turnip previously had a KGSL header at `src/freedreno/vulkan/msm_kgsl.h`.
- That header included:
  - `IOCTL_KGSL_PERFCOUNTER_GET`
  - `IOCTL_KGSL_PERFCOUNTER_PUT`
  - `IOCTL_KGSL_PERFCOUNTER_QUERY`
  - `IOCTL_KGSL_PERFCOUNTER_READ`
- Previous standalone probes suggested:
  - `/dev/kgsl-3d0` opens successfully.
  - `PERFCOUNTER_QUERY` works.
  - `PERFCOUNTER_GET` works.
  - `PERFCOUNTER_READ` fails with `EPERM`.
  - Running as root did not fix `PERFCOUNTER_READ`.

The current redo will reproduce these results step by step instead of relying on the old logs.

---

# 2026-06-04 — KGSL Ioctl Probe: QUERY / GET / READ / PUT

## Goal

Test whether KGSL performance-counter ioctls can access the target hardware counter:

```text
A8XX_PERF_SP_ALU_WORKING_CYCLES
```

This target maps to:

```text
Group:     SP
Group ID:  0x0A
Countable: 2
```

The probe also tests `ALWAYSON` group countable `0` as a simple reference counter.

## Header Used

The ioctl probe used Mesa/Turnip's KGSL header copied from:

```text
/Users/jerryyun/Desktop/adreno-gpu-profiler/third_party/mesa/src/freedreno/vulkan/msm_kgsl.h
```

to:

```text
tools/include/linux/msm_kgsl.h
```

Important definitions confirmed:

```text
KGSL_PERFCOUNTER_GROUP_SP       = 0x0A
KGSL_PERFCOUNTER_GROUP_ALWAYSON = 0x1B

IOCTL_KGSL_PERFCOUNTER_GET      = 0x38
IOCTL_KGSL_PERFCOUNTER_PUT      = 0x39
IOCTL_KGSL_PERFCOUNTER_QUERY    = 0x3A
IOCTL_KGSL_PERFCOUNTER_READ     = 0x3B
```

## Build Command

```bash
cd /Users/jerryyun/adreno_turnip

export NDK=/Users/jerryyun/android-ndk-r27d
export API=35
export CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang"

mkdir -p build
"$CC" -Itools/include tools/kgsl_ioctl_probe.c -o build/kgsl_ioctl_probe
```

## Run Commands

```bash
adb shell 'mkdir -p /data/local/tmp/jerry_work'
adb push build/kgsl_ioctl_probe /data/local/tmp/jerry_work/kgsl_ioctl_probe
adb shell 'chmod +x /data/local/tmp/jerry_work/kgsl_ioctl_probe'

OUT_DIR=logs/perfcounter_access/03_kgsl_ioctl_probe/$(date +%Y%m%d_%H%M%S)
mkdir -p "$OUT_DIR"

adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_ioctl_probe 0x1b 0' \
  2>&1 | tee "$OUT_DIR/kgsl_ioctl_alwayson0_shell.txt"

adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_ioctl_probe 0x0a 2' \
  2>&1 | tee "$OUT_DIR/kgsl_ioctl_sp2_shell.txt"

adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_ioctl_probe 0x1b 0"' \
  2>&1 | tee "$OUT_DIR/kgsl_ioctl_alwayson0_root.txt"

adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_ioctl_probe 0x0a 2"' \
  2>&1 | tee "$OUT_DIR/kgsl_ioctl_sp2_root.txt"
```

## Result: ALWAYSON Group, Countable 0

Shell result:

```text
[OK]   open(/dev/kgsl-3d0)
[OK]   PERFCOUNTER_QUERY max_counters=1 count=256
[OK]   PERFCOUNTER_GET offset=0x8e7 offset_hi=0x8e8
[FAIL] PERFCOUNTER_READ ret=-1 errno=1 (Operation not permitted)
[OK]   PERFCOUNTER_PUT
```

Root result:

```text
[OK]   open(/dev/kgsl-3d0)
[OK]   PERFCOUNTER_QUERY max_counters=1 count=256
[OK]   PERFCOUNTER_GET offset=0x8e7 offset_hi=0x8e8
[FAIL] PERFCOUNTER_READ ret=-1 errno=1 (Operation not permitted)
[OK]   PERFCOUNTER_PUT
```

## Result: SP Group, Countable 2

This is the direct target:

```text
A8XX_PERF_SP_ALU_WORKING_CYCLES
```

Shell result:

```text
[OK]   open(/dev/kgsl-3d0)
[OK]   PERFCOUNTER_QUERY max_counters=24 count=256
[OK]   PERFCOUNTER_GET offset=0x29c offset_hi=0x29d
[FAIL] PERFCOUNTER_READ ret=-1 errno=1 (Operation not permitted)
[OK]   PERFCOUNTER_PUT
```

Root result:

```text
[OK]   open(/dev/kgsl-3d0)
[OK]   PERFCOUNTER_QUERY max_counters=24 count=256
[OK]   PERFCOUNTER_GET offset=0x29c offset_hi=0x29d
[FAIL] PERFCOUNTER_READ ret=-1 errno=1 (Operation not permitted)
[OK]   PERFCOUNTER_PUT
```

## Logcat / dmesg Check

After the ioctl tests, logcat did not show an obvious relevant AVC denial for the KGSL perfcounter read failure. The visible logcat output only contained unrelated display/performance messages.

## Interpretation

The KGSL device node can be opened from adb shell and root. KGSL also allows perfcounter metadata query and counter reservation/programming.

However, KGSL blocks actual counter value reads through:

```text
IOCTL_KGSL_PERFCOUNTER_READ
```

with:

```text
errno=1 (Operation not permitted)
```

This happens for both:

```text
ALWAYSON group 0x1B countable 0
SP group 0x0A countable 2
```

and also happens when running through `su`.

## Current Conclusion

The target counter `A8XX_PERF_SP_ALU_WORKING_CYCLES` appears accessible far enough for KGSL to reserve/program it and return counter register offsets, but actual value reading is blocked by the kernel at `PERFCOUNTER_READ`.

This strongly suggests that the current Android kernel/KGSL driver has an internal permission or policy check that prevents userspace from directly reading performance counter values, even as root.

---

# Next Planned Step

Build a clean KGSL query-only probe that tests all KGSL counter groups without trying `GET`, `READ`, or `PUT` for every group. This will produce a clean group table showing which detailed performance-counter groups are exposed by the kernel.
