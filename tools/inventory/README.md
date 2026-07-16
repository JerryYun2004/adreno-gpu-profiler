# GPU Interface Inventory Tools

This directory contains host-side Bash scripts for recording the GPU, KGSL, DRM, sysfs, and tracepoint interfaces exposed by an Android device.

```text
tools/inventory/
├── gpu_counter_inventory.sh
└── inventory_kgsl_interfaces.sh
```

These scripts do not collect high-rate performance data. They create a static inventory of the interfaces available on a particular phone, Android build, and kernel.

## Why these tools are needed

Qualcomm KGSL and DRM interfaces vary between:

- devices;
- Adreno generations;
- Android releases;
- vendor kernels; and
- kernel configurations.

Before building or debugging the perf-counter streamer, sweeper, or KGSL trace tools, it is useful to confirm:

- which GPU device nodes exist;
- whether KGSL or DRM interfaces are available;
- which sysfs files are readable;
- which KGSL tracepoints exist;
- what each tracepoint format contains;
- whether tracefs or debugfs is mounted; and
- what device, kernel, and Android build produced the inventory.

The inventory provides a reproducible hardware/software snapshot that can be saved with profiling results.

## Relationship to the streamer and sweeper

The final profiling tools are:

```text
tools/profiling/perfcounter_streamer/adreno_perf_stream
tools/profiling/perfcounter_sweeper/streamer_sweeper
```

The inventory scripts support them indirectly:

```text
inventory scripts
    ↓
discover available KGSL, DRM, sysfs, and trace interfaces
    ↓
confirm device compatibility and useful nodes
    ↓
build/configure streamer, sweeper, samplers, and trace captures
    ↓
collect runtime performance data
```

They are useful for setup, debugging, and documentation, but they are not invoked by the streamer or sweeper during normal captures.

---

# Script summary

| Script | Scope | Root required | Main output |
|---|---|---:|---|
| `gpu_counter_inventory.sh` | Broad device, GPU node, KGSL, DRM, and tracing overview | Not always, but root improves coverage | One human-readable `inventory.txt` |
| `inventory_kgsl_interfaces.sh` | Detailed KGSL sysfs and tracepoint inventory | Yes | Multiple structured text, CSV, and tracepoint-format files |

Use `gpu_counter_inventory.sh` for a quick general snapshot.

Use `inventory_kgsl_interfaces.sh` when detailed KGSL node and tracepoint information is required.

---

# Requirements

## Host

- macOS or Linux
- Bash
- Android Debug Bridge (`adb`)
- standard shell tools:
  - `find`
  - `sort`
  - `sed`
  - `tee`
  - `date`

Check the connection:

```bash
adb devices -l
```

## Android device

For the most complete results:

- Qualcomm Adreno GPU;
- KGSL device interface;
- root access through `su`;
- tracefs mounted at:
  ```text
  /sys/kernel/tracing
  ```
- KGSL sysfs under:
  ```text
  /sys/class/kgsl/kgsl-3d0
  ```

The detailed script also expects the device shell to provide:

```text
find
timeout
cat
tr
sed
sort
```

---

# Initial setup

From the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

chmod +x tools/inventory/*.sh
```

Confirm root access:

```bash
adb shell 'su -c id'
```

Confirm the main KGSL path:

```bash
adb shell \
  'su -c "ls -ld /sys/class/kgsl/kgsl-3d0"'
```

---

# `gpu_counter_inventory.sh`

## Purpose

Creates a broad, human-readable inventory covering both the host and Android device.

It records:

- host date, working directory, and kernel information;
- connected ADB devices;
- Android model, hardware, platform, build fingerprint, release, and SDK;
- GPU device nodes under `/dev/dri` and `/dev/kgsl*`;
- SELinux labels for GPU device nodes;
- KGSL sysfs paths;
- top-level readable KGSL sysfs values;
- DRM sysfs paths;
- DRM driver and uevent information; and
- debugfs and tracefs availability.

## Default output location

```text
logs/perfcounter_access/00_inventory/<timestamp>/inventory.txt
```

Example:

```text
logs/perfcounter_access/00_inventory/20260716_142500/inventory.txt
```

## Run

```bash
./tools/inventory/gpu_counter_inventory.sh
```

## Custom output root

Set `OUT_ROOT` before running:

```bash
OUT_ROOT=results/device_inventory \
  ./tools/inventory/gpu_counter_inventory.sh
```

This creates:

```text
results/device_inventory/<timestamp>/inventory.txt
```

## Expected output sections

The generated text file contains sections such as:

```text
=== Host ===
=== ADB device ===
=== Android identity ===
=== GPU device nodes ===
=== SELinux labels for GPU device nodes ===
=== KGSL sysfs ===
=== KGSL readable sysfs values ===
=== DRM sysfs ===
=== DRM driver/module hints ===
=== Debugfs/tracing availability ===
```

## When to use it

Use this script when:

- documenting a new phone;
- checking whether `/dev/kgsl-3d0` exists;
- determining whether DRM render nodes are exposed;
- checking SELinux labels;
- comparing vendor kernels;
- recording the environment before profiler development; or
- diagnosing why a probe cannot open a GPU device.

## Limitations

- Most commands run without `su`, so protected values may be missing.
- It reads only top-level files directly under:
  ```text
  /sys/class/kgsl/kgsl-3d0/
  ```
  for the readable-value section.
- The output is optimized for human inspection rather than automated parsing.
- `set -u` is enabled, but `set -e` is not; individual command failures may appear in the log while the script continues.

---

# `inventory_kgsl_interfaces.sh`

## Purpose

Creates a detailed, structured inventory of KGSL-related interfaces using root access.

It records:

- all KGSL sysfs files up to four levels deep;
- readable KGSL sysfs values;
- all KGSL tracefs files;
- KGSL tracepoint names;
- the full `format` file for every KGSL tracepoint;
- files under `/sys/kernel/gpu`; and
- readable values under `/sys/kernel/gpu`.

## Run

Use the default timestamped output directory:

```bash
./tools/inventory/inventory_kgsl_interfaces.sh
```

Or provide an explicit output directory:

```bash
./tools/inventory/inventory_kgsl_interfaces.sh \
  results/kgsl_interface_inventory/oneplus_adreno830
```

## Default output location

```text
kgsl_interface_inventory/<timestamp>/
```

## Expected output structure

```text
kgsl_interface_inventory/<timestamp>/
├── kgsl_sysfs_files.txt
├── kgsl_readable_nodes_inventory.csv
├── kgsl_tracefs_files.txt
├── kgsl_tracepoint_names.txt
├── sys_kernel_gpu_files.txt
├── sys_kernel_gpu_inventory.csv
└── tracepoint_formats/
    ├── adreno_cmdbatch_done.format
    ├── adreno_cmdbatch_queued.format
    ├── gpu_frequency.format
    ├── kgsl_gpubusy.format
    └── ...
```

The exact filenames under `tracepoint_formats/` depend on the kernel.

## KGSL sysfs inventory

The script searches:

```text
/sys/class/kgsl/kgsl-3d0
```

to a maximum depth of four.

It saves the path list to:

```text
kgsl_sysfs_files.txt
```

Readable values are saved to:

```text
kgsl_readable_nodes_inventory.csv
```

CSV columns:

```text
path,value
```

## Excluded nodes

Potentially disruptive write-oriented nodes are skipped, including:

```text
driver/bind
driver/unbind
snapshot/force_panic
snapshot/minidump_test
snapshot/dump
```

This prevents the inventory process from intentionally reading known dangerous or test-oriented interfaces.

## Read timeout

Each candidate node is read with a short timeout:

```text
0.2 seconds
```

This reduces the chance that an unusual sysfs file blocks the inventory indefinitely.

## Tracepoint inventory

The script searches:

```text
/sys/kernel/tracing/events/kgsl
```

It records:

```text
kgsl_tracefs_files.txt
kgsl_tracepoint_names.txt
```

For each event, it saves:

```text
/sys/kernel/tracing/events/kgsl/<event>/format
```

to:

```text
tracepoint_formats/<event>.format
```

These format files are important because they document:

- event field names;
- field offsets;
- field sizes;
- signedness;
- event IDs; and
- the kernel-side print format.

This information is useful when writing parsers for captured KGSL traces.

## Global GPU-node inventory

The script also examines:

```text
/sys/kernel/gpu
```

It saves the file list to:

```text
sys_kernel_gpu_files.txt
```

and readable values to:

```text
sys_kernel_gpu_inventory.csv
```

This may reveal additional global GPU busy or accounting nodes outside the main KGSL sysfs tree.

## When to use it

Use this script when:

- porting the profiler to another Qualcomm phone;
- identifying available KGSL tracepoints;
- writing or updating trace parsers;
- finding candidate sysfs signals for live sampling;
- comparing kernel versions;
- confirming whether a tracepoint used by a capture script exists; or
- archiving the complete KGSL interface exposed by a device.

## Limitations

- It assumes tracefs is mounted at:
  ```text
  /sys/kernel/tracing
  ```
  and does not fall back to `/sys/kernel/debug/tracing`.
- It requires root for most useful results.
- Some nodes may change as a side effect of being read.
- Some files may still be skipped because of permissions, timeouts, or device-specific behavior.
- The script uses `|| true` around many capture steps, so missing interfaces may produce empty files rather than stopping the run.
- CSV escaping should be checked if a node value contains embedded quotation marks or unusual binary data.
- The script inventories the interfaces at one moment; dynamic values are not sampled over time.

---

# Recommended workflow

## 1. Record a general device snapshot

```bash
cd /Users/jerryyun/adreno-gpu-profiler

OUT_ROOT=results/device_inventory/general \
  ./tools/inventory/gpu_counter_inventory.sh
```

## 2. Record detailed KGSL interfaces

```bash
./tools/inventory/inventory_kgsl_interfaces.sh \
  results/device_inventory/kgsl_$(date +%Y%m%d_%H%M%S)
```

## 3. Inspect important files

```bash
find results/device_inventory -maxdepth 3 -type f | sort
```

Check available tracepoints:

```bash
cat results/device_inventory/kgsl_*/kgsl_tracepoint_names.txt
```

Search for useful busy and frequency interfaces:

```bash
grep -Ei \
  'busy|load|freq|clock|power|bus' \
  results/device_inventory/kgsl_*/kgsl_sysfs_files.txt
```

Inspect specific tracepoint fields:

```bash
cat \
  results/device_inventory/kgsl_*/tracepoint_formats/kgsl_gpubusy.format
```

## 4. Use the inventory to configure capture tools

Compare the available tracepoint names against scripts under:

```text
tools/capture/
```

Confirm that the required events exist before running a capture.

Examples:

```text
adreno_cmdbatch_queued
adreno_cmdbatch_submitted
adreno_cmdbatch_retired
kgsl_gpubusy
kgsl_pwrstats
gpu_frequency
kgsl_buslevel
```

## 5. Save the inventory with profiling results

For reproducibility, record:

- phone model;
- Android build fingerprint;
- kernel version;
- GPU device nodes;
- KGSL sysfs inventory;
- tracepoint list and formats;
- profiler commit;
- benchmark commit; and
- Vulkan driver information.

---

# Expected findings

A Qualcomm KGSL device will commonly expose:

```text
/dev/kgsl-3d0
/sys/class/kgsl/kgsl-3d0
/sys/kernel/tracing/events/kgsl
```

Some devices may also expose:

```text
/dev/dri/card0
/dev/dri/renderD128
/sys/class/drm
/sys/kernel/gpu
```

The exact interfaces are vendor- and kernel-dependent.

A missing path does not automatically mean the GPU is unsupported. It may indicate:

- the device uses a different interface;
- permissions block access;
- tracefs is mounted elsewhere;
- a kernel feature is disabled; or
- the vendor kernel exposes the information through another path.

---

# Output management

Inventory outputs can be large, especially the tracepoint format directory.

A practical repository layout is:

```text
results/device_inventory/
├── general/
└── kgsl/
```

Example:

```bash
OUT_ROOT=results/device_inventory/general \
  ./tools/inventory/gpu_counter_inventory.sh

./tools/inventory/inventory_kgsl_interfaces.sh \
  results/device_inventory/kgsl/$(date +%Y%m%d_%H%M%S)
```

Before committing generated inventories, check their size:

```bash
du -sh results/device_inventory
find results/device_inventory -type f | wc -l
```

Large or device-specific inventories may be better stored as experiment artifacts rather than permanent source files.

---

# Troubleshooting

## No device appears

```bash
adb kill-server
adb start-server
adb devices -l
```

Confirm USB debugging is enabled and authorize the host on the phone.

## Root commands fail

```bash
adb shell 'su -c id'
```

Expected output includes:

```text
uid=0(root)
```

## KGSL path is missing

```bash
adb shell \
  'find /sys/class -maxdepth 3 -iname "*kgsl*" 2>/dev/null'
```

Also inspect:

```bash
adb shell \
  'ls -l /dev/kgsl* /dev/dri/* 2>/dev/null'
```

## Tracepoint directory is empty

Check tracefs:

```bash
adb shell 'su -c "
mount | grep -E \"tracefs|debugfs\"
ls -ld /sys/kernel/tracing /sys/kernel/debug/tracing 2>/dev/null
"'
```

The detailed inventory script may need to be updated if the phone uses:

```text
/sys/kernel/debug/tracing
```

## CSV contains only a header

Possible causes include:

- root access failed;
- no readable nodes were found;
- `timeout` is unavailable on the device;
- the sysfs path differs;
- SELinux denied access; or
- the shell command failed but was ignored by `|| true`.

Check:

```bash
adb shell 'su -c "
which timeout
find -L /sys/class/kgsl/kgsl-3d0 -maxdepth 2 -type f | head
"'
```

---

# Notes

- Treat inventory output as device- and kernel-specific.
- Regenerate the inventory after an Android or kernel update.
- Do not write to unknown sysfs or debugfs nodes while exploring.
- The inventory scripts are designed to read interfaces only.
- Runtime behavior must be measured separately using capture, streamer, or sweeper tools.
