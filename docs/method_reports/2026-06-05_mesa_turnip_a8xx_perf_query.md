# Investigation Report: A8XX Raw Performance Counter Access through Mesa Turnip on Adreno 830 / KGSL

**Date:** 2026-06-05

**Project:** Mobile GPU Profiling on Adreno / KGSL

**Device:** OnePlus CPH2653 / Adreno 830 / Android 15 / KGSL

**Host:** macOS, Android NDK r27d

**Mesa branch:** local patched Mesa Turnip build under `~/adreno-gpu-profiler/third_party/mesa`

---

## 1. Objective

The goal of this investigation was to determine whether Mesa Turnip could be used as a no-kernel-modification path for reading detailed Adreno A8XX GPU hardware performance counters through `VK_KHR_performance_query`.

Previous direct KGSL ioctl experiments showed that performance counter metadata and reservation paths were partially available, but actual counter reads failed. Therefore, this experiment tested whether the Vulkan performance-query path in Turnip could bypass or avoid the direct `IOCTL_KGSL_PERFCOUNTER_READ` limitation.

The target question was:

> Can patched Mesa Turnip expose and read raw A8XX performance counters on Adreno 830 through the KGSL backend?
> 

---

## 2. High-Level Result

Patched Turnip can be made to expose A8XX raw counter metadata through `VK_KHR_performance_query`, but actual raw counter selection and measurement does not work on this Adreno 830 KGSL stack.

The key finding is:

> The counter list can be enumerated, query pools can be created, command-buffer writeback works, barriers work, and counter-register readback without selector programming works. However, when Turnip writes the A8XX perf-counter select register using `PKT4`, subsequent query result and availability writes do not complete. This matches Mesa’s comment that gen8+ performance counter management should use the KGSL kernel UAPI rather than direct userspace selector-register writes.
> 

This strongly suggests that on this device/kernel, raw A8XX performance counter access is blocked or unsupported through the current Turnip userspace command-stream path.

---

## 3. Background

### 3.1 Direct KGSL ioctl result from earlier testing

Before testing Turnip, a standalone KGSL ioctl probe was used to test the kernel performance counter interface directly.

The result was:

```
IOCTL_KGSL_PERFCOUNTER_QUERY    works
IOCTL_KGSL_PERFCOUNTER_GET      works
IOCTL_KGSL_PERFCOUNTER_READ     fails with EPERM
IOCTL_KGSL_PERFCOUNTER_READ as root also fails with EPERM
```

Example result pattern:

```
GET succeeded
READ failed: EPERM
```

This showed that the kernel exposes counter metadata and allows some reservation/programming operations, but blocks actual value reads.

### 3.2 Mesa Turnip performance-query status

Mesa Turnip already contains `VK_KHR_performance_query` support, but the KGSL/A8XX path is gated off by default.

The relevant Mesa behavior is:

```c
.KHR_performance_query =
   (TU_DEBUG(PERFC) || TU_DEBUG(PERFCRAW)) &&
   device->is_perf_cntr_selectable;
```

In the KGSL backend, selectable performance counters are normally disabled. In addition, Mesa normally restricts performance counter selection to pre-gen8 devices:

```c
device->is_perf_cntr_selectable &= (device->info->chip <= 7);
```

The local patch temporarily bypassed these gates to test whether A8XX raw performance query support could work experimentally.

---

## 4. Driver Switching Method

The Android Vulkan loader did not use the ICD JSON override in the normal shell environment. The working method was to temporarily bind-mount the patched Turnip HAL over Android’s vendor Vulkan HAL.

### 4.1 Push patched Turnip library

```bash
adb push /Users/jerryyun/adreno-gpu-profiler/third_party/mesa/build-android/src/freedreno/vulkan/libvulkan_freedreno.so \
  /data/local/tmp/jerry_work/libvulkan_freedreno.so

adb shell 'mkdir -p /data/local/tmp/jerry_work/05_driver_assets/turnip'

adb shell 'cp /data/local/tmp/jerry_work/libvulkan_freedreno.so /data/local/tmp/jerry_work/05_driver_assets/turnip/libvulkan_freedreno.so'

adb shell 'cp /data/local/tmp/jerry_work/libvulkan_freedreno.so /data/local/tmp/jerry_work/05_driver_assets/turnip/vulkan.adreno.so'
```

### 4.2 Bind-mount Turnip over the vendor HAL

```bash
adb shell 'su -c "umount /vendor/lib64/hw/vulkan.adreno.so" 2>/dev/null || true'

adb shell 'su -c "mount --bind /data/local/tmp/jerry_work/05_driver_assets/turnip/vulkan.adreno.so /vendor/lib64/hw/vulkan.adreno.so"'
```

### 4.3 Verify active Vulkan driver

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./02_probe_binaries/vk_probe | grep -E "driverName|driverID|driverInfo"'
```

Expected Turnip output:

```
driverName: turnip Mesa driver
driverInfo: Mesa 26.2.0-devel
driverID: 18
```

This confirmed that the test binaries were running through patched Turnip rather than the stock Qualcomm Vulkan driver.

---

## 5. Workload Sanity Check under Turnip

After switching to Turnip, the existing three-way Vulkan workloads were run to ensure the driver could execute real compute workloads.

### 5.1 Copy baseline

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./02_probe_binaries/vk_threeway_probe copy /data/local/tmp/jerry_work/00_shaders/copy_baseline.comp.spv 262144 1 8'
```

Result:

```
Verification PASSED for first 1024 elements.
```

### 5.2 ALU-heavy workload

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./02_probe_binaries/vk_threeway_probe alu /data/local/tmp/jerry_work/00_shaders/alu_heavy.comp.spv 262144 2048 8'
```

Result:

```
Verification PASSED for first 1024 elements.
```

### 5.3 Memory-heavy workload

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./02_probe_binaries/vk_threeway_probe mem /data/local/tmp/jerry_work/00_shaders/mem_heavy_clean.comp.spv 262144 512 8'
```

Result:

```
Verification PASSED for first 1024 elements.
```

This confirmed that patched Turnip was functional for normal compute workloads.

---

## 6. Exposing `VK_KHR_performance_query`

A small device-extension probe was created to enumerate device extensions and check whether `VK_KHR_performance_query` was visible.

### 6.1 Build and run extension probe

```bash
cd /Users/jerryyun/adreno_turnip/tools/vk_ext_probe

export NDK=/Users/jerryyun/android-ndk-r27d
export API=29
export HOST_TAG=darwin-x86_64

"$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android${API}-clang++" \
  -std=c++17 -O2 \
  vk_ext_probe.cpp \
  -o vk_ext_probe \
  -lvulkan

adb push vk_ext_probe /data/local/tmp/jerry_work/02_probe_binaries/vk_ext_probe
adb shell 'chmod +x /data/local/tmp/jerry_work/02_probe_binaries/vk_ext_probe'

adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_ext_probe' \
  2>&1 | tee turnip_ext_probe.log
```

### 6.2 Result

The probe reported:

```
VK_KHR_performance_query: YES
```

This means patched Turnip successfully exposed the Vulkan performance-query extension.

---

## 7. Initial Counter Enumeration Failure

The next step was to enumerate raw performance counters through:

```c
vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
```

The first enumeration result was:

```
queue family 0:
  performance counters: 0

queue family 1:
  performance counters: 0
```

Debug instrumentation showed:

```
TU_DEBUG_PERFCRAW=1
chip=8
gpu_id=6850
selectable=1
group_ptr=0x0
group_count=0
```

This meant Turnip entered the raw counter enumeration path, but `fd_perfcntrs(&phydev->dev_id, &group_count)` returned no A8XX groups.

---

## 8. Patch: Add Gen8 Case to `fd_perfcntrs()`

Inspection showed that the A8XX counter table was compiled into Mesa, but `fd_perfcntrs()` did not return it for chip generation 8.

The selector function had cases for older generations but no `case 8`.

### 8.1 Patch

The following case was added to `src/freedreno/perfcntrs/freedreno_perfcntr.c`:

```c
case 8:
   *count = a8xx_num_perfcntr_groups;
   return a8xx_perfcntr_groups;
```

### 8.2 Rebuild

```bash
cd /Users/jerryyun/adreno-gpu-profiler/third_party/mesa

/Users/jerryyun/Library/Python/3.11/bin/ninja \
  -C build-android \
  src/freedreno/vulkan/libvulkan_freedreno.so
```

Then the rebuilt driver was pushed and bind-mounted again.

### 8.3 Result after patch

After the gen8 case patch, raw performance counter enumeration succeeded:

```
queue family 0:
  performance counters: 2490
  passes needed for all counters: 30
```

Example enumerated counter:

```
counter[3]
  name:        PERF_CP_BUSY_CYCLES
  category:    CP
  unit:        GENERIC
  scope:       COMMAND_BUFFER
  storage:     UINT64
```

Important counter indices extracted from the enumeration log:

```
counter[1]   PERF_CP_ALWAYS_COUNT
counter[2]   PERF_CP_BUSY_GFX_CORE_IDLE
counter[3]   PERF_CP_BUSY_CYCLES
counter[590] PERF_UCHE_BUSY_CYCLES
counter[600] PERF_UCHE_READ_REQUESTS_SP
counter[936] PERF_SP_BUSY_CYCLES
counter[937] PERF_SP_ALU_WORKING_CYCLES
```

This proved that A8XX raw counter metadata exists and can be exposed to Vulkan after patching the gen8 selector.

---

## 9. End-to-End Counter Read Probe

A new probe, `vk_perf_read_probe`, was created to test the full Vulkan performance-query flow.

The intended flow was:

```
vkCreateDevice with VK_KHR_performance_query enabled
vkAcquireProfilingLockKHR
vkCreateQueryPool for one raw counter
vkCmdResetQueryPool
vkCmdBeginQuery
submit GPU work
vkCmdEndQuery
vkQueueSubmit
vkQueueWaitIdle
vkGetQueryPoolResults
```

The first target was:

```
counter[3] = PERF_CP_BUSY_CYCLES
```

### 9.1 First read test

```bash
adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_read_probe 3 512 64' \
  2>&1 | tee turnip_perf_read_cp_busy.log
```

### 9.2 Result

The query pool and command submission succeeded:

```
Acquired profiling lock
Created performance query pool
Work submitted and completed
```

But result readback failed:

```
vkGetQueryPoolResults failed: VkResult=2
```

`VkResult=2` corresponds to `VK_TIMEOUT`.

This indicated that the query availability flag was never set.

---

## 10. Submit Info Patch

`VK_KHR_performance_query` expects `VkPerformanceQuerySubmitInfoKHR` to be chained into the queue submit path with the counter pass index.

The submit block was patched to include:

```c
VkPerformanceQuerySubmitInfoKHR perf_submit{};
perf_submit.sType = VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;
perf_submit.counterPassIndex = 0;

VkSubmitInfo submit{};
submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
submit.pNext = &perf_submit;
submit.commandBufferCount = 1;
submit.pCommandBuffers = &cmd;
```

After rebuilding the app, the result was unchanged:

```
Read mode WAIT: VkResult=2 value=0
```

This ruled out a missing submit pass index as the main issue.

---

## 11. Partial and Availability Readback Test

The probe was modified to read results in three modes:

```
WAIT
PARTIAL
PARTIAL + AVAILABILITY
```

### 11.1 Result

```
Read mode WAIT: VkResult=2 value=0
Read mode PARTIAL: VkResult=0 value=0
Read mode PARTIAL+AVAIL: VkResult=0 value=0 availability=0
```

This showed:

```
The query BO remained zero.
The availability flag remained zero.
No real counter value was written.
```

---

## 12. Instrumenting Turnip Raw Query Path

Turnip’s raw performance-query path was instrumented in `tu_query_pool.cc`.

Debug prints confirmed the query was created and mapped correctly.

For `counter[3]`:

```
JERRY_RAW_CREATE:
  requested_counter_index=3
  app_idx=0
  gid=0
  cid=3
  group=CP
  countable=PERF_CP_BUSY_CYCLES
```

The raw begin and end functions were reached:

```
JERRY_RAW_BEGIN:
  query=0
  counter_count=1
  group_count=28
  has_pred_bit=1
  in_renderpass=0

JERRY_RAW_END:
  query=0
  counter_count=1
  group_count=28
  has_pred_bit=1
  in_renderpass=0
```

The final availability write packet was also emitted by Turnip:

```
JERRY_RAW_END_AVAILABLE_WRITE:
  query=0
  available_iova=0x4000059000
```

However, after GPU completion, readback still showed:

```
WAIT: VkResult=2 value=0
PARTIAL: VkResult=0 value=0
PARTIAL+AVAIL: value=0 availability=0
```

This meant that Turnip recorded the expected raw query commands, but the query BO was not updated by the actual GPU execution path.

---

## 13. Isolation Tests

To identify which command caused the failure, several temporary debug modes were added to the raw performance-query begin/end functions.

### 13.1 Availability-only test

This bypassed all raw counter programming and only emitted:

```
result = 0x12345678
availability = 1
```

Result:

```
Read mode WAIT: VkResult=0 value=305419896
Read mode PARTIAL: VkResult=0 value=305419896
```

`305419896` is decimal for `0x12345678`.

Conclusion:

```
The query BO is valid.
GPU command-stream writes to the query BO work.
CPU readback works.
Availability writeback works.
```

### 13.2 Barrier-only test

This added `emit_counter_barrier()` but still skipped counter select/read operations. It wrote:

```
result = 0xabcdef01
availability = 1
```

Result:

```
Read mode WAIT: VkResult=0 value=2882400001
Read mode PARTIAL: VkResult=0 value=2882400001
```

`2882400001` is decimal for `0xabcdef01`.

Conclusion:

```
emit_counter_barrier() is safe on A8XX/KGSL.
The failure is not caused by WFI or CP_BARRIER.
```

### 13.3 Read-only test without selector programming

This skipped the select register write, but performed:

```
CP_REG_TO_MEM from counter_lo=0x1b6
CP_MEM_TO_MEM result accumulation
availability = 1
```

Result:

```
Read mode WAIT: VkResult=0 value=0
Read mode PARTIAL: VkResult=0 value=0
```

Conclusion:

```
CP_REG_TO_MEM from the counter register does not break the command stream.
It returns zero when the selector has not been programmed, which is expected.
```

### 13.4 Select-only test with magic availability

This performed only the select-register write in begin, then skipped counter reads and wrote a magic result in end:

```
PKT4 select_reg = selector
result = 0x87654321
availability = 1
```

For `counter[3]`, the mapping was:

```
select_reg = 0x8d3
selector   = 0x3
counter_lo = 0x1b6
```

Result:

```
Read mode WAIT: VkResult=2 value=0
Read mode PARTIAL: VkResult=0 value=0
PARTIAL+AVAIL: value=0 availability=0
```

Conclusion:

```
The failure starts when the command stream writes the A8XX perf-counter select register.
```

### 13.5 Select-only comparison across CP selectors

The select-only test was repeated for three CP countables:

```
counter[1] = PERF_CP_ALWAYS_COUNT
counter[2] = PERF_CP_BUSY_GFX_CORE_IDLE
counter[3] = PERF_CP_BUSY_CYCLES
```

All three mapped to the same physical counter slot:

```
select_reg = 0x8d3
counter_lo = 0x1b6
```

with different selector values:

```
counter[1] selector = 0x1
counter[2] selector = 0x2
counter[3] selector = 0x3
```

All three failed in the same way:

```
Read mode WAIT: VkResult=2 value=0
Read mode PARTIAL: VkResult=0 value=0
availability=0
```

Conclusion:

```
The issue is not one bad selector value.
Writing the CP perf-counter select register itself appears to prevent later query writeback from completing.
```

---

## 14. A8XX Counter Metadata Inspection

The generated A8XX counter table is populated. For CP counters, the generated metadata includes:

```
PERF_CP_ALWAYS_COUNT        = 1
PERF_CP_BUSY_GFX_CORE_IDLE  = 2
PERF_CP_BUSY_CYCLES         = 3
```

The generated CP counter register table includes:

```
{ 0x08d1, {}, 0x01b2, 0x01b3 }
{ 0x08d2, {}, 0x01b4, 0x01b5 }
{ 0x08d3, {}, 0x01b6, 0x01b7 }
```

This matches the runtime mapping from the raw query debug output:

```
select_reg = 0x8d3
selector   = 0x3
counter_lo = 0x1b6
```

Therefore, the failure is not because the A8XX metadata is missing or obviously inconsistent.

---

## 15. Mesa KGSL Backend Inspection

Further inspection showed that Turnip’s KGSL backend does not implement a full KGSL-kernel-UAPI-backed Vulkan performance query path.

The only observed `IOCTL_KGSL_PERFCOUNTER_READ` call in the Turnip KGSL code is used for Perfetto/timestamp calibration:

```
KGSL_PERFCOUNTER_GROUP_ALWAYSON
countable = 0
```

No code path was found that connects `VK_KHR_performance_query` to a full KGSL sequence such as:

```
IOCTL_KGSL_PERFCOUNTER_QUERY
IOCTL_KGSL_PERFCOUNTER_GET
IOCTL_KGSL_PERFCOUNTER_READ
IOCTL_KGSL_PERFCOUNTER_PUT
```

Instead, the Vulkan raw query path still uses the older direct command-stream method:

```
PKT4 write select_reg
CP_REG_TO_MEM read counter register
CP_MEM_TO_MEM accumulate result
```

Mesa comments also indicate that gen8+ should use kernel UAPI for performance counter management rather than direct selector-register writes.

---

## 16. Final Interpretation

The investigation produced the following technical result:

```
Patched Turnip can expose A8XX raw performance counter metadata through VK_KHR_performance_query, but it cannot currently read real raw A8XX counter values on this Adreno 830 KGSL path.
```

More specifically:

```
Works:
  - Turnip HAL loading via bind mount
  - Vulkan compute workloads under Turnip
  - VK_KHR_performance_query extension exposure
  - A8XX raw counter enumeration after adding fd_perfcntrs() case 8
  - Query pool creation
  - Profiling lock acquisition
  - Command-buffer submission
  - Query BO writeback using CP_MEM_WRITE
  - emit_counter_barrier()
  - CP_REG_TO_MEM from counter register without selector programming

Fails:
  - Programming the A8XX perf-counter select register from the userspace command stream
  - Real raw counter value measurement
```

The strongest current conclusion is:

> On this Adreno 830 / Android 15 KGSL stack, raw A8XX performance counters are present in Mesa metadata and can be enumerated through patched Turnip, but actual counter selection/readback is blocked or unsupported through Turnip’s current userspace command-stream path. Mesa’s comments indicate that gen8+ performance counter management should use KGSL kernel UAPI, but Turnip does not currently implement a complete KGSL-UAPI-backed `VK_KHR_performance_query` path. Earlier standalone KGSL testing also showed that direct `IOCTL_KGSL_PERFCOUNTER_READ` fails with `EPERM`, even as root.
> 

This suggests that detailed A8XX hardware performance counter values are likely inaccessible on this phone without kernel/vendor support or a more privileged kernel-managed path.

---

## 17. Reproduction Summary

### 17.1 Build patched Turnip

```bash
cd /Users/jerryyun/adreno-gpu-profiler/third_party/mesa

/Users/jerryyun/Library/Python/3.11/bin/ninja \
  -C build-android \
  src/freedreno/vulkan/libvulkan_freedreno.so
```

### 17.2 Push and bind-mount Turnip

```bash
adb push build-android/src/freedreno/vulkan/libvulkan_freedreno.so \
  /data/local/tmp/jerry_work/libvulkan_freedreno.so

adb shell 'mkdir -p /data/local/tmp/jerry_work/05_driver_assets/turnip'

adb shell 'cp /data/local/tmp/jerry_work/libvulkan_freedreno.so /data/local/tmp/jerry_work/05_driver_assets/turnip/vulkan.adreno.so'

adb shell 'su -c "umount /vendor/lib64/hw/vulkan.adreno.so" 2>/dev/null || true'

adb shell 'su -c "mount --bind /data/local/tmp/jerry_work/05_driver_assets/turnip/vulkan.adreno.so /vendor/lib64/hw/vulkan.adreno.so"'
```

### 17.3 Verify Turnip

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./02_probe_binaries/vk_probe | grep -E "driverName|driverID|driverInfo"'
```

Expected:

```
driverName: turnip Mesa driver
driverID: 18
```

### 17.4 Enumerate counters

```bash
adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_enum_probe' \
  2>&1 | tee turnip_perf_enum_after_gen8_patch.log
```

Expected after adding `case 8` to `fd_perfcntrs()`:

```
performance counters: 2490
```

### 17.5 Test counter read

```bash
adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_read_probe 3 512 64' \
  2>&1 | tee turnip_perf_read_cp_busy.log
```

Observed for real raw path:

```
Read mode WAIT: VkResult=2 value=0
Read mode PARTIAL: VkResult=0 value=0
PARTIAL+AVAIL: value=0 availability=0
```

### 17.6 Select-only tests

```bash
adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_read_probe 1 512 64' \
  2>&1 | tee turnip_select_only_cp_always_count.log

adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_read_probe 2 512 64' \
  2>&1 | tee turnip_select_only_cp_busy_gfx_core_idle.log

adb shell 'cd /data/local/tmp/jerry_work && TU_DEBUG=perfcraw,startup ./02_probe_binaries/vk_perf_read_probe 3 512 64' \
  2>&1 | tee turnip_select_only_cp_busy_cycles.log
```

All three failed after writing `select_reg=0x8d3`.

---

## 18. Suggested Next Steps

### 18.1 Short-term project direction

Because raw hardware counters appear blocked or unsupported through the no-kernel-modification Turnip/KGSL path, the practical profiler should continue using available signals:

```
kgsl_pwrstats
gpu_busy
ram_wait
GPU frequency
bus level
tracepoints
command-batch timing
vendor vs Turnip workload comparison
```

These signals have already shown useful ALU-vs-memory separation, especially through `ram_wait_pct`.

### 18.2 Research follow-up

Possible future investigations:

1. Inspect the Android KGSL kernel driver to identify the permission check behind `IOCTL_KGSL_PERFCOUNTER_READ`.
2. Test the same Turnip patches on a different device/kernel where KGSL performance counter reads may be less restricted.
3. Investigate whether Qualcomm’s vendor profiling stack uses a privileged service or trusted path for A8XX counters.
4. Explore whether Perfetto exposes any additional GPU counter tracks through privileged Android tracing infrastructure.
5. Implement a prototype KGSL-UAPI-backed Turnip path only if `IOCTL_KGSL_PERFCOUNTER_READ` can be made to work.

---

## 19. Final Conclusion

This method was examined carefully and progressively:

```
1. Patched Turnip was loaded successfully.
2. Turnip workloads ran correctly.
3. VK_KHR_performance_query was exposed.
4. A8XX raw counter enumeration was enabled by patching fd_perfcntrs().
5. Counter readback infrastructure was validated with magic-value writeback tests.
6. The failure was isolated specifically to programming the A8XX perf-counter select register.
7. Mesa source comments and KGSL behavior indicate gen8+ counters should be managed through kernel UAPI.
8. Direct KGSL testing previously showed the kernel blocks PERFCOUNTER_READ with EPERM, even as root.
```

Therefore, the current conclusion is that **A8XX raw hardware counters are visible in metadata but not practically readable on this phone through the current no-kernel-modification Turnip/KGSL path**.