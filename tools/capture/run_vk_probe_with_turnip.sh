#!/usr/bin/env bash
set -euo pipefail

PHONE_WORK=/data/local/tmp/jerry_work
TURNIP_DIR=$PHONE_WORK/turnip
VENDOR_HAL=/vendor/lib64/hw/vulkan.adreno.so
TURNIP_HAL=$TURNIP_DIR/vulkan.adreno.so

echo "[host] Checking root..."
adb shell 'su -c id'

echo "[host] Checking Turnip files..."
adb shell "ls -lh $TURNIP_DIR"
adb shell "file $TURNIP_HAL"

echo "[host] Baseline before bind mount:"
adb shell "cd $PHONE_WORK && ./vk_probe 2>&1" \
  | grep -Ei "driverName|driverInfo|driverID" || true

echo "[host] Bind-mounting Turnip over vendor Vulkan HAL..."
adb shell "su -c 'mount --bind $TURNIP_HAL $VENDOR_HAL'"

cleanup() {
  echo "[host] Cleaning up bind mount..."
  adb shell "su -c 'umount $VENDOR_HAL'" || true
}
trap cleanup EXIT

echo "[host] Active mount:"
adb shell "su -c 'mount | grep vulkan.adreno.so'" || true

echo "[host] Running vk_probe with Turnip..."
adb logcat -c

adb shell "cd $PHONE_WORK; LD_LIBRARY_PATH=$TURNIP_DIR:\$LD_LIBRARY_PATH ./vk_probe 2>&1" \
  | tee ~/adreno_turnip/turnip_probe_latest.log

echo "[host] Key probe lines:"
grep -Ei "driverName|driverInfo|driverID|Mesa|Turnip|Freedreno|Qualcomm|error|failed" \
  ~/adreno_turnip/turnip_probe_latest.log || true

echo "[host] Relevant logcat:"
adb logcat -d | grep -Ei "vulkan|Driver Path|adreno|turnip|freedreno|mesa|kgsl|linker|dlopen|avc|denied|libc\+\+" \
  | tee ~/adreno_turnip/turnip_logcat_latest.txt || true

echo "[host] After cleanup, verifying vendor driver restored..."
# cleanup is also run by trap, but run now so verification is meaningful.
cleanup
trap - EXIT

adb shell "cd $PHONE_WORK && ./vk_probe 2>&1" \
  | grep -Ei "driverName|driverInfo|driverID" || true

echo "[host] Done."
