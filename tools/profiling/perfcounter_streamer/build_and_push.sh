#!/usr/bin/env bash
set -euo pipefail

: "${NDK:=$HOME/android-ndk-r27d}"
: "${API:=35}"
: "${HOST_TAG:=darwin-x86_64}"

make NDK="$NDK" API="$API" HOST_TAG="$HOST_TAG" all
adb shell 'su -c "mkdir -p /data/local/tmp/jerry_work"'
adb push adreno_perf_stream /data/local/tmp/adreno_perf_stream
adb shell chmod 755 /data/local/tmp/adreno_perf_stream

echo "[host] pushed to /data/local/tmp/adreno_perf_stream"
echo "[host] example: adb shell su -c '/data/local/tmp/adreno_perf_stream -i 1 SP_BUSY_CYCLES SP_ALU_WORKING_CYCLES'"
