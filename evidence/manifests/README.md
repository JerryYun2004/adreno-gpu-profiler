# Evidence Manifests

This directory contains inventory and integrity records for historical evidence
collected during development of the Adreno GPU profiler.

The manifests describe files stored under:

```text
evidence/raw_logs/
evidence/summaries/
```

and one organized device workspace under:

```text
/data/local/tmp/jerry_work/
```

Most manifests contain:

```text
human-readable file size
SHA-256 digest
repository-relative file path
```

One manifest, `device_jerry_work_manifest_after_organize.txt`, is a directory-tree
snapshot without hashes.

These files are **not runtime configuration**, **not profiler source code**, and
**not consumed by the perf-counter streamer or sweeper**. Their purpose is to
preserve:

- provenance;
- experiment inventory;
- evidence organization;
- file-integrity information;
- historical device state;
- relationships between raw logs and derived summaries; and
- confidence that files were not lost or silently modified during cleanup or
  repository reorganization.

---

# Directory role

The expected location is:

```text
evidence/
├── manifests/
│   ├── README.md
│   ├── 20260605_turnip_perf_query_manifest.txt
│   ├── device_jerry_work_manifest_after_organize.txt
│   ├── direct_counter_access_manifest.txt
│   ├── kgsl_tracepoints_manifest.txt
│   ├── phone_cleanup_old_manifest.txt
│   ├── phone_live_csv_manifest.txt
│   ├── phone_probe_logs_manifest.txt
│   ├── turnip_logcat_manifest.txt
│   └── vendor_turnip_comparison_manifest.txt
├── raw_logs/
│   └── ...
└── summaries/
    └── ...
```

The separation is:

```text
raw_logs/
    primary evidence collected from the phone, kernel, Mesa, logcat, probes,
    samplers, tracing tools, and experimental runs

summaries/
    derived or manually generated reports based on raw evidence

manifests/
    inventories and integrity records describing those evidence files
```

A manifest is metadata about evidence. It is not the evidence itself.

---

# Relationship to the main profiler products

The project’s main runtime products are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

The evidence manifests do not:

- build either tool;
- define Adreno counter names;
- issue KGSL `ioctl` calls;
- start a counter stream;
- run a counter sweep;
- parse profiler CSV files;
- control a Vulkan benchmark; or
- need to be pushed to the phone.

They support the project indirectly by documenting the investigations that led
to the final tools.

The broad development relationship is:

```text
DRM/KGSL/Turnip experiments
        ↓
raw logs and probe outputs
        ↓
summaries and comparisons
        ↓
design decisions for direct KGSL counter access
        ↓
perf-counter streamer and sweeper
        ↓
manifests preserve the supporting evidence
```

## Closest connection to the final profiler

The following manifests are most closely related to the counter-access design:

```text
direct_counter_access_manifest.txt
20260605_turnip_perf_query_manifest.txt
```

They preserve experiments involving:

- DRM MSM performance-counter access;
- KGSL device access;
- KGSL perf-counter `ioctl` behavior;
- Turnip performance-query experiments;
- counter selection;
- direct read attempts;
- availability handling; and
- experimental Mesa changes.

## Supporting validation evidence

These manifests describe evidence used to compare workload classes, drivers, and
coarse GPU behavior:

```text
kgsl_tracepoints_manifest.txt
vendor_turnip_comparison_manifest.txt
phone_live_csv_manifest.txt
phone_probe_logs_manifest.txt
turnip_logcat_manifest.txt
```

## Organization and cleanup evidence

These manifests preserve the state of the phone workspace and files archived
during cleanup:

```text
device_jerry_work_manifest_after_organize.txt
phone_cleanup_old_manifest.txt
```

---

# Status summary

| Manifest | Evidence type | Status | Main purpose |
|---|---|---|---|
| `20260605_turnip_perf_query_manifest.txt` | Raw Mesa/Turnip experiment logs | Historical, important | Records Turnip performance-query investigation |
| `device_jerry_work_manifest_after_organize.txt` | Device directory tree | Historical snapshot | Records organized `/data/local/tmp/jerry_work` layout |
| `direct_counter_access_manifest.txt` | Raw DRM/KGSL probe evidence | Historical, foundational | Records direct counter-access investigation |
| `kgsl_tracepoints_manifest.txt` | Derived tracepoint summaries | Historical, reusable | Inventories vendor/Turnip compute and memory event summaries |
| `phone_cleanup_old_manifest.txt` | Archived phone logs/scripts | Historical archive | Proves files were retained before cleanup |
| `phone_live_csv_manifest.txt` | Compressed live-sampling CSVs | Historical data | Inventories ALU/memory live telemetry captures |
| `phone_probe_logs_manifest.txt` | Probe log | Historical data | Inventories vendor Vulkan probe output |
| `turnip_logcat_manifest.txt` | Turnip logcat captures | Historical debugging data | Records driver loading/runtime diagnostics |
| `vendor_turnip_comparison_manifest.txt` | Derived comparison summaries | Historical, reusable | Inventories repeated vendor-versus-Turnip results |

“Historical” does not mean unimportant. It means the files record a specific
past experiment and should not be treated as the current state of the code,
phone, or driver.

---

# Manifest formats

# Size-and-hash manifest

Most files use this structure:

```text
# Manifest: <name>

Generated: <date and time>
Source: <source location>
Folder: <evidence folder>

## Files
<size>  <sha256>  <repository-relative path>
...
```

Example:

```text
4.0K  515e9531ed087fbbde10e8ec60ad39229bf63e3963493c55f08a6c3bbce9cf72  evidence/raw_logs/phone_probe_logs/vendor_vk_probe.log
```

The fields mean:

```text
4.0K
    human-readable allocated or reported size at manifest-generation time

515e...
    SHA-256 digest of the exact stored file bytes

evidence/raw_logs/phone_probe_logs/vendor_vk_probe.log
    expected repository-relative file path
```

## Size is descriptive, not authoritative

Human-readable values such as:

```text
4.0K
48K
212K
1.5M
0B
```

are useful for spotting unexpected size changes.

They should not be treated as exact byte counts because the original size
command may report:

- allocated disk blocks;
- rounded human-readable values; or
- logical file size, depending on the command used.

The SHA-256 digest is the stronger integrity field.

## SHA-256 meaning

A SHA-256 digest represents the exact byte content of one stored file.

If two files have the same SHA-256 digest, they contained identical bytes at
manifest-generation time.

Duplicate hashes are not automatically errors. They can indicate:

- repeated probe attempts produced identical output;
- files were copied under different names;
- two experiments reached the same result;
- one log was intentionally reused; or
- an empty file was recorded.

## Empty-file hash

The standard SHA-256 hash of an empty file is:

```text
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```

The direct-counter manifest contains an empty `dmesg_after_ioctl.txt` record.
That does not mean the manifest is corrupted; it means the stored file had zero
bytes.

---

# Tree-only manifest

`device_jerry_work_manifest_after_organize.txt` uses a different format:

```text
.
./00_shaders
./00_shaders/alu.comp.spv
...
```

This records paths only.

It does not record:

- hashes;
- sizes;
- permissions;
- owners;
- timestamps;
- symbolic-link targets; or
- binary versions.

Use it to understand organization and confirm expected filenames, not to verify
content integrity.

---

# File reference

# `20260605_turnip_perf_query_manifest.txt`

## Purpose

Inventories a focused Mesa/Turnip performance-query experiment stored under:

```text
evidence/raw_logs/20260605_turnip_perf_query/
```

The source is identified as:

```text
third_party/mesa experiment/kgsl-a8xx-perfcraw
```

This indicates that the logs came from an experimental Mesa branch or working
state related to raw A8xx KGSL performance-counter access.

## Main evidence categories

The manifest records:

```text
Turnip performance-counter enumeration
selected counter lists
direct-read experiments
availability-only experiments
select-only experiments
barrier-only experiments
unconditional-read experiments
strace output
post-Gen8-patch enumeration output
```

Important files include:

```text
selected_perf_counters.txt
turnip_perf_enum_after_gen8_patch.log
turnip_perfquery_strace.txt
turnip_perf_enum_debug.log
turnip_perf_read_raw_path_debug.log
```

## Why it was needed

This experiment helped investigate whether Turnip’s Vulkan performance-query
path could expose or read the desired hardware counters on the A8xx device.

It preserved evidence around questions such as:

- could counters be enumerated?
- could a selected counter be programmed?
- did availability logic block reads?
- did barriers affect the result?
- did unconditional reads behave differently?
- what changed after the Gen8 patch?
- what system calls occurred during the query path?

## Relationship to the final streamer/sweeper

This is not an input to the final tools.

It represents an earlier or parallel route through the Vulkan driver/query
stack.

The final phone-side profiler instead uses direct KGSL perf-counter access,
which avoids requiring the benchmark’s Vulkan driver to expose the desired
query interface.

The manifest remains important because it documents why the project explored
and then moved beyond the Turnip performance-query route.

## Notable integrity observations

The manifest contains duplicate hashes for some differently named logs.

Examples include:

```text
turnip_perf_read_unconditional_pass.log
turnip_perf_read_avail_only.log
```

and:

```text
turnip_select_only_cp_busy_cycles.log
turnip_perf_read_select_only.log
```

This indicates those file pairs were byte-identical at manifest-generation time.

That can be useful evidence that two experimental code paths produced the same
recorded output.

---

# `device_jerry_work_manifest_after_organize.txt`

## Purpose

Records the organized tree of the device workspace:

```text
/data/local/tmp/jerry_work/
```

after a phone-side cleanup and reorganization.

## Recorded top-level layout

The manifest shows:

```text
00_shaders/
01_samplers/
02_probe_binaries/
03_probe_logs/
04_live_csv/
05_driver_assets/
06_cleanup_old/
99_misc/
```

This numeric prefixing made the device workspace easier to navigate.

## Main categories

### `00_shaders/`

Contains SPIR-V workload modules such as:

```text
alu.comp.spv
alu_heavy.comp.spv
copy_baseline.comp.spv
mem.comp.spv
mem_heavy_clean.comp.spv
```

### `01_samplers/`

Contains KGSL/sysfs sampling scripts:

```text
kgsl_all_node_sampler.sh
kgsl_fast_sampler.sh
kgsl_live_sampler.sh
```

### `02_probe_binaries/`

Contains native Android test binaries:

```text
kgsl_alwayson_probe
kgsl_get_read_probe
kgsl_query_probe
vk_compute_probe
vk_mem_probe
vk_probe
vk_threeway_probe
```

### `03_probe_logs/`

Contains selected probe outputs.

### `04_live_csv/`

Contains live ALU/memory sampling CSV files.

### `05_driver_assets/turnip/`

Contains manually deployed Turnip assets:

```text
freedreno_icd.aarch64.json
libc++_shared.so
libvulkan_freedreno.so
vulkan.adreno.so
```

### `06_cleanup_old/`

Contains files moved aside during cleanup, including:

- old sampler scripts;
- PID/stop files;
- CSV outputs;
- stderr/stdout logs;
- trace setup scripts;
- logcat captures; and
- tracepoint state.

### `99_misc/`

Contains the manifest itself.

## Why it was needed

The device workspace accumulated:

- shaders;
- binaries;
- logs;
- samplers;
- Turnip libraries;
- temporary PID files;
- stale CSVs; and
- debugging artifacts.

This tree snapshot preserves the final organized state and helps answer:

- which binary was available on the phone?
- which shader names were deployed?
- where were samplers stored?
- where were Turnip assets located?
- which files were considered old cleanup material?
- what should a restored phone workspace look like?

## Relationship to the final streamer/sweeper

This manifest predates or sits alongside the final product layout.

It documents the broader experimental phone workspace from which the final
profiler workflow evolved.

It is useful when reconstructing old commands or locating historical probe names,
but it should not be treated as the current installation specification for the
streamer or sweeper.

## Limitation

Because this is path-only, it cannot prove that the binaries or shaders in the
tree match current repository sources.

---

# `direct_counter_access_manifest.txt`

## Purpose

Inventories the foundational investigation into direct GPU performance-counter
access.

The evidence is stored under:

```text
evidence/raw_logs/direct_counter_access/
```

## Main experiment groups

The manifest records:

```text
00_inventory/
01_drm_msm_probe/
02_kgsl_open_probe/
03_kgsl_ioctl_probe/
old_notes/
README.md
```

## `00_inventory/`

Records an early environment/device inventory.

## `01_drm_msm_probe/`

Contains experiments against the DRM MSM interface, including:

- device-node permissions;
- sysfs driver identity;
- shell versus root probes;
- performance-counter structure notes;
- performance-counter configuration attempts;
- logcat and dmesg after configuration;
- source-code searches for relevant DRM `ioctl` definitions; and
- repeated retry runs.

## `02_kgsl_open_probe/`

Contains tests for opening the KGSL device and comparing execution contexts.

Typical evidence includes:

```text
kgsl_permissions.txt
kgsl_open_probe.txt
shell_context.txt
```

## `03_kgsl_ioctl_probe/`

Contains direct KGSL `ioctl` experiments, including:

```text
ALWAYSON group reads
SP selector reads
shell/root comparisons
run summary
logcat
dmesg
```

## Why it was needed

The project needed to determine:

- whether `/dev/kgsl-3d0` could be opened;
- whether root changed access behavior;
- whether DRM MSM exposed the required counter interface;
- whether KGSL GET/READ operations were available;
- which groups/selectors produced usable register mappings;
- which operations returned permission errors;
- whether logs or kernel messages revealed failures; and
- which interface should become the basis of the profiler.

## Relationship to the final streamer/sweeper

This is the evidence set most directly connected to the final architecture.

The final profiler’s KGSL counter path was informed by experiments showing that
the project could:

- access KGSL;
- request selected counters;
- obtain register information;
- investigate direct reads; and
- distinguish shell/root behavior.

The manifest is not used at runtime, but it provides the provenance for the
chosen low-level access method.

## Duplicate-content observations

Several repeated probe files have identical hashes.

Examples include repeated:

```text
drm_perfcntr_config_probe_v2_root.txt
shell_context.txt
dev_dri_permissions.txt
drm_sysfs_driver.txt
```

These duplicates show that repeated attempts often captured identical state.

They should not be deduplicated blindly because their paths preserve experiment
time and context.

## Empty log observation

The manifest records:

```text
03_kgsl_ioctl_probe/20260604_123157/dmesg_after_ioctl.txt
```

as zero bytes with the standard empty-file hash.

This means no dmesg content was stored in that file at the time; it is still
valid evidence of an attempted capture.

---

# `kgsl_tracepoints_manifest.txt`

## Purpose

Inventories derived event summaries from KGSL tracepoint captures.

The summaries are stored under:

```text
evidence/summaries/kgsl_tracepoints/
```

with separate directories for:

```text
turnip/
vendor/
```

## Workload categories

Both driver groups contain:

```text
compute event summaries
memory event summaries
```

with several repeated trials.

## Why it was needed

Raw ftrace/tracefs logs are difficult to compare directly.

The summary files condense events such as:

- command-batch submission and retirement;
- power statistics;
- GPU busy samples;
- RAM wait;
- frequency;
- bus activity;
- memory allocation events; and
- per-context activity.

The manifest helps prove which summaries were present and unchanged.

## Relationship to analysis code

These summaries are closely related to:

```text
analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py
analysis/kgsl_trace_analysis/summarize_trials.py
```

The exact source parser should be confirmed from the experiment notes, but the
filenames match the output format expected by the repeated-trial summarizer.

## Relationship to streamer/sweeper

KGSL tracepoint analysis is complementary to hardware perf counters.

It provides:

```text
driver and scheduling behavior
coarse busy/RAM-wait evidence
frequency and bus context
command-batch structure
```

while the streamer/sweeper provides:

```text
Adreno block-level hardware counter deltas
```

The manifest is therefore supporting validation evidence rather than a runtime
dependency.

## Duplicate-content observations

Some repeated compute summaries have identical hashes.

Examples include two Turnip compute summaries and two vendor compute summaries.

This indicates some repeated runs produced exactly identical text summaries.

---

# `phone_cleanup_old_manifest.txt`

## Purpose

Inventories files pulled from the old phone workspace before or during cleanup.

The source phone pull is recorded as:

```text
/Users/jerryyun/adreno_phone_pull_20260605_143858/jerry_work
```

The archived evidence folder is:

```text
evidence/raw_logs/phone_cleanup_old/
```

## File categories

The manifest includes:

```text
compressed logcat logs
compressed sampler stdout/stderr
compressed fast-sampling CSVs
compressed sysfs busy CSVs
compressed all-node samples
compressed tracepoint state
old sampler scripts
old trace setup scripts
```

## Why it was needed

Deleting or reorganizing a phone workspace can destroy useful evidence.

This manifest proves that selected old files were preserved in the repository
before they were removed from or moved within the device workspace.

It also makes it possible to verify that compressed archive files have not
changed.

## Relationship to final products

These files are mostly superseded operational artifacts.

They are not needed to run the final streamer or sweeper, but they preserve:

- early sampling methods;
- early trace setup;
- old device state;
- stdout/stderr from sampler processes; and
- logs useful for diagnosing the evolution of the workflow.

## Compression note

Most data files are stored as:

```text
.gz
```

The recorded hash applies to the compressed bytes, including gzip metadata.

Recompressing identical uncompressed data can produce a different gzip hash.

Therefore:

```text
manifest SHA-256
    verifies the exact stored archive file

gzip -t
    verifies that the gzip stream is structurally readable

decompressed-content hash
    verifies logical content independently of gzip metadata
```

---

# `phone_live_csv_manifest.txt`

## Purpose

Inventories compressed CSV captures from early live KGSL/sysfs sampling.

The files are stored under:

```text
evidence/raw_logs/phone_live_csv/
```

## Recorded workloads

The manifest includes:

```text
alu_live_test.csv.gz
alu_live_long.csv.gz
mem_live_test.csv.gz
mem_live_long.csv.gz
test_fast_samples.csv.gz
```

## Why it was needed

These captures provided early workload-specific telemetry for comparing:

- ALU-heavy activity;
- memory-heavy activity;
- short test captures;
- longer sustained captures; and
- fast sampler behavior.

## Relationship to final products

These CSV files are not streamer CSVs.

They come from earlier live sampling of KGSL/sysfs nodes.

They helped establish coarse differences such as:

```text
ALU workload:
    high GPU busy
    lower RAM wait

memory workload:
    high GPU activity
    stronger RAM-wait behavior
```

The streamer and sweeper later provide more detailed hardware-counter evidence.

## Use today

Use these files for:

- historical reproduction;
- sampler schema inspection;
- comparison with later trace analysis;
- validating old daily logs; and
- checking how the project’s telemetry format evolved.

---

# `phone_probe_logs_manifest.txt`

## Purpose

Inventories one vendor Vulkan probe log:

```text
evidence/raw_logs/phone_probe_logs/vendor_vk_probe.log
```

## Why it was needed

The probe log records the vendor-driver environment seen by a native Vulkan
test program.

Depending on the probe version, it may include:

- Vulkan API version;
- device name;
- driver identity;
- queue-family information;
- extension support; and
- other device properties.

## Relationship to final products

The log provides environment context for benchmarks later observed by the
streamer, sweeper, or KGSL tools.

It is not a profiler input.

## Scope

This manifest contains one file and is intentionally small.

Its value is provenance, not volume.

---

# `turnip_logcat_manifest.txt`

## Purpose

Inventories logcat captures related to deploying and running Mesa Turnip.

The evidence folder is:

```text
evidence/raw_logs/turnip_logcat/
```

## Files

The manifest records:

```text
turnip_bindmount_logcat.txt
turnip_logcat.txt
turnip_logcat_latest.txt
```

## Why it was needed

Manual driver loading can fail because of:

- library dependencies;
- loader configuration;
- bind mounts;
- permissions;
- SELinux restrictions;
- unsupported device behavior;
- Vulkan-loader errors; or
- driver initialization failures.

Logcat provides system/runtime context not always printed by the benchmark.

## Relationship to final products

These logs support vendor-versus-Turnip workload testing.

The streamer/sweeper can collect the same GPU’s counters while either driver
runs the workload, but they do not explain all loader or driver errors.

The logcat evidence fills that gap.

---

# `vendor_turnip_comparison_manifest.txt`

## Purpose

Inventories derived comparison summaries for vendor Vulkan and Mesa Turnip.

The evidence folder is:

```text
evidence/summaries/vendor_turnip_comparison/
```

## Main categories

```text
vendor/
turnip/
kgsl/
trial_comparison_summary.txt
```

## Vendor summaries

Contain repeated:

```text
vendor_compute_summary_*.txt
vendor_mem_summary_*.txt
```

## Turnip summaries

Contain repeated:

```text
turnip_compute_summary_*.txt
turnip_mem_summary_*.txt
```

## KGSL summaries

Include focused trace summaries/analysis for:

```text
UI activity
compute activity
memory activity
```

## Combined trial summary

```text
trial_comparison_summary.txt
```

provides a higher-level comparison across repeated experiments.

## Why it was needed

One run can be influenced by:

- startup;
- warm-up;
- frequency scaling;
- thermal state;
- cache state;
- background activity; and
- driver setup.

Repeated summary files allow analysis of:

- means;
- standard deviations;
- steady-state trials;
- memory-to-ALU ratios;
- Turnip-to-vendor ratios; and
- reproducibility.

## Relationship to analysis code

This evidence is closely related to:

```text
analysis/kgsl_trace_analysis/parse_focused_kgsl_trace.py
analysis/kgsl_trace_analysis/summarize_trials.py
```

The repeated filenames match the historical summarizer’s expected input
patterns.

## Relationship to final products

This comparison helps determine whether profiler observations are:

- workload-dependent;
- driver-dependent;
- stable across trials; or
- affected by command submission and memory behavior.

It does not feed the streamer or sweeper directly.

---

# Why manifests are needed

# 1. Integrity verification

A manifest can detect:

- accidental edits;
- truncation;
- replacement;
- corrupted archives;
- wrong file copies; and
- incomplete migrations.

# 2. Reorganization safety

During repository cleanup, files may be moved into new directories.

Hashes make it possible to show that content survived even when paths changed.

# 3. Provenance

The header records:

- manifest name;
- generation time;
- source workspace;
- evidence destination.

# 4. Historical reproducibility

A researcher can determine which logs and summaries existed at the time of an
experiment.

# 5. Duplicate detection

Identical hashes reveal repeated or copied content.

This is useful for understanding repeated trials, but paths should be retained
when they encode separate experimental contexts.

# 6. Missing-file detection

A manifest lists expected files.

A verification script can report files that no longer exist.

# 7. Evidence/raw-summary separation

Manifests make it easier to distinguish:

```text
primary raw evidence
derived summaries
device inventory snapshots
```

---

# What manifests do not prove

A valid hash match proves:

```text
the current file bytes match the bytes recorded by the manifest
```

It does not prove:

- the original experiment was configured correctly;
- timestamps inside the log are accurate;
- the correct workload ran;
- a summary interpretation is scientifically valid;
- the source path was truthful;
- the device was in the intended thermal state;
- the file was never modified before manifest creation; or
- the manifest itself is independently authenticated.

For stronger provenance, a future workflow can additionally record:

- Git commit IDs;
- device build fingerprint;
- kernel version;
- driver identity;
- benchmark command;
- tool hashes;
- shader hashes;
- profiler configuration;
- signed release tags; or
- cryptographic signatures of the manifest.

---

# Inspect the manifests

Run from the repository root:

```bash
cd /Users/jerryyun/adreno-gpu-profiler
```

## List manifest files

```bash
find evidence/manifests \
  -maxdepth 1 \
  -type f \
  -print \
  | sort
```

## Show headers

```bash
for MANIFEST in evidence/manifests/*_manifest.txt; do
  echo "=== $MANIFEST ==="
  sed -n '1,8p' "$MANIFEST"
  echo
done
```

## Count hash entries

```bash
for MANIFEST in evidence/manifests/*_manifest.txt; do
  COUNT="$(
    grep -Ec \
      '^[^[:space:]]+[[:space:]]+[0-9a-f]{64}[[:space:]]+' \
      "$MANIFEST"
  )"

  printf "%-70s %5s\n" "$MANIFEST" "$COUNT"
done
```

The device tree manifest will report zero because it has no hashes.

---

# Verify one size-and-hash manifest

The following script:

- parses the size/hash/path lines;
- checks whether every file exists;
- recomputes SHA-256;
- reports mismatches;
- ignores descriptive size values; and
- returns a nonzero exit code on failure.

```bash
python3 - \
  evidence/manifests/direct_counter_access_manifest.txt <<'PY'
from pathlib import Path
import hashlib
import re
import sys

manifest = Path(sys.argv[1])

entry_re = re.compile(
    r"^(?P<size>\S+)\s+"
    r"(?P<sha>[0-9a-f]{64})\s+"
    r"(?P<path>.+)$"
)

entries = []

for lineno, line in enumerate(
    manifest.read_text(errors="replace").splitlines(),
    start=1,
):
    match = entry_re.match(line)

    if match:
        entries.append(
            (
                lineno,
                match.group("sha"),
                Path(match.group("path")),
            )
        )

if not entries:
    raise SystemExit(f"No hash entries found in {manifest}")

failed = False

for lineno, expected, path in entries:
    if not path.is_file():
        print(f"MISSING  line={lineno}  {path}")
        failed = True
        continue

    digest = hashlib.sha256()

    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)

    actual = digest.hexdigest()

    if actual != expected:
        print(
            f"MISMATCH line={lineno}  {path}\n"
            f"  expected={expected}\n"
            f"  actual=  {actual}"
        )
        failed = True
    else:
        print(f"OK       {path}")

if failed:
    raise SystemExit(1)

print(f"Verified {len(entries)} files from {manifest}")
PY
```

---

# Verify all size-and-hash manifests

```bash
python3 - <<'PY'
from pathlib import Path
import hashlib
import re

root = Path("evidence/manifests")

entry_re = re.compile(
    r"^(?P<size>\S+)\s+"
    r"(?P<sha>[0-9a-f]{64})\s+"
    r"(?P<path>.+)$"
)

total_entries = 0
failures = 0

for manifest in sorted(root.glob("*.txt")):
    entries = []

    for lineno, line in enumerate(
        manifest.read_text(errors="replace").splitlines(),
        start=1,
    ):
        match = entry_re.match(line)

        if match:
            entries.append(
                (
                    lineno,
                    match.group("sha"),
                    Path(match.group("path")),
                )
            )

    if not entries:
        print(f"SKIP     {manifest} (no hash entries)")
        continue

    manifest_failures = 0

    for lineno, expected, path in entries:
        total_entries += 1

        if not path.is_file():
            print(f"MISSING  {manifest}:{lineno}  {path}")
            failures += 1
            manifest_failures += 1
            continue

        digest = hashlib.sha256()

        with path.open("rb") as f:
            for chunk in iter(lambda: f.read(1024 * 1024), b""):
                digest.update(chunk)

        actual = digest.hexdigest()

        if actual != expected:
            print(f"MISMATCH {manifest}:{lineno}  {path}")
            print(f"         expected={expected}")
            print(f"         actual=  {actual}")
            failures += 1
            manifest_failures += 1

    if manifest_failures == 0:
        print(f"OK       {manifest} ({len(entries)} files)")
    else:
        print(
            f"FAILED   {manifest} "
            f"({manifest_failures}/{len(entries)} files)"
        )

print()
print(f"Checked entries: {total_entries}")
print(f"Failures:        {failures}")

raise SystemExit(1 if failures else 0)
PY
```

---

# Verify compressed evidence

Check gzip structure:

```bash
find evidence/raw_logs \
  -type f \
  -name '*.gz' \
  -print0 \
  | while IFS= read -r -d '' FILE; do
      if gzip -t "$FILE"; then
        echo "OK      $FILE"
      else
        echo "CORRUPT $FILE"
      fi
    done
```

Inspect a compressed CSV without modifying it:

```bash
gzip -cd \
  evidence/raw_logs/phone_live_csv/alu_live_long.csv.gz \
  | head
```

Record a decompressed-content hash:

```bash
gzip -cd \
  evidence/raw_logs/phone_live_csv/alu_live_long.csv.gz \
  | shasum -a 256
```

This decompressed hash is different in purpose from the stored archive hash in
the manifest.

---

# Detect duplicate content within a manifest

```bash
python3 - \
  evidence/manifests/direct_counter_access_manifest.txt <<'PY'
from collections import defaultdict
from pathlib import Path
import re
import sys

manifest = Path(sys.argv[1])

entry_re = re.compile(
    r"^\S+\s+"
    r"(?P<sha>[0-9a-f]{64})\s+"
    r"(?P<path>.+)$"
)

by_hash = defaultdict(list)

for line in manifest.read_text(errors="replace").splitlines():
    match = entry_re.match(line)

    if match:
        by_hash[match.group("sha")].append(match.group("path"))

duplicates = {
    digest: paths
    for digest, paths in by_hash.items()
    if len(paths) > 1
}

if not duplicates:
    print("No duplicate hashes.")
else:
    for digest, paths in sorted(duplicates.items()):
        print(digest)

        for path in paths:
            print(f"  {path}")
PY
```

Duplicate content should be reviewed, not automatically deleted.

---

# Detect paths listed in multiple manifests

```bash
python3 - <<'PY'
from collections import defaultdict
from pathlib import Path
import re

root = Path("evidence/manifests")

entry_re = re.compile(
    r"^\S+\s+"
    r"(?P<sha>[0-9a-f]{64})\s+"
    r"(?P<path>.+)$"
)

owners = defaultdict(list)

for manifest in sorted(root.glob("*.txt")):
    for line in manifest.read_text(errors="replace").splitlines():
        match = entry_re.match(line)

        if match:
            owners[match.group("path")].append(manifest.name)

for path, manifests in sorted(owners.items()):
    if len(manifests) > 1:
        print(path)

        for manifest in manifests:
            print(f"  {manifest}")
PY
```

---

# Check the device-tree snapshot against the phone

The historical manifest should not be overwritten.

Create a new current snapshot:

```bash
mkdir -p evidence/manifests/current_checks

adb shell '
  cd /data/local/tmp/jerry_work 2>/dev/null || exit 1
  find . -print | sort
' > \
  evidence/manifests/current_checks/device_jerry_work_manifest_current.txt
```

Compare path sets:

```bash
diff -u \
  evidence/manifests/device_jerry_work_manifest_after_organize.txt \
  evidence/manifests/current_checks/device_jerry_work_manifest_current.txt \
  | less
```

Differences are expected after continued development.

The purpose is to understand changes, not force the phone back to the old
snapshot.

---

# Generate a new hash manifest

Do not overwrite a historical manifest.

Create a new dated file.

Example for a new evidence directory:

```bash
EVIDENCE_DIR="evidence/raw_logs/example_capture"
MANIFEST="evidence/manifests/$(date +%Y%m%d)_example_capture_manifest.txt"
```

Generate it with Python:

```bash
python3 - \
  "$EVIDENCE_DIR" \
  "$MANIFEST" <<'PY'
from datetime import datetime
from pathlib import Path
import hashlib
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])

if not source.is_dir():
    raise SystemExit(f"Source directory not found: {source}")

entries = []

for path in sorted(source.rglob("*")):
    if not path.is_file():
        continue

    digest = hashlib.sha256()

    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)

    size = path.stat().st_size

    entries.append(
        (
            size,
            digest.hexdigest(),
            path.as_posix(),
        )
    )

def human_size(size):
    units = ["B", "K", "M", "G", "T"]
    value = float(size)

    for unit in units:
        if value < 1024 or unit == units[-1]:
            if unit == "B":
                return f"{int(value)}B"

            if value >= 10:
                return f"{value:.0f}{unit}"

            return f"{value:.1f}{unit}"

        value /= 1024

lines = [
    f"# Manifest: {source.name}",
    "",
    f"Generated: {datetime.now().astimezone().isoformat()}",
    f"Folder: {source.as_posix()}",
    "",
    "## Files",
]

for size, digest, path in entries:
    lines.append(
        f"{human_size(size):>6s}  {digest}  {path}"
    )

output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("\n".join(lines) + "\n")

print(f"Wrote {output}")
print(f"Files: {len(entries)}")
PY
```

This generator uses exact logical byte size to derive the human-readable size.

Its displayed size formatting may differ slightly from older manifests, but the
SHA-256 field remains directly comparable.

---

# Bash-only manifest generation

On macOS:

```bash
EVIDENCE_DIR="evidence/raw_logs/example_capture"
MANIFEST="evidence/manifests/$(date +%Y%m%d)_example_capture_manifest.txt"

{
  echo "# Manifest: $(basename "$EVIDENCE_DIR")"
  echo
  echo "Generated: $(date)"
  echo "Folder: $EVIDENCE_DIR"
  echo
  echo "## Files"

  find "$EVIDENCE_DIR" \
    -type f \
    -print0 \
    | sort -z \
    | while IFS= read -r -d '' FILE; do
        SIZE="$(du -h "$FILE" | awk '{print $1}')"
        SHA="$(shasum -a 256 "$FILE" | awk '{print $1}')"
        printf '%s  %s  %s\n' "$SIZE" "$SHA" "$FILE"
      done
} > "$MANIFEST"
```

The Python version is more portable and easier to validate.

---

# Check for unmanifested evidence files

The following script compares all files under `evidence/raw_logs` and
`evidence/summaries` with paths present in hash manifests.

```bash
python3 - <<'PY'
from pathlib import Path
import re

manifest_root = Path("evidence/manifests")
evidence_roots = [
    Path("evidence/raw_logs"),
    Path("evidence/summaries"),
]

entry_re = re.compile(
    r"^\S+\s+"
    r"[0-9a-f]{64}\s+"
    r"(?P<path>.+)$"
)

manifested = set()

for manifest in manifest_root.glob("*.txt"):
    for line in manifest.read_text(errors="replace").splitlines():
        match = entry_re.match(line)

        if match:
            manifested.add(match.group("path"))

actual = set()

for root in evidence_roots:
    if not root.exists():
        continue

    for path in root.rglob("*"):
        if path.is_file():
            actual.add(path.as_posix())

print("=== Unmanifested evidence files ===")

for path in sorted(actual - manifested):
    print(path)

print()
print("=== Manifest paths missing from repository ===")

for path in sorted(manifested - actual):
    print(path)
PY
```

Not every evidence file must be added to an old manifest.

For new files, create a new manifest rather than editing a historical snapshot.

---

# Path migration after repository reorganization

A historical manifest may contain an old path while the same content has moved.

There are two valid approaches.

## Preserve the original manifest

Recommended for historical provenance.

Keep:

```text
old recorded path
old generation time
old hash
```

Then create a new migration or current-state manifest.

## Create a remapped current manifest

Useful when evidence was intentionally reorganized.

Do not silently edit the original manifest because that makes it appear that the
new path existed at the original generation time.

A migration record can contain:

```text
old path
new path
SHA-256
migration date
Git commit
```

---

# Repository workflow

# Add a new manifest

```bash
git add \
  evidence/manifests/<new_manifest>.txt
```

Review:

```bash
git diff --cached -- \
  evidence/manifests/<new_manifest>.txt
```

Commit:

```bash
git commit -m "docs: add evidence manifest for <experiment>"
```

# Modify an existing manifest

Avoid modifying historical manifests unless correcting a documented generation
error.

Before changing one:

```bash
git log --follow -- \
  evidence/manifests/<manifest>.txt
```

After changing:

```bash
git diff -- \
  evidence/manifests/<manifest>.txt
```

Document why the historical record changed.

---

# Recommended naming convention

Use:

```text
YYYYMMDD_<experiment>_manifest.txt
```

Examples:

```text
20260605_turnip_perf_query_manifest.txt
20260715_streamer_validation_manifest.txt
20260715_width_sweep_latest2_manifest.txt
```

For undated legacy manifests, preserve their existing names.

New manifests should preferably include the date.

---

# Recommended manifest header

```text
# Manifest: <name>

Generated: <ISO-8601 timestamp>
Repository commit: <Git commit>
Source: <source location or command>
Folder: <repository-relative evidence folder>
Device: <model/build fingerprint when applicable>
Kernel: <kernel version when applicable>
Driver: <vendor/Turnip identity when applicable>
Tool: <capture script or binary and hash>

## Files
<size>  <sha256>  <path>
```

Not every field is needed for every experiment.

---

# Security and privacy considerations

The current manifests expose local paths such as:

```text
/Users/jerryyun/adreno_turnip
/Users/jerryyun/adreno_phone_pull_20260605_143858/jerry_work
```

These are not credentials, but they reveal:

- a local username;
- local folder names; and
- internal project organization.

For a public release, consider replacing source paths in new manifests with:

```text
<local-workspace>/adreno_turnip
<phone-pull>/jerry_work
```

Do not rewrite existing historical manifests without documenting the change.

Do not place the following in manifests:

- access tokens;
- passwords;
- private keys;
- device unlock credentials;
- full personal addresses;
- proprietary binaries that cannot be redistributed; or
- logs containing unreviewed sensitive user data.

Review logcat before publishing it publicly because system logs can contain:

- package names;
- device identifiers;
- local paths;
- process information;
- network information; or
- unrelated application logs.

---

# Known limitations

## No signature

The manifests contain hashes but are not cryptographically signed.

A person who can modify both evidence and manifest can replace both.

## Human-readable size ambiguity

The size column may be rounded or block-based.

## Historical path drift

Repository reorganization can make a path missing even when identical content
still exists elsewhere.

## Tree snapshot lacks hashes

The device workspace tree verifies names only.

## No directory hashes

Directory structure is represented indirectly by file paths.

## No permissions or ownership

The manifests cannot restore executable bits, Android ownership, or SELinux
contexts.

## No symlink metadata

The hash format assumes regular files.

## No automatic raw-to-summary linkage

A summary manifest does not identify exactly which raw log produced each
summary unless the summary or experiment notes record it.

## No tool-version metadata

Most existing headers do not record:

- parser version;
- Git commit;
- binary hash;
- driver version;
- device build;
- kernel version; or
- benchmark command.

## Compressed hash depends on gzip representation

Recompression can change the stored archive digest without changing decompressed
content.

## Duplicate hashes require context

Duplicate files may be meaningful repeated trials rather than unnecessary
copies.

---

# Recommended maintenance

1. Keep existing manifests immutable whenever possible.
2. Create new dated manifests for new captures.
3. Record Git commit IDs in future headers.
4. Record device and kernel identity for phone evidence.
5. Record driver identity for vendor-versus-Turnip runs.
6. Record benchmark commands and shader hashes.
7. Record streamer/sweeper binary hashes for formal experiments.
8. Add a reusable manifest generator under:
   ```text
   scripts/inventory/
   ```
9. Add a reusable verifier that exits nonzero on missing or mismatched files.
10. Verify `.gz` structure separately with `gzip -t`.
11. Consider recording decompressed hashes for compressed datasets.
12. Add migration manifests when evidence paths change.
13. Avoid deleting duplicate-hash files until experimental context is reviewed.
14. Add a small metadata file linking raw traces to generated summaries.
15. Preserve old device-tree snapshots and create new snapshots rather than
    replacing them.
16. Review logcat and local source paths before public release.
17. Add CI verification for manifests whose evidence is tracked in Git.
18. Separate very large evidence from source history if repository size becomes
    a problem, but keep the manifest and retrieval metadata.

---

# Suggested supporting scripts

A future repository structure could be:

```text
scripts/inventory/
├── make_evidence_manifest.py
├── verify_evidence_manifest.py
├── compare_evidence_manifests.py
├── find_duplicate_evidence.py
└── snapshot_device_workspace.sh
```

The scripts should:

- use repository-relative paths;
- process files in deterministic sorted order;
- stream file hashing rather than reading whole files into memory;
- support spaces in filenames;
- return nonzero on verification failure;
- distinguish missing files from hash mismatches;
- optionally verify gzip streams;
- optionally record Git/device metadata; and
- never overwrite an existing historical manifest by default.

---

# Quick-reference commands

## Verify JSON-like manifest structure manually

The manifests are plain text, not JSON.

Inspect:

```bash
less evidence/manifests/direct_counter_access_manifest.txt
```

## Verify one file directly

```bash
shasum -a 256 \
  evidence/raw_logs/phone_probe_logs/vendor_vk_probe.log
```

Compare with:

```text
phone_probe_logs_manifest.txt
```

## Verify gzip archives

```bash
find evidence/raw_logs \
  -name '*.gz' \
  -type f \
  -exec gzip -t {} +
```

## List duplicate hashes

```bash
awk '
  $2 ~ /^[0-9a-f]{64}$/ {
    print $2, $3
  }
' evidence/manifests/direct_counter_access_manifest.txt \
  | sort \
  | awk '
      previous_hash == $1 {
        if (!printed_previous) {
          print previous_line
        }

        print
        printed_previous = 1
      }

      previous_hash != $1 {
        printed_previous = 0
      }

      {
        previous_hash = $1
        previous_line = $0
      }
    '
```

The Python duplicate-check command documented earlier is more robust.

## Snapshot current phone workspace

```bash
adb shell '
  cd /data/local/tmp/jerry_work || exit 1
  find . -print | sort
' > \
  "evidence/manifests/$(date +%Y%m%d)_device_jerry_work_manifest.txt"
```

## Check repository state

```bash
git status --short -- \
  evidence/manifests \
  evidence/raw_logs \
  evidence/summaries
```

---

# Quick manifest summary

```text
20260605_turnip_perf_query_manifest.txt
    Turnip/Mesa Vulkan performance-query experiment
    enumeration, selection, read, availability, barrier, strace evidence

device_jerry_work_manifest_after_organize.txt
    path-only snapshot of organized phone workspace
    shaders, samplers, probes, logs, live CSVs, Turnip assets, cleanup files

direct_counter_access_manifest.txt
    foundational DRM MSM and KGSL direct counter-access evidence
    permissions, open tests, ioctl tests, root/shell comparison

kgsl_tracepoints_manifest.txt
    vendor and Turnip compute/memory event summaries
    repeated tracepoint-derived results

phone_cleanup_old_manifest.txt
    archived scripts and compressed logs preserved during phone cleanup

phone_live_csv_manifest.txt
    compressed ALU/memory live telemetry CSV captures

phone_probe_logs_manifest.txt
    vendor Vulkan probe log inventory

turnip_logcat_manifest.txt
    Turnip loader/runtime/bind-mount logcat evidence

vendor_turnip_comparison_manifest.txt
    repeated vendor/Turnip compute and memory summaries
    focused KGSL summaries and combined trial comparison
```
