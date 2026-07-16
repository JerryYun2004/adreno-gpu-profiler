#!/usr/bin/env python3
import re
import statistics as stats
from pathlib import Path

GROUPS = [
    ("vendor", "alu", "vendor_compute_summary_*.txt", 2),
    ("turnip", "alu", "turnip_compute_summary_*.txt", 2),
    ("vendor", "mem", "vendor_mem_summary_*.txt", 3),
    ("turnip", "mem", "turnip_mem_summary_*.txt", 3),
]

METRICS = {
    "submitted_batches": r"submitted batches:\s+([0-9.]+)",
    "retired_batches": r"retired batches:\s+([0-9.]+)",
    "avg_active_ticks": r"avg active:\s+([0-9.]+)",
    "max_active_ticks": r"max active:\s+([0-9.]+)",
    "avg_queue_to_start_ticks": r"avg queue_to_start:\s+([0-9.]+)",
    "max_queue_to_start_ticks": r"max queue_to_start:\s+([0-9.]+)",
    "avg_gmu_latency_ticks": r"avg gmu_latency:\s+([0-9.]+)",
    "max_gmu_latency_ticks": r"max gmu_latency:\s+([0-9.]+)",
    "avg_busy_pct": r"avg busy_pct:\s+([0-9.]+)%",
    "max_busy_pct": r"max busy_pct:\s+([0-9.]+)%",
    "avg_ram_wait_pct": r"avg ram_wait_pct:\s+([0-9.]+)%",
    "max_ram_wait_pct": r"max ram_wait_pct:\s+([0-9.]+)%",
    "min_freq_khz": r"min freq:\s+([0-9.]+)\s+kHz",
    "max_freq_khz": r"max freq:\s+([0-9.]+)\s+kHz",
}

def trial_num(path: Path) -> int:
    m = re.search(r"_(\d+)\.txt$", path.name)
    return int(m.group(1)) if m else -1

def parse_file(path: Path):
    text = path.read_text(errors="replace")
    out = {}
    for key, pattern in METRICS.items():
        matches = re.findall(pattern, text)
        if matches:
            # If duplicate avg_busy_pct appears because both pwrstats and gpubusy exist,
            # keep the first one, which is the kgsl_pwrstats section in the current parser.
            out[key] = float(matches[0])
    return out

def mean(xs):
    return stats.mean(xs) if xs else float("nan")

def stdev(xs):
    return stats.stdev(xs) if len(xs) >= 2 else 0.0

def fmt(x, digits=2):
    if x != x:
        return "NA"
    return f"{x:.{digits}f}"

rows = []

for driver, workload, pattern, first_trial in GROUPS:
    files = sorted(Path(".").glob(pattern), key=trial_num)
    files = [p for p in files if trial_num(p) >= first_trial]

    parsed = []
    for p in files:
        data = parse_file(p)
        data["trial"] = trial_num(p)
        parsed.append(data)

    if not parsed:
        print(f"WARNING: no files found for {driver} {workload} using {pattern}")
        continue

    row = {
        "driver": driver,
        "workload": workload,
        "trials": ",".join(str(d["trial"]) for d in parsed),
    }

    for key in METRICS:
        vals = [d[key] for d in parsed if key in d]
        row[key + "_mean"] = mean(vals)
        row[key + "_stdev"] = stdev(vals)

    rows.append(row)

print()
print("=== Steady-state summary ===")
print("Note: ALU uses trials 2-5; memory uses trials 3-5 to reduce warm-up/setup effects.")
print()

header = (
    f"{'driver':<8} {'workload':<8} {'trials':<10} "
    f"{'batches':>8} {'avg_active':>12} {'max_active':>12} "
    f"{'busy%':>8} {'ram_wait%':>10} {'freq_kHz':>10}"
)
print(header)
print("-" * len(header))

for r in rows:
    print(
        f"{r['driver']:<8} {r['workload']:<8} {r['trials']:<10} "
        f"{fmt(r['submitted_batches_mean'], 2):>8} "
        f"{fmt(r['avg_active_ticks_mean'], 1):>12} "
        f"{fmt(r['max_active_ticks_mean'], 1):>12} "
        f"{fmt(r['avg_busy_pct_mean'], 2):>8} "
        f"{fmt(r['avg_ram_wait_pct_mean'], 2):>10} "
        f"{fmt(r['max_freq_khz_mean'], 0):>10}"
    )

print()
print("=== Detailed means ± stdev ===")
for r in rows:
    print(f"\n[{r['driver']} / {r['workload']}] trials {r['trials']}")
    for key in [
        "submitted_batches",
        "retired_batches",
        "avg_active_ticks",
        "max_active_ticks",
        "avg_queue_to_start_ticks",
        "avg_gmu_latency_ticks",
        "avg_busy_pct",
        "avg_ram_wait_pct",
        "max_ram_wait_pct",
        "max_freq_khz",
    ]:
        print(
            f"  {key:<28} "
            f"{fmt(r[key + '_mean'], 2)} ± {fmt(r[key + '_stdev'], 2)}"
        )

print()
print("=== Useful ratios ===")
def find(driver, workload):
    for r in rows:
        if r["driver"] == driver and r["workload"] == workload:
            return r
    return None

for driver in ["vendor", "turnip"]:
    alu = find(driver, "alu")
    mem = find(driver, "mem")
    if alu and mem:
        active_ratio = mem["avg_active_ticks_mean"] / alu["avg_active_ticks_mean"]
        ram_ratio = mem["avg_ram_wait_pct_mean"] / alu["avg_ram_wait_pct_mean"]
        print(
            f"{driver:<8} mem/alu: "
            f"avg_active {active_ratio:.2f}x, "
            f"ram_wait {ram_ratio:.2f}x"
        )

vendor_mem = find("vendor", "mem")
turnip_mem = find("turnip", "mem")
vendor_alu = find("vendor", "alu")
turnip_alu = find("turnip", "alu")

if vendor_mem and turnip_mem:
    print(
        f"turnip/vendor memory: "
        f"avg_active {turnip_mem['avg_active_ticks_mean'] / vendor_mem['avg_active_ticks_mean']:.2f}x, "
        f"ram_wait {turnip_mem['avg_ram_wait_pct_mean'] / vendor_mem['avg_ram_wait_pct_mean']:.2f}x"
    )

if vendor_alu and turnip_alu:
    print(
        f"turnip/vendor alu: "
        f"avg_active {turnip_alu['avg_active_ticks_mean'] / vendor_alu['avg_active_ticks_mean']:.2f}x, "
        f"ram_wait {turnip_alu['avg_ram_wait_pct_mean'] / vendor_alu['avg_ram_wait_pct_mean']:.2f}x"
    )
