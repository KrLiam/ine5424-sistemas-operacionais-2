from argparse import ArgumentParser
import json
import glob
import os
from pathlib import Path
from typing import Any, Iterable, TypedDict
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

def parse_arguments():
    parser = ArgumentParser()

    parser.add_argument("--files", type=str, nargs="+")
    parser.add_argument("--x-column", type=str, default="elapsed_time")
    parser.add_argument("--x-label", type=str, required=False)
    parser.add_argument("--y-column", type=str, nargs="+", default=["total_out"])
    parser.add_argument("--y-scale", type=float, nargs="+", required=False)
    parser.add_argument("--y-label", type=str, required=False)
    parser.add_argument("--title", type=str, default="Benchmark result")
    parser.add_argument("--output", type=str, default="")

    args = parser.parse_args()

    if args.x_label is None:
        args.x_label = args.x_column
    if args.y_label is None:
        args.y_label = args.y_column[0] if len(args.y_column) == 1 else "Value"
    if args.y_scale is None:
        args.y_scale = [1] * len(args.y_column)
    
    return args


def read_files(values: str | list[str]) -> list[Any]:
    if isinstance(values, str):
        values = [values]

    results = []

    for value in values:
        files = glob.glob(value)

        for path in files:
            p = Path(path)

            if not p.exists():
                print(f"Result file '{p}' does not exist.")
                continue

            txt = p.read_text()
            results.append(json.loads(txt))
    
    return results

def get_axis(result: Any, column: str) -> list[Any]:
    snapshots = result["snapshots"]
    return [snapshot[column] for snapshot in snapshots]

def get_avg(axis: list[Any]) -> float:
    if not len(axis):
        return 0
    return sum(axis) / len(axis)


def plot(
    curves: list[tuple[list[float], list[float]]],
    curve_labels: list[str],
    x_label: str,
    y_label: str,
    output_path: str,
    title: str,
):
    fig, ax1 = plt.subplots(1, 1)
    fig.set_size_inches(20, 5)

    ax1.set_title(title)

    colors = ["r", "b", "g", "orange", "magenta", "black", "yellow"]

    for i, (x, y) in enumerate(curves):
        ax1.plot(x, y, colors[i % len(colors)], label=curve_labels[i])

    min_x = 0
    max_x = 1
    if curves:
        max_x = max(max(x) for x, _ in curves)
        min_x = min(min(x) for x, _ in curves)
    ax1.set_xbound(min_x, max_x)

    x_range = max_x - min_x
    tick_interval = max(x_range // 40, 2)
    ax1.xaxis.set_major_locator(ticker.MultipleLocator(tick_interval))

    ax1.set_xlabel(x_label)
    ax1.set_ylabel(y_label)
    ax1.legend()

    if not len(output_path):
        plt.show()
    else:
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(output_path, bbox_inches="tight")


def main():
    args = parse_arguments()

    results = read_files(args.files)

    if len(args.y_column) > 1:
        curve_labels = [
            f"{Path(p).stem.replace('_', ' ')} ({column})"
            for p in args.files
            for column in args.y_column
        ]
    else:
        curve_labels = [
            Path(p).stem.replace("_", " ") for p in args.files
        ]

    curves = []
    for result in results:
        x = get_axis(result, args.x_column)
        for i, y_column in enumerate(args.y_column):
            y = [
                yi / args.y_scale[i] for yi in get_axis(result, y_column)
            ]
            curves.append((x, y))
    
    plot(
        curves,
        curve_labels,
        args.x_label,
        args.y_label,
        args.output,
        args.title
    )

if __name__ == "__main__":
    main()