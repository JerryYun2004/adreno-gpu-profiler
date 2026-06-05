# Daily Log — Mobile GPU Profiling / Adreno 830 KGSL + Vulkan Investigation

## Goal

Today I followed up on the supervisor’s suggestion to avoid modifying Mesa first. The goal was to first create a simple GPU workload that runs directly on the phone, then test whether performance counters can be accessed through the existing Android/KGSL path.

The phone under test is:

```
Model: OnePlus CPH2653
Board: sun
Android version: 15
SDK/API: 35
ABI: arm64-v8a
Hardware: qcom
GPU: Adreno 830 / Adreno830v2
```

The main question was whether a standalone userspace program can use `/dev/kgsl-3d0` to access detailed KGSL performance counters while a Vulkan workload is running.

---

## 1. Confirmed Vulkan support on the phone

I first checked the phone’s Vulkan support using Android system commands. The phone exposes Vulkan through the Android Vulkan loader and reports the following driver:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
deviceName: Adreno (TM) 830
apiVersion: Vulkan 1.3.284
```

The phone also reports Vulkan compute support:

```
feature:android.hardware.vulkan.compute
feature:android.hardware.vulkan.level=1
feature:android.hardware.vulkan.version=4206592
```

Important note: the Vulkan path tested today used the **stock Qualcomm vendor Vulkan driver**, not Mesa/Turnip.

---

## 2. Installed Android NDK on CloudLab

The CloudLab machine initially did not have a usable Android NDK configured. I installed Android NDK r28 under:

```
/users/JerryYun/tools/android-ndk-r28
```

Then I set:

```bash
export ANDROID_NDK_HOME=/users/JerryYun/tools/android-ndk-r28
export PATH=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
```

Verified compiler:

```bash
which aarch64-linux-android35-clang++
aarch64-linux-android35-clang++ --version
```

Result:

```
/users/JerryYun/tools/android-ndk-r28/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang++
Target: aarch64-unknown-linux-android35
```

This matches the phone’s ABI and Android API level:

```
ABI: arm64-v8a
API: 35
```

---

## 3. Built and ran a basic Vulkan probe

I wrote and cross-compiled a small native Android binary called:

```
vk_probe
```

Purpose:

```
1. Create a Vulkan instance.
2. Enumerate Vulkan physical devices.
3. Print device name, vendor ID, device ID, API version, queue families, and timestamp support.
```

Build command on CloudLab:

```bash
cd ~/adreno-gpu-profiler/support_sw/vulkan_probe

aarch64-linux-android35-clang++ \
  -std=c++17 \
  -O2 \
  main.cpp \
  -o vk_probe \
  -lvulkan
```

Verified output binary:

```bash
file vk_probe
```

Result:

```
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked, interpreter /system/bin/linker64
```

Since the phone is connected to my local Mac, I copied the binary from CloudLab to Mac with `scp`, then pushed it to the phone:

```bash
scp -i ~/.ssh/id_ed25519_cloudlab \
  JerryYun@c220g5-110413.wisc.cloudlab.us:/users/JerryYun/adreno-gpu-profiler/support_sw/vulkan_probe/vk_probe \
  ~/adreno_phone_bins/

adb push ~/adreno_phone_bins/vk_probe /data/local/tmp/jerry_work/
adb shell chmod +x /data/local/tmp/jerry_work/vk_probe
adb shell /data/local/tmp/jerry_work/vk_probe
```

Output confirmed:

```
[vk_probe] Physical device count: 1
name: Adreno (TM) 830
vendorID: 0x5143
deviceID: 0x44050001
apiVersion: 1.3.284
queue families: 3
queue[0]: GRAPHICS COMPUTE TRANSFER
queue[1]: COMPUTE
queue[2]: TRANSFER-like / sparse flag
```

This confirmed that a command-line native Android binary can load Vulkan and see Adreno 830.

---

## 4. Updated `vk_probe` to print Vulkan driver identity

I updated `vk_probe` to query `VK_KHR_driver_properties` using `VkPhysicalDeviceDriverProperties`.

This was done to distinguish between the stock Qualcomm Vulkan driver and any future Mesa/Turnip driver.

The updated probe prints:

```
driverName
driverInfo
driverID
conformanceVersion
```

Current output:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
driverInfo: Driver Build: e1e25c2793, I95c8bc5752, 1733222351
Date: 12/03/24
Compiler Version: E031.47.18.13
driverID: 8
conformanceVersion: 1.3.0.8
```

This gives a clean baseline. If Mesa/Turnip is successfully loaded later, this `driverName` should change.

---

## 5. Built and ran a Vulkan compute workload

I created a headless Vulkan compute workload:

```
vk_compute_probe
```

Files:

```
support_sw/vulkan_compute_probe/alu.comp
support_sw/vulkan_compute_probe/alu.comp.spv
support_sw/vulkan_compute_probe/main.cpp
```

The shader is ALU-heavy and does repeated integer arithmetic per element:

```glsl
#version 450
layout(local_size_x = 256) in;
...
for (uint i = 0u; i < pc.iters; i++) {
    x = x * 1664525u + 1013904223u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    x *= 3266489917u;
    x ^= x >> 16;
    x += i ^ idx;
}
```

Compiled shader:

```bash
glslangValidator -V alu.comp -o alu.comp.spv
```

Output:

```
alu.comp.spv: Khronos SPIR-V binary, little-endian
```

Compiled Android binary:

```bash
aarch64-linux-android35-clang++ \
  -std=c++17 \
  -O2 \
  main.cpp \
  -o vk_compute_probe \
  -lvulkan
```

Transferred to the phone:

```bash
scp -i ~/.ssh/id_ed25519_cloudlab \
  JerryYun@c220g5-110413.wisc.cloudlab.us:/users/JerryYun/adreno-gpu-profiler/support_sw/vulkan_compute_probe/vk_compute_probe \
  ~/adreno_phone_bins/vulkan_compute_probe/

scp -i ~/.ssh/id_ed25519_cloudlab \
  JerryYun@c220g5-110413.wisc.cloudlab.us:/users/JerryYun/adreno-gpu-profiler/support_sw/vulkan_compute_probe/alu.comp.spv \
  ~/adreno_phone_bins/vulkan_compute_probe/

adb push ~/adreno_phone_bins/vulkan_compute_probe/vk_compute_probe /data/local/tmp/jerry_work/
adb push ~/adreno_phone_bins/vulkan_compute_probe/alu.comp.spv /data/local/tmp/jerry_work/
adb shell chmod +x /data/local/tmp/jerry_work/vk_compute_probe
```

Ran default workload:

```bash
/data/local/tmp/jerry_work/vk_compute_probe
```

Output:

```
[vk_compute_probe] Starting Vulkan compute workload
[vk_compute_probe] SPIR-V: /data/local/tmp/jerry_work/alu.comp.spv
[vk_compute_probe] elements=262144 alu_iters=512 dispatch_repeats=64
[vk_compute_probe] Using device: Adreno (TM) 830
[vk_compute_probe] Using queue family 1, flags=0x2
[vk_compute_probe] Submitting workload...
[vk_compute_probe] Workload complete.
[vk_compute_probe] Verifying output...
[vk_compute_probe] Verification PASSED for first 1024 elements.
[vk_compute_probe] Done.
```

This confirmed that a compute shader can run correctly on Adreno 830 from a command-line Android binary.

---

## 6. Verified KGSL coarse activity during Vulkan workload

I monitored KGSL sysfs nodes during the Vulkan workload:

```bash
while true; do
  echo "---- $(date) ----"
  adb shell cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
  adb shell cat /sys/class/kgsl/kgsl-3d0/gpubusy
  adb shell cat /sys/class/kgsl/kgsl-3d0/gpuclk
  sleep 0.5
done
```

During the workload:

```
gpu_busy_percentage: 98–99 %
gpubusy: nonzero
gpuclk: 1100000000
```

After workload completion:

```
gpu_busy_percentage: 0 %
gpubusy: 0 0
gpuclk: 222000000
```

This confirms the Vulkan compute workload is actually activating the GPU and causing KGSL busy/clock counters to respond.

---

## 7. Checked KGSL device permissions and system state

KGSL device node:

```bash
ls -l /dev/kgsl-3d0
```

Result:

```
crw-rw-rw- 1 system system 458, 0 /dev/kgsl-3d0
```

DRM nodes:

```bash
ls -l /dev/dri /dev/dri/*
```

Result:

```
/dev/dri/card0
/dev/dri/renderD128
```

Both are also world-readable/writable at the file-permission level.

Kernel and build information:

```bash
uname -a
cat /proc/version
getprop ro.bootimage.build.fingerprint
getprop ro.vendor.build.fingerprint
getprop ro.build.fingerprint
```

Result:

```
Linux localhost 6.6.30-android15-8-gb5f0c188ea2a-ab12656338-4k
Android 15
OnePlus/CPH2653EEA/OP5D55L1:15/AP3A.240617.008/V.R4T3.1c0bb8c-55cf-20dbb:user/release-keys
```

SELinux:

```bash
getenforce
```

Result:

```
Permissive
```

KGSL sysfs perfcounter node:

```bash
ls -l /sys/class/kgsl/kgsl-3d0/perfcounter
cat /sys/class/kgsl/kgsl-3d0/perfcounter
```

Result:

```
-rw-rw--w- 1 root shell ... /sys/class/kgsl/kgsl-3d0/perfcounter
0
```

This suggests the sysfs `perfcounter` node is not a direct dump of detailed counters.

---

## 8. Investigated Mesa KGSL perfcounter ioctl definitions

I inspected Mesa’s KGSL header:

```bash
sed -n '990,1120p' third_party/mesa/src/freedreno/vulkan/msm_kgsl.h
```

This confirmed the ioctl structs:

```c
struct kgsl_perfcounter_get
struct kgsl_perfcounter_put
struct kgsl_perfcounter_query
struct kgsl_perfcounter_read_group
struct kgsl_perfcounter_read
```

and ioctl definitions:

```c
IOCTL_KGSL_PERFCOUNTER_GET   0x38
IOCTL_KGSL_PERFCOUNTER_PUT   0x39
IOCTL_KGSL_PERFCOUNTER_QUERY 0x3A
IOCTL_KGSL_PERFCOUNTER_READ  0x3B
```

I also inspected Turnip KGSL code:

```bash
sed -n '1580,1635p' third_party/mesa/src/freedreno/vulkan/tu_knl_kgsl.cc
```

It reads:

```c
groupid = KGSL_PERFCOUNTER_GROUP_ALWAYSON
countable = 0
IOCTL_KGSL_PERFCOUNTER_READ
```

This gave a good reference for the first standalone KGSL read test.

---

## 9. Confirmed KGSL perfcounter group IDs

I searched Mesa’s KGSL header:

```bash
grep -R "KGSL_PERFCOUNTER_GROUP_ALWAYSON\|PERFCOUNTER_GROUP_ALWAYSON" -n third_party/mesa/src/freedreno/vulkan/msm_kgsl.h third_party/mesa/src/freedreno
grep -n "KGSL_PERFCOUNTER_GROUP" third_party/mesa/src/freedreno/vulkan/msm_kgsl.h | head -80
```

Important group IDs:

```c
#define KGSL_PERFCOUNTER_GROUP_CP        0x00
#define KGSL_PERFCOUNTER_GROUP_RBBM      0x01
#define KGSL_PERFCOUNTER_GROUP_UCHE      0x08
#define KGSL_PERFCOUNTER_GROUP_TP        0x09
#define KGSL_PERFCOUNTER_GROUP_SP        0x0A
#define KGSL_PERFCOUNTER_GROUP_RB        0x0B
#define KGSL_PERFCOUNTER_GROUP_ALWAYSON  0x1B
#define KGSL_PERFCOUNTER_GROUP_MAX       0x39
```

---

## 10. Built standalone KGSL always-on read probe

I wrote a small program:

```
kgsl_alwayson_probe
```

Purpose:

```
1. open("/dev/kgsl-3d0", O_RDWR)
2. call IOCTL_KGSL_PERFCOUNTER_READ
3. read group=ALWAYSON, countable=0
```

Result as normal shell user:

```
[kgsl_alwayson_probe] Opening /dev/kgsl-3d0
[kgsl_alwayson_probe] open succeeded, fd=3
[kgsl_alwayson_probe] Reading ALWAYSON group=0x1b countable=0
[kgsl_alwayson_probe] ioctl READ failed at sample 0: errno=1 (Operation not permitted)
```

Result as root:

```bash
su -c /data/local/tmp/jerry_work/kgsl_alwayson_probe
```

Still failed:

```
ioctl READ failed at sample 0: errno=1 (Operation not permitted)
```

Initial conclusion:

```
Opening /dev/kgsl-3d0 works, but PERFCOUNTER_READ is blocked with EPERM.
This is not fixed by root.
SELinux is permissive, so this is likely an internal KGSL/kernel policy or missing setup requirement.
```

---

## 11. Built standalone KGSL perfcounter query probe

I wrote:

```
kgsl_query_probe
```

Purpose:

```
1. open("/dev/kgsl-3d0")
2. sweep group IDs 0x00 to 0x38
3. call IOCTL_KGSL_PERFCOUNTER_QUERY
4. print max_counters and active countables
```

Result:

```
[kgsl_query_probe] Summary: success=55 fail=2
```

Most important successful groups:

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

Interpretation:

```
KGSL does expose detailed perfcounter group metadata.
The kernel is not blocking all perfcounter ioctls.
QUERY is allowed.
```

The value `4294967295` appears frequently in active countables. This is `0xffffffff`, likely meaning an unused/free counter slot.

---

## 12. Built standalone KGSL get/read/put probe

I wrote:

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

Tested counters:

```
SP   group=0x0a countable=0
CP   group=0x00 countable=0
RBBM group=0x01 countable=6
UCHE group=0x08 countable=13
```

Results:

```
PERFCOUNTER_GET succeeded
PERFCOUNTER_READ failed with errno=1 (Operation not permitted)
```

Examples:

```
SP countable 0:
GET succeeded, offset=0x29c offset_hi=0x29d
READ failed: EPERM

CP countable 0:
GET succeeded, offset=0x1ca offset_hi=0x1cb
READ failed: EPERM

RBBM countable 6:
GET succeeded, offset=0x1d2 offset_hi=0x1d3
READ failed: EPERM

UCHE countable 13:
GET succeeded, offset=0x254 offset_hi=0x255
READ failed: EPERM
```

Retested as root. Same result:

```
GET succeeds
READ fails with EPERM
```

Final conclusion for KGSL standalone probe:

```
open("/dev/kgsl-3d0")                 works
IOCTL_KGSL_PERFCOUNTER_QUERY          works
IOCTL_KGSL_PERFCOUNTER_GET            works
IOCTL_KGSL_PERFCOUNTER_READ           fails with EPERM
IOCTL_KGSL_PERFCOUNTER_READ as root   still fails with EPERM
```

This indicates the permission boundary is specifically at `PERFCOUNTER_READ`.

---

## 13. Current technical interpretation

The results suggest:

```
1. Detailed KGSL performance counters exist on this phone.
2. KGSL exposes metadata for many groups, including SP, UCHE, TP, RB, CP, RBBM, etc.
3. KGSL allows a standalone userspace program to reserve/program counters through GET.
4. KGSL blocks actual counter value reads through READ with EPERM.
5. Root does not fix the READ failure.
6. SELinux is permissive, so this is probably not ordinary SELinux enforcement.
```

Likely explanations:

```
1. The KGSL driver has an internal permission check for PERFCOUNTER_READ.
2. PERFCOUNTER_READ may require a specific KGSL context/session state.
3. PERFCOUNTER_READ may only work through trusted driver paths.
4. The vendor kernel may intentionally block direct userspace counter reads for security/safety.
```

This matches the supervisor’s concern that A8xx counters may exist but may not be exposed directly due to safety constraints.

---

## 14. Mesa/Turnip status

I confirmed that the Vulkan probes used the vendor Qualcomm driver, not Mesa/Turnip.

I updated `vk_probe` to print driver identity, and it currently reports:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
```

I searched the local Mesa build output:

```bash
find . -iname "*.so" | grep -Ei "vulkan|turnip|freedreno|mesa|kgsl|tu" | head -100
find third_party/mesa -iname "*.json" | grep -Ei "vulkan|icd|freedreno|turnip" | head -100
find third_party/mesa/build -maxdepth 5 -type f | grep -Ei "vulkan|turnip|freedreno|\.so|\.json" | head -200
```

Findings:

```
No Android/aarch64 libvulkan_freedreno.so found.
Current Mesa build only has static libraries, generated headers, and source/build artifacts.
```

The Mesa tree does include A8xx counter definitions:

```
src/freedreno/registers/adreno/a8xx_perfcntrs.json
build/src/freedreno/registers/adreno/a8xx_perfcntrs.py
build/src/freedreno/registers/adreno/a8xx_perfcntrs.xml.h
build/src/freedreno/perfcntrs/.../fd8_perfcntr.c.o
```

But this is not enough to run Mesa/Turnip on the phone. We still need to cross-build:

```
src/freedreno/vulkan/libvulkan_freedreno.so
```

for Android ARM64.

---

## 15. Supervisor’s Mesa/Turnip build script interpretation

The supervisor provided a bash script that:

```
1. Enters a Mesa source tree.
2. Defines Android NDK path and target API.
3. Writes a Meson cross file for Android/aarch64.
4. Configures Mesa for Android platform support.
5. Enables only the Freedreno Vulkan driver.
6. Selects KGSL as the Freedreno kernel mode driver.
7. Disables OpenGL/EGL/GLES/GLX and LLVM to simplify the build.
8. Builds src/freedreno/vulkan/libvulkan_freedreno.so.
```

Important options:

```bash
-Dplatforms=android
-Dandroid-stub=true
-Dandroid-strict=false
-Dvulkan-drivers=freedreno
-Dfreedreno-kmds=kgsl
-Dgallium-drivers=
-Dperfetto=false
```

The key output should be:

```
build-android/src/freedreno/vulkan/libvulkan_freedreno.so
```

This is the Mesa/Turnip Vulkan driver library we need to try loading on the phone.

For my environment, the paths should be adapted to:

```bash
cd /users/JerryYun/adreno-gpu-profiler/third_party/mesa

NDK=/users/JerryYun/tools/android-ndk-r28
HOST=linux-x86_64
API=35
TC=$NDK/toolchains/llvm/prebuilt/$HOST
```

---

## 16. Reproduction notes

Phone workspace used:

```
/data/local/tmp/jerry_work
```

Files pushed to the phone:

```
vk_probe
vk_compute_probe
alu.comp.spv
kgsl_alwayson_probe
kgsl_query_probe
kgsl_get_read_probe
```

CloudLab folders used:

```
~/adreno-gpu-profiler/support_sw/vulkan_probe
~/adreno-gpu-profiler/support_sw/vulkan_compute_probe
~/adreno-gpu-profiler/support_sw/kgsl_counter_probe
```

Typical CloudLab build command for Android binaries:

```bash
aarch64-linux-android35-clang++ \
  -std=c++17 \
  -O2 \
  main.cpp \
  -o output_binary \
  -lvulkan
```

For non-Vulkan KGSL ioctl probes:

```bash
aarch64-linux-android35-clang++ \
  -std=c++17 \
  -O2 \
  probe.cpp \
  -o probe_binary
```

Transfer path:

```
CloudLab → local Mac using scp
local Mac → phone using adb push
```

Example:

```bash
scp -i ~/.ssh/id_ed25519_cloudlab \
  JerryYun@c220g5-110413.wisc.cloudlab.us:/users/JerryYun/adreno-gpu-profiler/support_sw/.../binary \
  ~/adreno_phone_bins/...

adb push ~/adreno_phone_bins/.../binary /data/local/tmp/jerry_work/
adb shell chmod +x /data/local/tmp/jerry_work/binary
adb shell /data/local/tmp/jerry_work/binary
```

---

## 17. Main conclusion of the day

Today I successfully built a working Vulkan compute workload for the Adreno 830 phone and verified that it drives the GPU to high utilization through KGSL coarse counters.

I also built standalone KGSL perfcounter ioctl probes. These showed that KGSL exposes detailed counter group metadata and allows counter reservation through `PERFCOUNTER_GET`, but blocks actual counter reading through `PERFCOUNTER_READ` with `EPERM`, even when running as root and with SELinux permissive.

This narrows the problem significantly. The issue is not that counters are absent, and not that `/dev/kgsl-3d0` cannot be opened. The current blocker is specifically the `PERFCOUNTER_READ` ioctl.

Next steps:
```
1. Cross-build Mesa/Turnip for Android ARM64 using the supervisor’s script.
2. Push libvulkan_freedreno.so to /data/local/tmp/jerry_work/mesa_turnip.
3. Try to make vk_probe load Mesa/Turnip and confirm driverName changes.
4. If Mesa/Turnip can run, test whether its KGSL context path can access counters.
5. In parallel, inspect the OnePlus kernel source to find why PERFCOUNTER_READ returns -EPERM.
```