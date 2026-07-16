#!/usr/bin/env python3

import argparse
import csv
import os
import re
import statistics
from pathlib import Path


SYSFS_COLS = [
    "gpu_busy_percentage",
    "gpu_load",
    "cur_freq",
    "target_freq",
    "gpuclk",
    "clock_mhz",
    "cur_ab",
    "busmon_cur_freq",
    "thermal_pwrlevel",
    "throttling",
]


TRACE_EVENT_PATTERNS = {
    "benchmark_trace_lines": r"vk_compute_prob|vk_mem_prob",
    "cmdbatch_queued": r"adreno_cmdbatch_queued",
    "cmdbatch_submitted": r"adreno_cmdbatch_submitted",
    "cmdbatch_retired": r"adreno_cmdbatch_retired",
    "kgsl_mem_alloc": r"kgsl_mem_alloc",
    "kgsl_mem_free": r"kgsl_mem_free",
    "kgsl_mem_mmap": r"kgsl_mem_mmap",
    "kgsl_buslevel": r"kgsl_buslevel",
    "kgsl_gmu_pwrlevel": r"kgsl_gmu_pwrlevel",
    "kgsl_pwr_request_state": r"kgsl_pwr_request_state",
    "gpu_work_period": r"gpu_work_period",
    "kgsl_pwrstats": r"kgsl_pwrstats",
    "kgsl_gpubusy": r"kgsl_gpubusy",
}


def first_number(text: str):
    if text is None:
        return None
    match = re.search(r"-?\d+(?:\.\d+)?", text)
    if not match:
        return None
    return float(match.group(0))


def safe_read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(errors="replace")


def count_lines_matching(text: str, pattern: str) -> int:
    regex = re.compile(pattern)
    return sum(1 for line in text.splitlines() if regex.search(line))


def parse_sysfs_fast(run_dir: Path) -> dict:
    path = run_dir / "raw" / "sysfs_fast_samples.csv"
    result = {
        "sysfs_fast_samples": 0,
    }

    if not path.exists():
        for col in SYSFS_COLS:
            result[f"{col}_avg"] = None
            result[f"{col}_min"] = None
            result[f"{col}_max"] = None
            result[f"{col}_n"] = 0
        return result

    with path.open(newline="", errors="replace") as f:
        rows = list(csv.DictReader(f))

    result["sysfs_fast_samples"] = len(rows)

    for col in SYSFS_COLS:
        vals = []
        for row in rows:
            val = first_number(row.get(col, ""))
            if val is not None:
                vals.append(val)

        if vals:
            result[f"{col}_avg"] = statistics.mean(vals)
            result[f"{col}_min"] = min(vals)
            result[f"{col}_max"] = max(vals)
            result[f"{col}_n"] = len(vals)
        else:
            result[f"{col}_avg"] = None
            result[f"{col}_min"] = None
            result[f"{col}_max"] = None
            result[f"{col}_n"] = 0

    return result


def parse_trace(run_dir: Path) -> dict:
    path = run_dir / "raw" / "trace_raw.log"
    text = safe_read_text(path)

    result = {
        "trace_size_bytes": path.stat().st_size if path.exists() else 0,
    }

    for name, pattern in TRACE_EVENT_PATTERNS.items():
        result[name] = count_lines_matching(text, pattern)

    bus_vals = []
    bw_vals = []

    for line in text.splitlines():
        if "kgsl_buslevel" not in line:
            continue
        match = re.search(r"bus=(\d+).*avg_bw=(\d+)", line)
        if match:
            bus_vals.append(int(match.group(1)))
            bw_vals.append(int(match.group(2)))

    result["buslevel_values_n"] = len(bus_vals)

    if bus_vals:
        result["buslevel_bus_avg"] = statistics.mean(bus_vals)
        result["buslevel_bus_min"] = min(bus_vals)
        result["buslevel_bus_max"] = max(bus_vals)
    else:
        result["buslevel_bus_avg"] = None
        result["buslevel_bus_min"] = None
        result["buslevel_bus_max"] = None

    if bw_vals:
        result["buslevel_avg_bw_avg"] = statistics.mean(bw_vals)
        result["buslevel_avg_bw_min"] = min(bw_vals)
        result["buslevel_avg_bw_max"] = max(bw_vals)
    else:
        result["buslevel_avg_bw_avg"] = None
        result["buslevel_avg_bw_min"] = None
        result["buslevel_avg_bw_max"] = None

    alloc_sizes = []

    for line in text.splitlines():
        if "kgsl_mem_alloc" not in line:
            continue
        match = re.search(r"size=(\d+)", line)
        if match:
            alloc_sizes.append(int(match.group(1)))

    result["kgsl_mem_alloc_size_n"] = len(alloc_sizes)
    result["kgsl_mem_alloc_total_bytes"] = sum(alloc_sizes)

    if alloc_sizes:
        result["kgsl_mem_alloc_avg_bytes"] = statistics.mean(alloc_sizes)
        result["kgsl_mem_alloc_max_bytes"] = max(alloc_sizes)
    else:
        result["kgsl_mem_alloc_avg_bytes"] = None
        result["kgsl_mem_alloc_max_bytes"] = None

    active_vals = []

    for line in text.splitlines():
        if "adreno_cmdbatch_retired" not in line:
            continue
        match = re.search(r"active=(\d+)", line)
        if match:
            active_vals.append(int(match.group(1)))

    result["cmdbatch_active_n"] = len(active_vals)

    if active_vals:
        result["cmdbatch_active_avg"] = statistics.mean(active_vals)
        result["cmdbatch_active_min"] = min(active_vals)
        result["cmdbatch_active_max"] = max(active_vals)
        result["cmdbatch_active_total"] = sum(active_vals)
    else:
        result["cmdbatch_active_avg"] = None
        result["cmdbatch_active_min"] = None
        result["cmdbatch_active_max"] = None
        result["cmdbatch_active_total"] = None

    gpu_work_vals = []

    for line in text.splitlines():
        if "gpu_work_period" not in line:
            continue
        match = re.search(r"total_active_duration_ns=(\d+)", line)
        if match:
            gpu_work_vals.append(int(match.group(1)))

    result["gpu_work_period_active_n"] = len(gpu_work_vals)

    if gpu_work_vals:
        result["gpu_work_period_active_avg_ns"] = statistics.mean(gpu_work_vals)
        result["gpu_work_period_active_min_ns"] = min(gpu_work_vals)
        result["gpu_work_period_active_max_ns"] = max(gpu_work_vals)
        result["gpu_work_period_active_total_ns"] = sum(gpu_work_vals)
    else:
        result["gpu_work_period_active_avg_ns"] = None
        result["gpu_work_period_active_min_ns"] = None
        result["gpu_work_period_active_max_ns"] = None
        result["gpu_work_period_active_total_ns"] = None

    pwr_total = []
    pwr_busy = []
    pwr_ram_time = []
    pwr_ram_wait = []
    pwr_busy_pct = []
    pwr_ram_wait_pct = []

    for line in text.splitlines():
        if "kgsl_pwrstats" not in line:
            continue

        m_total = re.search(r"total=(\d+)", line)
        m_busy = re.search(r"busy=(\d+)", line)
        m_ram_time = re.search(r"ram_time=(\d+)", line)
        m_ram_wait = re.search(r"ram_wait=(\d+)", line)

        if not (m_total and m_busy and m_ram_time and m_ram_wait):
            continue

        total = int(m_total.group(1))
        busy = int(m_busy.group(1))
        ram_time = int(m_ram_time.group(1))
        ram_wait = int(m_ram_wait.group(1))

        pwr_total.append(total)
        pwr_busy.append(busy)
        pwr_ram_time.append(ram_time)
        pwr_ram_wait.append(ram_wait)

        if total > 0:
            pwr_busy_pct.append(busy / total * 100.0)

        if ram_time > 0:
            pwr_ram_wait_pct.append(ram_wait / ram_time * 100.0)

    result["pwrstats_parsed_n"] = len(pwr_total)

    if pwr_total:
        result["pwrstats_total_sum"] = sum(pwr_total)
        result["pwrstats_busy_sum"] = sum(pwr_busy)
        result["pwrstats_ram_time_sum"] = sum(pwr_ram_time)
        result["pwrstats_ram_wait_sum"] = sum(pwr_ram_wait)

        result["pwrstats_busy_pct_avg"] = statistics.mean(pwr_busy_pct) if pwr_busy_pct else None
        result["pwrstats_busy_pct_max"] = max(pwr_busy_pct) if pwr_busy_pct else None

        result["pwrstats_ram_wait_pct_avg"] = statistics.mean(pwr_ram_wait_pct) if pwr_ram_wait_pct else None
        result["pwrstats_ram_wait_pct_max"] = max(pwr_ram_wait_pct) if pwr_ram_wait_pct else None

        result["pwrstats_overall_busy_pct"] = (
            sum(pwr_busy) / sum(pwr_total) * 100.0 if sum(pwr_total) > 0 else None
        )

        result["pwrstats_overall_ram_wait_pct"] = (
            sum(pwr_ram_wait) / sum(pwr_ram_time) * 100.0 if sum(pwr_ram_time) > 0 else None
        )
    else:
        result["pwrstats_total_sum"] = None
        result["pwrstats_busy_sum"] = None
        result["pwrstats_ram_time_sum"] = None
        result["pwrstats_ram_wait_sum"] = None
        result["pwrstats_busy_pct_avg"] = None
        result["pwrstats_busy_pct_max"] = None
        result["pwrstats_ram_wait_pct_avg"] = None
        result["pwrstats_ram_wait_pct_max"] = None
        result["pwrstats_overall_busy_pct"] = None
        result["pwrstats_overall_ram_wait_pct"] = None

    return result


def parse_workload(run_dir: Path) -> dict:
    stdout = safe_read_text(run_dir / "raw" / "workload_stdout.log")
    stderr = safe_read_text(run_dir / "raw" / "workload_stderr.log")

    return {
        "verification_passed_count": stdout.count("Verification PASSED"),
        "workload_stdout_size_bytes": len(stdout.encode("utf-8", errors="replace")),
        "workload_stderr_size_bytes": len(stderr.encode("utf-8", errors="replace")),
    }


def parse_tracepoints(run_dir: Path) -> dict:
    path = run_dir / "metadata" / "tracepoints_enabled_state.csv"

    if not path.exists():
        return {
            "tracepoints_enabled_count": None,
        }

    count = 0
    with path.open(errors="replace") as f:
        for line in f:
            if line.startswith("1,"):
                count += 1

    return {
        "tracepoints_enabled_count": count,
    }


def parse_benchmark_config(run_dir: Path) -> dict:
    path = run_dir / "metadata" / "benchmark_config.txt"
    result = {}

    if not path.exists():
        return result

    with path.open(errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            result[f"config_{key.strip()}"] = value.strip()

    return result


def parse_run(run_dir: Path) -> dict:
    result = {
        "run_dir": str(run_dir),
        "run_name": run_dir.name,
    }

    result.update(parse_benchmark_config(run_dir))
    result.update(parse_workload(run_dir))
    result.update(parse_tracepoints(run_dir))
    result.update(parse_sysfs_fast(run_dir))
    result.update(parse_trace(run_dir))

    return result


def fmt(value):
    if value is None:
        return "NA"
    if isinstance(value, float):
        return f"{value:.2f}"
    return str(value)


def write_run_summary(path: Path, data: dict):
    important_keys = [
        "run_name",
        "config_benchmark",
        "config_workload_type",
        "config_repeat_count",
        "verification_passed_count",
        "tracepoints_enabled_count",
        "sysfs_fast_samples",
        "trace_size_bytes",
        "benchmark_trace_lines",
        "cmdbatch_queued",
        "cmdbatch_submitted",
        "cmdbatch_retired",
        "kgsl_mem_alloc",
        "kgsl_mem_free",
        "kgsl_mem_mmap",
        "kgsl_mem_alloc_total_bytes",
        "kgsl_mem_alloc_max_bytes",
        "kgsl_buslevel",
        "buslevel_values_n",
        "buslevel_bus_avg",
        "buslevel_bus_max",
        "buslevel_avg_bw_avg",
        "buslevel_avg_bw_max",
        "pwrstats_parsed_n",
        "pwrstats_overall_busy_pct",
        "pwrstats_busy_pct_avg",
        "pwrstats_busy_pct_max",
        "pwrstats_overall_ram_wait_pct",
        "pwrstats_ram_wait_pct_avg",
        "pwrstats_ram_wait_pct_max",
        "pwrstats_ram_wait_sum",
        "pwrstats_ram_time_sum",
        "pwrstats_parsed_n",
        "pwrstats_overall_busy_pct",
        "pwrstats_busy_pct_avg",
        "pwrstats_busy_pct_max",
        "pwrstats_overall_ram_wait_pct",
        "pwrstats_ram_wait_pct_avg",
        "pwrstats_ram_wait_pct_max",
        "pwrstats_ram_wait_sum",
        "pwrstats_ram_time_sum",
        "gpu_busy_percentage_avg",
        "gpu_busy_percentage_max",
        "gpu_load_avg",
        "gpu_load_max",
        "cur_freq_avg",
        "cur_freq_max",
        "gpuclk_avg",
        "gpuclk_max",
        "clock_mhz_avg",
        "clock_mhz_max",
        "cur_ab_avg",
        "cur_ab_max",
        "thermal_pwrlevel_max",
        "throttling_max",
    ]

    with path.open("w") as f:
        f.write(f"Run summary: {data.get('run_name')}\n")
        f.write("=" * 80 + "\n\n")

        for key in important_keys:
            f.write(f"{key}: {fmt(data.get(key))}\n")


def pct_diff(a, b):
    if a is None or b is None:
        return None
    try:
        a = float(a)
        b = float(b)
    except Exception:
        return None
    if a == 0:
        return None
    return (b - a) / a * 100.0


def write_compare_summary(path: Path, a_label: str, a: dict, b_label: str, b: dict):
    keys = [
        "verification_passed_count",
        "tracepoints_enabled_count",
        "sysfs_fast_samples",
        "benchmark_trace_lines",
        "cmdbatch_queued",
        "cmdbatch_submitted",
        "cmdbatch_retired",
        "kgsl_mem_alloc",
        "kgsl_mem_free",
        "kgsl_mem_mmap",
        "kgsl_mem_alloc_total_bytes",
        "kgsl_buslevel",
        "buslevel_bus_avg",
        "buslevel_bus_max",
        "buslevel_avg_bw_avg",
        "buslevel_avg_bw_max",
        "pwrstats_parsed_n",
        "pwrstats_overall_busy_pct",
        "pwrstats_busy_pct_avg",
        "pwrstats_busy_pct_max",
        "pwrstats_overall_ram_wait_pct",
        "pwrstats_ram_wait_pct_avg",
        "pwrstats_ram_wait_pct_max",
        "pwrstats_ram_wait_sum",
        "pwrstats_ram_time_sum",
        "gpu_busy_percentage_avg",
        "gpu_busy_percentage_max",
        "gpu_load_avg",
        "gpu_load_max",
        "cur_freq_avg",
        "cur_freq_max",
        "gpuclk_avg",
        "gpuclk_max",
        "clock_mhz_avg",
        "clock_mhz_max",
        "cur_ab_avg",
        "cur_ab_max",
        "throttling_max",
    ]

    with path.open("w") as f:
        f.write("KGSL run comparison\n")
        f.write("=" * 80 + "\n\n")
        f.write(f"A: {a_label}\n")
        f.write(f"B: {b_label}\n\n")

        f.write(f"{'metric':40s} {'A':>16s} {'B':>16s} {'B_vs_A_%':>12s}\n")
        f.write("-" * 90 + "\n")

        for key in keys:
            diff = pct_diff(a.get(key), b.get(key))
            diff_s = "NA" if diff is None else f"{diff:.2f}"
            f.write(f"{key:40s} {fmt(a.get(key)):>16s} {fmt(b.get(key)):>16s} {diff_s:>12s}\n")

        f.write("\nInterpretation hints:\n")
        f.write("- Event counts usually describe Vulkan/KGSL structure.\n")
        f.write("- Value fields such as gpu_load, cur_ab, and buslevel avg_bw are more useful for workload characterization.\n")
        f.write("- Higher cur_ab and buslevel avg_bw usually suggest higher memory/bus pressure.\n")
        f.write("- Higher gpu_load or gpu_busy_percentage suggests higher governor-visible utilization.\n")


def write_compare_csv(path: Path, a_label: str, a: dict, b_label: str, b: dict):
    keys = sorted(set(a.keys()) | set(b.keys()))

    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["metric", a_label, b_label, "b_vs_a_percent"])

        for key in keys:
            diff = pct_diff(a.get(key), b.get(key))
            writer.writerow([
                key,
                fmt(a.get(key)),
                fmt(b.get(key)),
                "NA" if diff is None else f"{diff:.2f}",
            ])


def main():
    parser = argparse.ArgumentParser(description="Compare two KGSL/sysfs/tracefs capture runs.")
    parser.add_argument("--a", required=True, help="First run directory, usually compute.")
    parser.add_argument("--b", required=True, help="Second run directory, usually memory.")
    parser.add_argument("--a-label", default="A")
    parser.add_argument("--b-label", default="B")
    parser.add_argument("--out-dir", default=None)

    args = parser.parse_args()

    a_dir = Path(args.a).resolve()
    b_dir = Path(args.b).resolve()

    if not a_dir.exists():
        raise FileNotFoundError(f"Run A not found: {a_dir}")
    if not b_dir.exists():
        raise FileNotFoundError(f"Run B not found: {b_dir}")

    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
    else:
        common = Path(os.path.commonpath([str(a_dir), str(b_dir)]))
        out_dir = common / "parsed_compare"

    out_dir.mkdir(parents=True, exist_ok=True)

    a = parse_run(a_dir)
    b = parse_run(b_dir)

    write_run_summary(out_dir / f"profile_summary_{args.a_label}.txt", a)
    write_run_summary(out_dir / f"profile_summary_{args.b_label}.txt", b)
    write_compare_summary(out_dir / "compare_summary.txt", args.a_label, a, args.b_label, b)
    write_compare_csv(out_dir / "compare_summary.csv", args.a_label, a, args.b_label, b)

    print(f"[ok] Wrote outputs to: {out_dir}")
    print(f"[ok] {out_dir / 'compare_summary.txt'}")
    print(f"[ok] {out_dir / 'compare_summary.csv'}")


if __name__ == "__main__":
    main()
