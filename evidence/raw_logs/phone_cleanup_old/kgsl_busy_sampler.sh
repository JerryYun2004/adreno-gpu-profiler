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
