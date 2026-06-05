#!/usr/bin/env python3

import re
from collections import defaultdict
from pathlib import Path
import argparse


def avg(xs):
    return sum(xs) / len(xs) if xs else 0.0


def main():
    parser = argparse.ArgumentParser(
        description="Parse focused KGSL trace output and summarize GPU activity."
    )
    parser.add_argument(
        "--input",
        default=str(Path.home() / "adreno_turnip/kgsl_focused_trace_ui_filtered.log"),
        help="Path to filtered KGSL trace log",
    )
    args = parser.parse_args()

    path = Path(args.input)

    if not path.exists():
        raise FileNotFoundError(f"Input trace file not found: {path}")

    event_re = re.compile(r": ([a-zA-Z0-9_]+):")

    retired_re = re.compile(
        r"adreno_cmdbatch_retired: ctx=(\d+).*?ts=(\d+).*?"
        r"start=(\d+) retire=(\d+).*?"
        r"submitted_to_rb=(\d+) retired_on_gmu=(\d+) active=(\d+)"
    )

    submitted_re = re.compile(
        r"adreno_cmdbatch_submitted: ctx=(\d+).*?ts=(\d+).*?"
        r"inflight=(\d+).*?time=([0-9.]+)"
    )

    pwrstats_re = re.compile(
        r"kgsl_pwrstats:.*?total=(\d+) busy=(\d+) "
        r"ram_time=(\d+) ram_wait=(\d+) context_count=(\d+)"
    )

    gpubusy_re = re.compile(
        r"kgsl_gpubusy:.*?busy=(\d+) elapsed=(\d+)"
    )

    freq_re = re.compile(
        r"gpu_frequency: gpu_freq=(\d+)Khz"
    )

    pwrlevel_re = re.compile(
        r"kgsl_pwrlevel:.*?pwrlevel=(\d+) freq=(\d+) "
        r"prev_pwrlevel=(\d+) prev_freq=(\d+)"
    )

    buslevel_re = re.compile(
        r"kgsl_buslevel:.*?pwrlevel=(\d+) bus=(\d+) avg_bw=(\d+)"
    )

    mem_re = re.compile(
        r"kgsl_mem_(alloc|map|free):.*?size=(\d+).*?tgid=(\d+) "
        r"usage=(.*?)(?: id=| flags=)"
    )

    event_counts = defaultdict(int)

    retired = []
    submitted = []
    pwrstats = []
    gpubusy = []
    freqs = []
    pwrlevels = []
    buslevels = []

    mem_by_usage = defaultdict(int)
    mem_event_counts = defaultdict(int)
    mem_by_kind_usage = defaultdict(int)

    ctx_counts = defaultdict(int)

    for line in path.read_text(errors="ignore").splitlines():
        m = event_re.search(line)
        if m:
            event_counts[m.group(1)] += 1

        m = retired_re.search(line)
        if m:
            ctx, ts, start, retire, submitted_to_rb, retired_on_gmu, active = map(
                int, m.groups()
            )

            retired.append(
                {
                    "ctx": ctx,
                    "ts": ts,
                    "start": start,
                    "retire": retire,
                    "submitted_to_rb": submitted_to_rb,
                    "retired_on_gmu": retired_on_gmu,
                    "active": active,
                    "queue_to_start": start - submitted_to_rb,
                    "gmu_latency": retired_on_gmu - retire,
                }
            )

            ctx_counts[ctx] += 1

        m = submitted_re.search(line)
        if m:
            ctx, ts, inflight, time_s = m.groups()
            submitted.append((int(ctx), int(ts), int(inflight), float(time_s)))

        m = pwrstats_re.search(line)
        if m:
            total, busy, ram_time, ram_wait, context_count = map(int, m.groups())

            busy_pct = 100.0 * busy / total if total else 0.0
            ram_wait_pct = 100.0 * ram_wait / ram_time if ram_time else 0.0

            pwrstats.append(
                (
                    total,
                    busy,
                    busy_pct,
                    ram_time,
                    ram_wait,
                    ram_wait_pct,
                    context_count,
                )
            )

        m = gpubusy_re.search(line)
        if m:
            busy, elapsed = map(int, m.groups())
            gpubusy.append(100.0 * busy / elapsed if elapsed else 0.0)

        m = freq_re.search(line)
        if m:
            freqs.append(int(m.group(1)))

        m = pwrlevel_re.search(line)
        if m:
            pwrlevels.append(tuple(map(int, m.groups())))

        m = buslevel_re.search(line)
        if m:
            pwrlevel, bus, avg_bw = map(int, m.groups())
            buslevels.append((pwrlevel, bus, avg_bw))

        m = mem_re.search(line)
        if m:
            kind, size, tgid, usage = m.groups()
            size = int(size)
            usage = usage.strip()

            mem_by_usage[usage] += size
            mem_event_counts[(kind, usage)] += 1
            mem_by_kind_usage[(kind, usage)] += size

    print("=== Event counts ===")
    for k, v in sorted(event_counts.items(), key=lambda x: -x[1]):
        print(f"{k:30s} {v}")

    print("\n=== Command batch timing ===")
    print(f"submitted batches: {len(submitted)}")
    print(f"retired batches:   {len(retired)}")

    if retired:
        active = [r["active"] for r in retired]
        qstart = [r["queue_to_start"] for r in retired]
        gmu_lat = [r["gmu_latency"] for r in retired]

        print(f"avg active:         {avg(active):.1f} ticks")
        print(f"max active:         {max(active)} ticks")
        print(f"avg queue_to_start: {avg(qstart):.1f} ticks")
        print(f"max queue_to_start: {max(qstart)} ticks")
        print(f"avg gmu_latency:    {avg(gmu_lat):.1f} ticks")
        print(f"max gmu_latency:    {max(gmu_lat)} ticks")

        print("\nTop 10 longest active command batches:")
        for r in sorted(retired, key=lambda x: x["active"], reverse=True)[:10]:
            print(
                f"ctx={r['ctx']} ts={r['ts']} "
                f"active={r['active']} "
                f"queue_to_start={r['queue_to_start']} "
                f"gmu_latency={r['gmu_latency']}"
            )

    print("\n=== Per-context retired batch counts ===")
    for ctx, count in sorted(ctx_counts.items(), key=lambda x: -x[1]):
        print(f"ctx={ctx}: {count}")

    print("\n=== kgsl_pwrstats ===")
    if pwrstats:
        busy_pcts = [x[2] for x in pwrstats]
        ram_wait_pcts = [x[5] for x in pwrstats]
        context_counts = [x[6] for x in pwrstats]

        print(f"samples:            {len(pwrstats)}")
        print(f"avg busy_pct:       {avg(busy_pcts):.2f}%")
        print(f"max busy_pct:       {max(busy_pcts):.2f}%")
        print(f"avg ram_wait_pct:   {avg(ram_wait_pcts):.2f}%")
        print(f"max ram_wait_pct:   {max(ram_wait_pcts):.2f}%")
        print(f"avg context_count:  {avg(context_counts):.2f}")
        print(f"max context_count:  {max(context_counts)}")
    else:
        print("No kgsl_pwrstats samples found.")

    print("\n=== kgsl_gpubusy ===")
    if gpubusy:
        print(f"samples:            {len(gpubusy)}")
        print(f"avg busy_pct:       {avg(gpubusy):.2f}%")
        print(f"max busy_pct:       {max(gpubusy):.2f}%")
    else:
        print("No kgsl_gpubusy samples found.")

    print("\n=== Frequency / power ===")
    if freqs:
        print(f"freq samples:       {freqs}")
        print(f"min freq:           {min(freqs)} kHz")
        print(f"max freq:           {max(freqs)} kHz")
    else:
        print("No gpu_frequency samples found.")

    if pwrlevels:
        print(f"pwrlevel changes:   {len(pwrlevels)}")
    else:
        print("No kgsl_pwrlevel changes found.")

    if buslevels:
        avg_bw = [x[2] for x in buslevels]
        print(f"buslevel samples:   {len(buslevels)}")
        print(f"avg avg_bw:         {avg(avg_bw):.1f}")
        print(f"max avg_bw:         {max(avg_bw)}")
    else:
        print("No kgsl_buslevel samples found.")

    print("\n=== Memory by usage ===")
    if mem_by_usage:
        for usage, total_size in sorted(mem_by_usage.items(), key=lambda x: -x[1]):
            print(f"{usage:25s} {total_size / (1024 * 1024):10.2f} MiB")
    else:
        print("No memory events found.")

    print("\n=== Memory event counts ===")
    if mem_event_counts:
        for (kind, usage), count in sorted(
            mem_event_counts.items(), key=lambda x: (-x[1], x[0])
        ):
            print(f"{kind:5s} {usage:25s} {count}")
    else:
        print("No memory event counts found.")

    print("\n=== Memory bytes by event type and usage ===")
    if mem_by_kind_usage:
        for (kind, usage), total_size in sorted(
            mem_by_kind_usage.items(), key=lambda x: -x[1]
        ):
            print(f"{kind:5s} {usage:25s} {total_size / (1024 * 1024):10.2f} MiB")
    else:
        print("No memory byte totals found.")


if __name__ == "__main__":
    main()
