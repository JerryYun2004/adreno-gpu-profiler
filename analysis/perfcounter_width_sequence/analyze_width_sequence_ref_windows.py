#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

import numpy as np
import pandas as pd


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--sweep-dir", required=True)
    p.add_argument("--reference-csv", required=True)
    p.add_argument("--widths", required=True, help="comma list, e.g. 128,256,512,1024,2048")
    p.add_argument("--out-dir", required=True)
    p.add_argument("--threshold-frac", type=float, default=0.08)
    p.add_argument("--min-active-samples", type=int, default=2)
    p.add_argument("--pad-samples", type=int, default=1)
    return p.parse_args()


def parse_group_chunk(path: Path):
    group_dir = path.parent.name
    m_group = re.match(r"\d+_(.+)", group_dir)
    group = m_group.group(1) if m_group else group_dir

    m_chunk = re.search(r"_chunk(\d+)\.csv$", path.name)
    chunk = int(m_chunk.group(1)) if m_chunk else -1
    return group, chunk


def detect_regions_from_reference(ref_csv: Path, widths, threshold_frac, min_active_samples, pad_samples):
    df = pd.read_csv(ref_csv)
    counter_cols = [c for c in df.columns if c != "elapsed_s"]

    if not counter_cols:
        raise RuntimeError(f"No counter columns in reference CSV: {ref_csv}")

    activity = df[counter_cols].abs().sum(axis=1).to_numpy(dtype=float)

    if np.max(activity) <= 0:
        raise RuntimeError(f"Reference CSV has no activity: {ref_csv}")

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

    if len(merged) > len(widths):
        scored = []
        for s, e in merged:
            scored.append((activity[s:e].sum(), s, e))
        scored.sort(reverse=True)
        merged = sorted([(s, e) for _, s, e in scored[:len(widths)]])

    if len(merged) != len(widths):
        raise RuntimeError(
            f"Reference detection failed: detected {len(merged)} regions, expected {len(widths)}. "
            f"Try another reference CSV or adjust --threshold-frac."
        )

    windows = []
    n = len(df)

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


def summarize_csv_with_windows(csv_path: Path, windows):
    group, chunk = parse_group_chunk(csv_path)
    df = pd.read_csv(csv_path)

    if "elapsed_s" not in df.columns:
        return []

    counter_cols = [c for c in df.columns if c != "elapsed_s"]
    rows = []

    for w in windows:
        # Apply by time, not index, so minor row-count variation is okay.
        region = df[
            (df["elapsed_s"] >= w["start_s"]) &
            (df["elapsed_s"] <= w["end_s"])
        ]

        for counter in counter_cols:
            values = region[counter].to_numpy(dtype=float) if len(region) else np.array([])

            nonzero = values[values != 0] if len(values) else np.array([])

            rows.append({
                "group": group,
                "chunk": chunk,
                "csv_file": csv_path.name,
                "csv_path": str(csv_path),
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
            })

    return rows


def classify_counters(summary: pd.DataFrame, widths):
    grouped = []

    for (group, counter), sub in summary.groupby(["group", "counter"]):
        totals = sub.groupby("width")["total"].sum().reindex(widths).fillna(0.0)

        total_all = float(totals.sum())
        t_first = float(totals.iloc[0])
        t_last = float(totals.iloc[-1])

        if total_all == 0:
            label = "zero_all_widths"
            ratio = np.nan
            corr = np.nan
            slope = 0.0
        else:
            ratio = t_last / t_first if t_first > 0 else np.inf

            x = np.array(widths, dtype=float)
            y = totals.to_numpy(dtype=float)

            corr = float(np.corrcoef(x, y)[0, 1]) if np.std(y) > 0 else 0.0
            slope = float(np.polyfit(x, y, 1)[0])

            if np.isfinite(ratio) and ratio > 8 and corr > 0.7:
                label = "strong_width_scaling"
            elif np.isfinite(ratio) and ratio > 2 and corr > 0.5:
                label = "moderate_width_scaling"
            elif np.isfinite(ratio) and ratio < 1.5:
                label = "flat_or_saturated"
            else:
                label = "active_mixed"

        row = {
            "group": group,
            "counter": counter,
            "label": label,
            "total_all_widths": total_all,
            "total_width_first": t_first,
            "total_width_last": t_last,
            "scaling_ratio_last_over_first": ratio,
            "corr_total_vs_width": corr,
            "slope_total_vs_width": slope,
        }

        for width in widths:
            row[f"total_w{width}"] = float(totals.loc[width])

        grouped.append(row)

    return pd.DataFrame(grouped).sort_values(
        ["label", "total_all_widths"],
        ascending=[True, False],
    )


def main():
    args = parse_args()

    sweep_dir = Path(args.sweep_dir).expanduser().resolve()
    ref_csv = Path(args.reference_csv).expanduser().resolve()
    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    widths = [int(x.strip()) for x in args.widths.split(",") if x.strip()]

    print(f"[analysis] sweep_dir={sweep_dir}")
    print(f"[analysis] reference_csv={ref_csv}")

    windows = detect_regions_from_reference(
        ref_csv=ref_csv,
        widths=widths,
        threshold_frac=args.threshold_frac,
        min_active_samples=args.min_active_samples,
        pad_samples=args.pad_samples,
    )

    windows_df = pd.DataFrame(windows)
    windows_path = out_dir / "reference_width_windows.csv"
    windows_df.to_csv(windows_path, index=False)

    print("[analysis] detected reference windows:")
    print(windows_df.to_string(index=False))

    csvs = sorted(sweep_dir.glob("**/*_chunk*.csv"))
    print(f"[analysis] found {len(csvs)} chunk CSV files")

    rows = []

    for i, csv_path in enumerate(csvs, start=1):
        rows.extend(summarize_csv_with_windows(csv_path, windows))

        if i % 25 == 0:
            print(f"[analysis] processed {i}/{len(csvs)}")

    summary = pd.DataFrame(rows)
    summary_path = out_dir / "counter_width_summary.csv"
    summary.to_csv(summary_path, index=False)

    classified = classify_counters(summary, widths)
    classified_path = out_dir / "counter_classification.csv"
    classified.to_csv(classified_path, index=False)

    classified[classified["label"] == "zero_all_widths"].to_csv(
        out_dir / "zero_counters.csv",
        index=False,
    )

    classified[classified["label"].isin(["strong_width_scaling", "moderate_width_scaling"])].to_csv(
        out_dir / "width_scaling_counters.csv",
        index=False,
    )

    classified[classified["label"] == "flat_or_saturated"].to_csv(
        out_dir / "flat_or_saturated_counters.csv",
        index=False,
    )

    print("[analysis] wrote:")
    print(f"  {windows_path}")
    print(f"  {summary_path}")
    print(f"  {classified_path}")
    print(f"  {out_dir / 'zero_counters.csv'}")
    print(f"  {out_dir / 'width_scaling_counters.csv'}")
    print(f"  {out_dir / 'flat_or_saturated_counters.csv'}")


if __name__ == "__main__":
    main()
