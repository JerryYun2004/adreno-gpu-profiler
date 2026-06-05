#!/bin/bash
set -euo pipefail

OUT_DIR="${1:-kgsl_interface_inventory/$(date +%Y%m%d_%H%M%S)}"

mkdir -p "$OUT_DIR"

echo "[host] Output directory: $OUT_DIR"

echo "[host] Saving KGSL sysfs tree..."
adb shell 'su -c "find -L /sys/class/kgsl/kgsl-3d0 -maxdepth 4 -type f 2>/dev/null | sort"' \
  > "$OUT_DIR/kgsl_sysfs_files.txt" || true

echo "[host] Saving readable KGSL sysfs node values..."
adb shell 'su -c "
OUT=/data/local/tmp/kgsl_readable_nodes_inventory.csv
BASE=/sys/class/kgsl/kgsl-3d0

echo \"path,value\" > \$OUT

find -L \$BASE -maxdepth 4 -type f 2>/dev/null | sort | while read f; do
  case \"\$f\" in
    */driver/bind|*/driver/unbind|*/snapshot/force_panic|*/snapshot/minidump_test|*/snapshot/dump)
      continue
      ;;
  esac

  v=\$(timeout 0.2 cat \"\$f\" 2>/dev/null | tr \"\n\" \" \" | sed \"s/[[:space:]]\+/ /g; s/^ //; s/ \$//\")

  if [ -n \"\$v\" ]; then
    v=\$(echo \"\$v\" | sed \"s/\"\"/\"\"\"\"/g\")
    echo \"\\\"\$f\\\",\\\"\$v\\\"\" >> \$OUT
  fi
done
"' || true

adb pull /data/local/tmp/kgsl_readable_nodes_inventory.csv \
  "$OUT_DIR/kgsl_readable_nodes_inventory.csv" >/dev/null || true

echo "[host] Saving KGSL tracepoint list..."
adb shell 'su -c "find /sys/kernel/tracing/events/kgsl -maxdepth 2 -type f 2>/dev/null | sort"' \
  > "$OUT_DIR/kgsl_tracefs_files.txt" || true

echo "[host] Saving KGSL tracepoint event names..."
adb shell 'su -c "find /sys/kernel/tracing/events/kgsl -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sed \"s#.*/##\" | sort"' \
  > "$OUT_DIR/kgsl_tracepoint_names.txt" || true

echo "[host] Saving KGSL tracepoint formats..."
mkdir -p "$OUT_DIR/tracepoint_formats"

while read -r event; do
  [ -z "$event" ] && continue
  adb shell "su -c 'cat /sys/kernel/tracing/events/kgsl/$event/format 2>/dev/null'" \
    > "$OUT_DIR/tracepoint_formats/${event}.format" || true
done < "$OUT_DIR/kgsl_tracepoint_names.txt"

echo "[host] Saving potentially relevant global GPU nodes..."
adb shell 'su -c "find /sys/kernel/gpu -type f 2>/dev/null | sort"' \
  > "$OUT_DIR/sys_kernel_gpu_files.txt" || true

adb shell 'su -c "
OUT=/data/local/tmp/sys_kernel_gpu_inventory.csv
echo \"path,value\" > \$OUT

find /sys/kernel/gpu -type f 2>/dev/null | sort | while read f; do
  v=\$(timeout 0.2 cat \"\$f\" 2>/dev/null | tr \"\n\" \" \" | sed \"s/[[:space:]]\+/ /g; s/^ //; s/ \$//\")
  if [ -n \"\$v\" ]; then
    v=\$(echo \"\$v\" | sed \"s/\"\"/\"\"\"\"/g\")
    echo \"\\\"\$f\\\",\\\"\$v\\\"\" >> \$OUT
  fi
done
"' || true

adb pull /data/local/tmp/sys_kernel_gpu_inventory.csv \
  "$OUT_DIR/sys_kernel_gpu_inventory.csv" >/dev/null || true

echo "[host] Done."
echo "[host] Saved inventory to: $OUT_DIR"
