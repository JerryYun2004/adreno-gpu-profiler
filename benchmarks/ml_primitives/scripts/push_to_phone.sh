#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PHONE_DIR="${PHONE_DIR:-/data/local/tmp/jerry_work/ml_primitives}"
adb shell "mkdir -p $PHONE_DIR/spv"
adb push "$ROOT/build/android/ml_primitive_bench" "$PHONE_DIR/ml_primitive_bench"
adb push "$ROOT/build/spv/"*.spv "$PHONE_DIR/spv/"
adb shell "chmod +x $PHONE_DIR/ml_primitive_bench"
echo "Pushed benchmark to $PHONE_DIR"
