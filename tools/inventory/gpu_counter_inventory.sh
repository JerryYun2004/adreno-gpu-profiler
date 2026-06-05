#!/usr/bin/env bash
set -u

OUT_ROOT="${OUT_ROOT:-logs/perfcounter_access/00_inventory}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$OUT_ROOT/$TS"
mkdir -p "$OUT_DIR"

LOG="$OUT_DIR/inventory.txt"

{
  echo "=== Host ==="
  date
  uname -a
  pwd
  echo

  echo "=== ADB device ==="
  adb devices -l
  echo

  echo "=== Android identity ==="
  adb shell 'id; uname -a; getprop ro.product.model; getprop ro.hardware; getprop ro.board.platform; getprop ro.build.fingerprint; getprop ro.build.version.release; getprop ro.build.version.sdk' 2>&1
  echo

  echo "=== GPU device nodes ==="
  adb shell 'ls -l /dev/dri /dev/dri/* /dev/kgsl* 2>/dev/null || true' 2>&1
  echo

  echo "=== SELinux labels for GPU device nodes ==="
  adb shell 'ls -lZ /dev/dri /dev/dri/* /dev/kgsl* 2>/dev/null || true' 2>&1
  echo

  echo "=== KGSL sysfs ==="
  adb shell 'find /sys/class/kgsl -maxdepth 4 -type f -o -type l 2>/dev/null | sort' 2>&1
  echo

  echo "=== KGSL readable sysfs values ==="
  adb shell 'for f in /sys/class/kgsl/kgsl-3d0/*; do if [ -f "$f" ]; then echo "--- $f"; cat "$f" 2>&1; fi; done' 2>&1
  echo

  echo "=== DRM sysfs ==="
  adb shell 'find /sys/class/drm -maxdepth 4 -type f -o -type l 2>/dev/null | sort' 2>&1
  echo

  echo "=== DRM driver/module hints ==="
  adb shell 'for d in /sys/class/drm/card* /sys/class/drm/renderD*; do echo "--- $d"; readlink -f "$d/device/driver" 2>/dev/null || true; cat "$d/device/uevent" 2>/dev/null || true; done' 2>&1
  echo

  echo "=== Debugfs/tracing availability ==="
  adb shell 'mount | grep -E "debugfs|tracefs" || true; ls -ld /sys/kernel/debug /sys/kernel/tracing /d 2>/dev/null || true' 2>&1
} | tee "$LOG"

echo "[host] Saved inventory to: $OUT_DIR"
