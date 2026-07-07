#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

import numpy as np
import pandas as pd


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--sweeps-root", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--widths", default="128,256,512,1024,2048")
    p.add_argument("--latest-n", type=int, default=0)
    p.add_argument("--threshold-frac", type=float, default=0.08)
    p.add_argument("--min-active-samples", type=int, default=1)
    p.add_argument("--pad-samples", type=int, default=10)
    p.add_argument("--groups", default="SP,TP,UCHE,HLSQ,RBBM,CP")
    return p.parse_args()


def parse_group_chunk(path: Path):
    group_dir = path.parent.name
    m_group = re.match(r"\d+_(.+)", group_dir)
    group = m_group.group(1) if m_group else group_dir

    m_chunk = re.search(r"_chunk(\d+)\.csv$", path.name)
    chunk = int(m_chunk.group(1)) if m_chunk else -1
    return group, chunk


def detect_regions(df, widths, threshold_frac, min_active_samples, pad_samples):
    cols = [c for c in df.columns if c != "elapsed_s"]
    if not cols:
        return []

    activity = df[cols].abs().sum(axis=1).to_numpy(dtype=float)
    if len(activity) == 0 or np.max(activity) <= 0:
        return []

    threshold = np.max(activity) * threshold_frac
    active = activity > threshold

    regions = []
    in_region = False
    start = 0

    for i, flag in enumerate(active):
        if flag and not in_region:
            start = i
            in_region = True
        elif not flag and in_region:
            end = i
            if end - start >= min_active_samples:
                regions.append((start, end))
            in_region = False

    if in_region:
        end = len(active)
        if end - start >= min_active_samples:
            regions.append((start, end))

    # Merge immediately adjacent split regions.
    merged = []
    for s, e in regions:
        if not merged:
            merged.append((s, e))
        else:
            ps, pe = merged[-1]
            if s - pe <= 1:
                merged[-1] = (ps, e)
            else:
                merged.append((s, e))

    # If too many regions, keep the strongest 5 by summed activity.
    if len(merged) > len(widths):
        scored = []
        for s, e in merged:
            scored.append((activity[s:e].sum(), s, e))
        scored.sort(reverse=True)
        merged = sorted([(s, e) for _, s, e in scored[:len(widths)]])

    if len(merged) != len(widths):
        return []

    n = len(df)
    windows = []

    for region_idx, ((s, e), width) in enumerate(zip(merged, widths), start=1):
        s2 = max(0, s - pad_samples)
        e2 = min(n, e + pad_samples)

        windows.append({
            "region_index": region_idx,
            "width": int(width),
            "start_idx": int(s2),
            "end_idx_exclusive": int(e2),
            "start_s": float(df["elapsed_s"].iloc[s2]),
            "end_s": float(df["elapsed_s"].iloc[e2 - 1]),
            "samples": int(e2 - s2),
        })

    return windows


def summarize_csv(csv_path, widths, args, run_name):
    group, chunk = parse_group_chunk(csv_path)

    df = pd.read_csv(csv_path)
    cols = [c for c in df.columns if c != "elapsed_s"]

    windows = detect_regions(
        df,
        widths,
        threshold_frac=args.threshold_frac,
        min_active_samples=args.min_active_samples,
        pad_samples=args.pad_samples,
    )

    quality = {
        "run": run_name,
        "group": group,
        "chunk": chunk,
        "csv_path": str(csv_path),
        "n_rows": len(df),
        "n_counters": len(cols),
        "n_regions_detected": len(windows),
        "expected_regions": len(widths),
        "region_ok": len(windows) == len(widths),
    }

    rows = []
    window_rows = []

    if len(windows) != len(widths):
        return rows, quality, window_rows

    for w in windows:
        window_rows.append({
            "run": run_name,
            "group": group,
            "chunk": chunk,
            "csv_path": str(csv_path),
            **w,
        })

        region = df.iloc[w["start_idx"]:w["end_idx_exclusive"]]

        for counter in cols:
            values = region[counter].to_numpy(dtype=float)
            nonzero = values[values != 0]

            rows.append({
                "run": run_name,
                "group": group,
                "chunk": chunk,
                "counter": counter,
                "width": int(w["width"]),
                "region_index": int(w["region_index"]),
                "region_start_s": float(w["start_s"]),
                "region_end_s": float(w["end_s"]),
                "samples": int(len(values)),
                "total": float(np.sum(values)) if len(values) else 0.0,
                "max": float(np.max(values)) if len(values) else 0.0,
                "mean": float(np.mean(values)) if len(values) else 0.0,
                "active_mean": float(np.mean(nonzero)) if len(nonzero) else 0.0,
                "active_samples": int(len(nonzero)),
                "csv_path": str(csv_path),
            })

    return rows, quality, window_rows


def classify(summary, widths):
    rows = []

    for (run, group, counter), sub in summary.groupby(["run", "group", "counter"]):
        totals = sub.groupby("width")["total"].sum().reindex(widths).fillna(0.0)

        total_all = float(totals.sum())
        first = float(totals.iloc[0])
        last = float(totals.iloc[-1])

        if total_all == 0:
            label = "zero_all_widths"
            ratio = np.nan
            corr = np.nan
        else:
            ratio = last / first if first > 0 else np.inf
            x = np.array(widths, dtype=float)
            y = totals.to_numpy(dtype=float)
            corr = float(np.corrcoef(x, y)[0, 1]) if np.std(y) > 0 else 0.0

            if np.isfinite(ratio) and ratio > 8 and corr > 0.7:
                label = "strong_width_scaling"
            elif np.isfinite(ratio) and ratio > 2 and corr > 0.5:
                label = "moderate_width_scaling"
            elif np.isfinite(ratio) and ratio < 1.5:
                label = "flat_or_saturated"
            else:
                label = "active_mixed"

        row = {
            "run": run,
            "group": group,
            "counter": counter,
            "label": label,
            "total_all_widths": total_all,
            "total_width_first": first,
            "total_width_last": last,
            "scaling_ratio_last_over_first": ratio,
            "corr_total_vs_width": corr,
        }

        for w in widths:
            row[f"total_w{w}"] = float(totals.loc[w])

        rows.append(row)

    return pd.DataFrame(rows)


def stability(classified, widths):
    rows = []

    for (group, counter), sub in classified.groupby(["group", "counter"]):
        labels = sub["label"].value_counts()
        majority_label = labels.index[0]
        majority_count = int(labels.iloc[0])

        ratios = sub["scaling_ratio_last_over_first"].replace([np.inf, -np.inf], np.nan).dropna()
        corrs = sub["corr_total_vs_width"].replace([np.inf, -np.inf], np.nan).dropna()

        row = {
            "group": group,
            "counter": counter,
            "n_runs": int(len(sub)),
            "majority_label": majority_label,
            "majority_label_count": majority_count,
            "label_consistency": majority_count / len(sub),
            "ratio_mean": float(ratios.mean()) if len(ratios) else np.nan,
            "ratio_std": float(ratios.std(ddof=1)) if len(ratios) > 1 else 0.0,
            "ratio_cv": float(ratios.std(ddof=1) / ratios.mean()) if len(ratios) > 1 and ratios.mean() != 0 else np.nan,
            "corr_mean": float(corrs.mean()) if len(corrs) else np.nan,
            "corr_std": float(corrs.std(ddof=1)) if len(corrs) > 1 else 0.0,
            "total_all_mean": float(sub["total_all_widths"].mean()),
            "total_all_std": float(sub["total_all_widths"].std(ddof=1)) if len(sub) > 1 else 0.0,
        }

        for w in widths:
            col = f"total_w{w}"
            vals = sub[col].to_numpy(dtype=float)
            row[f"{col}_mean"] = float(np.mean(vals))
            row[f"{col}_std"] = float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0

        row["stable_strong_scaling"] = (
            majority_label in ["strong_width_scaling", "moderate_width_scaling"]
            and row["label_consistency"] >= 0.67
            and (pd.isna(row["ratio_cv"]) or row["ratio_cv"] <= 0.25)
            and (pd.isna(row["corr_mean"]) or row["corr_mean"] >= 0.90)
        )

        row["near_16x_scaling"] = (
            row["ratio_mean"] >= 15.0
            and row["ratio_mean"] <= 17.0
            and row["corr_mean"] >= 0.95
        )

        rows.append(row)

    return pd.DataFrame(rows).sort_values(
        ["stable_strong_scaling", "near_16x_scaling", "total_all_mean"],
        ascending=[False, False, False],
    )


def find_sweeps(root, latest_n):
    sweeps = sorted(root.glob("sweep_*"), key=lambda p: p.stat().st_mtime, reverse=True)
    if latest_n > 0:
        sweeps = sweeps[:latest_n]
    return list(reversed(sweeps))


def main():
    args = parse_args()

    widths = [int(x.strip()) for x in args.widths.split(",") if x.strip()]
    groups = set(x.strip() for x in args.groups.split(",") if x.strip())

    root = Path(args.sweeps_root).resolve()
    out = Path(args.out_dir).resolve()
    out.mkdir(parents=True, exist_ok=True)

    sweeps = find_sweeps(root, args.latest_n)

    all_rows = []
    all_quality = []
    all_windows = []

    print(f"[analysis] sweeps={len(sweeps)}")
    print(f"[analysis] groups={sorted(groups)}")

    for sweep in sweeps:
        run_name = sweep.name
        csvs = sorted(sweep.glob("**/*_chunk*.csv"))

        print(f"[analysis] run={run_name}, csvs={len(csvs)}")

        for csv in csvs:
            group, _ = parse_group_chunk(csv)
            if group not in groups:
                continue

            rows, quality, windows = summarize_csv(csv, widths, args, run_name)
            all_quality.append(quality)
            all_rows.extend(rows)
            all_windows.extend(windows)

    quality_df = pd.DataFrame(all_quality)
    windows_df = pd.DataFrame(all_windows)
    summary_df = pd.DataFrame(all_rows)

    quality_df.to_csv(out / "local_region_detection_quality.csv", index=False)
    windows_df.to_csv(out / "local_width_windows.csv", index=False)
    summary_df.to_csv(out / "all_runs_counter_width_summary.csv", index=False)

    classified_df = classify(summary_df, widths)
    classified_df.to_csv(out / "all_runs_counter_classification.csv", index=False)

    stable_df = stability(classified_df, widths)
    stable_df.to_csv(out / "cross_run_stability_summary.csv", index=False)

    stable_df[stable_df["stable_strong_scaling"]].to_csv(out / "stable_width_scaling_counters.csv", index=False)
    stable_df[stable_df["near_16x_scaling"]].to_csv(out / "stable_near_16x_counters.csv", index=False)

    print("[analysis] wrote:")
    print(f"  {out / 'local_region_detection_quality.csv'}")
    print(f"  {out / 'local_width_windows.csv'}")
    print(f"  {out / 'all_runs_counter_width_summary.csv'}")
    print(f"  {out / 'all_runs_counter_classification.csv'}")
    print(f"  {out / 'cross_run_stability_summary.csv'}")
    print(f"  {out / 'stable_width_scaling_counters.csv'}")
    print(f"  {out / 'stable_near_16x_counters.csv'}")

    print()
    print("[analysis] detection quality by group:")
    print(
        quality_df.groupby("group")["region_ok"]
        .agg(["count", "sum", "mean"])
        .sort_values("mean", ascending=False)
        .to_string()
    )


if __name__ == "__main__":
    main()
