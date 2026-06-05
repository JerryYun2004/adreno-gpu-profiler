#!/usr/bin/env bash
set -euo pipefail

# Mesa source root: this script should be run from the Mesa directory.
MESA_DIR="$(pwd)"

# Find Android NDK.
# Edit this if your NDK is somewhere else.
if [ -n "${ANDROID_NDK_HOME:-}" ]; then
  NDK="$ANDROID_NDK_HOME"
elif [ -d "$HOME/Library/Android/sdk/ndk" ]; then
  NDK="$(find "$HOME/Library/Android/sdk/ndk" -maxdepth 1 -mindepth 1 -type d | sort | tail -1)"
elif ls "$HOME"/android-ndk-* >/dev/null 2>&1; then
  NDK="$(ls -d "$HOME"/android-ndk-* | sort | tail -1)"
else
  echo "ERROR: Could not find Android NDK."
  echo "Set ANDROID_NDK_HOME first, for example:"
  echo "  export ANDROID_NDK_HOME=/path/to/android-ndk-r27d"
  exit 1
fi

API=29

# NDK host tag.
UNAME_S="$(uname -s)"
case "$UNAME_S" in
  Darwin)
    HOST="darwin-x86_64"
    ;;
  Linux)
    HOST="linux-x86_64"
    ;;
  *)
    echo "ERROR: unsupported host OS: $UNAME_S"
    exit 1
    ;;
esac

TC="$NDK/toolchains/llvm/prebuilt/$HOST"

if [ ! -d "$TC" ]; then
  echo "ERROR: NDK toolchain path not found:"
  echo "  $TC"
  echo
  echo "Available prebuilt toolchains:"
  find "$NDK/toolchains/llvm/prebuilt" -maxdepth 1 -mindepth 1 -type d 2>/dev/null || true
  exit 1
fi

echo "[build] Mesa: $MESA_DIR"
echo "[build] NDK:  $NDK"
echo "[build] HOST: $HOST"
echo "[build] TC:   $TC"

cat > android-aarch64.cross <<CROSS
[binaries]
ar = '$TC/bin/llvm-ar'
c = ['$TC/bin/aarch64-linux-android${API}-clang']
cpp = ['$TC/bin/aarch64-linux-android${API}-clang++']
strip = '$TC/bin/llvm-strip'
pkg-config = '/bin/false'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[properties]
needs_exe_wrapper = true
CROSS

rm -rf build-android

meson setup build-android \
  --cross-file android-aarch64.cross \
  -Dallow-fallback-for=libdrm \
  --force-fallback-for=zlib,expat \
  --wrap-mode=default \
  -Dplatforms=android \
  -Dplatform-sdk-version=${API} \
  -Dandroid-stub=true \
  -Dandroid-strict=false \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dfreedreno-kmds=kgsl \
  -Dperfetto=false \
  -Dgles1=disabled \
  -Dgles2=disabled \
  -Degl=disabled \
  -Dglx=disabled \
  -Dllvm=disabled \
  -Dshared-llvm=disabled \
  -Dbuildtype=release \
  -Db_lto=false

ninja -C build-android src/freedreno/vulkan/libvulkan_freedreno.so

echo
echo "[build] Done. Output:"
find build-android -name "libvulkan_freedreno.so" -print
