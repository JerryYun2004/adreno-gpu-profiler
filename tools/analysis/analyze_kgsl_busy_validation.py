#!/usr/bin/env python3

import argparse
import csv
import math
import re
from pathlib import Path
from statistics import mean


def safe_float(x):
    if x is None:
        return None
    x = str(x).strip()
    if x == "":
        return None
    try:
        return float(x)
    except ValueError:
        return None


def avg(xs):
    xs = [x for x in xs if x is not None and not math.isnan(x)]
    return mean(xs) if xs else None


def fmt(x, digits=2):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "NA"
    return f"{x:.{digits}f}"


def pearson(xs, ys):
    pairs = [
        (x, y)
        for x, y in zip(xs, ys)
        if x is not None and y is not None and not math.isnan(x) and not math.isnan(y)
    ]
    if len(pairs) < 2:
        return None

    xs = [p[0] for p in pairs]
    ys = [p[1] for p in pairs]

    mx = mean(xs)
    my = mean(ys)

    vx = sum((x - mx) ** 2 for x in xs)
    vy = sum((y - my) ** 2 for y in ys)

    if vx == 0 or vy == 0:
        return None

    cov = sum((x - mx) * (y - my) for x, y in pairs)
    return cov / math.sqrt(vx * vy)


def nearest_sample(samples, t, max_dt):
    """
    samples: list of dicts with key 't_s'
    Return nearest sample within max_dt seconds.
    """
    best = None
    best_dt = None

    for s in samples:
        dt = abs(s["t_s"] - t)
        if best is None or dt < best_dt:
            best = s
            best_dt = dt

    if best is None or best_dt is None or best_dt > max_dt:
        return None

    return best


def parse_trace(path):
    """
    Parse KGSL filtered ftrace log.

    Important tracepoint busy definitions:
      kgsl_pwrstats busy_pct = 100 * busy / total
      kgsl_gpubusy  busy_pct = 100 * busy / elapsed
    """

    ts_event_re = re.compile(r"\s([0-9]+\.[0-9]+):\s+([a-zA-Z0-9_]+):")

    context_re = re.compile(
        r"kgsl_context_create:.*?ctx=(\d+).*?type=([A-Za-z0-9_]+)"
    )

    submitted_re = re.compile(
        r"adreno_cmdbatch_submitted: ctx=(\d+).*?ts=(\d+)"
    )

    retired_re = re.compile(
        r"adreno_cmdbatch_retired: ctx=(\d+).*?ts=(\d+).*?"
        r"start=(\d+) retire=(\d+).*?"
        r"submitted_to_rb=(\d+) retired_on_gmu=(\d+) active=(\d+)"
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

    target_contexts = set()
    submitted = []
    retired = []
    pwrstats = []
    gpubusy = []
    freqs = []

    lines = path.read_text(errors="replace").splitlines()

    for line in lines:
        tm = ts_event_re.search(line)
        if not tm:
            continue

        t_s = float(tm.group(1))
        event = tm.group(2)

        # In your workloads, context creation lines from vk_compute_probe/vk_mem_probe
        # identify the main workload context. UI contexts such as RenderThread can also
        # appear in the trace, so we try to isolate vk_* contexts.
        if "vk_compute_prob" in line or "vk_mem_probe" in line:
            cm = context_re.search(line)
            if cm:
                target_contexts.add(int(cm.group(1)))

        if event == "adreno_cmdbatch_submitted":
            m = submitted_re.search(line)
            if m:
                submitted.append(
                    {
                        "t_s": t_s,
                        "ctx": int(m.group(1)),
                        "ts": int(m.group(2)),
                    }
                )

        elif event == "adreno_cmdbatch_retired":
            m = retired_re.search(line)
            if m:
                ctx, ts, start, retire, submitted_to_rb, retired_on_gmu, active = map(
                    int, m.groups()
                )
                retired.append(
                    {
                        "t_s": t_s,
                        "ctx": ctx,
                        "ts": ts,
                        "start": start,
                        "retire": retire,
                        "submitted_to_rb": submitted_to_rb,
                        "retired_on_gmu": retired_on_gmu,
                        "active": active,
                    }
                )

        elif event == "kgsl_pwrstats":
            m = pwrstats_re.search(line)
            if m:
                total, busy, ram_time, ram_wait, context_count = map(int, m.groups())
                busy_pct = 100.0 * busy / total if total else None
                ram_wait_pct = 100.0 * ram_wait / ram_time if ram_time else None
                pwrstats.append(
                    {
                        "t_s": t_s,
                        "total": total,
                        "busy": busy,
                        "busy_pct": busy_pct,
                        "ram_time": ram_time,
                        "ram_wait": ram_wait,
                        "ram_wait_pct": ram_wait_pct,
                        "context_count": context_count,
                    }
                )

        elif event == "kgsl_gpubusy":
            m = gpubusy_re.search(line)
            if m:
                busy, elapsed = map(int, m.groups())
                busy_pct = 100.0 * busy / elapsed if elapsed else None
                gpubusy.append(
                    {
                        "t_s": t_s,
                        "busy": busy,
                        "elapsed": elapsed,
                        "busy_pct": busy_pct,
                    }
                )

        elif event == "gpu_frequency":
            m = freq_re.search(line)
            if m:
                freqs.append({"t_s": t_s, "freq_khz": int(m.group(1))})

    return {
        "target_contexts": target_contexts,
        "submitted": submitted,
        "retired": retired,
        "pwrstats": pwrstats,
        "gpubusy": gpubusy,
        "freqs": freqs,
    }


def parse_sysfs_csv(path):
    samples = []

    with path.open(newline="", errors="replace") as f:
        reader = csv.DictReader(f)
        for row in reader:
            t_s = safe_float(row.get("t_s"))
            if t_s is None:
                continue

            samples.append(
                {
                    "t_s": t_s,
                    "gpu_busy_percentage": safe_float(row.get("gpu_busy_percentage")),
                    "gpubusy_busy": safe_float(row.get("gpubusy_busy")),
                    "gpubusy_elapsed": safe_float(row.get("gpubusy_elapsed")),
                    "gpubusy_pct": safe_float(row.get("gpubusy_pct")),
                    "devfreq_gpu_load": safe_float(row.get("devfreq_gpu_load")),
                    "cur_freq_hz": safe_float(row.get("cur_freq_hz")),
                    "gpuclk_hz": safe_float(row.get("gpuclk_hz")),
                }
            )

    return samples


def infer_workload_window(trace_data, pad_s=0.02):
    """
    Prefer window from target vk_* context.
    Fallback to all command batches.
    Fallback to pwrstats range.
    """

    target_contexts = trace_data["target_contexts"]

    submitted = trace_data["submitted"]
    retired = trace_data["retired"]

    if target_contexts:
        sub_target = [x for x in submitted if x["ctx"] in target_contexts]
        ret_target = [x for x in retired if x["ctx"] in target_contexts]
    else:
        sub_target = []
        ret_target = []

    if sub_target and ret_target:
        start = min(x["t_s"] for x in sub_target)
        end = max(x["t_s"] for x in ret_target)
        return start - pad_s, end + pad_s, "target vk_* context"

    if submitted and retired:
        start = min(x["t_s"] for x in submitted)
        end = max(x["t_s"] for x in retired)
        return start - pad_s, end + pad_s, "all command batches"

    pwrstats = trace_data["pwrstats"]
    if pwrstats:
        start = min(x["t_s"] for x in pwrstats)
        end = max(x["t_s"] for x in pwrstats)
        return start, end, "pwrstats range"

    return None, None, "unknown"


def filter_window(samples, start, end):
    if start is None or end is None:
        return samples
    return [s for s in samples if start <= s["t_s"] <= end]


def write_aligned_csv(out_path, pwrstats, sysfs_samples, max_dt_s=0.15):
    rows = []

    for tr in pwrstats:
        nearest = nearest_sample(sysfs_samples, tr["t_s"], max_dt=max_dt_s)
        if nearest is None:
            rows.append(
                {
                    "trace_t_s": tr["t_s"],
                    "trace_pwrstats_busy_pct": tr["busy_pct"],
                    "trace_ram_wait_pct": tr["ram_wait_pct"],
                    "sysfs_t_s": "",
                    "dt_s": "",
                    "sysfs_gpu_busy_percentage": "",
                    "sysfs_gpubusy_pct": "",
                    "sysfs_devfreq_gpu_load": "",
                    "sysfs_cur_freq_hz": "",
                }
            )
        else:
            rows.append(
                {
                    "trace_t_s": tr["t_s"],
                    "trace_pwrstats_busy_pct": tr["busy_pct"],
                    "trace_ram_wait_pct": tr["ram_wait_pct"],
                    "sysfs_t_s": nearest["t_s"],
                    "dt_s": nearest["t_s"] - tr["t_s"],
                    "sysfs_gpu_busy_percentage": nearest["gpu_busy_percentage"],
                    "sysfs_gpubusy_pct": nearest["gpubusy_pct"],
                    "sysfs_devfreq_gpu_load": nearest["devfreq_gpu_load"],
                    "sysfs_cur_freq_hz": nearest["cur_freq_hz"],
                }
            )

    fieldnames = [
        "trace_t_s",
        "trace_pwrstats_busy_pct",
        "trace_ram_wait_pct",
        "sysfs_t_s",
        "dt_s",
        "sysfs_gpu_busy_percentage",
        "sysfs_gpubusy_pct",
        "sysfs_devfreq_gpu_load",
        "sysfs_cur_freq_hz",
    ]

    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", required=True, help="Filtered KGSL trace log")
    ap.add_argument("--sysfs", required=True, help="Sysfs busy CSV from sampler")
    ap.add_argument("--out-dir", required=True, help="Output directory")
    ap.add_argument("--workload-kind", default="unknown", help="compute or mem")
    ap.add_argument("--align-max-dt-s", type=float, default=0.15)
    args = ap.parse_args()

    trace_path = Path(args.trace)
    sysfs_path = Path(args.sysfs)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    trace_data = parse_trace(trace_path)
    sysfs_samples = parse_sysfs_csv(sysfs_path)

    start, end, window_source = infer_workload_window(trace_data)

    pwrstats_win = filter_window(trace_data["pwrstats"], start, end)
    gpubusy_win = filter_window(trace_data["gpubusy"], start, end)
    sysfs_win = filter_window(sysfs_samples, start, end)

    aligned_path = out_dir / "aligned_trace_vs_sysfs.csv"
    aligned_rows = write_aligned_csv(
        aligned_path,
        pwrstats_win,
        sysfs_win,
        max_dt_s=args.align_max_dt_s,
    )

    trace_pwr_busy = [x["busy_pct"] for x in pwrstats_win]
    trace_gpubusy = [x["busy_pct"] for x in gpubusy_win]

    sysfs_gpu_busy_percentage = [x["gpu_busy_percentage"] for x in sysfs_win]
    sysfs_gpubusy_pct = [x["gpubusy_pct"] for x in sysfs_win]
    sysfs_devfreq_gpu_load = [x["devfreq_gpu_load"] for x in sysfs_win]

    aligned_trace = []
    aligned_sysfs_busy_pct = []
    aligned_sysfs_gpu_load = []

    for row in aligned_rows:
        aligned_trace.append(safe_float(row["trace_pwrstats_busy_pct"]))
        aligned_sysfs_busy_pct.append(safe_float(row["sysfs_gpu_busy_percentage"]))
        aligned_sysfs_gpu_load.append(safe_float(row["sysfs_devfreq_gpu_load"]))

    avg_trace_pwr = avg(trace_pwr_busy)
    avg_trace_gpubusy = avg(trace_gpubusy)
    avg_sysfs_gpu_busy_percentage = avg(sysfs_gpu_busy_percentage)
    avg_sysfs_gpubusy_pct = avg(sysfs_gpubusy_pct)
    avg_sysfs_gpu_load = avg(sysfs_devfreq_gpu_load)

    corr_busy = pearson(aligned_trace, aligned_sysfs_busy_pct)
    corr_load = pearson(aligned_trace, aligned_sysfs_gpu_load)

    abs_err = None
    rel_err = None
    if avg_trace_pwr is not None and avg_sysfs_gpu_busy_percentage is not None:
        abs_err = abs(avg_sysfs_gpu_busy_percentage - avg_trace_pwr)
        rel_err = 100.0 * abs_err / avg_trace_pwr if avg_trace_pwr else None

    print("=== KGSL busy validation ===")
    print(f"workload kind:                  {args.workload_kind}")
    print(f"trace file:                     {trace_path}")
    print(f"sysfs csv:                      {sysfs_path}")
    print()
    print("=== Workload window ===")
    print(f"window source:                  {window_source}")
    print(f"start_s:                        {fmt(start, 6)}")
    print(f"end_s:                          {fmt(end, 6)}")
    print(f"duration_s:                     {fmt(end - start if start is not None and end is not None else None, 6)}")
    print(f"target contexts:                {sorted(trace_data['target_contexts'])}")
    print()
    print("=== Sample counts inside window ===")
    print(f"trace kgsl_pwrstats samples:    {len(pwrstats_win)}")
    print(f"trace kgsl_gpubusy samples:     {len(gpubusy_win)}")
    print(f"sysfs samples:                  {len(sysfs_win)}")
    print(f"aligned pairs:                  {sum(1 for x in aligned_sysfs_busy_pct if x is not None)}")
    print()
    print("=== Average busy comparison ===")
    print(f"trace pwrstats avg busy%:       {fmt(avg_trace_pwr)}")
    print(f"trace gpubusy avg busy%:        {fmt(avg_trace_gpubusy)}")
    print(f"sysfs gpu_busy_percentage avg:  {fmt(avg_sysfs_gpu_busy_percentage)}")
    print(f"sysfs gpubusy avg busy%:        {fmt(avg_sysfs_gpubusy_pct)}")
    print(f"sysfs devfreq gpu_load avg:     {fmt(avg_sysfs_gpu_load)}")
    print()
    print("=== Error and correlation ===")
    print(f"abs error, sysfs vs pwrstats:   {fmt(abs_err)} percentage points")
    print(f"rel error, sysfs vs pwrstats:   {fmt(rel_err)}%")
    print(f"corr, pwrstats vs sysfs busy:   {fmt(corr_busy, 4)}")
    print(f"corr, pwrstats vs gpu_load:     {fmt(corr_load, 4)}")
    print()
    print("=== Output files ===")
    print(f"aligned csv:                    {aligned_path}")

    print()
    print("=== Interpretation hint ===")
    if corr_busy is not None and corr_busy >= 0.7:
        print("The time-series trend is strongly directionally consistent.")
    elif corr_busy is not None and corr_busy >= 0.4:
        print("The time-series trend is moderately consistent, but sampling-window mismatch may be visible.")
    elif corr_busy is not None:
        print("The time-series correlation is weak. Check sampling interval, workload duration, and whether sysfs updates slowly.")
    else:
        print("Correlation was not computed. Usually this means there were too few aligned samples or one source was nearly constant.")

    if abs_err is not None:
        if abs_err <= 10:
            print("The average busy values are reasonably close for a coarse validation.")
        else:
            print("The average busy values differ noticeably. This may still be explainable if the sysfs node and tracepoint use different averaging windows.")


if __name__ == "__main__":
    main()
