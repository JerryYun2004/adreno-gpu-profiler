#!/usr/bin/env bash
set -euo pipefail

DEST_ROOT="/Users/jerryyun/adreno-gpu-profiler/results/perfcounter_sweeps"

mkdir -p "$DEST_ROOT"

echo "[host] Finding latest sweep on phone..."

LATEST_SWEEP=$(adb shell 'su -c "ls -td /data/local/tmp/jerry_work/perfcounter_sweeps/sweep_* 2>/dev/null | head -n 1"' | tr -d '\r')

if [ -z "$LATEST_SWEEP" ]; then
  echo "[host] ERROR: No sweep folder found on phone."
  exit 1
fi

SWEEP_NAME=$(basename "$LATEST_SWEEP")

echo "[host] Latest sweep:"
echo "       $LATEST_SWEEP"
echo

echo "[host] Pulling to:"
echo "       $DEST_ROOT/$SWEEP_NAME"
echo

adb pull "$LATEST_SWEEP" "$DEST_ROOT/"

echo
echo "[host] Pull complete."

echo "[host] Pulled files:"
find "$DEST_ROOT/$SWEEP_NAME" -maxdepth 3 -type f | sort | head -n 40

echo
echo "[host] Summary file:"
find "$DEST_ROOT/$SWEEP_NAME" -name "summary.csv" -print

echo
echo "[host] Done."
