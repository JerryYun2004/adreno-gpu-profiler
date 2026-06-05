#!/bin/bash
set -euo pipefail

# Validate KGSL sysfs busy nodes against KGSL tracepoint/systrace busy values.
#
# Usage:
#   ./tools/run_kgsl_busy_validation.sh compute
#   ./tools/run_kgsl_busy_validation.sh mem
#
# Optional:
#   SAMPLE_INTERVAL_S=0.05 ./tools/run_kgsl_busy_validation.sh compute
#   OUT_ROOT=results/kgsl_busy_validation ./tools/run_kgsl_busy_validation.sh mem

KIND="${1:-compute}"
SAMPLE_INTERVAL_S="${SAMPLE_INTERVAL_S:-0.05}"
WORKLOAD_REPEAT="${WORKLOAD_REPEAT:-1}"
POST_SAMPLE_S="${POST_SAMPLE_S:-0.5}"

WORKDIR="/data/local/tmp/jerry_work"

case "$KIND" in
  compute)
    WORKLOAD="./vk_compute_probe"
    ;;
  mem|memory)
    KIND="mem"
    WORKLOAD="./vk_mem_probe mem.comp.spv"
    ;;
  *)
    echo "Usage: $0 {compute|mem}"
    exit 1
    ;;
esac

OUT_ROOT="${OUT_ROOT:-$HOME/adreno-gpu-profiler/results/kgsl_busy_validation}"
RUN_ID="$(date +%Y%m%d_%H%M%S)_${KIND}"
OUT_DIR="$OUT_ROOT/$RUN_ID"

RAW_TRACE="$OUT_DIR/kgsl_trace_raw.log"
FILTERED_TRACE="$OUT_DIR/kgsl_trace_filtered.log"
SYSFS_CSV="$OUT_DIR/kgsl_sysfs_busy_samples.csv"
SAMPLER_LOG="$OUT_DIR/kgsl_sysfs_sampler_device.log"
WORKLOAD_LOG="$OUT_DIR/workload_stdout.log"
SUMMARY_TXT="$OUT_DIR/validation_summary.txt"

DEVICE_TRACE_SETUP="/data/local/tmp/kgsl_trace_setup_validation.sh"
DEVICE_SAMPLER="/data/local/tmp/kgsl_busy_sampler.sh"
DEVICE_SYSFS_CSV="/data/local/tmp/kgsl_sysfs_busy_samples.csv"
DEVICE_SAMPLER_LOG="/data/local/tmp/kgsl_busy_sampler.log"
DEVICE_SAMPLER_PID="/data/local/tmp/kgsl_busy_sampler.pid"
DEVICE_STOP_FLAG="/data/local/tmp/kgsl_busy_sampler.stop"

mkdir -p "$OUT_DIR"

echo "[host] Output directory:"
echo "  $OUT_DIR"
echo

cleanup() {
  echo "[host] Cleanup: stop trace and sampler if still running..."
  adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on 2>/dev/null || true"' >/dev/null 2>&1 || true
  adb shell "su -c 'touch $DEVICE_STOP_FLAG 2>/dev/null || true'" >/dev/null 2>&1 || true
  adb shell "su -c 'if [ -f $DEVICE_SAMPLER_PID ]; then kill \$(cat $DEVICE_SAMPLER_PID) 2>/dev/null || true; fi'" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "[host] Create device-side trace setup script..."
adb shell "cat > $DEVICE_TRACE_SETUP <<'EOS'
#!/system/bin/sh
set -eu

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

echo "[host] Create device-side sysfs sampler..."
cat > /tmp/kgsl_busy_sampler.sh <<'EOS'
#!/system/bin/sh

INTERVAL_S="${1:-0.05}"
OUT="${2:-/data/local/tmp/kgsl_sysfs_busy_samples.csv}"

KGSL=/sys/class/kgsl/kgsl-3d0
STOP=/data/local/tmp/kgsl_busy_sampler.stop

rm -f "$STOP"

echo "t_s,gpu_busy_percentage,gpubusy_busy,gpubusy_elapsed,gpubusy_pct,devfreq_gpu_load,cur_freq_hz,gpuclk_hz" > "$OUT"

while [ ! -e "$STOP" ]; do
  # /proc/uptime is close to the same monotonic timebase used by ftrace timestamps.
  t_s="$(cat /proc/uptime | awk '{print $1}')"

  gpu_busy_percentage=""
  if [ -r "$KGSL/gpu_busy_percentage" ]; then
    gpu_busy_percentage="$(cat "$KGSL/gpu_busy_percentage" 2>/dev/null | awk '{print $1}')"
  fi

  gpubusy_busy=""
  gpubusy_elapsed=""
  gpubusy_pct=""
  if [ -r "$KGSL/gpubusy" ]; then
    set -- $(cat "$KGSL/gpubusy" 2>/dev/null)
    gpubusy_busy="${1:-}"
    gpubusy_elapsed="${2:-}"
    if [ -n "$gpubusy_busy" ] && [ -n "$gpubusy_elapsed" ]; then
      gpubusy_pct="$(awk -v b="$gpubusy_busy" -v e="$gpubusy_elapsed" 'BEGIN { if (e > 0) printf "%.6f", 100.0*b/e; else printf "" }')"
    fi
  fi

  devfreq_gpu_load=""
  if [ -r "$KGSL/devfreq/gpu_load" ]; then
    devfreq_gpu_load="$(cat "$KGSL/devfreq/gpu_load" 2>/dev/null | awk '{print $1}')"
  fi

  cur_freq_hz=""
  if [ -r "$KGSL/devfreq/cur_freq" ]; then
    cur_freq_hz="$(cat "$KGSL/devfreq/cur_freq" 2>/dev/null | awk '{print $1}')"
  fi

  gpuclk_hz=""
  if [ -r "$KGSL/gpuclk" ]; then
    gpuclk_hz="$(cat "$KGSL/gpuclk" 2>/dev/null | awk '{print $1}')"
  fi

  echo "$t_s,$gpu_busy_percentage,$gpubusy_busy,$gpubusy_elapsed,$gpubusy_pct,$devfreq_gpu_load,$cur_freq_hz,$gpuclk_hz" >> "$OUT"

  sleep "$INTERVAL_S"
done
EOS

adb push /tmp/kgsl_busy_sampler.sh "$DEVICE_SAMPLER" >/dev/null
rm -f /tmp/kgsl_busy_sampler.sh

adb shell "chmod +x $DEVICE_TRACE_SETUP $DEVICE_SAMPLER"

echo "[host] Stop, clear, and enable focused KGSL tracepoints..."
adb shell "su -c 'sh $DEVICE_TRACE_SETUP'"

echo "[host] Remove old sampler files..."
adb shell "su -c 'rm -f $DEVICE_SYSFS_CSV $DEVICE_SAMPLER_LOG $DEVICE_SAMPLER_PID $DEVICE_STOP_FLAG'"

echo "[host] Start sysfs sampler at interval ${SAMPLE_INTERVAL_S}s..."
adb shell "su -c 'sh $DEVICE_SAMPLER $SAMPLE_INTERVAL_S $DEVICE_SYSFS_CSV > $DEVICE_SAMPLER_LOG 2>&1 & echo \$! > $DEVICE_SAMPLER_PID'"

sleep 0.2

echo "[host] Start KGSL tracing..."
adb shell 'su -c "echo 1 > /sys/kernel/tracing/tracing_on"'

echo "[host] Run workload: $WORKLOAD"
echo "[host] Workload repeat count: $WORKLOAD_REPEAT"

set +e
adb shell "cd $WORKDIR && i=1; while [ \$i -le $WORKLOAD_REPEAT ]; do echo \"[device] workload iteration \$i/$WORKLOAD_REPEAT\"; $WORKLOAD || exit \$?; i=\$((i+1)); done" | tee "$WORKLOAD_LOG"
WORKLOAD_RC=${PIPESTATUS[0]}
set -e

echo "[host] Keep sampling for ${POST_SAMPLE_S}s after workload to catch delayed sysfs updates..."
sleep "$POST_SAMPLE_S"

echo "[host] Stop KGSL tracing..."
adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'

echo "[host] Stop sysfs sampler..."
adb shell "su -c 'touch $DEVICE_STOP_FLAG'"
sleep 0.3
adb shell "su -c 'if [ -f $DEVICE_SAMPLER_PID ]; then kill \$(cat $DEVICE_SAMPLER_PID) 2>/dev/null || true; fi'"

echo "[host] Pull trace and sysfs logs..."
adb shell 'su -c "cat /sys/kernel/tracing/trace"' > "$RAW_TRACE"
adb shell "su -c 'cat $DEVICE_SYSFS_CSV 2>/dev/null || true'" > "$SYSFS_CSV"
adb shell "su -c 'cat $DEVICE_SAMPLER_LOG 2>/dev/null || true'" > "$SAMPLER_LOG"

echo "[host] Filter KGSL trace lines..."
grep -E "kgsl|adreno_cmdbatch|gpu_frequency" "$RAW_TRACE" > "$FILTERED_TRACE" || true

echo "[host] Analyze validation result..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZER="$SCRIPT_DIR/analyze_kgsl_busy_validation.py"

if [ -f "$ANALYZER" ]; then
  python3 "$ANALYZER" \
    --trace "$FILTERED_TRACE" \
    --sysfs "$SYSFS_CSV" \
    --out-dir "$OUT_DIR" \
    --workload-kind "$KIND" | tee "$SUMMARY_TXT"
else
  echo "[host] Analyzer not found: $ANALYZER"
  echo "[host] Skipping analysis."
fi

echo
echo "[host] Workload return code: $WORKLOAD_RC"
echo "[host] Files written:"
echo "  $RAW_TRACE"
echo "  $FILTERED_TRACE"
echo "  $SYSFS_CSV"
echo "  $SUMMARY_TXT"
echo

exit "$WORKLOAD_RC"
