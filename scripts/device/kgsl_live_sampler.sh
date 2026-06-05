#!/system/bin/sh

INTERVAL="${1:-0.05}"

echo "timestamp_ms,gpu_load,gpu_busy_percentage,kernel_gpu_busy,clock_mhz,cur_freq,gpuclk,bus_split,gpubusy"

while true; do
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
    ' 2>/dev/null

    sleep "$INTERVAL"
done
