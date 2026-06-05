#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="$HOME/adreno_turnip"
RAW="$OUT_DIR/kgsl_trace_adb_ui.log"
FILTERED="$OUT_DIR/kgsl_trace_adb_ui_filtered.log"

adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'
adb shell 'su -c "echo > /sys/kernel/tracing/trace"'

adb shell 'su -c "echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_gpubusy/enable"'
adb shell 'su -c "echo 1 > /sys/kernel/tracing/events/kgsl/gpu_frequency/enable"'
adb shell 'su -c "echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_pwrlevel/enable"'

adb shell 'su -c "echo 1 > /sys/kernel/tracing/tracing_on"'

adb shell 'input keyevent KEYCODE_WAKEUP'
adb shell 'input swipe 500 1600 500 400 300'
adb shell 'input swipe 500 400 500 1600 300'
adb shell 'input swipe 500 1600 500 400 300'
adb shell 'input swipe 500 400 500 1600 300'
sleep 3

adb shell 'su -c "echo 0 > /sys/kernel/tracing/tracing_on"'

adb shell 'su -c "cat /sys/kernel/tracing/trace"' | tee "$RAW"

grep -E "kgsl_gpubusy|gpu_frequency|kgsl_pwrlevel" "$RAW" > "$FILTERED"

python3 - <<'PY'
import re
from pathlib import Path

path = Path.home() / "adreno_turnip/kgsl_trace_adb_ui_filtered.log"

busy_re = re.compile(r"kgsl_gpubusy:.*busy=(\d+) elapsed=(\d+)")
freq_re = re.compile(r"gpu_frequency: gpu_freq=(\d+)Khz")

busy_samples = []
freqs = []

for line in path.read_text(errors="ignore").splitlines():
    m = busy_re.search(line)
    if m:
        busy = int(m.group(1))
        elapsed = int(m.group(2))
        pct = 100.0 * busy / elapsed if elapsed else 0.0
        busy_samples.append((busy, elapsed, pct))

    m = freq_re.search(line)
    if m:
        freqs.append(int(m.group(1)))

print("GPU busy samples:")
for busy, elapsed, pct in busy_samples:
    print(f"busy={busy} elapsed={elapsed} busy_pct={pct:.2f}%")

if busy_samples:
    avg = sum(p for _, _, p in busy_samples) / len(busy_samples)
    print(f"\nAverage busy over samples: {avg:.2f}%")

if freqs:
    print(f"\nFrequency samples: {freqs}")
    print(f"Min freq: {min(freqs)} kHz")
    print(f"Max freq: {max(freqs)} kHz")
PY
