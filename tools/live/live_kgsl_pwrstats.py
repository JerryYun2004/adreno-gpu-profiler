#!/usr/bin/env python3

import argparse
import csv
import re
import subprocess
import sys
import time
from pathlib import Path


PWR_RE = re.compile(
    r"kgsl_pwrstats:.*?"
    r"total=(?P<total>\d+).*?"
    r"busy=(?P<busy>\d+).*?"
    r"ram_time=(?P<ram_time>\d+).*?"
    r"ram_wait=(?P<ram_wait>\d+)"
)

GPUBUSY_RE = re.compile(
    r"kgsl_gpubusy:.*?busy=(?P<busy>\d+).*?elapsed=(?P<elapsed>\d+)"
)

BUSLEVEL_RE = re.compile(
    r"kgsl_buslevel:.*"
)


def run(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=check)


def adb_su(cmd: str, check: bool = True) -> subprocess.CompletedProcess:
    return run(["adb", "shell", "su", "-c", cmd], check=check)


def setup_tracing() -> None:
    # Use global tracing instance. You confirmed /sys/kernel/tracing/events/kgsl exists.
    cmds = [
        "echo 0 > /sys/kernel/tracing/tracing_on",
        "echo 0 > /sys/kernel/tracing/events/kgsl/enable",
        "echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_pwrstats/enable",
        "echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_gpubusy/enable",
        "echo 1 > /sys/kernel/tracing/events/kgsl/kgsl_buslevel/enable",
        "echo > /sys/kernel/tracing/trace",
        "echo 1 > /sys/kernel/tracing/tracing_on",
    ]
    adb_su(" ; ".join(cmds))


def cleanup_tracing() -> None:
    cmds = [
        "echo 0 > /sys/kernel/tracing/tracing_on",
        "echo 0 > /sys/kernel/tracing/events/kgsl/kgsl_pwrstats/enable",
        "echo 0 > /sys/kernel/tracing/events/kgsl/kgsl_gpubusy/enable",
        "echo 0 > /sys/kernel/tracing/events/kgsl/kgsl_buslevel/enable",
    ]
    adb_su(" ; ".join(cmds), check=False)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="Output CSV path on host")
    ap.add_argument("--duration", type=float, default=15.0)
    args = ap.parse_args()

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    setup_tracing()

    proc = subprocess.Popen(
        ["adb", "exec-out", "su", "-c", "cat /sys/kernel/tracing/trace_pipe"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=1,
    )

    start = time.time()

    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "host_time_s",
                "event",
                "busy_pct",
                "ram_wait_pct",
                "total",
                "busy",
                "ram_time",
                "ram_wait",
                "gpubusy_busy",
                "gpubusy_elapsed",
                "raw",
            ],
        )
        writer.writeheader()

        try:
            while time.time() - start < args.duration:
                line = proc.stdout.readline()
                if not line:
                    continue

                now = time.time()
                line = line.strip()

                m = PWR_RE.search(line)
                if m:
                    total = int(m.group("total"))
                    busy = int(m.group("busy"))
                    ram_time = int(m.group("ram_time"))
                    ram_wait = int(m.group("ram_wait"))

                    busy_pct = 100.0 * busy / total if total else 0.0
                    ram_wait_pct = 100.0 * ram_wait / ram_time if ram_time else 0.0

                    writer.writerow({
                        "host_time_s": f"{now:.6f}",
                        "event": "kgsl_pwrstats",
                        "busy_pct": f"{busy_pct:.3f}",
                        "ram_wait_pct": f"{ram_wait_pct:.3f}",
                        "total": total,
                        "busy": busy,
                        "ram_time": ram_time,
                        "ram_wait": ram_wait,
                        "gpubusy_busy": "",
                        "gpubusy_elapsed": "",
                        "raw": line,
                    })
                    f.flush()
                    print(f"pwrstats busy={busy_pct:6.2f}% ram_wait={ram_wait_pct:6.2f}%")

                    continue

                m = GPUBUSY_RE.search(line)
                if m:
                    b = int(m.group("busy"))
                    e = int(m.group("elapsed"))
                    writer.writerow({
                        "host_time_s": f"{now:.6f}",
                        "event": "kgsl_gpubusy",
                        "busy_pct": "",
                        "ram_wait_pct": "",
                        "total": "",
                        "busy": "",
                        "ram_time": "",
                        "ram_wait": "",
                        "gpubusy_busy": b,
                        "gpubusy_elapsed": e,
                        "raw": line,
                    })
                    f.flush()
                    continue

                if "kgsl_buslevel:" in line:
                    writer.writerow({
                        "host_time_s": f"{now:.6f}",
                        "event": "kgsl_buslevel",
                        "busy_pct": "",
                        "ram_wait_pct": "",
                        "total": "",
                        "busy": "",
                        "ram_time": "",
                        "ram_wait": "",
                        "gpubusy_busy": "",
                        "gpubusy_elapsed": "",
                        "raw": line,
                    })
                    f.flush()

        except KeyboardInterrupt:
            pass
        finally:
            proc.terminate()
            cleanup_tracing()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
