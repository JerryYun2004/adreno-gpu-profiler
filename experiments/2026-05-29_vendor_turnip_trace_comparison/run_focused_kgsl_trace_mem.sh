#!/bin/bash
set -euo pipefail

OUT_DIR="$HOME/adreno_turnip"
RAW_LOG="$OUT_DIR/kgsl_focused_trace_compute.log"
FILTERED_LOG="$OUT_DIR/kgsl_focused_trace_compute_filtered.log"
SUMMARY="$OUT_DIR/kgsl_focused_trace_compute_summary.txt"

WORKDIR=/data/local/tmp/jerry_work
WORKLOAD="./vk_mem_probe mem.comp.spv"

echo "[host] Create device-side trace setup script..."

adb shell "cat > /data/local/tmp/kgsl_trace_setup_compute.sh <<'EOS'
#!/system/bin/sh

TRACE=/sys/kernel/tracing
EVENTS=\$TRACE/events/kgsl

echo 0 > \$TRACE/tracing_on
echo > \$TRACE/trace

for e in \$EVENTS/*/enable; do
  echo 0 > \"\$e\" 2>/dev/null || true
done

for ev in \
  adreno_cmdbatch_queued \
  adreno_cmdbatch_submitted \
  adreno_cmdbatch_ready \
  adreno_cmdbatch_retired \
  adreno_cmdbatch_done \
  kgsl_issueibcmds \
  kgsl_gpubusy \
  kgsl_pwrstats \
  kgsl_pwrlevel \
  kgsl_buslevel \
  gpu_frequency \
  kgsl_mem_alloc \
  kgsl_mem_map \
  kgsl_mem_free \
  kgsl_mem_sync_cache \
  kgsl_context_create \
  kgsl_pwr_set_state \
  kgsl_pwr_request_state
do
  if [ -e \$EVENTS/\$ev/enable ]; then
    echo 1 > \$EVENTS/\$ev/enable
  fi
done
EOS"

adb shell 'chmod +x /data/local/tmp/kgsl_trace_setup_compute.sh'

echo "[host] Stop, clear, disable old events, and enable focused KGSL events..."
adb shell 'su -c "sh /data/local/tmp/kgsl_trace_setup_compute.sh"'

echo "[host] Start tracing..."
adb shell 'su -c "echo 1 > /sys/kernel/tracing/tracing_on"'

echo "[host] Run Vulkan compute workload..."
adb shell "cd $WORKDIR && $WORKLOAD"

echo "[host] Stop tracing..."
adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'

echo "[host] Pull trace..."
adb shell 'su -c "cat /sys/kernel/tracing/trace"' > "$RAW_LOG"

echo "[host] Filter KGSL lines..."
grep -E "kgsl|adreno_cmdbatch|gpu_frequency" "$RAW_LOG" > "$FILTERED_LOG" || true

echo "[host] Event summary..."
awk '
{
  for (i = 1; i <= NF; i++) {
    if ($i ~ /kgsl_|adreno_cmdbatch_|gpu_frequency/) {
      name=$i
      sub(/:.*/, "", name)
      count[name]++
    }
  }
}
END {
  for (name in count) print count[name], name
}
' "$FILTERED_LOG" | sort -nr | tee "$SUMMARY"

echo
echo "[host] Files written:"
echo "  $RAW_LOG"
echo "  $FILTERED_LOG"
echo "  $SUMMARY"
