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
    "benchmark_trace_lines": r"vk_compute_prob|vk_mem_prob|vk_threeway_pro",
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


def first_number(text):
    if text is None:
        return None
    m = re.search(r"-?\d+(?:\.\d+)?", str(text))
    if not m:
        return None
    return float(m.group(0))


def safe_read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(errors="replace")


def count_lines_matching(text: str, pattern: str) -> int:
    rx = re.compile(pattern)
    return sum(1 for line in text.splitlines() if rx.search(line))


def mean_or_none(vals):
    return statistics.mean(vals) if vals else None


def parse_config(run_dir: Path) -> dict:
    path = run_dir / "metadata" / "benchmark_config.txt"
    out = {}

    if not path.exists():
        return out

    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[f"config_{k.strip()}"] = v.strip()

    return out


def parse_tracepoints(run_dir: Path) -> dict:
    path = run_dir / "metadata" / "tracepoints_enabled_state.csv"

    if not path.exists():
        return {"tracepoints_enabled_count": None}

    count = 0
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("1,"):
            count += 1

    return {"tracepoints_enabled_count": count}


def parse_workload(run_dir: Path) -> dict:
    stdout = safe_read_text(run_dir / "raw" / "workload_stdout.log")
    stderr = safe_read_text(run_dir / "raw" / "workload_stderr.log")

    return {
        "verification_passed_count": stdout.count("Verification PASSED"),
        "workload_stdout_size_bytes": len(stdout.encode("utf-8", errors="replace")),
        "workload_stderr_size_bytes": len(stderr.encode("utf-8", errors="replace")),
    }


def parse_sysfs(run_dir: Path) -> dict:
    path = run_dir / "raw" / "sysfs_fast_samples.csv"

    out = {"sysfs_fast_samples": 0}

    if not path.exists():
        for c in SYSFS_COLS:
            out[f"{c}_avg"] = None
            out[f"{c}_min"] = None
            out[f"{c}_max"] = None
            out[f"{c}_n"] = 0
        return out

    with path.open(newline="", errors="replace") as f:
        rows = list(csv.DictReader(f))

    out["sysfs_fast_samples"] = len(rows)

    for c in SYSFS_COLS:
        vals = []
        for r in rows:
            v = first_number(r.get(c, ""))
            if v is not None:
                vals.append(v)

        out[f"{c}_avg"] = mean_or_none(vals)
        out[f"{c}_min"] = min(vals) if vals else None
        out[f"{c}_max"] = max(vals) if vals else None
        out[f"{c}_n"] = len(vals)

    return out


def parse_trace(run_dir: Path) -> dict:
    path = run_dir / "raw" / "trace_raw.log"
    text = safe_read_text(path)

    out = {
        "trace_size_bytes": path.stat().st_size if path.exists() else 0,
    }

    for name, pattern in TRACE_EVENT_PATTERNS.items():
        out[name] = count_lines_matching(text, pattern)

    bus_vals = []
    bw_vals = []

    alloc_sizes = []
    cmdbatch_active = []
    gpu_work_active_ns = []

    pwr_total = []
    pwr_busy = []
    pwr_ram_time = []
    pwr_ram_wait = []
    pwr_busy_pct = []
    pwr_ram_wait_pct = []

    for line in text.splitlines():
        if "kgsl_buslevel" in line:
            m = re.search(r"bus=(\d+).*avg_bw=(\d+)", line)
            if m:
                bus_vals.append(int(m.group(1)))
                bw_vals.append(int(m.group(2)))

        if "kgsl_mem_alloc" in line:
            m = re.search(r"size=(\d+)", line)
            if m:
                alloc_sizes.append(int(m.group(1)))

        if "adreno_cmdbatch_retired" in line:
            m = re.search(r"active=(\d+)", line)
            if m:
                cmdbatch_active.append(int(m.group(1)))

        if "gpu_work_period" in line:
            m = re.search(r"total_active_duration_ns=(\d+)", line)
            if m:
                gpu_work_active_ns.append(int(m.group(1)))

        if "kgsl_pwrstats" in line:
            m_total = re.search(r"total=(\d+)", line)
            m_busy = re.search(r"busy=(\d+)", line)
            m_ram_time = re.search(r"ram_time=(\d+)", line)
            m_ram_wait = re.search(r"ram_wait=(\d+)", line)

            if m_total and m_busy and m_ram_time and m_ram_wait:
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

    out["buslevel_values_n"] = len(bus_vals)
    out["buslevel_bus_avg"] = mean_or_none(bus_vals)
    out["buslevel_bus_min"] = min(bus_vals) if bus_vals else None
    out["buslevel_bus_max"] = max(bus_vals) if bus_vals else None
    out["buslevel_avg_bw_avg"] = mean_or_none(bw_vals)
    out["buslevel_avg_bw_min"] = min(bw_vals) if bw_vals else None
    out["buslevel_avg_bw_max"] = max(bw_vals) if bw_vals else None

    out["kgsl_mem_alloc_size_n"] = len(alloc_sizes)
    out["kgsl_mem_alloc_total_bytes"] = sum(alloc_sizes)
    out["kgsl_mem_alloc_avg_bytes"] = mean_or_none(alloc_sizes)
    out["kgsl_mem_alloc_max_bytes"] = max(alloc_sizes) if alloc_sizes else None

    out["cmdbatch_active_n"] = len(cmdbatch_active)
    out["cmdbatch_active_avg"] = mean_or_none(cmdbatch_active)
    out["cmdbatch_active_min"] = min(cmdbatch_active) if cmdbatch_active else None
    out["cmdbatch_active_max"] = max(cmdbatch_active) if cmdbatch_active else None
    out["cmdbatch_active_total"] = sum(cmdbatch_active) if cmdbatch_active else None

    out["gpu_work_period_active_n"] = len(gpu_work_active_ns)
    out["gpu_work_period_active_avg_ns"] = mean_or_none(gpu_work_active_ns)
    out["gpu_work_period_active_min_ns"] = min(gpu_work_active_ns) if gpu_work_active_ns else None
    out["gpu_work_period_active_max_ns"] = max(gpu_work_active_ns) if gpu_work_active_ns else None
    out["gpu_work_period_active_total_ns"] = sum(gpu_work_active_ns) if gpu_work_active_ns else None

    out["pwrstats_parsed_n"] = len(pwr_total)
    out["pwrstats_total_sum"] = sum(pwr_total) if pwr_total else None
    out["pwrstats_busy_sum"] = sum(pwr_busy) if pwr_busy else None
    out["pwrstats_ram_time_sum"] = sum(pwr_ram_time) if pwr_ram_time else None
    out["pwrstats_ram_wait_sum"] = sum(pwr_ram_wait) if pwr_ram_wait else None

    out["pwrstats_busy_pct_avg"] = mean_or_none(pwr_busy_pct)
    out["pwrstats_busy_pct_max"] = max(pwr_busy_pct) if pwr_busy_pct else None
    out["pwrstats_ram_wait_pct_avg"] = mean_or_none(pwr_ram_wait_pct)
    out["pwrstats_ram_wait_pct_max"] = max(pwr_ram_wait_pct) if pwr_ram_wait_pct else None

    if pwr_total and sum(pwr_total) > 0:
        out["pwrstats_overall_busy_pct"] = sum(pwr_busy) / sum(pwr_total) * 100.0
    else:
        out["pwrstats_overall_busy_pct"] = None

    if pwr_ram_time and sum(pwr_ram_time) > 0:
        out["pwrstats_overall_ram_wait_pct"] = sum(pwr_ram_wait) / sum(pwr_ram_time) * 100.0
    else:
        out["pwrstats_overall_ram_wait_pct"] = None

    return out


def parse_run(label: str, run_dir: Path) -> dict:
    data = {
        "label": label,
        "run_dir": str(run_dir),
        "run_name": run_dir.name,
    }

    data.update(parse_config(run_dir))
    data.update(parse_workload(run_dir))
    data.update(parse_tracepoints(run_dir))
    data.update(parse_sysfs(run_dir))
    data.update(parse_trace(run_dir))

    return data


def fmt(v):
    if v is None:
        return "NA"
    if isinstance(v, float):
        return f"{v:.2f}"
    return str(v)


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


KEY_METRICS = [
    "verification_passed_count",
    "tracepoints_enabled_count",
    "sysfs_fast_samples",
    "benchmark_trace_lines",
    "cmdbatch_queued",
    "cmdbatch_submitted",
    "cmdbatch_retired",
    "kgsl_mem_alloc_total_bytes",
    "pwrstats_overall_busy_pct",
    "pwrstats_busy_pct_avg",
    "pwrstats_overall_ram_wait_pct",
    "pwrstats_ram_wait_pct_avg",
    "pwrstats_ram_wait_sum",
    "gpu_work_period_active_total_ns",
    "gpu_load_avg",
    "gpu_load_max",
    "gpu_busy_percentage_avg",
    "gpu_busy_percentage_max",
    "cur_freq_avg",
    "gpuclk_avg",
    "clock_mhz_avg",
    "cur_ab_avg",
    "cur_ab_max",
    "buslevel_avg_bw_avg",
    "buslevel_avg_bw_max",
    "throttling_max",
]


def write_key_metrics_csv(path: Path, runs: list[dict]):
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["metric"] + [r["label"] for r in runs])

        for key in KEY_METRICS:
            writer.writerow([key] + [fmt(r.get(key)) for r in runs])


def write_all_metrics_csv(path: Path, runs: list[dict]):
    keys = sorted(set().union(*(r.keys() for r in runs)))

    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["metric"] + [r["label"] for r in runs])

        for key in keys:
            writer.writerow([key] + [fmt(r.get(key)) for r in runs])


def write_summary_txt(path: Path, runs: list[dict]):
    by_label = {r["label"]: r for r in runs}
    copy = by_label["copy"]
    alu = by_label["alu"]
    mem = by_label["mem"]

    def line(f, metric):
        f.write(
            f"{metric:42s}"
            f"{fmt(copy.get(metric)):>16s}"
            f"{fmt(alu.get(metric)):>16s}"
            f"{fmt(mem.get(metric)):>16s}\n"
        )

    def compare_line(f, metric, a_label, b_label):
        a = by_label[a_label].get(metric)
        b = by_label[b_label].get(metric)
        d = pct_diff(a, b)
        d_s = "NA" if d is None else f"{d:.2f}%"
        f.write(f"- {metric}: {a_label}={fmt(a)}, {b_label}={fmt(b)}, change={d_s}\n")

    with path.open("w") as f:
        f.write("Three-way KGSL/sysfs/tracefs aggregate summary\n")
        f.write("=" * 90 + "\n\n")

        for r in runs:
            f.write(f"{r['label']}:\n")
            f.write(f"  run_dir: {r['run_dir']}\n")
            f.write(f"  run_name: {r['run_name']}\n")
            f.write(f"  workload_type: {r.get('config_workload_type', 'NA')}\n")
            f.write(f"  command: {r.get('config_command', 'NA')}\n\n")

        f.write("Key metrics\n")
        f.write("-" * 90 + "\n")
        f.write(f"{'metric':42s}{'copy':>16s}{'alu':>16s}{'mem':>16s}\n")
        f.write("-" * 90 + "\n")

        for metric in KEY_METRICS:
            line(f, metric)

        f.write("\nPairwise highlights\n")
        f.write("-" * 90 + "\n")

        f.write("\ncopy -> alu:\n")
        for metric in [
            "pwrstats_overall_busy_pct",
            "gpu_load_avg",
            "gpu_busy_percentage_avg",
            "gpu_work_period_active_total_ns",
            "pwrstats_overall_ram_wait_pct",
        ]:
            compare_line(f, metric, "copy", "alu")

        f.write("\ncopy -> mem:\n")
        for metric in [
            "pwrstats_overall_busy_pct",
            "gpu_load_avg",
            "gpu_busy_percentage_avg",
            "gpu_work_period_active_total_ns",
            "pwrstats_overall_ram_wait_pct",
            "cur_ab_avg",
        ]:
            compare_line(f, metric, "copy", "mem")

        f.write("\nalu -> mem:\n")
        for metric in [
            "pwrstats_overall_busy_pct",
            "pwrstats_overall_ram_wait_pct",
            "pwrstats_ram_wait_pct_avg",
            "pwrstats_ram_wait_sum",
            "gpu_load_avg",
            "gpu_busy_percentage_avg",
            "cur_ab_avg",
            "buslevel_avg_bw_avg",
            "kgsl_mem_alloc_total_bytes",
            "cmdbatch_queued",
        ]:
            compare_line(f, metric, "alu", "mem")

        f.write("\nInterpretation\n")
        f.write("-" * 90 + "\n")
        f.write(
            "The copy baseline should show the lowest GPU activity. The ALU-heavy workload "
            "should show high overall busy/load with relatively low RAM wait. The memory-heavy "
            "workload should show high overall busy plus much higher RAM wait. Identical or "
            "similar command-batch counts and KGSL allocation totals are useful controls: they "
            "show that differences come from runtime behavior rather than setup structure.\n"
        )


def main():
    parser = argparse.ArgumentParser(
        description="Aggregate copy/ALU/memory KGSL/sysfs/tracefs benchmark runs."
    )
    parser.add_argument("--copy", required=True, help="copy baseline run directory")
    parser.add_argument("--alu", required=True, help="ALU-heavy run directory")
    parser.add_argument("--mem", required=True, help="memory-heavy run directory")
    parser.add_argument("--out-dir", required=True, help="output directory")

    args = parser.parse_args()

    copy_dir = Path(args.copy).resolve()
    alu_dir = Path(args.alu).resolve()
    mem_dir = Path(args.mem).resolve()
    out_dir = Path(args.out_dir).resolve()

    for d in [copy_dir, alu_dir, mem_dir]:
        if not d.exists():
            raise FileNotFoundError(f"Run directory not found: {d}")

    out_dir.mkdir(parents=True, exist_ok=True)

    runs = [
        parse_run("copy", copy_dir),
        parse_run("alu", alu_dir),
        parse_run("mem", mem_dir),
    ]

    write_summary_txt(out_dir / "aggregate_summary.txt", runs)
    write_key_metrics_csv(out_dir / "aggregate_key_metrics.csv", runs)
    write_all_metrics_csv(out_dir / "aggregate_all_metrics.csv", runs)

    print(f"[ok] Wrote: {out_dir / 'aggregate_summary.txt'}")
    print(f"[ok] Wrote: {out_dir / 'aggregate_key_metrics.csv'}")
    print(f"[ok] Wrote: {out_dir / 'aggregate_all_metrics.csv'}")


if __name__ == "__main__":
    main()
