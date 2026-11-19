#!/usr/bin/env python3
"""Plot memory-investigation results for the in-memory and block-based SSTables."""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter, FixedLocator


ROOT = Path(__file__).resolve().parent.parent
RESULTS = ROOT / "bench" / "results"
DATASETS = {
    "In-memory SSTable": RESULTS / "memory_investigation_results_in_memory.csv",
    "Block-based SSTable": RESULTS / "memory_investigation_results_block_based.csv",
}
COLORS = {
    "In-memory SSTable": "#D55E00",
    "Block-based SSTable": "#0072B2",
}


def read_csv(path: Path) -> list[dict[str, float]]:
    with path.open(newline="") as csv_file:
        return [
            {key: float(value) for key, value in row.items()}
            for row in csv.DictReader(csv_file)
        ]


def format_key_count(value: float, _position: int) -> str:
    return f"{int(value):,}"


def make_plot(
    rows_by_format: dict[str, list[dict[str, float]]],
    x_ticks: list[float],
    column: str,
    title: str,
    ylabel: str,
    filename: str,
    output_dir: Path,
) -> None:
    fig, ax = plt.subplots(figsize=(10.5, 6.2), layout="constrained")

    for label, rows in rows_by_format.items():
        rows.sort(key=lambda row: row["keys"])
        ax.plot(
            [row["keys"] for row in rows],
            [row[column] for row in rows],
            marker="o",
            markersize=6,
            linewidth=2.2,
            color=COLORS[label],
            label=label,
        )

    ax.set_xscale("log", base=2)
    ax.xaxis.set_major_locator(FixedLocator(x_ticks))
    ax.xaxis.set_major_formatter(FuncFormatter(format_key_count))
    ax.tick_params(axis="x", labelsize=9)
    ax.set_yscale("log")
    ax.set_xlabel("Number of keys (log₂ scale)")
    ax.set_ylabel(ylabel)
    ax.set_title(title, loc="left", fontsize=15, weight="bold", pad=14)
    ax.grid(True, which="major", color="#D7DDE3", linewidth=0.8)
    ax.grid(True, which="minor", axis="y", color="#E9EDF1", linewidth=0.45)
    ax.set_axisbelow(True)
    ax.legend(frameon=False, loc="upper left")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    fig.savefig(output_dir / f"{filename}.png", dpi=220, facecolor="white")
    fig.savefig(output_dir / f"{filename}.svg", facecolor="white")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=RESULTS / "plots" / "memory-investigation",
        help="directory for PNG and SVG plots",
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    rows_by_format = {label: read_csv(path) for label, path in DATASETS.items()}
    key_counts = sorted({int(row["keys"]) for rows in rows_by_format.values() for row in rows})
    axis_ticks = [key_counts[index] for index in (0, 2, 4, 6, 8, 10)]

    make_plot(
        rows_by_format,
        axis_ticks,
        "peak_rss_mb",
        "Sampled peak RSS vs dataset size",
        "Peak RSS (MiB, log scale)",
        "peak-rss-vs-keys",
        args.output_dir,
    )
    make_plot(
        rows_by_format,
        axis_ticks,
        "warm_read_latency_us",
        "Mean warm-read latency vs dataset size",
        "Mean GET latency (µs/op, log scale)",
        "mean-get-latency-vs-keys",
        args.output_dir,
    )

    print(f"Wrote plots to {args.output_dir}")


if __name__ == "__main__":
    main()
