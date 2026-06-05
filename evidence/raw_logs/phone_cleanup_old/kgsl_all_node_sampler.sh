#!/system/bin/sh

OUT=/data/local/tmp/kgsl_all_node_samples.csv
STOP=/data/local/tmp/kgsl_all_node_sampler.stop
BASE=/sys/class/kgsl/kgsl-3d0
INTERVAL="${1:-0.02}"

rm -f "$OUT" "$STOP"

echo "timestamp_ns,path,value" > "$OUT"

NODES=$(
{
  find -L "$BASE" -maxdepth 2 -type f 2>/dev/null
  find -L "$BASE/devfreq" -maxdepth 2 -type f 2>/dev/null
  find -L "$BASE/power" -maxdepth 1 -type f 2>/dev/null
  find -L "$BASE/device/power" -maxdepth 1 -type f 2>/dev/null
  find -L "$BASE/device/devfreq" -maxdepth 3 -type f 2>/dev/null
  find -L "$BASE/device/kgsl-busmon" -maxdepth 4 -type f 2>/dev/null
  find -L "$BASE/device/of_node" -maxdepth 3 -type f 2>/dev/null
  find -L "$BASE/device/coresight-gfx" -maxdepth 2 -type f 2>/dev/null
  find -L "$BASE/device/coresight-gfx-cx" -maxdepth 2 -type f 2>/dev/null
} | sort -u
)

while [ ! -f "$STOP" ]; do
  TS=$(date +%s%N)

  for f in $NODES; do
    case "$f" in
      */driver/bind|*/driver/unbind|*/snapshot/force_panic|*/snapshot/minidump_test)
        continue
        ;;
    esac

    v=$(timeout 0.1 cat "$f" 2>/dev/null | tr '\n' ' ' | sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//')

    if [ -n "$v" ]; then
      v_escaped=$(echo "$v" | sed 's/"/""/g')
      echo "$TS,\"$f\",\"$v_escaped\"" >> "$OUT"
    fi
  done

  sleep "$INTERVAL"
done
