# KGSL Runtime Counter Probes

This directory contains small Android ARM64 programs used to test Qualcomm KGSL performance-counter ioctls directly.

```text
tools/probes/runtime/kgsl_counter_probe/
├── kgsl_alwayson_0_read_only
├── kgsl_alwayson_0_read_only.cpp
├── kgsl_alwayson_probe
├── kgsl_alwayson_probe.cpp
├── kgsl_get_read_probe
├── kgsl_get_read_probe.cpp
├── kgsl_query_probe
└── kgsl_query_probe.cpp
```

Each extensionless file is a prebuilt Android executable corresponding to the `.cpp` source with the same name.

## Purpose

These probes were used during early profiler development to answer four questions:

1. Can `/dev/kgsl-3d0` be opened from a rooted process?
2. Which KGSL performance-counter groups are exposed by the kernel?
3. How many hardware counter slots are available in each group?
4. Do the `GET`, `READ`, and `PUT` ioctls work for a selected countable?

The probes helped validate the low-level interface later used by:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

They remain diagnostic utilities. The final streamer and sweeper do **not** execute or link these probe binaries.

---

# KGSL ioctl sequence

The probes use performance-counter structures and ioctl numbers derived from Mesa/Freedreno's KGSL interface definitions.

```text
PERFCOUNTER_GET    ioctl 0x38
PERFCOUNTER_PUT    ioctl 0x39
PERFCOUNTER_QUERY  ioctl 0x3A
PERFCOUNTER_READ   ioctl 0x3B
```

A normal allocated-counter test follows:

```text
open /dev/kgsl-3d0
        ↓
PERFCOUNTER_GET
        ↓
PERFCOUNTER_READ
        ↓
PERFCOUNTER_PUT
        ↓
close
```

`GET` requests a hardware counter slot for a group/countable pair.  
`READ` retrieves the current 64-bit counter value.  
`PUT` releases the requested counter slot.  
`QUERY` reports information about a counter group without allocating a countable.

---

# File summary

| Program | Main test | Default target |
|---|---|---|
| `kgsl_query_probe` | Queries all group IDs from `0x00` through `0x38`. | All known groups |
| `kgsl_get_read_probe` | Performs `GET → READ ×10 → PUT`. | SP group `0x0A`, countable `0` |
| `kgsl_alwayson_probe` | Reads without calling `GET` or `PUT`. | ALWAYSON group `0x1B`, countable `0` |
| `kgsl_alwayson_0_read_only` | Generalized allocated or read-only test. | SP group `0x0A`, countable `0` |

---

# `kgsl_query_probe`

## What it does

Opens:

```text
/dev/kgsl-3d0
```

and calls `IOCTL_KGSL_PERFCOUNTER_QUERY` for every group ID from:

```text
0x00 through 0x38
```

For each successful query, it prints:

- hexadecimal group ID;
- known group name;
- `max_counters`;
- the first reported active countables; and
- whether the ioctl succeeded.

The source includes names for groups such as:

```text
CP
RBBM
PC
VFD
HLSQ
VPC
TSE
RAS
UCHE
TP
SP
RB
LRZ
ALWAYSON
```

and later A8xx/BV-related groups.

## Why it is needed

This probe establishes which groups the device kernel exposes and how many counter slots can be active in each group.

That information is important to the sweeper because the sweeper must divide a large group into chunks that fit within the group's hardware counter capacity.

The final sweeper contains its own group and countable tables; it does not run this probe automatically.

## Run

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_query_probe"'
```

## Expected output

```text
[kgsl_query_probe] Opening /dev/kgsl-3d0
[kgsl_query_probe] Sweeping group IDs 0x00 to 0x38

[OK]   group=0x00 CP             max_counters=...
[OK]   group=0x01 RBBM           max_counters=...
[FAIL] group=0x... UNKNOWN       errno=...

[kgsl_query_probe] Summary: success=... fail=...
```

A failed group does not necessarily indicate a profiler error. The kernel may not expose that group on the current GPU.

---

# `kgsl_get_read_probe`

## What it does

Tests one group/countable pair using the full allocation lifecycle:

```text
PERFCOUNTER_GET
10 PERFCOUNTER_READ calls
PERFCOUNTER_PUT
```

The interval between reads is fixed at:

```text
100 ms
```

Default arguments:

```text
groupid   = 0x0A  (SP)
countable = 0
```

## Syntax

```text
kgsl_get_read_probe [groupid] [countable]
```

Arguments are parsed with base detection, so both decimal and hexadecimal values are accepted.

## Examples

Default SP test:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_get_read_probe"'
```

SP group, countable 2:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_get_read_probe 0x0a 2"'
```

ALWAYSON group, countable 0:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_get_read_probe 0x1b 0"'
```

## Expected output

```text
PERFCOUNTER_GET succeeded
returned offset=0x... offset_hi=0x...
sample=000 value=... delta=N/A
sample=001 value=... delta=...
...
PERFCOUNTER_PUT succeeded
```

The returned offsets identify the low and high register locations assigned by KGSL.

## Why it is needed

This is the smallest end-to-end test of the same basic ioctl path used by the final streamer:

- allocate a counter;
- read its value repeatedly;
- calculate deltas; and
- release it.

Use this probe when diagnosing whether a failure comes from:

- opening the KGSL device;
- requesting the counter;
- reading the counter;
- releasing the counter; or
- permissions enforced by the kernel.

---

# `kgsl_alwayson_probe`

## What it does

Reads:

```text
ALWAYSON group = 0x1B
countable      = 0
```

without first calling `PERFCOUNTER_GET`.

It repeatedly issues `PERFCOUNTER_READ`, prints the absolute value and delta, and then closes the KGSL device.

## Syntax

```text
kgsl_alwayson_probe [sample_count] [sleep_us]
```

Defaults:

```text
sample_count = 20
sleep_us     = 100000
```

## Examples

Default capture:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_probe"'
```

Take 100 samples at 10 ms intervals:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_probe 100 10000"'
```

## Why it is needed

The ALWAYSON counter can provide a simple test of whether a kernel permits direct reads from an already available counter source.

It was useful for separating:

```text
READ permission or ABI problems
```

from:

```text
GET allocation problems
```

It is a diagnostic reference, not a substitute for workload-specific counters.

---

# `kgsl_alwayson_0_read_only`

## What it does

This is a generalized version of the `GET/READ/PUT` probe.

It supports two modes:

```text
GET_READ_PUT
READ_ONLY
```

Default target:

```text
SP group = 0x0A
countable = 0
```

Despite its filename, the program is not limited to ALWAYSON. The group and countable are command-line arguments.

Its log prefix remains:

```text
[kgsl_get_read_probe]
```

because it evolved from the general get/read probe.

## Syntax

```text
kgsl_alwayson_0_read_only [groupid] [countable] [read_only]
```

## Examples

Normal allocated SP test:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_0_read_only 0x0a 0"'
```

Direct read of ALWAYSON countable 0:

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_0_read_only 0x1b 0 read_only"'
```

The alias below is also accepted:

```text
--read-only
```

## Why it is needed

This probe allows the same group/countable pair to be tested both:

- with explicit allocation; and
- without allocation.

That comparison helps determine whether a failure is associated with `GET`, `READ`, or ownership of the selected counter.

---

# Prebuilt binaries

The checked binaries are Android ARM64 PIE executables using:

```text
/system/bin/linker64
```

Current uploaded build metadata:

| Binary | Android target recorded in ELF |
|---|---:|
| `kgsl_alwayson_probe` | API 35 |
| `kgsl_get_read_probe` | API 35 |
| `kgsl_query_probe` | API 35 |
| `kgsl_alwayson_0_read_only` | API 29 |

All four are dynamically linked and currently not stripped.

The different API targets are historical build details, not an intentional functional distinction. They may be rebuilt with one common API level that is supported by the target phone.

---

# Build

## Required tools

- Android NDK
- Bash
- ARM64 Android C++ compiler from the NDK
- `adb` for deployment

The sources define the small KGSL ioctl structures locally, so no additional project libraries are required.

## Build all probes on macOS

From the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

export ANDROID_NDK_HOME="$HOME/android-ndk-r27d"
export API=35
export HOST_TAG=darwin-x86_64

CXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android${API}-clang++"

for src in tools/probes/runtime/kgsl_counter_probe/*.cpp; do
  out="${src%.cpp}"

  "$CXX" \
    -std=c++17 \
    -O2 \
    -Wall \
    -Wextra \
    "$src" \
    -o "$out"
done
```

## Build on Linux

Use:

```bash
export HOST_TAG=linux-x86_64
```

with the same commands.

## Reproduce the historical API-29 read-only binary

```bash
CXX29="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android29-clang++"

"$CXX29" \
  -std=c++17 \
  -O2 \
  -Wall \
  -Wextra \
  tools/probes/runtime/kgsl_counter_probe/kgsl_alwayson_0_read_only.cpp \
  -o tools/probes/runtime/kgsl_counter_probe/kgsl_alwayson_0_read_only
```

## Inspect the binaries

```bash
file tools/probes/runtime/kgsl_counter_probe/kgsl_*
```

Optional ELF inspection:

```bash
readelf -h \
  tools/probes/runtime/kgsl_counter_probe/kgsl_query_probe
```

---

# Push to the phone

Create a dedicated device directory:

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/kgsl_counter_probe'
```

Push the binaries:

```bash
adb push \
  tools/probes/runtime/kgsl_counter_probe/kgsl_query_probe \
  tools/probes/runtime/kgsl_counter_probe/kgsl_get_read_probe \
  tools/probes/runtime/kgsl_counter_probe/kgsl_alwayson_probe \
  tools/probes/runtime/kgsl_counter_probe/kgsl_alwayson_0_read_only \
  /data/local/tmp/jerry_work/kgsl_counter_probe/
```

Set executable permissions:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_*'
```

Verify:

```bash
adb shell \
  'ls -lh /data/local/tmp/jerry_work/kgsl_counter_probe'
```

---

# Device preparation

Confirm root and the KGSL device:

```bash
adb shell 'su -c id'

adb shell \
  'su -c "ls -lZ /dev/kgsl-3d0"'
```

Some kernels expose a sysfs switch for performance-counter access:

```bash
adb shell \
  'su -c "
if [ -e /sys/class/kgsl/kgsl-3d0/perfcounter ]; then
  echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter
  cat /sys/class/kgsl/kgsl-3d0/perfcounter
fi
"'
```

Whether this switch exists or is required depends on the vendor kernel.

---

# Recommended diagnostic order

Run the probes in this order:

## 1. Discover groups

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_query_probe"'
```

Confirm which groups return successfully and record each `max_counters` value.

## 2. Test ALWAYSON direct read

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_probe"'
```

This checks the basic `READ` path without allocation.

## 3. Test full allocation lifecycle

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_get_read_probe 0x0a 0"'
```

This checks `GET`, `READ`, and `PUT`.

## 4. Compare allocated and read-only behavior

```bash
adb shell \
  'su -c "/data/local/tmp/jerry_work/kgsl_counter_probe/kgsl_alwayson_0_read_only 0x1b 0 read_only"'
```

Use the result to isolate allocation-related failures.

---

# Connection to the final streamer

The probes established the core behavior later generalized by `adreno_perf_stream`:

```text
probe
    one group
    one countable
    fixed sample count
    terminal output

streamer
    named countables
    one or more active counters
    configurable interval
    continuous sampling
    CSV output
    cleanup and usability features
```

The streamer adds:

- generated Adreno countable tables;
- name-based counter selection;
- fuzzy matching;
- multiple simultaneous counters;
- timestamped CSV output;
- continuous capture;
- workload-oriented operation; and
- more robust cleanup.

---

# Connection to the final sweeper

The query probe helped determine:

- which groups are supported;
- how many counter slots each group exposes; and
- which groups should be included in broad discovery.

The sweeper adds:

- complete generated countable names;
- automatic chunking based on hardware capacity;
- repeated benchmark execution;
- one CSV per chunk;
- benchmark logs;
- metadata;
- summary files; and
- organized sweep directories.

The sweeper does not call `kgsl_query_probe` at runtime.

---

# Expected results

## Successful `QUERY`

Expect:

- multiple `[OK]` group lines;
- plausible `max_counters` values;
- failures for unsupported groups; and
- a final success/failure count.

## Successful `GET`

Expect:

- return code zero;
- low and high register offsets; and
- no `EBUSY`, `EINVAL`, or permission failure.

## Successful `READ`

Expect:

- 64-bit values;
- non-negative deltas for monotonically increasing counters; and
- larger deltas while related GPU activity occurs.

A zero delta can be valid when the selected hardware block is idle.

## Successful `PUT`

Expect:

```text
PERFCOUNTER_PUT succeeded
```

This confirms that the allocated slot was released.

---

# Troubleshooting

## `open` fails

Check:

```bash
adb shell \
  'su -c "ls -lZ /dev/kgsl-3d0"'
```

Possible causes:

- wrong device path;
- insufficient privileges;
- SELinux denial; or
- the device does not use KGSL.

## `QUERY` fails for every group

Confirm:

- the binary is ARM64 Android;
- root access works;
- the ioctl ABI matches the kernel; and
- `/dev/kgsl-3d0` is the active GPU node.

## `GET` fails

Common interpretations include:

- unsupported group or countable;
- no available hardware slot;
- counter already owned;
- performance counters disabled; or
- kernel permission restrictions.

Check group support with `kgsl_query_probe` first.

## `READ` fails after a successful `GET`

This indicates that allocation and reading are controlled separately by the kernel.

Record:

```text
ioctl return value
errno
group ID
countable
Android build
kernel version
```

Then compare with the read-only ALWAYSON test.

## Values remain zero

Possible causes:

- the selected hardware block is idle;
- the countable does not apply to the current workload;
- the shader or benchmark did not execute;
- the counter mapping is incorrect; or
- the kernel returns a valid but inactive counter.

Run a controlled workload and compare multiple countables.

---

# Usage notes

- Run these probes as root.
- Do not run `GET/READ/PUT` probes concurrently with the streamer or sweeper.
- Two tools requesting the same limited counter slots can interfere with each other.
- Always allow the probe to execute `PUT` after a successful `GET`.
- If a process is terminated after `GET`, close/restart the process and verify counter access before continuing.
- Treat group IDs and countables as GPU- and kernel-specific.
- Use the generated A8xx tables in the final tools for human-readable countable names.
- Record the exact binary or source commit when saving diagnostic logs.
