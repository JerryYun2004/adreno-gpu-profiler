# Investigation Report: Mesa PPS / Perfetto Path for Adreno 830 Counter Collection

**Date range:** 2026-05-26 to 2026-05-27

**Project:** Mobile GPU Profiling on Adreno / KGSL

**Method:** Mesa PPS / Perfetto Freedreno datasource

**Status:** Blocked as a direct path on this phone

---

## 1. Objective

The goal of this method was to determine whether Mesa’s existing PPS / Perfetto infrastructure could be used to collect detailed Adreno GPU performance counters from the Android phone without writing a new profiling backend from scratch.

The intended path was:

```
Mesa PPS / Perfetto
        ↓
Freedreno datasource
        ↓
A8XX counter metadata
        ↓
GPU counter access
        ↓
Perfetto trace / live profiling output
```

This was attractive because Mesa already contains Freedreno counter definitions and existing PPS code for GPU counter sampling.

---

## 2. Device Interface Discovery

The first step was to inspect which GPU-related device interfaces the Android phone exposes.

The phone exposes:

```
/dev/dri/card0
/dev/dri/renderD128
/dev/kgsl-3d0
/sys/class/kgsl/kgsl-3d0
```

The KGSL path reports the real GPU model:

```
Adreno830v2
```

KGSL also exposes useful coarse status/profiling nodes:

```
gpu_busy_percentage
gpubusy
gpuclk
gpu_clock_stats
gpu_available_frequencies
perfcounter
```

However, the visible DRM render node does not appear to correspond to a Freedreno GPU render node. The DRM path resolves to the Qualcomm display/KMS subsystem:

```
/sys/devices/platform/soc/ae00000.qcom,mdss_mdp
DRIVER=msm_drm
OF_NAME=qcom,mdss_mdp
OF_COMPATIBLE_0=qcom,sde-kms
```

This suggests that `/dev/dri/renderD128` is associated with display/KMS infrastructure, not necessarily the Adreno GPU execution engine.

---

## 3. Mesa Source Inspection

The relevant Mesa source areas were identified as:

```
src/tool/pps/
src/freedreno/ds/
src/freedreno/perfcntrs/
src/freedreno/drm/
src/freedreno/vulkan/tu_knl_kgsl.cc
src/freedreno/vulkan/msm_kgsl.h
```

Important PPS files include:

```
src/tool/pps/pps_producer.cc
src/tool/pps/pps_driver.cc
src/tool/pps/pps_device.cc
```

Freedreno PPS-related files include:

```
src/freedreno/ds/fd_pps_driver.cc
src/freedreno/ds/fd_pps_a6xx.cc
src/freedreno/ds/fd_pps_a7xx.cc
```

There was no obvious:

```
src/freedreno/ds/fd_pps_a8xx.cc
```

This suggested that Mesa may have A8XX counter definitions, but the PPS datasource implementation may not have an explicit A8XX path.

Mesa does contain detailed A8XX counter definitions, including groups such as:

```
CP
RBBM
PC
VFD
HLSQ
UCHE
TP
SP
RB
VSC
CCU
LRZ
CMP
UFC
```

These are much more detailed than the coarse KGSL sysfs signals and are the type of counters needed for low-level bottleneck analysis.

---

## 4. Mesa PPS Build Work

Mesa’s PPS producer was built on the CloudLab machine.

The build initially failed due to missing or outdated dependencies:

```
Meson too old
glslangValidator missing
libdrm missing
Wayland dependencies missing
```

These issues were fixed by:

```
Using a Python virtual environment with newer Meson
Installing glslangValidator
Installing libdrm-dev
Switching to a minimal Mesa configuration without X11/Wayland platform support
```

The successful Meson configuration used:

```bash
-Dperfetto=true
-Ddatasources=freedreno
-Dtools=drm-shim,freedreno
-Dgallium-drivers=freedreno
-Dvulkan-drivers=freedreno
-Dplatforms=[]
-Dglx=disabled
-Degl=disabled
-Dgbm=disabled
-Dgles1=disabled
-Dgles2=disabled
-Dopengl=false
-Dbuildtype=debugoptimized
```

The build succeeded and produced:

```
build/src/tool/pps/pps-producer
```

---

## 5. Runtime Test on CloudLab

The unchanged `pps-producer` was run with `strace` on CloudLab.

Observed behavior:

```
Opened /lib/x86_64-linux-gnu/libdrm.so.2
Opened /dev/dri
Did not open /dev/kgsl-3d0
```

The program inspected the CloudLab server’s DRM device, which was `mgag200`, then failed with:

```
Failed to find any driver
```

This was expected because CloudLab does not have an Adreno GPU.

Additional tests were attempted:

```bash
./build/src/tool/pps/pps-producer /dev/dri/renderD128
./build/src/tool/pps/pps-producer msm
```

Both failed. This showed that the program treats the argument as a device or driver name, not as a raw device path.

The binary only reported `msm` as a supported driver, but the test environment did not expose a Freedreno-compatible GPU DRM node.

---

## 6. Why This Method Failed

This method failed because the current Mesa PPS producer appears to be built around DRM/MSM device discovery, while the Android phone exposes the real Adreno GPU primarily through KGSL.

The key mismatch is:

```
Mesa PPS expects:
  /dev/dri render node usable through Freedreno/MSM DRM

Phone provides:
  real GPU path through /dev/kgsl-3d0
  visible /dev/dri/renderD128 appears display/KMS-related
```

Therefore, the existing PPS producer does not naturally attach to the Adreno 830 execution engine on this Android device.

The likely missing bridge is:

```
Mesa PPS / Perfetto
        ↓
A8XX counter metadata
        ↓
KGSL-backed counter access
        ↓
/dev/kgsl-3d0
        ↓
Adreno 830 hardware counters
```

At the time of testing, this bridge did not exist in the PPS producer path.

---

## 7. Evidence Supporting the Failure Mode

The phone’s KGSL path clearly identifies the GPU as:

```
Adreno830v2
```

The phone’s DRM node resolves to:

```
qcom,mdss_mdp
qcom,sde-kms
msm_drm
```

The unchanged PPS producer uses:

```
libdrm
/dev/dri discovery
```

and does not attempt to open:

```
/dev/kgsl-3d0
```

Therefore, the failure is not because Mesa lacks all A8XX counter metadata. The failure is that the existing PPS runtime path is not wired to the phone’s KGSL GPU interface.

---

## 8. Reproduction Steps

### 8.1 Inspect phone GPU interfaces

```bash
adb shell 'ls -l /dev/dri /dev/dri/* /dev/kgsl-3d0'
adb shell 'readlink -f /sys/class/kgsl/kgsl-3d0'
adb shell 'cat /sys/class/kgsl/kgsl-3d0/gpu_model 2>/dev/null || true'
adb shell 'cat /sys/kernel/gpu/gpu_model 2>/dev/null || true'
```

### 8.2 Inspect DRM node identity

```bash
adb shell 'readlink -f /sys/class/drm/renderD128/device'
adb shell 'cat /sys/class/drm/renderD128/device/uevent'
```

Expected observation:

```
DRIVER=msm_drm
OF_COMPATIBLE=qcom,sde-kms
```

### 8.3 Build Mesa PPS producer

```bash
cd ~/adreno-gpu-profiler/third_party/mesa

meson setup build \
  -Dperfetto=true \
  -Ddatasources=freedreno \
  -Dtools=drm-shim,freedreno \
  -Dgallium-drivers=freedreno \
  -Dvulkan-drivers=freedreno \
  -Dplatforms=[] \
  -Dglx=disabled \
  -Degl=disabled \
  -Dgbm=disabled \
  -Dgles1=disabled \
  -Dgles2=disabled \
  -Dopengl=false \
  -Dbuildtype=debugoptimized

ninja -C build src/tool/pps/pps-producer
```

### 8.4 Run PPS producer with syscall tracing

```bash
strace -f -e openat,open,ioctl,read,write \
  ./build/src/tool/pps/pps-producer \
  2>&1 | tee pps_producer_strace.log
```

Expected observation on CloudLab:

```
Opens /dev/dri
Does not open /dev/kgsl-3d0
Fails to find suitable driver
```

---

## 9. Conclusion

The Mesa PPS / Perfetto method was examined and built successfully, but it is not directly usable on this phone in its current form.

The method failed because the phone’s actual Adreno GPU path is KGSL-based, while the tested PPS producer is DRM/Freedreno-discovery-oriented. The visible DRM node on the phone appears to correspond to Qualcomm display/KMS rather than a Freedreno-compatible GPU render node.

This method is therefore blocked unless a KGSL-backed PPS/Freedreno datasource is implemented or an Android build of PPS can be adapted to open and sample `/dev/kgsl-3d0` directly.

---

## 10. Practical Follow-Up

This method motivated the next investigation:

```
Build a minimal standalone KGSL performance-counter ioctl probe.
```

The reason was that before modifying Mesa PPS, it was necessary to determine whether the KGSL kernel interface could directly query, reserve, and read hardware counters.

That led to the standalone KGSL ioctl method report.