#!/usr/bin/env bash
set -euo pipefail

OP="${OP:-softmax}"
VARIANT="${VARIANT:-three_pass}"
WIDTHS="${WIDTHS:-128 256 512 1024 2048}"
TOTAL_ELEMENTS="${TOTAL_ELEMENTS:-65536}"
REPEATS="${REPEATS:-32}"
INTERVAL="${INTERVAL:-0.005}"

PHONE_DIR="${PHONE_DIR:-/data/local/tmp/jerry_work/ml_primitives}"
OUT_DIR="${OUT_DIR:-results/${OP}_${VARIANT}_width_sweep}"

mkdir -p "$OUT_DIR/raw_csv"
mkdir -p "$OUT_DIR/runtime_logs"

case "${OP}_${VARIANT}" in
  softmax_three_pass)
    SPV="spv/softmax_three_pass.spv"
    ;;
  softmax_online)
    SPV="spv/softmax_online.spv"
    ;;
  softmax_fused_lmem)
    SPV="spv/softmax_fused_lmem.spv"
    ;;
  rmsnorm_basic)
    SPV="spv/rmsnorm_basic.spv"
    ;;
  *)
    echo "Unsupported or currently disabled OP/VARIANT: OP=$OP VARIANT=$VARIANT"
    echo "Supported: softmax/three_pass, softmax/online, softmax/fused_lmem, rmsnorm/basic"
    exit 1
    ;;
esac

adb shell 'su -c "echo 1 > /sys/class/kgsl/kgsl-3d0/perfcounter"'

for W in $WIDTHS; do
  if [[ -n "${FIXED_ROWS:-}" ]]; then
    ROWS="${FIXED_ROWS}"
  else
    ROWS=$((TOTAL_ELEMENTS / W))
  fi

  NAME="${OP}_${VARIANT}_width_${W}"
  PHONE_CSV="/data/local/tmp/jerry_work/${NAME}_perf.csv"
  HOST_LOG="${OUT_DIR}/runtime_logs/${NAME}_runtime.txt"

  echo "===== ${NAME}: width=${W}, rows=${ROWS}, repeats=${REPEATS} ====="

  adb shell "su -c '
    rm -f ${PHONE_CSV}
    /data/local/tmp/adreno_perf_stream --csv -n -i ${INTERVAL} \
      SP_BUSY_CYCLES \
      SP_ALU_WORKING_CYCLES \
      UCHE_BUSY_CYCLES \
      UCHE_RAM_READ_REQ \
      UCHE_VBIF_READ_BEATS_SP \
      SP_UCHE_READ_TRANS \
      > ${PHONE_CSV}
  '" &

  PROF_PID=$!
  sleep 0.2

  adb shell "su -c '
    cd ${PHONE_DIR}
    ./ml_primitive_bench \
      --op ${OP} \
      --variant ${VARIANT} \
      --spv ${SPV} \
      --width ${W} \
      --rows ${ROWS} \
      --repeats ${REPEATS} \
      --csv
  '" | tee "${HOST_LOG}"

  sleep 0.2

  # Stop profiler robustly. Some adb shell + su sessions do not exit cleanly
  # after SIGINT, so escalate if needed and never block forever.
  adb shell 'su -c "pkill -INT adreno_perf_stream || true"' >/dev/null 2>&1 || true
  sleep 0.2
  adb shell 'su -c "pkill -TERM adreno_perf_stream || true"' >/dev/null 2>&1 || true
  sleep 0.2
  adb shell 'su -c "pkill -KILL adreno_perf_stream || true"' >/dev/null 2>&1 || true

  # Avoid hanging forever on the background adb process.
  for _ in 1 2 3 4 5; do
    if ! kill -0 "${PROF_PID}" 2>/dev/null; then
      break
    fi
    sleep 0.2
  done
  if kill -0 "${PROF_PID}" 2>/dev/null; then
    kill "${PROF_PID}" 2>/dev/null || true
  fi
  wait "${PROF_PID}" 2>/dev/null || true

  adb pull "${PHONE_CSV}" "${OUT_DIR}/raw_csv/${NAME}_perf.csv" >/dev/null || {
    echo "Warning: failed to pull ${PHONE_CSV}"
  }

  echo "[done] ${NAME}"
done

echo "All done. Results saved under:"
echo "  ${OUT_DIR}"
