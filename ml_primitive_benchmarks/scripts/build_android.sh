#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your Android NDK path, e.g. ~/Library/Android/sdk/ndk/27.2.12479018}"
API="${ANDROID_API:-35}"
CXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang++"
if [[ ! -x "$CXX" ]]; then
  CXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang++"
fi
OUT="$ROOT/build/android"
mkdir -p "$OUT"
"$CXX" -std=c++17 -O3 -DNDEBUG \
  "$ROOT/support_sw/vulkan_ml_primitive_bench/ml_primitive_bench.cpp" \
  -o "$OUT/ml_primitive_bench" \
  -lvulkan -llog
file "$OUT/ml_primitive_bench"
echo "Built $OUT/ml_primitive_bench"
