I first inspected the GPU device interfaces exposed by the Android phone. The phone exposes both a DRM path and a KGSL path:

```
/dev/dri/card0
/dev/dri/renderD128
/dev/kgsl-3d0
/sys/class/kgsl/kgsl-3d0
```

From KGSL, I confirmed the actual GPU model:

```
Adreno830v2
```

I also confirmed KGSL exposes useful coarse profiling/status nodes, including:

```
gpu_busy_percentage
gpubusy
gpuclk
gpu_clock_stats
gpu_available_frequencies
perfcounter
```

The GPU clock and maximum clock were readable, and the available frequency table was visible. The `perfcounter` node exists, but reading it only returned `0`, so it is probably a control/interface node rather than a simple text dump of all counters.

I then checked the DRM path more carefully. Although `/dev/dri/renderD128` exists, it resolves to:

```
/sys/devices/platform/soc/ae00000.qcom,mdss_mdp
```

with:

```
DRIVER=msm_drm
OF_NAME=qcom,mdss_mdp
OF_COMPATIBLE_0=qcom,sde-kms
```

This suggests the visible DRM node is associated with the Qualcomm display/KMS subsystem, not clearly the Adreno GPU execution engine.

## Mesa/Freedreno source investigation

I uploaded the relevant Mesa tree structure. From that, we identified the key source areas:

```
src/tool/pps/
src/freedreno/ds/
src/freedreno/perfcntrs/
src/freedreno/drm/
src/freedreno/vulkan/tu_knl_kgsl.cc
src/freedreno/vulkan/msm_kgsl.h
```

The tree showed Mesa has PPS files:

```
src/tool/pps/pps_producer.cc
src/tool/pps/pps_driver.cc
src/tool/pps/pps_device.cc
```

and Freedreno PPS files:

```
src/freedreno/ds/fd_pps_driver.cc
src/freedreno/ds/fd_pps_a6xx.cc
src/freedreno/ds/fd_pps_a7xx.cc
```

but no obvious:

```
src/freedreno/ds/fd_pps_a8xx.cc
```

That suggests Mesa may have A8xx counter definitions, but the PPS implementation may only explicitly handle A6xx/A7xx right now.

I also confirmed Mesa has detailed A8xx counter definitions. The A8xx counter config includes groups such as:

```
CP, RBBM, PC, VFD, HLSQ, UCHE, TP, SP, RB, VSC, CCU, LRZ, CMP, UFC
```

These are the detailed hardware-block counters we care about, much more detailed than coarse KGSL nodes like `gpu_busy_percentage` or `gpuclk`.

We also found that Mesa’s KGSL header contains performance-counter ioctl definitions such as:

```
IOCTL_KGSL_PERFCOUNTER_GET
IOCTL_KGSL_PERFCOUNTER_PUT
IOCTL_KGSL_PERFCOUNTER_QUERY
IOCTL_KGSL_PERFCOUNTER_READ
```

and that `tu_knl_kgsl.cc` uses KGSL-related performance counter structures in the Turnip KGSL path.

## Mesa build work

I built Mesa’s current `pps-producer` on the CloudLab machine.

The build initially failed because:

```
meson was too old
glslangValidator was missing
libdrm was missing
Wayland dependencies were missing
```

I fixed these step by step by using a Python virtual environment with a newer Meson, installing `glslangValidator`, installing `libdrm-dev`, and then switching to a minimal Mesa configuration without X11/Wayland platform support.

The successful Meson configuration used:

```
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

Meson succeeded and reported:

```
Perfetto: Enabled YES
Vulkan Drivers: freedreno
Gallium Drivers: freedreno
```

Then I built:

```
ninja-C build src/tool/pps/pps-producer
```

and successfully produced:

```
build/src/tool/pps/pps-producer
```

## Runtime test result

I ran `pps-producer` unchanged with `strace` on CloudLab.

The important result is that it opened:

```
/lib/x86_64-linux-gnu/libdrm.so.2
/dev/dri
```

but did **not** try to open:

```
/dev/kgsl-3d0
```

It inspected the CloudLab server’s DRM device, which was `mgag200`, then failed with:

```
Failed to find any driver
```

This is expected because CloudLab does not have an Adreno GPU.

I also tried:

```
./build/src/tool/pps/pps-producer /dev/dri/renderD128
./build/src/tool/pps/pps-producer msm
```

Both failed. This showed that `pps-producer` treats the command-line argument as a device/driver name, not as a raw device path, and that the binary only reports `msm` as the supported driver.

## Main discoveries

The key discoveries today are:

1. **The phone’s real GPU path appears to be KGSL**, not the visible DRM render node. The KGSL path reports `Adreno830v2`.
2. **The visible DRM node exists, but appears display/KMS-related**, resolving to `qcom,mdss_mdp` / `qcom,sde-kms`.
3. **Mesa already has detailed A8xx counter definitions**, including shader, texture, cache, command processor, and render backend counter groups.
4. **Mesa/Turnip already has KGSL ioctl definitions**, including performance-counter get/query/read/put ioctls.
5. **Current `pps-producer` appears DRM/MSM-oriented**, because the unchanged binary uses `libdrm` and `/dev/dri` discovery and does not directly open `/dev/kgsl-3d0`.
6. **Current Freedreno PPS source appears A6xx/A7xx-oriented**, with no obvious `fd_pps_a8xx.cc`.

## Current interpretation

The project is likely an **interface/compatibility problem**.

The counters exist in Mesa’s A8xx definitions, and KGSL likely provides an ioctl interface to access performance counters. But the current Mesa PPS producer appears to be wired around DRM/MSM discovery, while the Android phone exposes the real Adreno GPU through KGSL.

So the likely missing bridge is:

```
Mesa PPS / Perfetto
        ↓
A8xx counter mapping
        ↓
KGSL-backed counter access path
        ↓
/dev/kgsl-3d0
        ↓
Adreno 830 performance counters
```

## Suggested next steps

The next clean step is to test on the actual phone, not only CloudLab.

First, cross-compile or build an Android/aarch64 version of `pps-producer`, push it to the phone, and run:

```
adb push pps-producer /data/local/tmp/
adb shell
su
cd /data/local/tmp
chmod+x pps-producer

strace-f-e openat,open,ioctl,read,write \
  ./pps-producer \
2>&1 |tee /sdcard/pps_phone_strace.log
```

The key question is whether it opens only:

```
/dev/dri/renderD128
```

or whether it ever opens:

```
/dev/kgsl-3d0
```

In parallel, the next development-focused step should be a **minimal standalone KGSL perfcounter test**, before fully modifying Mesa PPS. The first success target should be:

> Open `/dev/kgsl-3d0`, use KGSL perfcounter ioctls to request/read one simple counter, and verify the value changes under a GPU workload.
> 

Only after that works should I integrate the logic into Mesa PPS and add/adapt A8xx PPS support.