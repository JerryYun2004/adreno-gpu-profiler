#!/usr/bin/env python3
import csv
import glob
import os
import re
import pandas as pd

ROOT = os.environ.get("RESULT_DIR", "results/ml_primitive_width_sweep")
rows = []

for rt in glob.glob(os.path.join(ROOT, "runtime_logs", "*_runtime.txt")):
    data = {}
    with open(rt, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if "=" in line:
                k, v = line.strip().split("=", 1)
                data[k] = v
    name = os.path.basename(rt).replace("_runtime.txt", "")
    m = re.search(r"width_(\d+)", name)
    if not m:
        continue
    width = int(m.group(1))
    perf = os.path.join(ROOT, "perf_csv", f"{name}_perf.csv")
    counters = {}
    if os.path.exists(perf):
        try:
            df = pd.read_csv(perf)
            for col in df.columns:
                if col != "elapsed_s" and pd.api.types.is_numeric_dtype(df[col]):
                    counters[f"sum_{col}"] = df[col].sum()
            if "SP_BUSY_CYCLES" in df.columns and "SP_ALU_WORKING_CYCLES" in df.columns:
                sb = df["SP_BUSY_CYCLES"].sum()
                sa = df["SP_ALU_WORKING_CYCLES"].sum()
                counters["alu_working_over_sp_busy"] = sa / sb if sb else None
            if "UCHE_BUSY_CYCLES" in df.columns and "SP_BUSY_CYCLES" in df.columns:
                sb = df["SP_BUSY_CYCLES"].sum()
                ub = df["UCHE_BUSY_CYCLES"].sum()
                counters["uche_busy_over_sp_busy"] = ub / sb if sb else None
        except Exception as e:
            counters["perf_parse_error"] = str(e)
    rows.append({"name": name, "width": width, **data, **counters})

out = pd.DataFrame(rows).sort_values(["name", "width"])
out_path = os.path.join(ROOT, "summary.csv")
out.to_csv(out_path, index=False)
print(out.to_string(index=False))
print(f"\nWrote {out_path}")
