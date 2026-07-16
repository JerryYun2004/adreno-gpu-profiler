# Build and Device Configuration

This directory contains configuration files used to build and run the Mesa
Turnip Vulkan driver for an Android ARM64 device.

The two files belong to different stages of the workflow:

```text
configs/build/mesa_android_cross/android-aarch64.cross
    ↓
Meson uses the Android NDK toolchain to cross-compile Mesa/Turnip
    ↓
libvulkan_freedreno.so
    ↓
push the library and ICD manifest to the Android device
    ↓
configs/device/turnip/freedreno_icd.aarch64.json
    ↓
the Vulkan loader discovers and loads Turnip
```

These files do not implement GPU profiling and are not compiled into the
perf-counter streamer or sweeper. They support an optional driver environment
used by benchmarks that the profiler observes.

---

# Directory layout

```text
configs/
├── build/
│   └── mesa_android_cross/
│       └── android-aarch64.cross
└── device/
    └── turnip/
        └── freedreno_icd.aarch64.json
```

The directory separates:

```text
build configuration
    host-side settings used while compiling software

device configuration
    runtime settings copied to or used on the Android phone
```

This distinction should be preserved when adding future configuration files.

---

# Relationship to the main profiler tools

The main profiling products are:

```text
tools/profiling/perfcounter_streamer/
tools/profiling/perfcounter_sweeper/
```

## Direct relationship

The streamer and sweeper do not require either file to access Adreno hardware
performance counters.

They use the project’s KGSL perf-counter path and can collect counters while a
workload runs through either:

- the phone’s vendor Vulkan driver; or
- Mesa Turnip.

The configuration files in this directory do not:

- issue KGSL perf-counter `ioctl` calls;
- define Adreno counter groups;
- start or stop the streamer;
- select streamer presets;
- create sweeper CSV files; or
- become linked into the profiler binaries.

## Indirect relationship

These files matter when the workload being profiled should run through Turnip.

The full experiment path is:

```text
android-aarch64.cross
    ↓
build Mesa Turnip for Android ARM64
    ↓
freedreno_icd.aarch64.json
    ↓
force a Vulkan benchmark to load Turnip
    ↓
streamer/sweeper collects GPU counters during that benchmark
```

This enables controlled comparisons such as:

```text
same shader + vendor Vulkan driver
versus
same shader + Turnip Vulkan driver
```

Possible comparison dimensions include:

- command-batch structure;
- GPU active duration;
- SP activity;
- UCHE/TP behavior;
- RAM wait;
- shader compiler output;
- dispatch organization;
- counter totals; and
- performance stability.

## When these files are not needed

Neither file is needed when:

- profiling a non-Vulkan workload;
- running only the vendor Vulkan driver;
- analyzing existing CSV files;
- building only the streamer or sweeper;
- using KGSL trace analysis on previously captured logs; or
- running host-side analysis scripts.

---

# Status summary

| File | Status | Stage | Main purpose |
|---|---|---|---|
| `build/mesa_android_cross/android-aarch64.cross` | Functional but machine-specific | Host build | Tells Meson how to cross-compile for Android ARM64 |
| `device/turnip/freedreno_icd.aarch64.json` | Functional but path-specific | Device runtime | Tells the Vulkan loader where the Turnip shared library is located |

Both files are configuration inputs rather than generated outputs.

---

# File reference

# `build/mesa_android_cross/android-aarch64.cross`

## Purpose

A Meson cross-compilation file for building software on macOS while targeting
Android ARM64.

The file tells Meson:

- which archive tool to use;
- which C compiler to use;
- which C++ compiler to use;
- which strip tool to use;
- that host `pkg-config` must not be used;
- that the target operating system is Android;
- that the target architecture is AArch64;
- that the target is little-endian; and
- that target executables cannot run directly on the build machine.

The file is intended primarily for building Mesa and the Turnip Vulkan driver.

## Current contents

The active tool paths are:

```text
NDK root:
    /Users/jerryyun/android-ndk-r27d

NDK host tools:
    toolchains/llvm/prebuilt/darwin-x86_64/bin

C compiler:
    aarch64-linux-android29-clang

C++ compiler:
    aarch64-linux-android29-clang++

archive tool:
    llvm-ar

strip tool:
    llvm-strip
```

The target Android API encoded in the compiler names is:

```text
29
```

The current file therefore targets the Android API 29 ABI surface, even when the
result is run on a newer Android device.

This is different from the device’s current Android version.

## `[binaries]`

```ini
[binaries]
ar = '.../llvm-ar'
c = ['.../aarch64-linux-android29-clang']
cpp = ['.../aarch64-linux-android29-clang++']
strip = '.../llvm-strip'
pkg-config = '/bin/false'
```

### `ar`

The LLVM archive utility creates static libraries such as:

```text
libsomething.a
```

during the Mesa build.

### `c`

The Android NDK Clang C compiler.

The executable name encodes:

```text
target architecture: aarch64
target OS:           Android
minimum API:         29
```

The NDK compiler driver automatically selects the appropriate Android target and
sysroot based on that name.

### `cpp`

The corresponding Android NDK C++ compiler.

Mesa is primarily C, but some dependencies or build checks can require a C++
compiler.

### `strip`

Removes symbols and other optional information from final binaries when Meson or
the install step requests stripping.

During development, an unstripped library is often easier to debug.

### `pkg-config = '/bin/false'`

This deliberately disables `pkg-config` during the cross build.

Why this matters:

- host macOS `.pc` files describe host libraries;
- those libraries cannot be linked into an Android ARM64 target;
- accidental host dependency discovery can make a cross build fail or produce
  invalid results.

With `/bin/false`, a `pkg-config` query fails immediately instead of silently
selecting a host library.

The tradeoff is that target dependencies requiring `pkg-config` must be supplied
through:

- Meson subprojects;
- explicit include/library paths;
- the Android NDK;
- or another target-aware dependency mechanism.

## `[host_machine]`

```ini
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
```

In Meson terminology, the “host machine” is the machine where the compiled
program will run.

It does not mean the Mac used to launch Meson.

The values describe:

```text
operating system: Android
architecture:     64-bit ARM
endianness:       little-endian
```

This matches the project’s ARM64 Android phone.

## `[properties]`

```ini
[properties]
needs_exe_wrapper = true
```

The Mac cannot directly execute Android ARM64 binaries produced during the
cross build.

This property tells Meson not to assume that target test executables can run on
the build host.

No emulator or wrapper is configured, so build-time tests requiring target
execution may be skipped or treated as unavailable.

## Why this file is needed

Without a cross file, Meson would normally build for the local Mac.

That would produce:

```text
macOS Mach-O binaries
```

instead of:

```text
Android ARM64 ELF shared libraries
```

The file makes the target explicit and prevents host/target tool confusion.

## Status

Functional but machine-specific.

The absolute paths contain:

```text
/Users/jerryyun/
```

and assume:

```text
android-ndk-r27d
darwin-x86_64
API 29 compiler wrappers
```

Another developer cannot use the file unchanged unless the NDK exists at the
same path.

## Portability limitation

The following can vary between machines:

- user home directory;
- NDK installation directory;
- NDK revision;
- host prebuilt tag;
- Android target API;
- Mac architecture; and
- whether the toolchain is installed manually or by Android Studio.

For reproducible collaboration, the repository should either:

1. generate a local cross file from environment variables; or
2. provide a template and keep machine-specific files untracked.

---

# `device/turnip/freedreno_icd.aarch64.json`

## Purpose

A Vulkan Installable Client Driver manifest for Mesa Turnip.

The Vulkan loader reads the JSON file to determine:

- which shared library implements the Vulkan driver; and
- which Vulkan API version the manifest advertises.

Current content:

```json
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so",
    "api_version": "1.3.0"
  }
}
```

## `file_format_version`

```text
1.0.0
```

This describes the ICD manifest file format.

It is not the Mesa version and not the Vulkan driver version.

## `ICD.library_path`

```text
/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so
```

This is the absolute device-side path to the Turnip Vulkan shared library.

The Vulkan loader must be able to:

- open this path;
- load the ELF shared object;
- resolve its dependencies; and
- call the Vulkan ICD entry points.

If the library is pushed somewhere else, the manifest must be updated.

## `ICD.api_version`

```text
1.3.0
```

This is metadata presented to the Vulkan loader.

It is not proof that every Vulkan 1.3 feature is supported.

The actual runtime driver capabilities should be checked with:

```text
vulkaninfo
```

or a project Vulkan probe.

## Why this file is needed

The Turnip library is being deployed manually under:

```text
/data/local/tmp/
```

rather than installed as the system Vulkan driver.

The Android Vulkan loader does not automatically know that this library should
be used.

For shell-launched native test programs, the project can point the loader to the
manifest through an environment variable.

The manifest then points the loader to the Turnip library.

## Status

Functional but device-path-specific.

It assumes the library is installed exactly at:

```text
/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so
```

Moving or renaming the library without updating the JSON will cause driver
loading to fail.

---

# How the two files connect

The cross file and ICD manifest are related, but they are not interchangeable.

```text
android-aarch64.cross
    used by Meson on the Mac
    controls compilation

freedreno_icd.aarch64.json
    used by the Vulkan loader on the phone
    controls runtime discovery
```

The cross file does not get loaded by Vulkan.

The ICD manifest does not control compilation.

A typical end-to-end workflow is:

```text
1. configure Mesa with Meson and android-aarch64.cross
2. compile Turnip
3. locate libvulkan_freedreno.so
4. push the library to /data/local/tmp/jerry_work/turnip/
5. push freedreno_icd.aarch64.json to the same directory
6. set the loader environment variable
7. launch a Vulkan benchmark
8. confirm that the benchmark reports Turnip
9. run streamer/sweeper during the benchmark
```

---

# Toolchain requirements

## Host tools

Required or commonly used:

```text
Git
Python 3
Meson
Ninja
Android NDK
ADB
file
shasum
```

Recommended:

```text
jq
readelf or llvm-readelf
```

Check:

```bash
git --version
python3 --version
meson --version
ninja --version
adb version
```

Install Meson and Ninja in a virtual environment:

```bash
cd /Users/jerryyun/adreno-gpu-profiler

python3 -m venv .venv
source .venv/bin/activate

python3 -m pip install --upgrade pip
python3 -m pip install meson ninja
```

## Android NDK

The committed cross file expects:

```text
/Users/jerryyun/android-ndk-r27d
```

Check:

```bash
test -d /Users/jerryyun/android-ndk-r27d \
  && echo "NDK found" \
  || echo "NDK missing"
```

Check every configured tool:

```bash
CROSS="configs/build/mesa_android_cross/android-aarch64.cross"

grep -nE '^(ar|c|cpp|strip|pkg-config)' "$CROSS"
```

Direct tool checks:

```bash
/Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android29-clang \
  --version

/Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar \
  --version
```

## Mesa source

The repository stores Mesa under:

```text
third_party/mesa/
```

Check the submodule:

```bash
git submodule status third_party/mesa
git -C third_party/mesa status --short
```

## Android device

The target device must:

- use ARM64;
- support Vulkan;
- allow shell execution from `/data/local/tmp`;
- allow the benchmark to load the manually deployed library; and
- have any required Turnip runtime dependencies available.

Check architecture:

```bash
adb shell getprop ro.product.cpu.abi
```

Expected:

```text
arm64-v8a
```

---

# Validate the configuration files

# Validate the Meson cross file

Ask Meson to parse and use it during a small setup operation rather than treating
it as a general INI file.

At minimum, verify every configured file exists:

```bash
python3 - <<'PY'
from pathlib import Path
import ast
import re

path = Path("configs/build/mesa_android_cross/android-aarch64.cross")
text = path.read_text()

for key in ["ar", "c", "cpp", "strip"]:
    match = re.search(rf"^{re.escape(key)}\s*=\s*(.+)$", text, re.MULTILINE)
    if not match:
        print(f"{key}: missing")
        continue

    value = ast.literal_eval(match.group(1))
    tool = value[0] if isinstance(value, list) else value
    print(f"{key}: {'OK' if Path(tool).exists() else 'MISSING'}: {tool}")
PY
```

# Validate the ICD JSON syntax

Using Python:

```bash
python3 -m json.tool \
  configs/device/turnip/freedreno_icd.aarch64.json
```

Using `jq`:

```bash
jq . \
  configs/device/turnip/freedreno_icd.aarch64.json
```

# Print the configured device library path

```bash
python3 - <<'PY'
import json
from pathlib import Path

p = Path("configs/device/turnip/freedreno_icd.aarch64.json")
data = json.loads(p.read_text())
print(data["ICD"]["library_path"])
PY
```

Expected:

```text
/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so
```

---

# Recommended portable cross-file workflow

The committed cross file is useful as a record of the working environment, but
a generated local file is more portable.

## Set local build variables

```bash
cd /Users/jerryyun/adreno-gpu-profiler

export ANDROID_NDK_HOME="/Users/jerryyun/android-ndk-r27d"
export ANDROID_API=29
export NDK_HOST_TAG="darwin-x86_64"
```

## Generate a local cross file

```bash
mkdir -p build/config

cat > build/config/android-aarch64.local.cross <<EOF
[binaries]
ar = '${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/bin/llvm-ar'
c = ['${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/bin/aarch64-linux-android${ANDROID_API}-clang']
cpp = ['${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/bin/aarch64-linux-android${ANDROID_API}-clang++']
strip = '${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/bin/llvm-strip'
pkg-config = '/bin/false'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[properties]
needs_exe_wrapper = true
EOF
```

Inspect:

```bash
cat build/config/android-aarch64.local.cross
```

The generated file should normally remain untracked:

```bash
printf '\n# Local generated build configuration\nbuild/config/*.local.cross\n' \
  >> .gitignore
```

Check for an existing rule first:

```bash
grep -n 'local.cross' .gitignore
```

---

# Build Mesa Turnip

Mesa build options can change between revisions. The cross file only selects the
toolchain and target; it does not enable Turnip by itself.

Use the options supported by the checked-out Mesa revision.

## Clean build directory

```bash
cd /Users/jerryyun/adreno-gpu-profiler

rm -rf build/mesa-android-aarch64
```

## Example Meson setup

Using the committed cross file:

```bash
meson setup \
  build/mesa-android-aarch64 \
  third_party/mesa \
  --cross-file configs/build/mesa_android_cross/android-aarch64.cross \
  --buildtype release \
  -Dplatforms=android \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dllvm=disabled
```

Using the generated local cross file:

```bash
meson setup \
  build/mesa-android-aarch64 \
  third_party/mesa \
  --cross-file build/config/android-aarch64.local.cross \
  --buildtype release \
  -Dplatforms=android \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dllvm=disabled
```

The exact accepted syntax for empty driver lists and optional Mesa features
depends on the checked-out Mesa revision.

If setup reports an unknown option, inspect the available options:

```bash
meson configure build/mesa-android-aarch64
```

or inspect Mesa’s Meson options:

```bash
grep -nE \
  "option\('(platforms|vulkan-drivers|gallium-drivers|llvm)'" \
  third_party/mesa/meson_options.txt
```

## Compile

```bash
meson compile -C build/mesa-android-aarch64
```

Equivalent Ninja command:

```bash
ninja -C build/mesa-android-aarch64
```

## Reconfigure

After changing options:

```bash
meson setup \
  --reconfigure \
  build/mesa-android-aarch64
```

After changing the cross file or target toolchain, a clean or wiped setup is
safer:

```bash
meson setup \
  --wipe \
  build/mesa-android-aarch64 \
  third_party/mesa \
  --cross-file build/config/android-aarch64.local.cross \
  --buildtype release \
  -Dplatforms=android \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dllvm=disabled
```

---

# Locate and validate the Turnip library

## Locate the output

```bash
find build/mesa-android-aarch64 \
  -type f \
  -name 'libvulkan_freedreno.so' \
  -print
```

A common location is similar to:

```text
build/mesa-android-aarch64/src/freedreno/vulkan/libvulkan_freedreno.so
```

Use the actual result from `find`.

## Confirm file type

```bash
TURNIP_SO="$(
  find build/mesa-android-aarch64 \
    -type f \
    -name 'libvulkan_freedreno.so' \
    -print \
    -quit
)"

file "$TURNIP_SO"
```

Expected properties include:

```text
ELF
64-bit
ARM aarch64
shared object
```

## Inspect ELF header

With the NDK tool:

```bash
/Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf \
  -h "$TURNIP_SO"
```

Check:

```text
Class:   ELF64
Machine: AArch64
Type:    DYN
```

## Check dependencies

```bash
/Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf \
  -d "$TURNIP_SO" \
  | grep NEEDED
```

Any non-system dependency may also need to be pushed to the device.

## Record hash

```bash
shasum -a 256 "$TURNIP_SO"
```

Store the hash with experiment metadata so vendor/Turnip comparisons identify
the exact driver build.

---

# Push Turnip and the ICD manifest

## Set paths

```bash
cd /Users/jerryyun/adreno-gpu-profiler

REMOTE_TURNIP="/data/local/tmp/jerry_work/turnip"

TURNIP_SO="$(
  find build/mesa-android-aarch64 \
    -type f \
    -name 'libvulkan_freedreno.so' \
    -print \
    -quit
)"
```

Check that the library was found:

```bash
test -n "$TURNIP_SO" -a -f "$TURNIP_SO" \
  || {
    echo "Turnip library not found" >&2
    exit 1
  }
```

## Create the device directory

```bash
adb shell "mkdir -p '$REMOTE_TURNIP'"
```

## Push the driver

```bash
adb push \
  "$TURNIP_SO" \
  "$REMOTE_TURNIP/libvulkan_freedreno.so"
```

## Push the manifest

```bash
adb push \
  configs/device/turnip/freedreno_icd.aarch64.json \
  "$REMOTE_TURNIP/freedreno_icd.aarch64.json"
```

## Set permissions

```bash
adb shell \
  "chmod 755 '$REMOTE_TURNIP/libvulkan_freedreno.so'"

adb shell \
  "chmod 644 '$REMOTE_TURNIP/freedreno_icd.aarch64.json'"
```

## Confirm files

```bash
adb shell \
  "ls -lh '$REMOTE_TURNIP'"
```

## Confirm the manifest path matches the pushed library

```bash
adb shell \
  "test -f '$REMOTE_TURNIP/libvulkan_freedreno.so' \
   && echo 'Turnip library present' \
   || echo 'Turnip library missing'"
```

The configured JSON path must remain:

```text
/data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so
```

---

# Run a Vulkan program with Turnip

The following workflow is intended for native command-line programs launched
through `adb shell`.

## Basic environment

```bash
adb shell '
  export VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json
  export LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:${LD_LIBRARY_PATH}

  /data/local/tmp/jerry_work/vulkan_probe
'
```

Replace the final executable with the actual Vulkan program.

## One-line form

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe'
```

## Use a three-way benchmark

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/threeway/vk_threeway_probe \
     alu \
     /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
     262144 \
     2048 \
     64'
```

## Use an ML primitive benchmark

Conceptually:

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/ml_primitives/ml_primitive_bench \
     ...'
```

Use the current ML benchmark CLI documented under:

```text
benchmarks/ml_primitives/
```

## Loader debug output

For loader troubleshooting:

```bash
adb shell \
  'VK_LOADER_DEBUG=all \
   VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe' \
  2>&1 \
  | tee results/mesa_debug/turnip_loader_debug.log
```

Loader debug support depends on the Vulkan loader build.

---

# Confirm Turnip is active

Do not assume that setting the manifest succeeded.

The launched application should report a device or driver string associated with
Mesa/Turnip rather than the vendor Qualcomm Vulkan driver.

## Use a project Vulkan probe

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe'
```

Inspect:

```text
deviceName
driverName
driverInfo
apiVersion
driverVersion
```

## Use `vulkaninfo` when available

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/vulkaninfo --summary'
```

## Compare with the vendor driver

Run without the Turnip environment:

```bash
adb shell \
  '/data/local/tmp/jerry_work/vulkan_probe'
```

Then run with Turnip:

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe'
```

Save both outputs:

```bash
mkdir -p results/mesa_debug/driver_identity

adb shell \
  '/data/local/tmp/jerry_work/vulkan_probe' \
  | tee results/mesa_debug/driver_identity/vendor.txt

adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe' \
  | tee results/mesa_debug/driver_identity/turnip.txt
```

---

# Use Turnip with the perf-counter streamer

Use two terminals.

## Terminal 1: start the streamer

```bash
adb shell \
  'su -c "/data/local/tmp/adreno_perf_stream \
    -i 0.001 \
    -n SP_ALU_WORKING_CYCLES \
    --csv"' \
  | tee results/mesa_debug/turnip_sp_alu_working.csv
```

## Terminal 2: launch the workload with Turnip

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/threeway/vk_threeway_probe \
     alu \
     /data/local/tmp/jerry_work/threeway/alu_heavy.comp.spv \
     262144 \
     2048 \
     64'
```

The streamer does not need the Turnip environment because it is not a Vulkan
program.

Only the benchmark process must be pointed to the Turnip ICD.

---

# Use Turnip with the perf-counter sweeper

The sweeper invokes an external benchmark command.

The benchmark command should include the Turnip environment.

Conceptually:

```bash
env \
  VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
  LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip \
  /data/local/tmp/jerry_work/threeway/vk_threeway_probe \
    mem \
    /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
    262144 \
    512 \
    64
```

The exact quoting depends on whether the sweeper runs:

- directly on the phone;
- through `adb shell`;
- through `su -c`; or
- through a wrapper script.

Before a long sweep, run the exact benchmark command manually and confirm that:

```text
the workload passes verification
the reported driver is Turnip
the process exit code is zero
```

---

# Vendor-versus-Turnip experiment workflow

## 1. Keep the benchmark constant

Use the same:

```text
runner binary
SPIR-V hash
input size
iteration count
dispatch repeats
counter selection
sampling interval
device state
```

## 2. Run vendor driver

Do not set `VK_ICD_FILENAMES`.

```bash
adb shell \
  '/data/local/tmp/jerry_work/threeway/vk_threeway_probe \
   mem \
   /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
   262144 \
   512 \
   64'
```

## 3. Run Turnip

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/threeway/vk_threeway_probe \
     mem \
     /data/local/tmp/jerry_work/threeway/mem_heavy_clean.comp.spv \
     262144 \
     512 \
     64'
```

## 4. Record identities and hashes

```bash
shasum -a 256 \
  "$TURNIP_SO" \
  benchmarks/microbenchmarks/threeway/shaders/mem_heavy_clean.comp.spv \
  benchmarks/microbenchmarks/threeway/runner/vk_threeway_probe
```

## 5. Record the Mesa revision

```bash
git -C third_party/mesa rev-parse HEAD
git -C third_party/mesa describe --always --dirty
```

## 6. Record the cross file

```bash
cp \
  configs/build/mesa_android_cross/android-aarch64.cross \
  results/mesa_debug/android-aarch64.cross.used
```

## 7. Record the manifest

```bash
cp \
  configs/device/turnip/freedreno_icd.aarch64.json \
  results/mesa_debug/freedreno_icd.aarch64.json.used
```

---

# Troubleshooting

# Meson cannot find the compiler

Example cause:

```text
the NDK is not installed at /Users/jerryyun/android-ndk-r27d
```

Check:

```bash
ls -l \
  /Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android29-clang
```

Fix by:

- installing the expected NDK;
- editing the committed cross file; or
- generating a local cross file.

# Meson uses host dependencies

The cross file intentionally sets:

```text
pkg-config = /bin/false
```

If Mesa setup reports a missing dependency, do not replace this with the host
`pkg-config` without checking architecture.

A host dependency can produce an invalid cross build.

# Meson option is unknown

The checked-out Mesa version may use different option names or values.

Inspect:

```bash
meson configure build/mesa-android-aarch64
```

and:

```bash
sed -n '1,260p' third_party/mesa/meson_options.txt
```

# Build output is Mach-O

The cross file was not applied correctly.

Check:

```bash
file "$TURNIP_SO"
```

An Android driver must be an AArch64 ELF shared object, not a macOS Mach-O file.

# Build output is x86-64

The wrong compiler was used.

Check the compiler line in:

```text
build/mesa-android-aarch64/meson-logs/meson-log.txt
```

# Vulkan loader cannot open the manifest

Check JSON syntax:

```bash
python3 -m json.tool \
  configs/device/turnip/freedreno_icd.aarch64.json
```

Check remote path:

```bash
adb shell \
  'ls -l /data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json'
```

# Vulkan loader cannot open the shared library

Check:

```bash
adb shell \
  'ls -l /data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so'
```

Check architecture:

```bash
adb pull \
  /data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so \
  /tmp/libvulkan_freedreno.so

file /tmp/libvulkan_freedreno.so
```

Check dependencies:

```bash
/Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf \
  -d /tmp/libvulkan_freedreno.so \
  | grep NEEDED
```

# A dependency is missing

Push the required target ARM64 dependency into:

```text
/data/local/tmp/jerry_work/turnip/
```

and ensure:

```text
LD_LIBRARY_PATH
```

contains that directory.

Do not push macOS or x86-64 libraries.

# The vendor driver still loads

Confirm the environment is applied to the benchmark process itself:

```bash
adb shell '
  export VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json
  echo "$VK_ICD_FILENAMES"
  /data/local/tmp/jerry_work/vulkan_probe
'
```

Also confirm the application is a shell-launched native binary and not a
separate Android app process that discards the shell environment.

# The advertised API version differs

The JSON contains:

```text
1.3.0
```

but actual driver/runtime reporting may differ.

Treat runtime `vulkaninfo` or probe output as the authoritative result.

# Permission denied

Set:

```bash
adb shell \
  'chmod 755 /data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so'
```

The JSON only needs to be readable:

```bash
adb shell \
  'chmod 644 /data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json'
```

# SELinux or loader restrictions

A correct file path and permission do not guarantee that every Android process
can load a library from `/data/local/tmp`.

The current workflow is intended for native binaries launched from `adb shell`.

An APK or system process may require a different deployment method.

---

# Expected outputs

# From the cross file

A successful build should produce an Android ARM64 Turnip shared library:

```text
libvulkan_freedreno.so
```

Expected properties:

```text
ELF shared object
AArch64
Android-compatible
Mesa Turnip Vulkan ICD
```

# From the ICD manifest

A successful runtime launch should:

- load `libvulkan_freedreno.so`;
- enumerate a Vulkan physical device through Turnip;
- report a Mesa/Turnip driver identity;
- execute the benchmark;
- pass benchmark verification; and
- return exit code zero.

# From the profiler

When used with the streamer or sweeper, expected outputs remain the normal
profiler outputs:

```text
streamer CSV
sweeper group/chunk CSVs
benchmark logs
metadata
```

The configuration files do not create a new profiler output format.

---

# Security and repository considerations

## The cross file contains a local path

The file exposes only a local filesystem path, not a credential.

However, the path is personal and reduces repository portability.

## The ICD manifest contains a device path

The device path is not secret.

It should remain synchronized with deployment scripts.

## Do not put secrets in configuration files

Do not add:

- signing keys;
- passwords;
- private SSH keys;
- access tokens;
- device unlock credentials; or
- proprietary driver binaries

to this directory.

## Generated files

Do not store large Meson build directories under `configs/`.

Recommended:

```text
build/mesa-android-aarch64/
```

The `configs/` directory should contain small, reviewable inputs.

---

# Known limitations

## Machine-specific NDK path

The cross file is tied to:

```text
/Users/jerryyun/android-ndk-r27d
```

## Fixed Android API

The compiler wrappers target:

```text
Android API 29
```

Changing the API requires changing both `c` and `cpp`.

## Fixed host prebuilt tag

The file assumes:

```text
darwin-x86_64
```

## No explicit sysroot property

The configuration relies on the NDK Clang driver to infer its sysroot.

## No target-aware `pkg-config`

`pkg-config` is disabled.

This prevents host contamination but can require extra dependency configuration.

## No executable wrapper

The file says a wrapper is needed but does not define one.

Target programs cannot be run during the host build.

## Fixed remote library path

The ICD manifest expects one exact device path.

## Fixed manifest API version

The JSON advertises Vulkan 1.3.0 regardless of the exact Mesa build.

## No deployment script in this directory

The commands are documented here, but the directory does not currently include:

```text
build_turnip.sh
push_turnip.sh
run_with_turnip.sh
verify_turnip.sh
```

## No build metadata

The config files do not record:

- Mesa commit;
- NDK revision verification;
- Meson version;
- build options;
- library hash;
- device build fingerprint; or
- benchmark configuration.

## Shell-only runtime assumption

The manifest workflow is best suited to native binaries launched through
`adb shell`.

---

# Recommended maintenance

1. Replace the committed absolute-path cross file with a portable template.
2. Add a script that generates a local cross file from:
   - `ANDROID_NDK_HOME`
   - `ANDROID_API`
   - `NDK_HOST_TAG`
3. Add a build script that records the full Meson command.
4. Add a deployment script that pushes:
   - `libvulkan_freedreno.so`
   - the ICD JSON
   - required dependent libraries
5. Add a wrapper that launches a command with the Turnip environment.
6. Add a verification script that prints:
   - driver name
   - API version
   - Mesa commit
   - library hash
7. Consider naming the device path through one shared variable in deployment
   scripts.
8. Generate the ICD JSON during deployment so `library_path` cannot drift.
9. Record the exact cross file and manifest with every formal experiment.
10. Use a separate build directory for each:
    - Mesa revision
    - NDK revision
    - Android API
    - build type
11. Keep vendor and Turnip benchmark binaries and shaders identical during
    comparisons.
12. Confirm workload verification before starting a long profiler sweep.
13. Do not infer actual Vulkan feature support only from `api_version` in the
    manifest.
14. Add checks that the output is AArch64 ELF before pushing.
15. Add checks that the JSON’s `library_path` exists on the device.

---

# Suggested future structure

```text
configs/
├── README.md
├── build/
│   └── mesa_android_cross/
│       ├── android-aarch64.cross.template
│       └── README.md
└── device/
    └── turnip/
        ├── freedreno_icd.aarch64.json.template
        └── README.md

scripts/
├── build/
│   ├── generate_android_cross_file.sh
│   └── build_turnip_android.sh
├── device/
│   ├── push_turnip.sh
│   └── verify_turnip.sh
└── run/
    └── run_with_turnip.sh
```

A generated local cross file and generated runtime manifest can live under:

```text
build/config/
```

and should normally be ignored by Git.

---

# Quick-reference workflow

## 1. Validate configuration

```bash
python3 -m json.tool \
  configs/device/turnip/freedreno_icd.aarch64.json

test -x \
  /Users/jerryyun/android-ndk-r27d/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android29-clang
```

## 2. Configure Mesa

```bash
meson setup \
  build/mesa-android-aarch64 \
  third_party/mesa \
  --cross-file configs/build/mesa_android_cross/android-aarch64.cross \
  --buildtype release \
  -Dplatforms=android \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dllvm=disabled
```

## 3. Build

```bash
meson compile -C build/mesa-android-aarch64
```

## 4. Locate driver

```bash
TURNIP_SO="$(
  find build/mesa-android-aarch64 \
    -type f \
    -name 'libvulkan_freedreno.so' \
    -print \
    -quit
)"
```

## 5. Push

```bash
adb shell \
  'mkdir -p /data/local/tmp/jerry_work/turnip'

adb push \
  "$TURNIP_SO" \
  /data/local/tmp/jerry_work/turnip/libvulkan_freedreno.so

adb push \
  configs/device/turnip/freedreno_icd.aarch64.json \
  /data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json
```

## 6. Run through Turnip

```bash
adb shell \
  'VK_ICD_FILENAMES=/data/local/tmp/jerry_work/turnip/freedreno_icd.aarch64.json \
   LD_LIBRARY_PATH=/data/local/tmp/jerry_work/turnip:$LD_LIBRARY_PATH \
   /data/local/tmp/jerry_work/vulkan_probe'
```

## 7. Confirm driver identity

Check the program’s:

```text
driverName
deviceName
apiVersion
driverVersion
```

## 8. Profile

Start the streamer or sweeper separately, then launch the benchmark with the
Turnip environment.

---

# Quick file summary

```text
android-aarch64.cross
    host-side Meson cross file
    selects Android NDK ARM64 tools
    targets Android API 29
    disables host pkg-config
    marks target executables as non-runnable on macOS

freedreno_icd.aarch64.json
    device-side Vulkan ICD manifest
    points to libvulkan_freedreno.so
    advertises Vulkan API 1.3.0
    used to select Turnip for shell-launched Vulkan programs
```
