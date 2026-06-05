# Daily Log — Turnip/KGSL Performance Counter Investigation

## Tasks Completed

Today I investigated whether Mesa Turnip can be used as a userspace path to access Adreno A8XX GPU performance counters through the KGSL backend.

I first confirmed that the target A8XX performance counters exist in Mesa’s Freedreno counter definitions, including:

- `A8XX_PERF_CP_BUSY_CYCLES`
- `A8XX_PERF_PC_US_BUSY_CYCLES`
- `A8XX_PERF_VFD_BUSY_CYCLES`

I then inspected Mesa’s Turnip performance-query implementation and found that `VK_KHR_performance_query` is connected to Freedreno’s generic performance-counter tables through `fd_perfcntrs()`. I also confirmed that the Turnip KGSL backend explicitly disables selectable performance counters by setting `device->is_perf_cntr_selectable = false`.

To test whether this software gate could be bypassed, I patched Mesa/Turnip to experimentally force-enable KGSL performance-counter selection and bypass the gen8/A8XX restriction. I committed the patch locally under:

```
experiment/kgsl-a8xx-perfcraw
commit 30124cafb4a
Experimentally enable KGSL A8XX raw performance queries
```

I then built an Android ARM64 Turnip KGSL driver successfully using the Android NDK. The build produced:

```
build-android/src/freedreno/vulkan/libvulkan_freedreno.so
```

Finally, I pushed the patched `libvulkan_freedreno.so` to the phone and attempted to run the existing Vulkan probe binaries against it.

## Key Discoveries / Findings

Mesa does contain A8XX performance-counter metadata. The A8XX counters are present in:

```
src/freedreno/registers/adreno/a8xx_perfcntrs.xml
src/freedreno/registers/adreno/a8xx_perfcntrs.json
src/freedreno/perfcntrs/fd8_perfcntr.c
```

Turnip’s Vulkan query-pool code uses `fd_perfcntrs()` for raw performance-query enumeration when `TU_DEBUG=perfcraw` is enabled.

However, Turnip’s KGSL backend currently disables selectable performance counters:

```c
device->is_perf_cntr_selectable = false;
```

There is also a gen8+ restriction in `tu_device.cc`:

```c
device->is_perf_cntr_selectable &= (device->info->chip <= 7);
```

This means that on A8XX/Adreno 830, `VK_KHR_performance_query` is normally not exposed through the KGSL backend, even if `TU_DEBUG=perfcraw` is set.

I patched both software gates experimentally. The Android Turnip KGSL build completed successfully, meaning the patch compiles for the actual Android target.

The existing `vk_probe` binary is linked against Android’s normal `libvulkan.so`, so it still uses the stock Qualcomm Vulkan driver by default. The output confirmed this:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
```

Setting `LD_LIBRARY_PATH` and `LD_PRELOAD` was not enough to force the existing probe to use the patched Turnip library.

Replacing `libvulkan.so` with `libvulkan_freedreno.so` also failed because Turnip is not a full Vulkan loader replacement. It does not provide all loader-level symbols such as `vkEnumerateInstanceVersion`.

Therefore, the current blocker is no longer building Turnip. The current blocker is making the test application actually load the patched Turnip driver instead of the default Qualcomm driver.

## Reproduction Steps

### 1. Inspect A8XX counter definitions

```bash
cd ~/adreno-gpu-profiler/third_party/mesa

grep -R "A8XX_PERF_CP_BUSY_CYCLES" -n src 2>/dev/null || true
grep -R "A8XX_PERF_PC_US_BUSY_CYCLES" -n src 2>/dev/null || true
grep -R "A8XX_PERF_VFD_BUSY_CYCLES" -n src 2>/dev/null || true
```

### 2. Inspect Turnip performance-query and KGSL paths

```bash
grep -R "fd_perfcntrs" -n src/freedreno 2>/dev/null | head -100

grep -R "is_perf_cntr_selectable" -n src/freedreno/vulkan

grep -R "KGSL_PERFCOUNTER_GET\|KGSL_PERFCOUNTER_PUT\|KGSL_PERFCOUNTER_QUERY\|KGSL_PERFCOUNTER_READ" \
  -n src/freedreno/vulkan
```

Important files inspected:

```
src/freedreno/vulkan/tu_query_pool.cc
src/freedreno/vulkan/tu_device.cc
src/freedreno/vulkan/tu_knl_kgsl.cc
src/freedreno/vulkan/msm_kgsl.h
src/freedreno/perfcntrs/fd8_perfcntr.c
```

### 3. Patch Turnip KGSL performance-query gates

Patched:

```
src/freedreno/vulkan/tu_knl_kgsl.cc
src/freedreno/vulkan/tu_device.cc
```

Main change:

```c
device->is_perf_cntr_selectable = true;
```

and bypassed the gen8/A8XX restriction that normally disables selectable counters.

Committed as:

```bash
git checkout -b experiment/kgsl-a8xx-perfcraw
git add src/freedreno/vulkan/tu_knl_kgsl.cc src/freedreno/vulkan/tu_device.cc
git commit -m "Experimentally enable KGSL A8XX raw performance queries"
```

### 4. Build Android ARM64 Turnip KGSL driver

Used an Android cross-build script based on:

```bash
NDK=$HOME/android-ndk-r27d
API=29
HOST=darwin-x86_64
TC=$NDK/toolchains/llvm/prebuilt/$HOST
```

Important Meson options:

```bash
-Dplatforms=android
-Dplatform-sdk-version=29
-Dandroid-stub=true
-Dandroid-strict=false
-Dvulkan-drivers=freedreno
-Dgallium-drivers=
-Dfreedreno-kmds=kgsl
-Dperfetto=false
-Dgles1=disabled
-Dgles2=disabled
-Degl=disabled
-Dglx=disabled
-Dllvm=disabled
-Dshared-llvm=disabled
-Dbuildtype=release
-Db_lto=false
```

Build command:

```bash
ninja -C build-android src/freedreno/vulkan/libvulkan_freedreno.so
```

Build succeeded with:

```
[718/718] Linking target src/freedreno/vulkan/libvulkan_freedreno.so
```

### 5. Push patched Turnip to phone

```bash
adb push build-android/src/freedreno/vulkan/libvulkan_freedreno.so \
  /data/local/tmp/jerry_work/
```

Also copied it into the existing Turnip asset directory:

```bash
adb shell 'cp /data/local/tmp/jerry_work/libvulkan_freedreno.so /data/local/tmp/jerry_work/05_driver_assets/turnip/libvulkan_freedreno.so'

adb shell 'cp /data/local/tmp/jerry_work/libvulkan_freedreno.so /data/local/tmp/jerry_work/05_driver_assets/turnip/vulkan.adreno.so'
```

### 6. Run existing Vulkan probe

```bash
adb shell 'su -c "cd /data/local/tmp/jerry_work && env LD_LIBRARY_PATH=/data/local/tmp/jerry_work:/data/local/tmp/jerry_work/05_driver_assets/turnip TU_DEBUG=perfcraw,startup MESA_VK_DEVICE_SELECT_DEBUG=1 ./02_probe_binaries/vk_probe"'
```

Observed output still showed the Qualcomm vendor driver:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
```

### 7. Check binary linkage

```bash
adb shell 'readelf -d /data/local/tmp/jerry_work/02_probe_binaries/vk_probe | grep -i needed'
```

Result:

```
NEEDED Shared library: [libvulkan.so]
```

This confirms the probe uses Android’s normal Vulkan loader.

### 8. Inspect Turnip symbols

```bash
NDK="$HOME/android-ndk-r27d"
READELF="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf"

$READELF -Ws build-android/src/freedreno/vulkan/libvulkan_freedreno.so \
  | grep -E "vk_icdGetInstanceProcAddr|vkGetInstanceProcAddr|vkEnumerateInstance|HAL_MODULE_INFO|vulkan"

strings build-android/src/freedreno/vulkan/libvulkan_freedreno.so \
  | grep -E "vk_icdGetInstanceProcAddr|vkGetInstanceProcAddr|HAL_MODULE_INFO|Turnip|freedreno|Mesa" \
  | head -100
```

Confirmed Turnip/Mesa strings are present, including:

```
turnip Mesa driver
Mesa 3D Vulkan HAL
Turnip Adreno (TM)
Mesa 26.2.0-devel
freedreno
vkGetInstanceProcAddr
vk_icdGetInstanceProcAddr
```

## Issues / Limitations

The patched Turnip driver was built successfully, but it has not yet been executed by the current probe binaries.

The existing probes are linked against Android’s system `libvulkan.so`, so the Android Vulkan loader still selects the stock Qualcomm driver.

`LD_LIBRARY_PATH` and `LD_PRELOAD` did not force the app to use the patched Turnip library.

Copying `libvulkan_freedreno.so` as `libvulkan.so` caused a linker failure because Turnip is not a complete Vulkan loader replacement and does not export all expected loader-level symbols.

The phone does not have `strace` installed, so direct ioctl tracing is currently unavailable unless a static Android `strace` binary is pushed.

The compute probes also failed to run because their SPIR-V shader files were not present in the expected location:

```
/data/local/tmp/jerry_work/alu.comp.spv
/data/local/tmp/jerry_work/mem.comp.spv
```

This is separate from the Turnip loading issue.

There is still uncertainty about whether the patched performance-query path will actually work once Turnip is loaded. Even if `VK_KHR_performance_query` becomes exposed, actual counter reads may still fail with `EPERM`, `EINVAL`, or device loss because KGSL expects perf-counter management through kernel UAPI.

## Most Likely Next Step

The next practical step is to make a test application actually load the patched Turnip driver instead of the Qualcomm vendor driver.

The most promising approaches are:

1. Build a custom direct Turnip/HAL probe that loads `libvulkan_freedreno.so` or `vulkan.adreno.so` through the correct Android HAL-style entry points.
2. Alternatively, find the previous Turnip loading method used in the project and adapt it to the patched driver.
3. Once the probe reports a Mesa/Turnip driver name instead of the Qualcomm driver name, rerun with:

```bash
TU_DEBUG=perfcraw,startup
```

and check whether:

```
VK_KHR_performance_query
```

is exposed.

If the extension appears, the next step is to enumerate raw A8XX performance counters and test whether the target counters can be queried:

```
A8XX_PERF_CP_BUSY_CYCLES
A8XX_PERF_PC_US_BUSY_CYCLES
A8XX_PERF_VFD_BUSY_CYCLES
```

If the extension does not appear or query execution fails, the likely conclusion is that Turnip’s KGSL backend requires additional implementation of KGSL performance-counter UAPI support before A8XX raw counters can be used.