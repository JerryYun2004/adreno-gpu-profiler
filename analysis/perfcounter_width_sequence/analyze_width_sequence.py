#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

import numpy as np
import pandas as pd


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--sweep-dir", required=True)
    p.add_argument("--widths", required=True, help="comma list, e.g. 128,256,512,1024,2048")
    p.add_argument("--out-dir", required=True)
    p.add_argument("--threshold-frac", type=float, default=0.08)
    p.add_argument("--min-active-samples", type=int, default=2)
    return p.parse_args()


def parse_group_chunk(path: Path):
    # Example:
    # .../03_PC/PC_chunk010.csv
    group_dir = path.parent.name
    m_group = re.match(r"\d+_(.+)", group_dir)
    group = m_group.group(1) if m_group else group_dir

    m_chunk = re.search(r"_chunk(\d+)\.csv$", path.name)
    chunk = int(m_chunk.group(1)) if m_chunk else -1
    return group, chunk


def find_active_regions(df, counter_cols, n_regions, threshold_frac=0.08, min_active_samples=2):
    """
    Detect workload regions using aggregate activity across all counters in this CSV.
    This avoids relying on one specific counter name.

    Returns list of (start_index, end_index_exclusive).
    """
    if len(df) == 0:
        return []

    activity = df[counter_cols].abs().sum(axis=1).to_numpy(dtype=float)

    if np.max(activity) <= 0:
        return []

    threshold = np.max(activity) * threshold_frac
    active = activity > threshold

    regions = []
    in_region = False
    start = 0

    for i, is_active in enumerate(active):
        if is_active and not in_region:
            start = i
            in_region = True
        elif not is_active and in_region:
            end = i
            if end - start >= min_active_samples:
                regions.append((start, end))
            in_region = False

    if in_region:
        end = len(active)
        if end - start >= min_active_samples:
            regions.append((start, end))

    # Sometimes one burst can split into multiple nearby subregions.
    # Merge regions separated by only 1 sample.
    merged = []
    for r in regions:
        if not merged:
            merged.append(r)
        else:
            prev_start, prev_end = merged[-1]
            cur_start, cur_end = r
            if cur_start - prev_end <= 1:
                merged[-1] = (prev_start, cur_end)
            else:
                merged.append(r)

    # Keep strongest n regions if too many are detected.
    if len(merged) > n_regions:
        scored = []
        for s, e in merged:
            scored.append((activity[s:e].sum(), s, e))
        scored.sort(reverse=True)
        selected = sorted([(s, e) for _, s, e in scored[:n_regions]])
        return selected

    return merged


def summarize_csv(csv_path: Path, widths, args):
    group, chunk = parse_group_chunk(csv_path)

    df = pd.read_csv(csv_path)
    if "elapsed_s" not in df.columns:
        return [], {
            "csv_path": str(csv_path),
            "group": group,
            "chunk": chunk,
            "error": "missing elapsed_s",
        }

    counter_cols = [c for c in df.columns if c != "elapsed_s"]

    if not counter_cols:
        return [], {
            "csv_path": str(csv_path),
            "group": group,
            "chunk": chunk,
            "error": "no counters",
        }

    regions = find_active_regions(
        df,
        counter_cols,
        n_regions=len(widths),
        threshold_frac=args.threshold_frac,
        min_active_samples=args.min_active_samples,
    )

    quality = {
        "csv_path": str(csv_path),
        "group": group,
        "chunk": chunk,
        "n_rows": len(df),
        "n_counters": len(counter_cols),
        "n_regions_detected": len(regions),
        "expected_regions": len(widths),
        "region_ok": len(regions) == len(widths),
    }

    rows = []

    # If region detection fails, still produce whole-file fallback.
    if len(regions) != len(widths):
        for counter in counter_cols:
            values = df[counter].to_numpy(dtype=float)
            rows.append({
                "group": group,
                "chunk": chunk,
                "csv_file": csv_path.name,
                "csv_path": str(csv_path),
                "counter": counter,
                "width": -1,
                "region_index": -1,
                "region_start_s": float(df["elapsed_s"].iloc[0]),
                "region_end_s": float(df["elapsed_s"].iloc[-1]),
                "samples": len(values),
                "total": float(np.sum(values)),
                "max": float(np.max(values)) if len(values) else 0.0,
                "mean": float(np.mean(values)) if len(values) else 0.0,
                "active_mean": float(np.mean(values[values != 0])) if np.any(values != 0) else 0.0,
                "active_samples": int(np.count_nonzero(values)),
                "detection_status": "fallback_whole_file",
            })
        return rows, quality

    for region_idx, ((s, e), width) in enumerate(zip(regions, widths), start=1):
        region_df = df.iloc[s:e]
        region_start_s = float(region_df["elapsed_s"].iloc[0])
        region_end_s = float(region_df["elapsed_s"].iloc[-1])

        for counter in counter_cols:
            values = region_df[counter].to_numpy(dtype=float)
            nonzero = values[values != 0]

            rows.append({
                "group": group,
                "chunk": chunk,
                "csv_file": csv_path.name,
                "csv_path": str(csv_path),
                "counter": counter,
                "width": int(width),
                "region_index": region_idx,
                "region_start_s": region_start_s,
                "region_end_s": region_end_s,
                "samples": len(values),
                "total": float(np.sum(values)),
                "max": float(np.max(values)) if len(values) else 0.0,
                "mean": float(np.mean(values)) if len(values) else 0.0,
                "active_mean": float(np.mean(nonzero)) if len(nonzero) else 0.0,
                "active_samples": int(len(nonzero)),
                "detection_status": "region_detected",
            })

    return rows, quality


def classify_counters(summary: pd.DataFrame, widths):
    good = summary[summary["width"].isin(widths)].copy()

    grouped = []

    for (group, counter), sub in good.groupby(["group", "counter"]):
        totals = sub.groupby("width")["total"].sum().reindex(widths).fillna(0.0)

        total_all = float(totals.sum())
        zero_all = total_all == 0.0

        t_first = float(totals.iloc[0])
        t_last = float(totals.iloc[-1])

        scaling_ratio = np.nan
        if t_first > 0:
            scaling_ratio = t_last / t_first

        # Simple trend fit against width.
        x = np.array(widths, dtype=float)
        y = totals.to_numpy(dtype=float)

        if np.all(y == 0):
            corr = np.nan
            slope = 0.0
        else:
            corr = float(np.corrcoef(x, y)[0, 1]) if np.std(y) > 0 else 0.0
            slope = float(np.polyfit(x, y, 1)[0])

        # Heuristic class.
        if zero_all:
            label = "zero_all_widths"
        elif np.isfinite(scaling_ratio) and scaling_ratio > 8 and corr > 0.7:
            label = "strong_width_scaling"
        elif np.isfinite(scaling_ratio) and scaling_ratio > 2 and corr > 0.5:
            label = "moderate_width_scaling"
        elif np.isfinite(scaling_ratio) and scaling_ratio < 1.5 and total_all > 0:
            label = "flat_or_saturated"
        else:
            label = "active_mixed"

        grouped.append({
            "group": group,
            "counter": counter,
            "total_all_widths": total_all,
            "total_width_first": t_first,
            "total_width_last": t_last,
            "scaling_ratio_last_over_first": scaling_ratio,
            "corr_total_vs_width": corr,
            "slope_total_vs_width": slope,
            "label": label,
            **{f"total_w{w}": float(totals.loc[w]) for w in widths},
        })

    return pd.DataFrame(grouped).sort_values(
        ["label", "total_all_widths"],
        ascending=[True, False],
    )


def main():
    args = parse_args()

    sweep_dir = Path(args.sweep_dir).expanduser().resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    widths = [int(x.strip()) for x in args.widths.split(",") if x.strip()]

    csvs = sorted(sweep_dir.glob("**/*_chunk*.csv"))

    all_rows = []
    quality_rows = []

    print(f"[analysis] sweep_dir={sweep_dir}")
    print(f"[analysis] found {len(csvs)} chunk CSV files")

    for i, csv_path in enumerate(csvs, start=1):
        rows, quality = summarize_csv(csv_path, widths, args)
        all_rows.extend(rows)
        quality_rows.append(quality)

        if i % 25 == 0:
            print(f"[analysis] processed {i}/{len(csvs)}")

    summary = pd.DataFrame(all_rows)
    quality = pd.DataFrame(quality_rows)

    summary_path = out_dir / "counter_width_summary.csv"
    quality_path = out_dir / "region_detection_quality.csv"

    summary.to_csv(summary_path, index=False)
    quality.to_csv(quality_path, index=False)

    classified = classify_counters(summary, widths)
    classified_path = out_dir / "counter_classification.csv"
    classified.to_csv(classified_path, index=False)

    zero_path = out_dir / "zero_counters.csv"
    classified[classified["label"] == "zero_all_widths"].to_csv(zero_path, index=False)

    scaling_path = out_dir / "strong_width_scaling_counters.csv"
    classified[classified["label"].isin(["strong_width_scaling", "moderate_width_scaling"])].to_csv(
        scaling_path,
        index=False,
    )

    saturated_path = out_dir / "flat_or_saturated_counters.csv"
    classified[classified["label"] == "flat_or_saturated"].to_csv(saturated_path, index=False)

    print("[analysis] wrote:")
    print(f"  {summary_path}")
    print(f"  {quality_path}")
    print(f"  {classified_path}")
    print(f"  {zero_path}")
    print(f"  {scaling_path}")
    print(f"  {saturated_path}")

    bad_regions = quality[quality["region_ok"] == False]
    if len(bad_regions):
        print()
        print("[warning] Some files did not detect the expected number of regions.")
        print(f"          See: {quality_path}")
        print(bad_regions[["group", "chunk", "n_regions_detected", "expected_regions", "csv_path"]].head(20).to_string(index=False))


if __name__ == "__main__":
    main()
