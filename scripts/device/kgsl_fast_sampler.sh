#!/system/bin/sh

OUT="${1:-/data/local/tmp/kgsl_fast_samples.csv}"
INTERVAL="${2:-0.05}"
DURATION="${3:-10}"

echo "timestamp_ms,gpu_load,gpu_busy_percentage,kernel_gpu_busy,clock_mhz,cur_freq,gpuclk,bus_split,gpubusy" > "$OUT"

START="$(date +%s)"

while true; do
    NOW="$(date +%s)"
    ELAPSED=$((NOW - START))

    if [ "$ELAPSED" -ge "$DURATION" ]; then
        break
    fi

    su -c '
        BASE="/sys/class/kgsl/kgsl-3d0"

        TS="$(date +%s%3N 2>/dev/null)"
        if [ -z "$TS" ]; then
            TS="$(date +%s)000"
        fi

        read_node() {
            V="$(cat "$1" 2>/dev/null | tr "\n" " " | sed "s/[[:space:]]\+/ /g; s/^ //; s/ $//")"
            if [ -z "$V" ]; then
                echo "NA"
            else
                echo "$V"
            fi
        }

        GPU_LOAD="$(read_node "$BASE/devfreq/gpu_load")"
        GPU_BUSY_PERCENTAGE="$(read_node "$BASE/gpu_busy_percentage")"
        KERNEL_GPU_BUSY="$(read_node "/sys/kernel/gpu/gpu_busy")"
        CLOCK_MHZ="$(read_node "$BASE/clock_mhz")"
        CUR_FREQ="$(read_node "$BASE/devfreq/cur_freq")"
        GPUCLK="$(read_node "$BASE/gpuclk")"
        BUS_SPLIT="$(read_node "$BASE/bus_split")"
        GPUBUSY="$(read_node "$BASE/gpubusy")"

        echo "$TS,$GPU_LOAD,$GPU_BUSY_PERCENTAGE,$KERNEL_GPU_BUSY,$CLOCK_MHZ,$CUR_FREQ,$GPUCLK,$BUS_SPLIT,\"$GPUBUSY\""
    ' 2>/dev/null >> "$OUT"

    sleep "$INTERVAL"
done
