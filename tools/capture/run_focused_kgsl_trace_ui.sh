#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="$HOME/adreno_turnip"
RAW="$OUT_DIR/kgsl_focused_trace_ui.log"
FILTERED="$OUT_DIR/kgsl_focused_trace_ui_filtered.log"
SUMMARY="$OUT_DIR/kgsl_focused_trace_ui_summary.txt"

echo "[host] Stop and clear tracing..."
adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'
adb shell 'su -c "echo > /sys/kernel/tracing/trace"'

echo "[host] Disable all KGSL events first..."
adb shell 'su -c "for e in /sys/kernel/tracing/events/kgsl/*/enable; do echo 0 > \$e 2>/dev/null || true; done"'

echo "[host] Enable focused KGSL events..."
for ev in \
  adreno_cmdbatch_queued \
  adreno_cmdbatch_ready \
  adreno_cmdbatch_submitted \
  adreno_cmdbatch_retired \
  adreno_cmdbatch_done \
  adreno_cmdbatch_fault \
  kgsl_issueibcmds \
  kgsl_context_create \
  kgsl_context_destroy \
  adreno_drawctxt_switch \
  kgsl_mem_alloc \
  kgsl_mem_free \
  kgsl_mem_map \
  kgsl_mem_sync_cache \
  kgsl_gpubusy \
  kgsl_pwrstats \
  kgsl_pwrlevel \
  gpu_frequency \
  kgsl_buslevel \
  kgsl_clk \
  kgsl_rail \
  kgsl_pwr_set_state \
  kgsl_pwr_request_state
do
  adb shell "su -c 'echo 1 > /sys/kernel/tracing/events/kgsl/$ev/enable 2>/dev/null || true'"
done

echo "[host] Start tracing..."
adb shell 'su -c "echo 1 > /sys/kernel/tracing/tracing_on"'

echo "[host] Generate ADB UI activity..."
adb shell 'input keyevent KEYCODE_WAKEUP' || true
adb shell 'input keyevent KEYCODE_HOME' || true
adb shell 'input swipe 500 1600 500 400 300' || true
adb shell 'input swipe 500 400 500 1600 300' || true
adb shell 'input swipe 500 1600 500 400 300' || true
adb shell 'input swipe 500 400 500 1600 300' || true
sleep 3

echo "[host] Stop tracing..."
adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'

echo "[host] Pull trace..."
adb shell 'su -c "cat /sys/kernel/tracing/trace"' | tee "$RAW" >/dev/null

echo "[host] Filter KGSL lines..."
grep -E "adreno_cmdbatch|kgsl_issueibcmds|kgsl_context|drawctxt|kgsl_mem_|kgsl_gpubusy|kgsl_pwrstats|kgsl_pwrlevel|gpu_frequency|kgsl_buslevel|kgsl_clk|kgsl_rail|kgsl_pwr_" "$RAW" \
  | tee "$FILTERED" >/dev/null || true

echo "[host] Event summary..."
grep -oE ': [a-zA-Z0-9_]+:' "$FILTERED" \
  | sed 's/^: //; s/:$//' \
  | sort | uniq -c | sort -nr \
  | tee "$SUMMARY"

echo
echo "[host] Files written:"
echo "  $RAW"
echo "  $FILTERED"
echo "  $SUMMARY"
