# Third-Party Modules

This directory contains external projects used by the Adreno GPU profiler repository. They are maintained upstream and included as Git submodules rather than as project-owned source code.

| Module | Purpose in this repository |
|---|---|
| [`glslang`](https://github.com/KhronosGroup/glslang) | Khronos reference compiler for GLSL and HLSL. It provides `glslangValidator`, which is used to compile Vulkan compute shaders (`.comp`) into SPIR-V binaries (`.spv`) for the GPU benchmarks. |
| [`mesa`](https://gitlab.freedesktop.org/mesa/mesa) | Open-source graphics stack containing Freedreno and the Turnip Vulkan driver for Qualcomm Adreno GPUs. It is used for Turnip Android builds, driver comparisons, and reference information related to Adreno hardware and performance counters. |

## Setup

Clone the repository together with its submodules:

```bash
git clone --recurse-submodules <repository-url>
```

For an existing clone, initialize and update them with:

```bash
git submodule update --init --recursive
```

To inspect the checked-out revisions:

```bash
git submodule status
```

## Usage

### Compile compute shaders with glslang

```bash
third_party/glslang/build/StandAlone/glslangValidator   -V   --target-env vulkan1.1   path/to/shader.comp   -o path/to/shader.comp.spv
```

A system-installed `glslangValidator` may be used instead.

### Build Turnip from Mesa

Run the repository helper from the Mesa source root:

```bash
cd third_party/mesa
../../scripts/build_turnip_android_kgsl.sh
```

This produces an Android ARM64 Turnip Vulkan driver build. Mesa is optional for the normal KGSL streamer and sweeper workflow; the profiler can also be used with the device's vendor Vulkan driver.

## Notes

- Avoid making unrelated changes inside these directories.
- Update a dependency by changing its submodule commit in this repository.
- Submodule contents are not included automatically by a normal clone unless they are initialized.
- Refer to each upstream project's own documentation for detailed build and licensing information.
