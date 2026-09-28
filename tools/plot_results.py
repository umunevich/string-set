#!/usr/bin/env python3
"""Renders benchmarks/results.csv into charts for REPORT.md.

Run with the project venv (see README / REPORT.md for setup):
    .venv/bin/python tools/plot_results.py
"""
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "benchmarks" / "results.csv"
OUT_DIR = ROOT / "benchmarks"

# Colors: validated categorical palette (dataviz skill), fixed hue order.
BLUE = "#2a78d6"      # slot 1 -- custom robin-hood implementation
ORANGE = "#eb6834"    # slot 2 -- naive std::unordered_set baseline
AQUA = "#1baf7a"      # slot 3 -- I/O read phase
YELLOW = "#eda100"    # slot 4 -- I/O write phase
GRID = "#d9d9d6"
TEXT = "#0b0b0b"
MUTED = "#52514e"

IMPL_LABEL = {"strset": "custom (robin-hood)", "strset_naive": "baseline (std::unordered_set)"}
IMPL_COLOR = {"strset": BLUE, "strset_naive": ORANGE}


def style_axes(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(MUTED)
    ax.spines["bottom"].set_color(MUTED)
    ax.grid(True, which="major", color=GRID, linewidth=0.8, zorder=0)
    ax.set_axisbelow(True)
    ax.tick_params(colors=TEXT, labelsize=9)


def load_rows():
    rows = defaultdict(list)  # (impl, scale) -> list of dict
    with open(CSV_PATH) as f:
        for r in csv.DictReader(f):
            rows[(r["impl"], int(r["scale"]))].append(r)
    return rows


def aggregate(rows):
    # (impl, scale) -> dict of median metric
    agg = {}
    for key, recs in rows.items():
        agg[key] = {
            field: statistics.median(float(r[field]) for r in recs)
            for field in ("read_ms", "process_ms", "write_ms", "total_ms", "wall_ms")
        }
    return agg


def plot_scaling(agg, scales):
    fig, ax = plt.subplots(figsize=(7.2, 4.8), dpi=170)
    fig.patch.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")

    for impl in ("strset", "strset_naive"):
        ys = [agg[(impl, s)]["total_ms"] for s in scales]
        ax.plot(scales, ys, marker="o", markersize=6, linewidth=2,
                 color=IMPL_COLOR[impl], label=IMPL_LABEL[impl], zorder=3)

    # O(N) reference line anchored at the custom impl's first point.
    x0, y0 = scales[0], agg[("strset", scales[0])]["total_ms"]
    ref_y = [y0 * (s / x0) for s in scales]
    ax.plot(scales, ref_y, linestyle="--", linewidth=1.3, color=MUTED,
             label="O(N) reference", zorder=2)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("operations (N)", color=TEXT, fontsize=10)
    ax.set_ylabel("total time, read+process+write (ms, log scale)", color=TEXT, fontsize=10)
    ax.set_title("End-to-end processing time vs. input size", color=TEXT, fontsize=12, pad=12)
    ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{int(v):,}"))
    style_axes(ax)
    ax.legend(frameon=False, fontsize=9, loc="upper left")
    fig.tight_layout()
    fig.savefig(OUT_DIR / "scaling.png", facecolor=fig.get_facecolor())
    plt.close(fig)


def plot_throughput(agg, scales):
    fig, ax = plt.subplots(figsize=(7.2, 4.8), dpi=170)
    fig.patch.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")

    for impl in ("strset", "strset_naive"):
        ys = [s / agg[(impl, s)]["total_ms"] * 1000.0 / 1e6 for s in scales]  # M ops/sec
        ax.plot(scales, ys, marker="o", markersize=6, linewidth=2,
                 color=IMPL_COLOR[impl], label=IMPL_LABEL[impl], zorder=3)

    ax.set_xscale("log")
    ax.set_xlabel("operations (N)", color=TEXT, fontsize=10)
    ax.set_ylabel("throughput (million ops/sec)", color=TEXT, fontsize=10)
    ax.set_title("Throughput vs. input size", color=TEXT, fontsize=12, pad=12)
    ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{int(v):,}"))
    ax.set_ylim(bottom=0)
    style_axes(ax)
    ax.legend(frameon=False, fontsize=9, loc="lower left")
    fig.tight_layout()
    fig.savefig(OUT_DIR / "throughput.png", facecolor=fig.get_facecolor())
    plt.close(fig)


def plot_breakdown(agg, scales):
    fig, ax = plt.subplots(figsize=(7.2, 4.8), dpi=170)
    fig.patch.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")

    x = range(len(scales))
    read_ms = [agg[("strset", s)]["read_ms"] for s in scales]
    process_ms = [agg[("strset", s)]["process_ms"] for s in scales]
    write_ms = [agg[("strset", s)]["write_ms"] for s in scales]
    totals = [r + p + w for r, p, w in zip(read_ms, process_ms, write_ms)]
    read_pct = [r / t * 100 for r, t in zip(read_ms, totals)]
    process_pct = [p / t * 100 for p, t in zip(process_ms, totals)]
    write_pct = [w / t * 100 for w, t in zip(write_ms, totals)]

    width = 0.6
    gap = 2  # px-equivalent surface gap between stacked segments
    ax.bar(x, read_pct, width, color=AQUA, label="read (I/O)", zorder=3,
           edgecolor="#fcfcfb", linewidth=gap)
    ax.bar(x, process_pct, width, bottom=read_pct, color=BLUE, label="process (CPU)", zorder=3,
           edgecolor="#fcfcfb", linewidth=gap)
    bottom2 = [r + p for r, p in zip(read_pct, process_pct)]
    ax.bar(x, write_pct, width, bottom=bottom2, color=YELLOW, label="write (I/O)", zorder=3,
           edgecolor="#fcfcfb", linewidth=gap)

    ax.set_xticks(list(x))
    ax.set_xticklabels([f"{s:,}" for s in scales])
    ax.set_ylim(0, 100)
    ax.set_xlabel("operations (N)", color=TEXT, fontsize=10)
    ax.set_ylabel("share of total time (%)", color=TEXT, fontsize=10)
    ax.set_title("Custom implementation: time breakdown by phase", color=TEXT, fontsize=12, pad=12)
    style_axes(ax)
    ax.grid(False, axis="x")
    ax.legend(frameon=False, fontsize=9, loc="upper center", ncol=3, bbox_to_anchor=(0.5, -0.15))
    fig.tight_layout()
    fig.savefig(OUT_DIR / "breakdown.png", facecolor=fig.get_facecolor(), bbox_inches="tight")
    plt.close(fig)


def main():
    if not CSV_PATH.exists():
        print(f"missing {CSV_PATH}; run tools/run_benchmarks.sh first", file=sys.stderr)
        sys.exit(1)

    rows = load_rows()
    agg = aggregate(rows)
    scales = sorted({s for (_, s) in agg.keys()})

    plot_scaling(agg, scales)
    plot_throughput(agg, scales)
    plot_breakdown(agg, scales)

    print("wrote benchmarks/scaling.png, throughput.png, breakdown.png", file=sys.stderr)

    # Also print a plain-text summary table for REPORT.md.
    print(f"\n{'impl':<14}{'scale':>10}{'read_ms':>10}{'process_ms':>12}{'write_ms':>10}{'total_ms':>10}{'Mops/s':>9}")
    for impl in ("strset", "strset_naive"):
        for s in scales:
            m = agg[(impl, s)]
            mops = s / m["total_ms"] / 1000.0
            print(f"{impl:<14}{s:>10,}{m['read_ms']:>10.3f}{m['process_ms']:>12.3f}"
                  f"{m['write_ms']:>10.3f}{m['total_ms']:>10.3f}{mops:>9.3f}")


if __name__ == "__main__":
    main()
