#!/usr/bin/env bash
set -euo pipefail

: "${NDK:=$HOME/android-ndk-r27d}"
: "${API:=35}"
: "${HOST_TAG:=darwin-x86_64}"

make NDK="$NDK" API="$API" HOST_TAG="$HOST_TAG" all
adb shell 'su -c "mkdir -p /data/local/tmp/jerry_work/perfcounter_sweeps"'
adb push streamer_sweeper /data/local/tmp/streamer_sweeper
adb shell chmod 755 /data/local/tmp/streamer_sweeper

echo "[host] pushed to /data/local/tmp/streamer_sweeper"
echo "[host] example: adb shell su -c '/data/local/tmp/streamer_sweeper --time 2 --bench-args \"--width 1024 --rows 2048 --repeats 512 --csv\"'"
echo "[host] burst example: adb shell su -c '/data/local/tmp/streamer_sweeper --time 3 --bursts 10 --burst-sleep 0.1 --bench-args \"--width 1024 --rows 256 --repeats 32 --csv\"'"
echo "[host] width sequence example: adb shell su -c '/data/local/tmp/streamer_sweeper --time 4 --widths 128,256,512,1024,2048 --width-sleep 0.1 --bench-args \"--rows 128 --repeats 16 --csv\"'"
