#!/usr/bin/env python3
"""Generate comprehensive comparison plots for four Python parsers."""

import pathlib
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import pandas as pd

OUT = pathlib.Path(__file__).parent
PARSERS = {
    "ruff":        {"file": OUT / "ruff.csv",        "label": "Ruff",        "color": "#7C3AED"},
    "zuban":       {"file": OUT / "zuban.csv",       "label": "Zuban",       "color": "#059669"},
    "rustpython":  {"file": OUT / "rustpython.csv",  "label": "RustPython",  "color": "#D97706"},
    "cpython":     {"file": OUT / "cpython.csv",     "label": "CPython (C)", "color": "#DC2626"},
}

STDLIB_ROOT = "/home/user/cpython/Lib/"


def load_data():
    frames = {}
    for key, info in PARSERS.items():
        df = pd.read_csv(info["file"])
        df["parser"] = info["label"]
        df["color"] = info["color"]
        df["time_ms"] = df["time_us"] / 1000.0
        df["kb"] = df["bytes"] / 1024.0
        # Extract top-level module name from path
        df["relpath"] = df["file"].str.replace(STDLIB_ROOT, "", regex=False)
        df["module"] = df["relpath"].apply(lambda p: p.split("/")[0].replace(".py", ""))
        frames[key] = df
    return frames


def save(fig, name):
    path = OUT / f"{name}.png"
    fig.savefig(path, dpi=180, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"  saved {path}")


# ── Plot 1: Overall bar chart ──────────────────────────────────────────────

def plot_overall_bars(frames):
    fig, axes = plt.subplots(1, 3, figsize=(16, 5))
    fig.suptitle("Overall Performance Comparison — CPython stdlib (1,867 files, 34.3 MB)",
                 fontsize=14, fontweight="bold", y=1.02)

    names, colors = [], []
    total_ms, throughput_mbs, throughput_lps = [], [], []
    for key in PARSERS:
        df = frames[key]
        names.append(PARSERS[key]["label"])
        colors.append(PARSERS[key]["color"])
        total_ms.append(df["time_ms"].sum())
        mb = df["bytes"].sum() / (1024 * 1024)
        secs = df["time_ms"].sum() / 1000
        throughput_mbs.append(mb / secs)
        throughput_lps.append(df["lines"].sum() / secs)

    # Total time
    ax = axes[0]
    bars = ax.bar(names, total_ms, color=colors, edgecolor="white", linewidth=0.5)
    for bar, v in zip(bars, total_ms):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 20,
                f"{v:.0f} ms", ha="center", va="bottom", fontsize=9, fontweight="bold")
    ax.set_ylabel("Total parse time (ms)")
    ax.set_title("Total Time (lower is better)")
    ax.set_ylim(0, max(total_ms) * 1.18)

    # Throughput MB/s
    ax = axes[1]
    bars = ax.bar(names, throughput_mbs, color=colors, edgecolor="white", linewidth=0.5)
    for bar, v in zip(bars, throughput_mbs):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                f"{v:.1f}", ha="center", va="bottom", fontsize=11, fontweight="bold")
    ax.set_ylabel("MB/s")
    ax.set_title("Throughput (higher is better)")
    ax.set_ylim(0, max(throughput_mbs) * 1.18)

    # Throughput lines/s
    ax = axes[2]
    bars = ax.bar(names, [v/1000 for v in throughput_lps], color=colors, edgecolor="white", linewidth=0.5)
    for bar, v in zip(bars, throughput_lps):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height()/1000 + 10,
                f"{v/1000:.0f}K", ha="center", va="bottom", fontsize=11, fontweight="bold")
    ax.set_ylabel("Thousands of lines/s")
    ax.set_title("Line Throughput (higher is better)")
    ax.set_ylim(0, max(throughput_lps)/1000 * 1.18)

    for ax in axes:
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "01_overall_bars")


# ── Plot 2: Per-file time distribution (box + violin) ──────────────────────

def plot_time_distribution(frames):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle("Per-File Parse Time Distribution", fontsize=14, fontweight="bold", y=1.02)

    data_lists = []
    labels = []
    colors = []
    for key in PARSERS:
        df = frames[key]
        data_lists.append(df["time_ms"].values)
        labels.append(PARSERS[key]["label"])
        colors.append(PARSERS[key]["color"])

    # Box plot
    ax = axes[0]
    bp = ax.boxplot(data_lists, labels=labels, patch_artist=True, showfliers=False,
                    medianprops=dict(color="black", linewidth=1.5))
    for patch, c in zip(bp["boxes"], colors):
        patch.set_facecolor(c)
        patch.set_alpha(0.7)
    ax.set_ylabel("Parse time per file (ms)")
    ax.set_title("Box Plot (outliers hidden)")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # Histogram overlay
    ax = axes[1]
    for dlist, label, c in zip(data_lists, labels, colors):
        # Clip at 99th percentile for readability
        p99 = np.percentile(dlist, 99)
        clipped = dlist[dlist <= p99]
        ax.hist(clipped, bins=80, alpha=0.55, label=label, color=c, edgecolor="white", linewidth=0.3)
    ax.set_xlabel("Parse time per file (ms)")
    ax.set_ylabel("Number of files")
    ax.set_title("Histogram (clipped at 99th percentile)")
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "02_time_distribution")


# ── Plot 3: Time vs file size scatter ──────────────────────────────────────

def plot_time_vs_size(frames):
    fig, axes = plt.subplots(1, 4, figsize=(22, 5.5))
    fig.suptitle("Parse Time vs File Size", fontsize=14, fontweight="bold", y=1.02)

    for i, key in enumerate(PARSERS):
        ax = axes[i]
        df = frames[key]
        info = PARSERS[key]
        ax.scatter(df["kb"], df["time_ms"], s=8, alpha=0.45, color=info["color"],
                   edgecolors="none")
        # Linear fit
        mask = df["kb"] > 0
        z = np.polyfit(df.loc[mask, "kb"], df.loc[mask, "time_ms"], 1)
        xs = np.linspace(0, df["kb"].max(), 100)
        ax.plot(xs, np.polyval(z, xs), "--", color="black", alpha=0.6, linewidth=1.2,
                label=f"slope = {z[0]:.3f} ms/KB")
        ax.set_xlabel("File size (KB)")
        ax.set_ylabel("Parse time (ms)")
        ax.set_title(info["label"])
        ax.legend(fontsize=9)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "03_time_vs_filesize")


# ── Plot 4: Throughput vs file size (binned) ───────────────────────────────

def plot_throughput_by_size_bin(frames):
    fig, ax = plt.subplots(figsize=(13, 6))
    fig.suptitle("Throughput by File Size Bucket", fontsize=14, fontweight="bold", y=1.02)

    bins = [0, 1, 5, 10, 25, 50, 100, 500]
    bin_labels = ["<1 KB", "1-5 KB", "5-10 KB", "10-25 KB", "25-50 KB",
                  "50-100 KB", "100-500 KB"]

    x = np.arange(len(bin_labels))
    n_parsers = len(PARSERS)
    width = 0.8 / n_parsers

    all_throughputs = {}
    for j, key in enumerate(PARSERS):
        df = frames[key].copy()
        info = PARSERS[key]
        df = df[df["kb"] < 500]  # Exclude outlier bucket
        df["size_bin"] = pd.cut(df["kb"], bins=bins, labels=bin_labels, right=False)
        grouped = df.groupby("size_bin", observed=True)
        throughputs = []
        for lbl in bin_labels:
            if lbl in grouped.groups:
                g = grouped.get_group(lbl)
                mb = g["bytes"].sum() / (1024 * 1024)
                secs = g["time_ms"].sum() / 1000
                throughputs.append(mb / secs if secs > 0 else 0)
            else:
                throughputs.append(0)
        all_throughputs[key] = throughputs
        offset = (j - (n_parsers - 1) / 2) * width
        bars = ax.bar(x + offset, throughputs, width, label=info["label"],
                      color=info["color"], edgecolor="white", linewidth=0.5)
        for bar, v in zip(bars, throughputs):
            if v > 0:
                ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                        f"{v:.0f}", ha="center", va="bottom", fontsize=7, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(bin_labels, rotation=30, ha="right")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("How throughput varies by file size (files < 500 KB)")
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    save(fig, "04_throughput_by_size")


# ── Plot 5: Slowdown ratio per file ───────────────────────────────────────

def plot_slowdown_ratio(frames):
    others = [k for k in PARSERS if k != "ruff"]
    fig, axes = plt.subplots(1, len(others), figsize=(7 * len(others), 5.5))
    if len(others) == 1:
        axes = [axes]
    fig.suptitle("Slowdown Ratio Relative to Ruff (per file)", fontsize=14, fontweight="bold", y=1.02)

    ruff = frames["ruff"].set_index("file")

    for i, key in enumerate(others):
        ax = axes[i]
        info = PARSERS[key]
        other = frames[key].set_index("file")
        # Join on file path
        joined = ruff[["time_us", "kb", "lines"]].join(other[["time_us"]], rsuffix="_other")
        joined = joined.dropna()
        # Avoid division by zero
        joined = joined[joined["time_us"] > 0]
        joined["ratio"] = joined["time_us_other"] / joined["time_us"]

        ax.scatter(joined["kb"], joined["ratio"], s=8, alpha=0.4, color=info["color"],
                   edgecolors="none")
        median_ratio = joined["ratio"].median()
        ax.axhline(y=median_ratio, color="black", linestyle="--", linewidth=1.2,
                   label=f"median = {median_ratio:.2f}x")
        ax.axhline(y=1.0, color="gray", linestyle=":", linewidth=0.8, alpha=0.5)
        ax.set_xlabel("File size (KB)")
        ax.set_ylabel(f"{info['label']} / Ruff time ratio")
        ax.set_title(f"{info['label']} vs Ruff")
        ax.set_ylim(0, min(joined["ratio"].quantile(0.98) * 1.2, 30))
        ax.legend(fontsize=10)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "05_slowdown_ratio")


# ── Plot 6: Top-level module heatmap ──────────────────────────────────────

def plot_module_heatmap(frames):
    # Find the 20 modules with the most total bytes
    ruff = frames["ruff"]
    module_sizes = ruff.groupby("module")["bytes"].sum().sort_values(ascending=False)
    top_modules = module_sizes.head(20).index.tolist()

    fig, ax = plt.subplots(figsize=(10, 8))
    fig.suptitle("Parse Time by Top-Level Module (top 20 by size)", fontsize=13, fontweight="bold", y=1.01)

    matrix = []
    for key in PARSERS:
        df = frames[key]
        sub = df[df["module"].isin(top_modules)]
        row = sub.groupby("module")["time_ms"].sum().reindex(top_modules).fillna(0).values
        matrix.append(row)

    matrix = np.array(matrix)
    parser_labels = [PARSERS[k]["label"] for k in PARSERS]

    im = ax.imshow(matrix, aspect="auto", cmap="YlOrRd")
    ax.set_xticks(np.arange(len(top_modules)))
    ax.set_xticklabels(top_modules, rotation=55, ha="right", fontsize=8)
    ax.set_yticks(np.arange(len(parser_labels)))
    ax.set_yticklabels(parser_labels, fontsize=11)

    # Annotate cells
    for i in range(len(parser_labels)):
        for j in range(len(top_modules)):
            val = matrix[i, j]
            text_color = "white" if val > matrix.max() * 0.6 else "black"
            ax.text(j, i, f"{val:.0f}", ha="center", va="center", fontsize=7, color=text_color)

    cbar = fig.colorbar(im, ax=ax, shrink=0.6)
    cbar.set_label("Total parse time (ms)")
    fig.tight_layout()
    save(fig, "06_module_heatmap")


# ── Plot 7: CDF ───────────────────────────────────────────────────────────

def plot_cdf(frames):
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))
    fig.suptitle("Cumulative Distribution of Per-File Parse Times", fontsize=14, fontweight="bold", y=1.02)

    # Linear scale
    ax = axes[0]
    for key in PARSERS:
        df = frames[key]
        info = PARSERS[key]
        sorted_t = np.sort(df["time_ms"].values)
        cdf = np.arange(1, len(sorted_t)+1) / len(sorted_t)
        ax.plot(sorted_t, cdf, label=info["label"], color=info["color"], linewidth=1.5)
    ax.set_xlabel("Parse time (ms)")
    ax.set_ylabel("Fraction of files")
    ax.set_title("Linear scale")
    ax.set_xlim(0, frames["ruff"]["time_ms"].quantile(0.99) * 5)
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # Log scale
    ax = axes[1]
    for key in PARSERS:
        df = frames[key]
        info = PARSERS[key]
        sorted_t = np.sort(df["time_ms"].values)
        cdf = np.arange(1, len(sorted_t)+1) / len(sorted_t)
        ax.plot(sorted_t, cdf, label=info["label"], color=info["color"], linewidth=1.5)
    ax.set_xscale("log")
    ax.set_xlabel("Parse time (ms, log scale)")
    ax.set_ylabel("Fraction of files")
    ax.set_title("Log scale")
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "07_cdf")


# ── Plot 8: Time per line vs file complexity ──────────────────────────────

def plot_time_per_line(frames):
    n = len(PARSERS)
    fig, axes = plt.subplots(1, n, figsize=(5.5 * n, 5.5))
    fig.suptitle("Parsing Cost per Line vs File Size", fontsize=14, fontweight="bold", y=1.02)

    for i, key in enumerate(PARSERS):
        ax = axes[i]
        df = frames[key].copy()
        info = PARSERS[key]
        df = df[df["lines"] > 10]  # skip tiny files
        df["us_per_line"] = df["time_us"] / df["lines"]
        ax.scatter(df["lines"], df["us_per_line"], s=8, alpha=0.45,
                   color=info["color"], edgecolors="none")
        median_upl = df["us_per_line"].median()
        ax.axhline(y=median_upl, color="black", linestyle="--", linewidth=1.2,
                   label=f"median = {median_upl:.2f} us/line")
        ax.set_xlabel("File size (lines)")
        ax.set_ylabel("Microseconds per line")
        ax.set_title(info["label"])
        ax.set_ylim(0, df["us_per_line"].quantile(0.97) * 1.3)
        ax.legend(fontsize=9)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "08_time_per_line")


# ── Plot 9: Error comparison ──────────────────────────────────────────────

def plot_errors(frames):
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    fig.suptitle("Parse Error Comparison", fontsize=14, fontweight="bold", y=1.02)

    # Error count bar
    ax = axes[0]
    names, colors, err_counts, ok_counts = [], [], [], []
    for key in PARSERS:
        df = frames[key]
        info = PARSERS[key]
        names.append(info["label"])
        colors.append(info["color"])
        err_counts.append(df["has_error"].sum())
        ok_counts.append((~df["has_error"]).sum())

    bars = ax.bar(names, err_counts, color=colors, edgecolor="white", linewidth=0.5)
    for bar, v in zip(bars, err_counts):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.2,
                str(v), ha="center", va="bottom", fontsize=12, fontweight="bold")
    ax.set_ylabel("Files with parse errors")
    ax.set_title("Error Count")
    ax.set_ylim(0, max(err_counts) * 1.3)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # Venn-like: which files fail in which parsers
    ax = axes[1]
    error_sets = {}
    for key in PARSERS:
        df = frames[key]
        error_sets[PARSERS[key]["label"]] = set(df[df["has_error"]]["file"].values)

    all_error_files = sorted(set().union(*error_sets.values()))
    if all_error_files:
        matrix_data = []
        short_names = []
        for f in all_error_files:
            short = f.replace(STDLIB_ROOT, "")
            # Truncate long paths
            if len(short) > 45:
                short = "..." + short[-42:]
            short_names.append(short)
            row = []
            for key in PARSERS:
                row.append(1.0 if f in error_sets[PARSERS[key]["label"]] else 0.0)
            matrix_data.append(row)

        matrix_data = np.array(matrix_data)
        parser_labels = [PARSERS[k]["label"] for k in PARSERS]
        im = ax.imshow(matrix_data.T, aspect="auto", cmap="Reds", vmin=0, vmax=1)
        ax.set_xticks(np.arange(len(short_names)))
        ax.set_xticklabels(short_names, rotation=75, ha="right", fontsize=6)
        ax.set_yticks(np.arange(len(parser_labels)))
        ax.set_yticklabels(parser_labels, fontsize=10)
        ax.set_title("Which files fail in which parser")
        for i in range(len(parser_labels)):
            for j in range(len(short_names)):
                val = matrix_data[j, i]
                ax.text(j, i, "X" if val > 0.5 else "", ha="center", va="center",
                        fontsize=8, fontweight="bold", color="white")

    fig.tight_layout()
    save(fig, "09_errors")


# ── Plot 10: Relative speedup stacked area ────────────────────────────────

def plot_speedup_by_file_sorted(frames):
    fig, ax = plt.subplots(figsize=(14, 5.5))
    fig.suptitle("Per-File Speedup of Ruff over Other Parsers (files sorted by size)",
                 fontsize=13, fontweight="bold", y=1.02)

    ruff = frames["ruff"].sort_values("bytes").reset_index(drop=True)
    ruff_times = ruff["time_us"].values.astype(float)

    for key in [k for k in PARSERS if k != "ruff"]:
        other = frames[key].set_index("file")
        # Align by file order
        other_times = []
        for f in ruff["file"]:
            if f in other.index:
                other_times.append(float(other.loc[f, "time_us"]))
            else:
                other_times.append(np.nan)
        other_times = np.array(other_times)
        ratio = other_times / np.maximum(ruff_times, 1)
        # Smooth with rolling average for readability
        window = 30
        smoothed = pd.Series(ratio).rolling(window, center=True, min_periods=1).median()
        info = PARSERS[key]
        ax.plot(ruff["kb"].values, smoothed.values, label=info["label"],
                color=info["color"], linewidth=1.2, alpha=0.85)

    ax.axhline(y=1.0, color="gray", linestyle=":", linewidth=0.8)
    ax.set_xlabel("File size (KB, sorted)")
    ax.set_ylabel("Slowdown factor vs Ruff")
    ax.set_title("Rolling median (window=30) of per-file slowdown, sorted by file size")
    ax.legend()
    ax.set_ylim(0, 12)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    save(fig, "10_speedup_by_size")


# ── Plot 11: Cumulative time contribution ─────────────────────────────────

def plot_cumulative_time(frames):
    n = len(PARSERS)
    fig, axes = plt.subplots(1, n, figsize=(5.5 * n, 5))
    fig.suptitle("Cumulative Parse Time (files sorted large → small)",
                 fontsize=14, fontweight="bold", y=1.02)

    for i, key in enumerate(PARSERS):
        ax = axes[i]
        df = frames[key].sort_values("time_ms", ascending=False).reset_index(drop=True)
        info = PARSERS[key]
        cumsum = df["time_ms"].cumsum()
        total = cumsum.iloc[-1]
        pct = cumsum / total * 100
        ax.fill_between(range(len(pct)), pct, alpha=0.4, color=info["color"])
        ax.plot(pct, color=info["color"], linewidth=1.5)
        # Mark 80% line
        idx_80 = (pct >= 80).idxmax()
        ax.axvline(x=idx_80, color="black", linestyle="--", linewidth=0.8)
        ax.axhline(y=80, color="gray", linestyle=":", linewidth=0.5, alpha=0.5)
        ax.text(idx_80 + 20, 82, f"80% at file #{idx_80}\n({idx_80*100/len(pct):.0f}% of files)",
                fontsize=8, ha="left")
        ax.set_xlabel("File rank (sorted by parse time)")
        ax.set_ylabel("Cumulative % of total time")
        ax.set_title(f"{info['label']} — total {total:.0f} ms")
        ax.set_ylim(0, 105)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    save(fig, "11_cumulative_time")


# ── Plot 12: Bytes-per-microsecond density ─────────────────────────────────

def plot_efficiency_density(frames):
    fig, ax = plt.subplots(figsize=(10, 6))
    fig.suptitle("Parsing Efficiency Distribution (bytes processed per microsecond)",
                 fontsize=13, fontweight="bold", y=1.02)

    for key in PARSERS:
        df = frames[key].copy()
        info = PARSERS[key]
        df = df[df["time_us"] > 0]
        df["bytes_per_us"] = df["bytes"] / df["time_us"]
        p99 = df["bytes_per_us"].quantile(0.99)
        clipped = df["bytes_per_us"][df["bytes_per_us"] <= p99]
        ax.hist(clipped, bins=60, alpha=0.5, label=f'{info["label"]} (median={df["bytes_per_us"].median():.1f})',
                color=info["color"], edgecolor="white", linewidth=0.3)

    ax.set_xlabel("Bytes / microsecond")
    ax.set_ylabel("Number of files")
    ax.legend(fontsize=10)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    save(fig, "12_efficiency_density")


# ── Main ──────────────────────────────────────────────────────────────────

def main():
    print("Loading data...")
    frames = load_data()

    # Quick summary
    for key in PARSERS:
        df = frames[key]
        total_ms = df["time_ms"].sum()
        mb = df["bytes"].sum() / (1024 * 1024)
        secs = total_ms / 1000
        print(f"  {PARSERS[key]['label']:12s}: {total_ms:8.0f} ms total, "
              f"{mb/secs:.1f} MB/s, {df['has_error'].sum()} errors")

    print("\nGenerating plots...")
    plot_overall_bars(frames)
    plot_time_distribution(frames)
    plot_time_vs_size(frames)
    plot_throughput_by_size_bin(frames)
    plot_slowdown_ratio(frames)
    plot_module_heatmap(frames)
    plot_cdf(frames)
    plot_time_per_line(frames)
    plot_errors(frames)
    plot_speedup_by_file_sorted(frames)
    plot_cumulative_time(frames)
    plot_efficiency_density(frames)

    print("\nDone! All plots saved to", OUT)


if __name__ == "__main__":
    main()
