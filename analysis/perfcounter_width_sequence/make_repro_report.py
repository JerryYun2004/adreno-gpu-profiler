#!/usr/bin/env python3
import argparse
from pathlib import Path

import pandas as pd


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--analysis-dir", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args()


def table(df, cols, n=50):
    if len(df) == 0:
        return "(no rows)"
    return df[cols].head(n).to_string(index=False)


def main():
    args = parse_args()
    analysis_dir = Path(args.analysis_dir)
    out = Path(args.out)

    stable = pd.read_csv(analysis_dir / "cross_run_stability_summary.csv")
    windows = pd.read_csv(analysis_dir / "all_runs_reference_windows.csv")
    classified = pd.read_csv(analysis_dir / "all_runs_counter_classification.csv")

    near16 = stable[stable["near_16x_scaling"]].sort_values("total_w2048_mean", ascending=False)

    keywords = "BUSY|STALL|STARVE|LATENCY|READ|WRITE|REQ|TRANS|MISS|VBIF|UCHE|CACHE|TP"
    bottleneck = stable[
        stable["counter"].str.contains(keywords, case=False, na=False) &
        (stable["total_all_mean"] > 0)
    ].sort_values("total_w2048_mean", ascending=False)

    key_counters = [
        "SP_BUSY_CYCLES",
        "SP_ALU_WORKING_CYCLES",
        "SP_FULL_ALU_MUL_INSTRUCTIONS",
        "SP_GM_STORE_INSTRUCTIONS",
        "SP_UCHE_WRITE_TRANS",
        "SP_STALL_CYCLES_TP",
        "SP_LOW_EFFICIENCY_STARVED_BY_TP",
        "UCHE_BUSY_CYCLES",
        "UCHE_RAM_READ_REQ",
        "UCHE_RAM_WRITE_REQ",
        "UCHE_WRITE_REQUESTS_SP",
        "UCHE_READ_REQUESTS_TP",
        "UCHE_VBIF_LATENCY_CYCLES",
        "TP_L1_CACHELINE_REQUESTS",
        "TP_STARVE_CYCLES_UCHE",
    ]

    key = classified[classified["counter"].isin(key_counters)].sort_values(["counter", "run"])

    cols_stable = [
        "group", "counter", "majority_label",
        "ratio_mean", "ratio_cv", "corr_mean", "label_consistency",
        "total_w128_mean", "total_w2048_mean",
    ]

    cols_key = [
        "run", "group", "counter", "label",
        "scaling_ratio_last_over_first", "corr_total_vs_width",
        "total_w128", "total_w256", "total_w512", "total_w1024", "total_w2048",
    ]

    lines = []
    lines.append("# Fused Softmax Width-Sequence Reproducibility Report")
    lines.append("")
    lines.append("## Purpose")
    lines.append("")
    lines.append("This report compares repeated `streamer_sweeper` width-sequence runs using the same fused softmax configuration.")
    lines.append("")
    lines.append("The goal is to check whether the previously observed counter trends are stable and reproducible, rather than caused by one-off noise.")
    lines.append("")
    lines.append("## Benchmark Configuration")
    lines.append("")
    lines.append("```text")
    lines.append("widths:      128,256,512,1024,2048")
    lines.append("rows:        128")
    lines.append("repeats:     4")
    lines.append("time:        4 s")
    lines.append("width-sleep: 0.1 s")
    lines.append("variant:     fused_lmem softmax")
    lines.append("```")
    lines.append("")
    lines.append("## Reference Windows")
    lines.append("")
    lines.append("Each run used `11_SP/SP_chunk001.csv` to detect the five width regions, then applied padded windows to every counter chunk.")
    lines.append("")
    lines.append("```text")
    lines.append(windows.to_string(index=False))
    lines.append("```")
    lines.append("")
    lines.append("## Stable Near-16x Scaling Counters")
    lines.append("")
    lines.append("Since `2048 / 128 = 16`, counters that repeatedly scale near 16x are strong evidence that the width sequence is being captured correctly.")
    lines.append("")
    lines.append("```text")
    lines.append(table(near16, cols_stable, 60))
    lines.append("```")
    lines.append("")
    lines.append("## Memory/Cache/TP/UCHE Bottleneck-Looking Counters")
    lines.append("")
    lines.append("These counters are relevant to cache traffic, memory requests, stalls, starvation, and latency.")
    lines.append("")
    lines.append("```text")
    lines.append(table(bottleneck, cols_stable, 80))
    lines.append("```")
    lines.append("")
    lines.append("## Key Counter Run-to-Run Comparison")
    lines.append("")
    lines.append("```text")
    lines.append(table(key, cols_key, 200))
    lines.append("```")
    lines.append("")
    lines.append("## Interpretation")
    lines.append("")
    lines.append("The finding is considered reproducible if the same counter families repeatedly show high width correlation, low run-to-run variation, and similar scaling behavior.")
    lines.append("")
    lines.append("The main expected stable pattern is:")
    lines.append("")
    lines.append("- SP instruction and transaction counters scale with softmax width.")
    lines.append("- UCHE/TP request and transaction counters scale close to the 16x width ratio.")
    lines.append("- UCHE/VBIF latency and TP/UCHE starvation counters grow strongly at larger widths.")
    lines.append("- SP busy and ALU working cycles grow less than raw element count, because they represent cycle/utilization behavior under parallel execution.")
    lines.append("")
    lines.append("If these patterns appear across repeated runs, the previous conclusion is strengthened: larger fused-softmax widths increasingly stress the memory/cache/TP/UCHE path rather than only increasing SP ALU work.")
    lines.append("")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines))

    print(f"[report] wrote {out}")


if __name__ == "__main__":
    main()
