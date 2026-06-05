#!/usr/bin/env python3

import argparse
import csv
import re
import subprocess
import sys
import time
from collections import deque

import matplotlib.pyplot as plt


def first_number(s):
    if s is None:
        return None
    m = re.search(r"-?\d+(?:\.\d+)?", str(s))
    if not m:
        return None
    return float(m.group(0))


def main():
    parser = argparse.ArgumentParser(description="Live KGSL/sysfs GPU profiler plotter.")
    parser.add_argument("--interval", default="0.02", help="On-device sample interval in seconds.")
    parser.add_argument("--window", type=float, default=10.0, help="Rolling plot window in seconds.")
    parser.add_argument("--out", default=None, help="Optional CSV output path.")
    args = parser.parse_args()

    cmd = [
        "adb",
        "exec-out",
        "su",
        "-c",
        f"/data/local/tmp/kgsl_live_sampler.sh {args.interval}",
    ]

    print("[host] Starting live sampler:")
    print("[host]", " ".join(cmd))
    print("[host] Press Ctrl+C to stop.")

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    header = proc.stdout.readline().strip()
    if not header:
        print("[error] No header received from sampler.")
        return 1

    fields = next(csv.reader([header]))

    csv_file = None
    writer = None

    if args.out:
        csv_file = open(args.out, "w", newline="")
        writer = csv.writer(csv_file)
        writer.writerow(fields)

    t0 = None
    ts = deque()
    gpu_load = deque()
    gpu_busy = deque()
    clock_mhz = deque()
    cur_ab = deque()

    plt.ion()
    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()

    line_load, = ax1.plot([], [], label="gpu_load")
    line_busy, = ax1.plot([], [], label="gpu_busy_percentage")
    line_freq, = ax2.plot([], [], label="clock_mhz")
    line_ab, = ax2.plot([], [], label="cur_ab")

    ax1.set_xlabel("Time (s)")
    ax1.set_ylabel("Busy / load (%)")
    ax2.set_ylabel("Frequency MHz / cur_ab")

    ax1.set_ylim(0, 105)
    ax2.set_ylim(0, 1200)

    fig.legend(loc="upper right")
    fig.canvas.manager.set_window_title("Live KGSL GPU profiler")

    try:
        for line in proc.stdout:
            line = line.strip()
            if not line:
                continue

            row_list = next(csv.reader([line]))
            if len(row_list) != len(fields):
                continue

            row = dict(zip(fields, row_list))

            if writer:
                writer.writerow(row_list)
                csv_file.flush()

            raw_ts = first_number(row.get("timestamp_ns"))
            if raw_ts is None:
                continue

            if t0 is None:
                t0 = raw_ts

            t = (raw_ts - t0) / 1e9

            v_load = first_number(row.get("gpu_load"))
            v_busy = first_number(row.get("gpu_busy_percentage"))
            v_clock = first_number(row.get("clock_mhz"))
            v_ab = first_number(row.get("cur_ab"))

            ts.append(t)
            gpu_load.append(v_load if v_load is not None else 0.0)
            gpu_busy.append(v_busy if v_busy is not None else 0.0)
            clock_mhz.append(v_clock if v_clock is not None else 0.0)
            cur_ab.append(v_ab if v_ab is not None else 0.0)

            while ts and (ts[-1] - ts[0]) > args.window:
                ts.popleft()
                gpu_load.popleft()
                gpu_busy.popleft()
                clock_mhz.popleft()
                cur_ab.popleft()

            line_load.set_data(ts, gpu_load)
            line_busy.set_data(ts, gpu_busy)
            line_freq.set_data(ts, clock_mhz)
            line_ab.set_data(ts, cur_ab)

            if ts:
                ax1.set_xlim(max(0, ts[-1] - args.window), max(args.window, ts[-1]))

            ax1.set_title(
                f"Live GPU signals | load={gpu_load[-1]:.0f}% busy={gpu_busy[-1]:.0f}% "
                f"clock={clock_mhz[-1]:.0f} MHz cur_ab={cur_ab[-1]:.0f}"
            )

            fig.canvas.draw()
            fig.canvas.flush_events()
            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n[host] Stopping live plotter.")

    finally:
        proc.terminate()
        if csv_file:
            csv_file.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
