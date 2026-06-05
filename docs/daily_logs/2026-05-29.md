## Daily Log — Adreno GPU Profiling / Turnip / KGSL Tracepoint Investigation

### 1. Confirmed Turnip can be loaded on the phone

I successfully cross-built Mesa/Turnip for Android ARM64 and pushed the driver to the phone under:

```bash
/data/local/tmp/jerry_work/turnip/
```

Important files on the phone:

```bash
/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so
/data/local/tmp/jerry_work/turnip/vulkan.adreno.so
/data/local/tmp/jerry_work/turnip/libc++_shared.so
/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json
```

The Android Vulkan loader did not use the ICD JSON override from the normal shell environment. However, using root and a temporary bind mount worked:

```bash
adb shell 'su -c "mount --bind /data/local/tmp/jerry_work/turnip/vulkan.adreno.so /vendor/lib64/hw/vulkan.adreno.so"'
```

After bind-mounting, `vk_probe` reported:

```
driverName: turnip Mesa driver
driverInfo: Mesa 26.2.0-devel
driverID: 18
```

After unmounting:

```bash
adb shell 'su -c "umount /vendor/lib64/hw/vulkan.adreno.so"'
```

`vk_probe` returned to the vendor driver:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
driverID: 8
```

Conclusion: Turnip can be temporarily loaded on the phone without reinstalling anything, using a root bind mount.

---

### 2. Confirmed direct KGSL performance-counter read is blocked

I tested a direct KGSL ioctl probe using:

```bash
/dev/kgsl-3d0
```

The original probe tested:

```
PERFCOUNTER_GET -> PERFCOUNTER_READ -> PERFCOUNTER_PUT
```

For SP counter group:

```bash
./kgsl_get_read_probe 0x0A 13
```

Result:

```
PERFCOUNTER_GET succeeded
PERFCOUNTER_READ failed: errno=1 (Operation not permitted)
```

I then checked Mesa’s KGSL header and confirmed:

```c
KGSL_PERFCOUNTER_GROUP_SP       = 0x0A
KGSL_PERFCOUNTER_GROUP_ALWAYSON = 0x1B
```

I patched the probe to support a `read_only` mode, matching Turnip’s Perfetto-style path:

```bash
./kgsl_alwayson_0_read_only 0x1B 0 read_only
```

This also failed:

```
PERFCOUNTER_READ ioctl ret=-1 errno=1 (Operation not permitted)
```

I tested both normal shell and root:

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./kgsl_alwayson_0_read_only 0x1B 0 read_only 2>&1'

adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_alwayson_0_read_only 0x1B 0 read_only 2>&1"'
```

Both failed with `EPERM`.

I also tested SP counter as root:

```bash
adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_alwayson_0_read_only 0x0A 13 2>&1"'
```

Result:

```
PERFCOUNTER_GET succeeded
PERFCOUNTER_READ failed: errno=1 (Operation not permitted)
PERFCOUNTER_PUT succeeded
```

Conclusion: KGSL allows reserving/configuring counters through `PERFCOUNTER_GET`, but blocks `PERFCOUNTER_READ`.

---

### 3. Checked whether the block was due to permissions or SELinux

I verified root access:

```bash
adb shell 'su -c id'
```

Result:

```
uid=0(root) gid=0(root) context=u:r:magisk:s0
```

I checked the KGSL device node:

```bash
adb shell 'ls -lZ /dev/kgsl-3d0'
```

The node was accessible and not blocked by normal Unix permissions.

I checked SELinux/logcat/dmesg:

```bash
adb logcat -c

adb shell 'su -c "cd /data/local/tmp/jerry_work && ./kgsl_alwayson_0_read_only 0x1B 0 read_only 2>&1"'

adb logcat -d | grep -Ei "avc|denied|kgsl|perfcounter|perf|ioctl|selinux"

adb shell 'su -c "dmesg | grep -Ei \"avc|denied|kgsl|perfcounter|perf|ioctl\" | tail -100"'
```

No relevant AVC denial or dmesg error was found.

The phone was also already in SELinux permissive mode during later checks, but the read still failed.

Conclusion: The `PERFCOUNTER_READ` block is likely inside the KGSL kernel driver logic/policy, not normal file permissions, shell/root user privileges, or visible SELinux enforcement.

---

### 4. Explored available KGSL debugfs/sysfs paths

I checked:

```bash
adb shell 'su -c "find /sys/kernel/debug/kgsl/kgsl-3d0 -maxdepth 2 -type f -o -type d | sort"'
```

The phone exposes debugfs nodes such as:

```
/sys/kernel/debug/kgsl/kgsl-3d0/active_cnt
/sys/kernel/debug/kgsl/kgsl-3d0/events
/sys/kernel/debug/kgsl/kgsl-3d0/globals
/sys/kernel/debug/kgsl/kgsl-3d0/ctx
/sys/kernel/debug/kgsl/kgsl-3d0/preemption
```

But no obvious raw performance-counter debugfs node was exposed.

I also checked basic GPU sysfs status:

```bash
adb shell 'su -c "cat /sys/kernel/gpu/gpu_busy"'
adb shell 'su -c "cat /sys/kernel/gpu/gpu_clock"'
adb shell 'su -c "cat /sys/kernel/gpu/gpu_freq_table"'
adb shell 'su -c "cat /sys/kernel/gpu/gpu_model"'
```

Confirmed GPU model:

```
Adreno830v2
```

Conclusion: sysfs/debugfs exposes basic GPU status but not detailed raw SP/UCHE/TP counters.

---

### 5. Confirmed KGSL tracepoints work

I listed KGSL tracepoints:

```bash
adb shell 'su -c "find /sys/kernel/tracing/events/kgsl -maxdepth 2 -type f -name format | sort"'
```

Many useful tracepoints exist, including:

```
adreno_cmdbatch_queued
adreno_cmdbatch_submitted
adreno_cmdbatch_retired
adreno_cmdbatch_done
kgsl_issueibcmds
kgsl_mem_alloc
kgsl_mem_map
kgsl_mem_free
kgsl_gpubusy
kgsl_pwrstats
kgsl_pwrlevel
gpu_frequency
kgsl_buslevel
```

A first `vk_probe` trace produced no events because `vk_probe` only enumerates Vulkan properties and does not submit meaningful GPU work.

I then generated UI activity remotely through ADB and successfully captured KGSL events:

```bash
adb shell 'input keyevent KEYCODE_WAKEUP'
adb shell 'input swipe 500 1600 500 400 300'
adb shell 'input swipe 500 400 500 1600 300'
```

Using `kgsl_gpubusy`, I parsed GPU busy samples:

```
31.81%
48.72%
24.64%
47.04%
20.44%
```

Average:

```
34.53%
```

Frequency samples showed the GPU scaling between:

```
160 MHz, 222 MHz, 342 MHz
```

Conclusion: KGSL tracepoints are usable as a no-kernel-modification profiling path.

---

### 6. Built a focused KGSL trace script

I created and ran:

```bash
~/adreno_turnip/run_focused_kgsl_trace_ui.sh
```

This script enables selected KGSL tracepoints, generates ADB UI activity, captures the trace, filters useful lines, and summarizes event counts.

Focused trace result:

```
1819 adreno_cmdbatch_queued
1359 adreno_cmdbatch_done
1347 adreno_cmdbatch_submitted
1347 adreno_cmdbatch_ready
1336 adreno_cmdbatch_retired
257  kgsl_pwrstats
154  kgsl_mem_free
152  kgsl_mem_map
113  kgsl_mem_alloc
61   kgsl_buslevel
20   kgsl_pwr_set_state
14   kgsl_pwr_request_state
3    kgsl_gpubusy
2    kgsl_context_create
2    kgsl_pwrlevel
2    gpu_frequency
1    kgsl_mem_sync_cache
```

Conclusion: The tracepoints expose command-batch lifecycle, memory activity, power/frequency changes, and coarse busy/RAM-wait statistics.

---

### 7. Built and fixed a KGSL trace parser

I created:

```bash
~/adreno_turnip/parse_focused_kgsl_trace.py
```

It parses:

```
adreno_cmdbatch_retired
adreno_cmdbatch_submitted
kgsl_pwrstats
kgsl_gpubusy
gpu_frequency
kgsl_pwrlevel
kgsl_buslevel
kgsl_mem_alloc/map/free
```

I fixed the memory usage regex so it correctly captures strings like:

```
VK/others( 38)
```

instead of truncating them.

Final parser output showed:

```
submitted batches: 1347
retired batches:   1336
avg active:         19954.8 ticks
max active:         106326 ticks
avg queue_to_start: 21320.2 ticks
max queue_to_start: 243191 ticks
avg gmu_latency:    263.9 ticks
max gmu_latency:    944 ticks
```

Per-context retired batch counts:

```
ctx=8: 548
ctx=3: 423
ctx=18: 363
ctx=20: 2
```

Power stats:

```
kgsl_pwrstats samples: 257
avg busy_pct:          59.80%
max busy_pct:          99.76%
avg ram_wait_pct:      22.19%
max ram_wait_pct:      82.37%
avg context_count:     2.99
max context_count:     4
```

Memory activity by usage:

```
egl_image        522.12 MiB
surface           69.09 MiB
VK/others( 38)    38.05 MiB
gl                 5.08 MiB
any(0)             1.98 MiB
texture            1.88 MiB
command            1.05 MiB
VK/others( 32)     0.12 MiB
```

Conclusion: The parser now produces useful profiling summaries from KGSL trace logs.

---

### 8. Current overall conclusion

Direct raw hardware performance counters are still blocked:

```
IOCTL_KGSL_PERFCOUNTER_READ -> EPERM
```

This happens even as root and with SELinux permissive, so it is likely blocked by KGSL driver-internal policy.

However, a practical no-kernel-modification profiling path exists using KGSL tracepoints. This gives:

```
command-batch lifecycle timing
per-context command counts
GPU active/busy time
RAM wait proxy through kgsl_pwrstats
frequency and power-level transitions
bus-level changes
GPU memory allocation/map/free behavior
```

This is not equivalent to raw SP/UCHE/TP hardware counters, but it is a usable profiling baseline.

---

### 9. Immediate next step

The next step is to replace noisy UI activity with a controlled Vulkan workload.

Planned workflow:

```
1. Build or locate a simple Vulkan compute workload.
2. Run focused KGSL tracing around it using the vendor Qualcomm driver.
3. Bind-mount Turnip.
4. Run the exact same workload under Turnip.
5. Compare:
   - submitted/retired command batches
   - average and max active ticks
   - kgsl_pwrstats busy percentage
   - ram_wait percentage
   - memory allocation by usage
   - frequency and power-level changes
```

This will turn the current tracepoint exploration into a reproducible profiler experiment.

## What I did today

Today I continued investigating GPU profiling on the Adreno 830 phone. Since direct `KGSL_PERFCOUNTER_READ` is blocked with `EPERM`, I focused on using KGSL tracepoints as a no-kernel-modification profiling path.

I first confirmed that I can cleanly switch between the vendor Qualcomm Vulkan driver and Mesa Turnip using a root bind mount. I verified the active driver using `vk_probe`.

I then moved from noisy UI-triggered tracing to controlled Vulkan compute workloads. I used an existing ALU-heavy workload:

```bash
vk_compute_probe alu.comp.spv
```

and built a new memory-heavy workload:

```bash
vk_mem_probe mem.comp.spv
```

The memory-heavy workload was created by copying the existing Vulkan compute probe, adding a new memory-heavy shader, and patching the CPU verifier so the output could be checked correctly.

Both workloads passed verification under both the vendor driver and Turnip.

## What I discovered

The KGSL tracepoint path works well enough to collect mid-level GPU profiling data, including:

```
command-batch queued/submitted/retired/done events
active command-batch time
queue-to-start latency
GMU latency
GPU busy percentage
RAM-wait percentage
GPU frequency
bus-level changes
memory allocation/free events
```

This is more detailed than basic KGSL status nodes such as `gpu_busy` or `gpu_clock`, but still much less detailed than true hardware performance counters.

The main result is that `kgsl_pwrstats ram_wait_pct` seems directionally useful. It increased significantly when switching from the ALU-heavy workload to the memory-heavy workload.

Steady-state summary:

```
Vendor + ALU:
  batches:     2
  avg active:  ~1.56M ticks
  busy:        ~65.61%
  ram_wait:    ~3.37%
  freq:        1.1 GHz

Vendor + memory:
  batches:     2
  avg active:  ~3.14M ticks
  busy:        ~80.79%
  ram_wait:    ~44.68%
  freq:        1.1 GHz

Turnip + ALU:
  batches:     1
  avg active:  ~4.13M ticks
  busy:        ~99.43%
  ram_wait:    ~38.63%
  freq:        1.1 GHz

Turnip + memory:
  batches:     1
  avg active:  ~11.98M ticks
  busy:        ~88.54%
  ram_wait:    ~72.58%
  freq:        1.1 GHz
```

Important ratios:

```
Vendor memory vs ALU:
  active time: 2.02x
  ram_wait:    13.27x

Turnip memory vs ALU:
  active time: 2.90x
  ram_wait:    1.88x

Turnip vs vendor on ALU:
  active time: 2.65x
  ram_wait:    11.47x

Turnip vs vendor on memory:
  active time: 3.81x
  ram_wait:    1.62x
```

The key interpretation is that the tracepoint profiler can distinguish ALU-heavy and memory-heavy workloads at the driver/kernel level. The memory-heavy workload caused much higher RAM-wait and longer active time under both drivers.

I also found that Turnip consistently uses fewer command batches than the vendor driver, but shows longer active time and higher RAM-wait percentage. Both drivers stayed at 1.1 GHz during the steady-state comparisons, so the difference was not caused by GPU frequency changes.

## How to reproduce

### 1. Confirm the active driver

Vendor driver:

```bash
adb shell 'su -c "umount /vendor/lib64/hw/vulkan.adreno.so" 2>/dev/null || true'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_probe | grep -E "driverName|driverID"'
```

Expected:

```
driverName: Qualcomm Technologies Inc. Adreno Vulkan Driver
driverID: 8
```

Turnip:

```bash
adb shell 'su -c "mount --bind /data/local/tmp/jerry_work/turnip/vulkan.adreno.so /vendor/lib64/hw/vulkan.adreno.so"'
adb shell 'cd /data/local/tmp/jerry_work && ./vk_probe | grep -E "driverName|driverID"'
```

Expected:

```
driverName: turnip Mesa driver
driverID: 18
```

### 2. Run the ALU-heavy workload

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_compute_probe alu.comp.spv'
```

Expected:

```
Verification PASSED for first 1024 elements.
```

### 3. Run the memory-heavy workload

```bash
adb shell 'cd /data/local/tmp/jerry_work && ./vk_mem_probe mem.comp.spv'
```

Expected:

```
Verification PASSED for first 1024 elements.
```

### 4. Run KGSL tracepoint trials

For ALU workload:

```bash
cd ~/adreno_turnip

for i in 1 2 3 4 5; do
  echo "=== ALU trial $i ==="
  ./run_focused_kgsl_trace_compute.sh
  cp kgsl_focused_trace_compute.log compute_trace_$i.log
  cp kgsl_focused_trace_compute_filtered.log compute_trace_filtered_$i.log
  python3 parse_focused_kgsl_trace.py --input compute_trace_filtered_$i.log > compute_summary_$i.txt
done
```

For memory workload:

```bash
cd ~/adreno_turnip

for i in 1 2 3 4 5; do
  echo "=== Memory trial $i ==="
  ./run_focused_kgsl_trace_mem.sh
  cp kgsl_focused_trace_mem.log mem_trace_$i.log
  cp kgsl_focused_trace_mem_filtered.log mem_trace_filtered_$i.log
  python3 parse_focused_kgsl_trace.py --input mem_trace_filtered_$i.log > mem_summary_$i.txt
done
```

For the full experiment, run the above once under the vendor driver and once under Turnip, saving the files as:

```
vendor_compute_summary_*.txt
turnip_compute_summary_*.txt
vendor_mem_summary_*.txt
turnip_mem_summary_*.txt
```

### 5. Aggregate results

```bash
cd ~/adreno_turnip
python3 summarize_trials.py | tee trial_comparison_summary.txt
```

This produces the steady-state comparison table and useful ratios.

## Files created or updated

Important generated files:

```
run_focused_kgsl_trace_compute.sh
run_focused_kgsl_trace_mem.sh
parse_focused_kgsl_trace.py
summarize_trials.py
trial_comparison_summary.txt
vendor_compute_summary_*.txt
turnip_compute_summary_*.txt
vendor_mem_summary_*.txt
turnip_mem_summary_*.txt
```

Important workload files:

```
support_sw/vulkan_compute_probe/
support_sw/vulkan_mem_probe/
support_sw/kgsl_counter_probe/
support_sw/vulkan_probe/
```

## Current conclusion

Direct detailed performance-counter reads are still blocked, but the KGSL tracepoint path is now a working intermediate profiler. It provides useful driver/kernel-level signals and can distinguish ALU-heavy and memory-heavy workloads. It should be treated as a mid-level profiling method, not a replacement for raw hardware counters.