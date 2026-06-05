# Investigation Report: Direct KGSL Performance-Counter ioctl Path

**Date range:** 2026-05-28 to 2026-05-29

**Project:** Mobile GPU Profiling on Adreno / KGSL

**Method:** Standalone userspace access to `/dev/kgsl-3d0` performance-counter ioctls

**Status:** Blocked at `IOCTL_KGSL_PERFCOUNTER_READ` with `EPERM`

---

## 1. Objective

The goal of this method was to test the KGSL performance-counter interface directly, without relying on Mesa PPS or Turnip.

The intended path was:

```
Standalone native Android binary
        ↓
open("/dev/kgsl-3d0")
        ↓
IOCTL_KGSL_PERFCOUNTER_QUERY
IOCTL_KGSL_PERFCOUNTER_GET
IOCTL_KGSL_PERFCOUNTER_READ
IOCTL_KGSL_PERFCOUNTER_PUT
        ↓
Detailed Adreno hardware counter values
```

This was intended as the simplest possible test of whether the kernel permits direct access to detailed Adreno performance counters.

---

## 2. Device and Environment

Device:

```
OnePlus CPH2653
Android 15
SDK/API 35
ABI arm64-v8a
Hardware qcom
GPU Adreno 830 / Adreno830v2
```

Important device path:

```
/dev/kgsl-3d0
```

The KGSL node could be opened by userspace programs. The file permissions were permissive enough that normal file access was not the blocker.

SELinux was checked and later observed to be permissive, but the counter read failure still occurred.

---

## 3. Vulkan Workload Baseline

Before testing counters, a native Vulkan compute workload was built and run to confirm the GPU could be driven from command-line Android binaries.

The Vulkan probe confirmed:

```
Device: Adreno (TM) 830
Vendor ID: 0x5143
API version: Vulkan 1.3.284
Driver: Qualcomm Technologies Inc. Adreno Vulkan Driver
```

The compute workload completed successfully:

```
vk_compute_probe
Verification PASSED for first 1024 elements.
```

During the workload, KGSL coarse sysfs nodes responded:

```
gpu_busy_percentage: 98–99 %
gpubusy: nonzero
gpuclk: 1100000000
```

After the workload completed:

```
gpu_busy_percentage: 0 %
gpubusy: 0 0
gpuclk: 222000000
```

This confirmed that the workload genuinely activated the GPU and that KGSL coarse metrics were functional.

---

## 4. KGSL ioctl Interface Inspection

Mesa’s KGSL header was inspected:

```
src/freedreno/vulkan/msm_kgsl.h
```

This confirmed the relevant ioctl structures and definitions:

```c
struct kgsl_perfcounter_get
struct kgsl_perfcounter_put
struct kgsl_perfcounter_query
struct kgsl_perfcounter_read_group
struct kgsl_perfcounter_read
```

Relevant ioctls:

```
IOCTL_KGSL_PERFCOUNTER_GET
IOCTL_KGSL_PERFCOUNTER_PUT
IOCTL_KGSL_PERFCOUNTER_QUERY
IOCTL_KGSL_PERFCOUNTER_READ
```

Important KGSL performance-counter group IDs included:

```
KGSL_PERFCOUNTER_GROUP_CP        = 0x00
KGSL_PERFCOUNTER_GROUP_RBBM      = 0x01
KGSL_PERFCOUNTER_GROUP_UCHE      = 0x08
KGSL_PERFCOUNTER_GROUP_TP        = 0x09
KGSL_PERFCOUNTER_GROUP_SP        = 0x0A
KGSL_PERFCOUNTER_GROUP_RB        = 0x0B
KGSL_PERFCOUNTER_GROUP_ALWAYSON  = 0x1B
```

Turnip’s KGSL backend also uses `IOCTL_KGSL_PERFCOUNTER_READ` for an always-on timestamp-related Perfetto path, which provided a reference for the standalone probe.

---

## 5. Probe 1: Always-On Read Probe

A minimal probe was built:

```
kgsl_alwayson_probe
```

Purpose:

```
1. open("/dev/kgsl-3d0", O_RDWR)
2. call IOCTL_KGSL_PERFCOUNTER_READ
3. read group = ALWAYSON, countable = 0
```

### 5.1 Result as normal shell user

```
[kgsl_alwayson_probe] Opening /dev/kgsl-3d0
[kgsl_alwayson_probe] open succeeded, fd=3
[kgsl_alwayson_probe] Reading ALWAYSON group=0x1b countable=0
[kgsl_alwayson_probe] ioctl READ failed at sample 0: errno=1 (Operation not permitted)
```

### 5.2 Result as root

Command:

```bash
adb shell 'su -c /data/local/tmp/jerry_work/kgsl_alwayson_probe'
```

Result:

```
ioctl READ failed: errno=1 (Operation not permitted)
```

Conclusion:

```
Opening /dev/kgsl-3d0 works.
PERFCOUNTER_READ is blocked with EPERM.
Root does not fix the failure.
```

---

## 6. Probe 2: KGSL Perfcounter Query Probe

A second probe was built:

```
kgsl_query_probe
```

Purpose:

```
1. open("/dev/kgsl-3d0")
2. sweep group IDs 0x00 to 0x38
3. call IOCTL_KGSL_PERFCOUNTER_QUERY
4. print max counters and active countables
```

### 6.1 Result

The query probe succeeded for most groups:

```
Summary: success=55 fail=2
```

Important successful groups included:

```
CP       group=0x00 max_counters=14
RBBM     group=0x01 max_counters=4
PC       group=0x02 max_counters=8
VFD      group=0x03 max_counters=8
HLSQ     group=0x04 max_counters=6
UCHE     group=0x08 max_counters=24
TP       group=0x09 max_counters=12
SP       group=0x0a max_counters=24
RB       group=0x0b max_counters=8
VBIF     group=0x0d max_counters=8
VSC      group=0x17 max_counters=2
CCU      group=0x18 max_counters=5
LRZ      group=0x19 max_counters=4
CMP      group=0x1a max_counters=4
ALWAYSON group=0x1b max_counters=1 active_countables=[0]
UFC      group=0x2b max_counters=4
BV_SP    group=0x31 max_counters=12
```

Only two groups failed:

```
BV_CCU group=0x37 errno=22 Invalid argument
BV_RB  group=0x38 errno=22 Invalid argument
```

Conclusion:

```
KGSL exposes detailed performance-counter group metadata.
QUERY is allowed.
The kernel is not blocking all performance-counter ioctls.
```

This was important because it showed that counters are present and discoverable.

---

## 7. Probe 3: GET / READ / PUT Probe

A third probe was built:

```
kgsl_get_read_probe
```

Purpose:

```
1. open("/dev/kgsl-3d0")
2. call IOCTL_KGSL_PERFCOUNTER_GET
3. call IOCTL_KGSL_PERFCOUNTER_READ
4. call IOCTL_KGSL_PERFCOUNTER_PUT
```

Tested groups and countables included:

```
SP   group=0x0a countable=0
CP   group=0x00 countable=0
RBBM group=0x01 countable=6
UCHE group=0x08 countable=13
```

### 7.1 Results

For all tested groups:

```
PERFCOUNTER_GET succeeded
PERFCOUNTER_READ failed with errno=1 (Operation not permitted)
PERFCOUNTER_PUT succeeded
```

Example SP result:

```
GET succeeded
offset=0x29c
offset_hi=0x29d
READ failed: EPERM
```

Example CP result:

```
GET succeeded
offset=0x1ca
offset_hi=0x1cb
READ failed: EPERM
```

Example RBBM result:

```
GET succeeded
offset=0x1d2
offset_hi=0x1d3
READ failed: EPERM
```

Example UCHE result:

```
GET succeeded
offset=0x254
offset_hi=0x255
READ failed: EPERM
```

The same tests were repeated as root. The result did not change:

```
GET succeeds
READ fails with EPERM
PUT succeeds
```

---

## 8. Permission and SELinux Checks

Root access was verified:

```bash
adb shell 'su -c id'
```

Expected root-style result:

```
uid=0(root)
```

KGSL device node labeling and permissions were checked:

```bash
adb shell 'ls -lZ /dev/kgsl-3d0'
```

Logcat and dmesg were checked for SELinux or KGSL denial messages:

```bash
adb logcat -c

adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_alwayson_0_read_only 0x1B 0 read_only 2>&1"'

adb logcat -d | grep -Ei "avc|denied|kgsl|perfcounter|perf|ioctl|selinux"

adb shell 'su -c "dmesg | grep -Ei \"avc|denied|kgsl|perfcounter|perf|ioctl\" | tail -100"'
```

No useful AVC denial or kernel log explaining the failure was found.

Conclusion:

```
The block is probably not ordinary Unix file permission.
The block is probably not visible SELinux enforcement.
The block is likely inside KGSL driver logic or a vendor kernel policy.
```

---

## 9. Why This Method Failed

This method failed specifically at:

```
IOCTL_KGSL_PERFCOUNTER_READ
```

The important distinction is:

```
open("/dev/kgsl-3d0")                 works
IOCTL_KGSL_PERFCOUNTER_QUERY          works
IOCTL_KGSL_PERFCOUNTER_GET            works
IOCTL_KGSL_PERFCOUNTER_PUT            works
IOCTL_KGSL_PERFCOUNTER_READ           fails with EPERM
IOCTL_KGSL_PERFCOUNTER_READ as root   still fails with EPERM
```

Therefore, the problem is not that KGSL counters are absent. The problem is that the kernel refuses to return actual counter values to this userspace path.

Likely explanations:

```
1. KGSL has an internal permission check for PERFCOUNTER_READ.
2. PERFCOUNTER_READ requires a privileged/trusted context not available to adb shell or Magisk root.
3. PERFCOUNTER_READ may be intentionally blocked on production Android builds.
4. The vendor driver may access counters through a different private/trusted path.
```

---

## 10. Reproduction Steps

### 10.1 Build a KGSL ioctl probe

Example build command for non-Vulkan KGSL probes:

```bash
aarch64-linux-android35-clang++ \
  -std=c++17 \
  -O2 \
  probe.cpp \
  -o probe_binary
```

Push to the phone:

```bash
adb push probe_binary /data/local/tmp/jerry_work/
adb shell 'chmod +x /data/local/tmp/jerry_work/probe_binary'
```

### 10.2 Run always-on read probe

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_alwayson_probe'
adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_alwayson_probe"'
```

Expected result:

```
open succeeded
PERFCOUNTER_READ failed: errno=1 (Operation not permitted)
```

### 10.3 Run group query probe

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_query_probe'
```

Expected result:

```
Most groups return metadata successfully.
```

### 10.4 Run GET / READ / PUT probe

Example:

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_get_read_probe 0x0A 0'
adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_get_read_probe 0x0A 0"'
```

Expected result:

```
GET succeeded
READ failed: EPERM
PUT succeeded
```

---

## 11. Final Conclusion

The standalone KGSL ioctl method was examined carefully and reached a precise blocker.

The method proved:

```
Detailed KGSL performance-counter metadata exists.
KGSL exposes many hardware counter groups.
Userspace can open /dev/kgsl-3d0.
Userspace can query counter groups.
Userspace can reserve/configure counters using GET.
Userspace cannot read counter values using READ.
Root does not bypass the READ failure.
```

Therefore, the current conclusion is:

> Direct KGSL hardware performance-counter access is blocked at `IOCTL_KGSL_PERFCOUNTER_READ` on this Adreno 830 / Android 15 device. The kernel allows metadata query and counter reservation but refuses actual value readback with `EPERM`.
> 

This result strongly influenced later experiments with Mesa Turnip. It suggested that even if Turnip could expose A8XX counters through Vulkan, actual counter value access might still fail due to KGSL/kernel policy.

---

## 12. Follow-Up Direction

Because direct detailed counter reads were blocked, the project moved toward two parallel paths:

```
1. Try Mesa Turnip VK_KHR_performance_query as an alternative userspace path.
2. Build a practical no-kernel-modification profiler using KGSL tracepoints and sysfs nodes.
```

The Turnip path later showed that A8XX raw counters can be enumerated but not actually selected/read through the current KGSL command-stream path.

The tracepoint/sysfs path became the more practical profiling direction.