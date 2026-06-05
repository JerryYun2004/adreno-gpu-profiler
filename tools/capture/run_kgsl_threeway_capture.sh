#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
REPEAT="${2:-30}"
SAMPLE_INTERVAL="${3:-0.005}"

if [ "$MODE" != "copy" ] && [ "$MODE" != "alu" ] && [ "$MODE" != "mem" ]; then
  echo "Usage: $0 copy|alu|mem [repeat_count] [sample_interval_s]"
  exit 1
fi

RUN_ROOT=/Users/jerryyun/adreno_turnip/kgsl_full_capture
STAMP=$(date +%Y%m%d_%H%M%S)

N=262144
DISPATCH_REPEATS=64

if [ "$MODE" = "copy" ]; then
  RUN_NAME="${STAMP}_vendor_threeway_copy_repeat${REPEAT}_fast_full_clean"
  BENCH_CMD="./vk_threeway_probe copy copy_baseline.comp.spv $N 1 $DISPATCH_REPEATS"
  WORKLOAD_TYPE="copy_baseline"
  TRACE_PROC_PATTERN="vk_threeway_pro"
elif [ "$MODE" = "alu" ]; then
  RUN_NAME="${STAMP}_vendor_threeway_alu_repeat${REPEAT}_fast_full_clean"
  BENCH_CMD="./vk_threeway_probe alu alu_heavy.comp.spv $N 2048 $DISPATCH_REPEATS"
  WORKLOAD_TYPE="alu_heavy"
  TRACE_PROC_PATTERN="vk_threeway_pro"
else
  RUN_NAME="${STAMP}_vendor_threeway_mem_repeat${REPEAT}_fast_full_clean"
  BENCH_CMD="./vk_threeway_probe mem mem_heavy_clean.comp.spv $N 512 $DISPATCH_REPEATS"
  WORKLOAD_TYPE="mem_heavy_clean"
  TRACE_PROC_PATTERN="vk_threeway_pro"
fi

OUT_DIR="$RUN_ROOT/$RUN_NAME"

mkdir -p "$OUT_DIR"/{metadata,raw,parsed}

echo "[host] OUT_DIR=$OUT_DIR"
echo "[host] mode=$MODE"
echo "[host] workload_type=$WORKLOAD_TYPE"
echo "[host] repeat=$REPEAT"
echo "[host] sample_interval=$SAMPLE_INTERVAL"
echo "[host] command=$BENCH_CMD"

adb shell 'su -c "
TR=/sys/kernel/tracing
[ -d \$TR/events ] || TR=/sys/kernel/debug/tracing

echo 0 > \$TR/tracing_on
echo > \$TR/trace
echo 0 > \$TR/events/enable 2>/dev/null || true

if [ -d \$TR/events/kgsl ]; then
  for e in \$(find \$TR/events/kgsl -type f -name enable 2>/dev/null); do
    echo 1 > \$e 2>/dev/null || true
  done
fi

if [ -d \$TR/events/gpu_mem ]; then
  for e in \$(find \$TR/events/gpu_mem -type f -name enable 2>/dev/null); do
    echo 1 > \$e 2>/dev/null || true
  done
fi

if [ -f \$TR/events/power/gpu_work_period/enable ]; then
  echo 1 > \$TR/events/power/gpu_work_period/enable 2>/dev/null || true
fi

echo 1 > \$TR/tracing_on
"'

adb shell 'su -c "rm -f /data/local/tmp/kgsl_fast_sampler.stop /data/local/tmp/kgsl_fast_samples.csv"'

adb shell "su -c '/data/local/tmp/kgsl_fast_sampler.sh $SAMPLE_INTERVAL >/data/local/tmp/kgsl_fast_sampler_stdout.log 2>/data/local/tmp/kgsl_fast_sampler_stderr.log &'"

sleep 0.2

for i in $(seq 1 "$REPEAT"); do
  echo "[host] $MODE iteration $i"
  adb shell "cd /data/local/tmp/jerry_work && $BENCH_CMD" >> "$OUT_DIR/raw/workload_stdout.log" 2>> "$OUT_DIR/raw/workload_stderr.log"
done

adb shell 'su -c "touch /data/local/tmp/kgsl_fast_sampler.stop"'

sleep 1

adb shell 'su -c "
TR=/sys/kernel/tracing
[ -d \$TR/events ] || TR=/sys/kernel/debug/tracing

echo 0 > \$TR/tracing_on
cat \$TR/trace
"' > "$OUT_DIR/raw/trace_raw.log"

adb pull /data/local/tmp/kgsl_fast_samples.csv "$OUT_DIR/raw/sysfs_fast_samples.csv" >/dev/null

adb shell 'su -c "cat /data/local/tmp/kgsl_fast_sampler_stderr.log 2>/dev/null || true"' \
  > "$OUT_DIR/raw/kgsl_fast_sampler_stderr.log"

adb shell 'su -c "
TR=/sys/kernel/tracing
[ -d \$TR/events ] || TR=/sys/kernel/debug/tracing

OUT=/data/local/tmp/tracepoints_enabled_state.csv
rm -f \$OUT

for e in \$(find \$TR/events -maxdepth 3 -type f -name enable 2>/dev/null); do
  case \$e in
    *kgsl*|*gpu_mem*|*gpu_work_period*|*gpu_frequency*|*adreno*|*gpu*)
      v=\$(cat \$e 2>/dev/null)
      echo \"\$v,\$e\" >> \$OUT
      ;;
  esac
done
"'

adb pull /data/local/tmp/tracepoints_enabled_state.csv "$OUT_DIR/metadata/tracepoints_enabled_state.csv" >/dev/null

cat > "$OUT_DIR/metadata/benchmark_config.txt" <<EOF2
benchmark=vk_threeway_probe
mode=$MODE
workload_type=$WORKLOAD_TYPE
repeat_count=$REPEAT
n=$N
dispatch_repeats=$DISPATCH_REPEATS
sample_interval_s=$SAMPLE_INTERVAL
driver=vendor
command=$BENCH_CMD
notes=Three-way clean benchmark capture with fast KGSL/sysfs sampler and all KGSL/GPU tracepoints enabled.
EOF2

echo
echo "[host] Saved capture to:"
echo "$OUT_DIR"

echo
echo "[host] File sizes:"
ls -lh "$OUT_DIR/raw"

echo
echo "[host] Validation:"
echo "Verification PASSED count:"
grep -c "Verification PASSED" "$OUT_DIR/raw/workload_stdout.log" || true

echo "Fast sysfs sample rows:"
wc -l "$OUT_DIR/raw/sysfs_fast_samples.csv"

echo "$TRACE_PROC_PATTERN trace lines:"
grep -c "$TRACE_PROC_PATTERN" "$OUT_DIR/raw/trace_raw.log" || true

echo "Enabled tracepoint count:"
grep '^1,' "$OUT_DIR/metadata/tracepoints_enabled_state.csv" | wc -l

echo
echo "[host] OUT_DIR=$OUT_DIR"
