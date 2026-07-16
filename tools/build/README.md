# `cross_compile_x86_adreno`

This directory contains a host-side Bash script for cross-compiling Mesa's Turnip Vulkan driver for an ARM64 Android device with a Qualcomm Adreno GPU.

## Purpose

`cross_compile_x86_adreno` configures and builds:

```text
libvulkan_freedreno.so
```

using:

- an x86-64 Linux host;
- Android NDK r27d;
- Android API level 29;
- Mesa's Freedreno Vulkan driver;
- the KGSL kernel backend; and
- a release build without LLVM, EGL, OpenGL, or GLES.

The resulting library is an Android ARM64 build of the Mesa Turnip Vulkan driver.

## Relationship to the profiler

This script is **not required** to build or run the KGSL perf-counter streamer or sweeper.

It is useful for optional experiments involving:

```text
Qualcomm vendor Vulkan driver
versus
Mesa Turnip Vulkan driver
```

The profiler can collect KGSL hardware counters while a workload runs through either driver, provided the workload executes correctly on the device.

A newer and more portable version of this workflow is available at:

```text
scripts/build_turnip_android_kgsl.sh
```

The script in this directory is retained as the original Linux-host build setup.

## What the script does

The script performs the following steps:

1. Changes into the Mesa source directory.
2. Defines the Android NDK, host toolchain, and API level.
3. Generates a Meson cross-compilation file:
   ```text
   android-aarch64.cross
   ```
4. Configures Mesa in:
   ```text
   build-android/
   ```
5. Builds only:
   ```text
   src/freedreno/vulkan/libvulkan_freedreno.so
   ```

## Current hardcoded configuration

```bash
MESA_DIR=/users/Lakshman/mesa
NDK=/users/Lakshman/android-ndk-r27d
HOST=linux-x86_64
API=29
```

These paths are specific to the original development machine. Update them before running the script on another system.

For example:

```bash
cd /Users/jerryyun/adreno-gpu-profiler/third_party/mesa

NDK="$HOME/android-ndk-r27d"
HOST="darwin-x86_64"
API=29
```

On macOS, prefer using:

```text
scripts/build_turnip_android_kgsl.sh
```

because it detects the host platform and common NDK locations automatically.

## Build configuration

The Meson command enables:

```text
platforms=android
vulkan-drivers=freedreno
freedreno-kmds=kgsl
buildtype=release
```

It disables components that are not needed for the Turnip Vulkan build:

```text
Gallium drivers
Perfetto
OpenGL ES 1
OpenGL ES 2
EGL
GLX
LLVM
shared LLVM
link-time optimization
```

It also sets:

```text
android-stub=true
android-strict=false
```

`android-strict=false` prevents Mesa from dropping Vulkan extensions that are not present on Android's compatibility allowlist.

Fallback dependencies are enabled for:

```text
libdrm
zlib
expat
```

## Requirements

### Host system

The script was written for:

```text
Linux x86-64
```

Required tools include:

- Bash
- Git
- Python 3
- Meson 1.4.0
- Ninja
- CMake
- Make
- `pkg-config`
- Bison
- Flex
- Python packages:
  - Mako
  - PyYAML
  - setuptools
- Android NDK r27d
- Mesa source tree

Example Ubuntu setup:

```bash
sudo apt update

sudo apt install -y \
  git \
  cmake \
  make \
  ninja-build \
  python3 \
  python3-mako \
  python3-pip \
  python3-venv \
  pkg-config \
  bison \
  flex
```

Create a Python environment:

```bash
python3 -m venv .venv
source .venv/bin/activate

pip install \
  meson==1.4.0 \
  pyyaml \
  setuptools
```

## Optional glslang setup

The comments at the top of the script include instructions for building Khronos glslang:

```bash
git clone https://github.com/KhronosGroup/glslang.git
cd glslang

python3 update_glslang_sources.py

cmake -S . -B build
cmake --build build -j16
sudo cmake --install build

which glslangValidator
```

`glslangValidator` compiles Vulkan GLSL compute shaders from `.comp` source into `.spv` modules.

It is useful for the benchmark workflow, but it is not directly invoked by this Turnip build script.

This repository also includes glslang as a submodule under:

```text
third_party/glslang/
```

## Usage

Make the script executable:

```bash
chmod +x tools/build/cross_compile_x86_adreno/cross_compile_x86_adreno
```

After updating its hardcoded paths, run:

```bash
./tools/build/cross_compile_x86_adreno/cross_compile_x86_adreno
```

Because the script changes into its own hardcoded Mesa directory, it may be launched from any working directory.

## Expected files

The script creates:

```text
/users/Lakshman/mesa/android-aarch64.cross
/users/Lakshman/mesa/build-android/
```

The requested build product should appear somewhere under:

```text
/users/Lakshman/mesa/build-android/
```

Locate it with:

```bash
find /users/Lakshman/mesa/build-android \
  -name libvulkan_freedreno.so \
  -print
```

After adapting the script to this repository, use:

```bash
find third_party/mesa/build-android \
  -name libvulkan_freedreno.so \
  -print
```

## Rebuilding

The script does not remove an existing `build-android/` directory.

For a clean rebuild:

```bash
rm -rf /users/Lakshman/mesa/build-android
```

or, for the repository-local Mesa checkout:

```bash
rm -rf third_party/mesa/build-android
```

Then rerun the script.

## Limitations

- Mesa and NDK paths are hardcoded.
- The host toolchain is fixed to `linux-x86_64`.
- The Android API level is fixed to 29.
- Existing Meson build directories are not cleaned automatically.
- The script builds the driver but does not deploy or select it on the phone.
- Driver installation and activation depend on the Android environment and are outside the scope of this script.
- It is retained mainly for reproducibility of the original build environment; the portable helper under `scripts/` is preferred for normal use.
