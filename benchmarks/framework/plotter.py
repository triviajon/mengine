"""
Unified plotter for benchmark results.

Generates paper-quality plots from results JSON files.
Supports filtering engines/strategies, axis limits, log scale,
and multiple export formats (PNG, PDF, SVG).
"""

import json
import os
import re
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from .benchmark import Benchmark, Strategy


# Paper-quality defaults
PAPER_STYLE = {
    "font.size": 14,
    "axes.titlesize": 16,
    "axes.labelsize": 14,
    "legend.fontsize": 11,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "axes.grid": False,
    "font.family": "serif",
    "figure.figsize": (8, 6),
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
}


def load_results(path: str) -> dict:
    if not os.path.exists(path):
        return {}
    with open(path, "r") as f:
        return json.load(f)


def parse_result_key(key: str, benchmark: Benchmark) -> tuple[str | None, dict[str, int]]:
    """
    Parse a result key back into (strategy_id, params).
    
    Keys look like: "mengine_n100" or "coq_repeatrewrite_n100_m3"
    Returns (strategy_name_with_engine, {param_name: value})
    """
    # Identify which strategy this key belongs to
    for strategy in benchmark.strategies:
        # Build what the prefix should look like
        if strategy.engine == "mengine":
            prefix = "mengine_"
        else:
            prefix = f"{strategy.engine}_{strategy.name}_"
        
        if key.startswith(prefix):
            param_str = key[len(prefix):]
            params = {}
            for match in re.finditer(r"([a-zA-Z]+)(\d+)", param_str):
                params[match.group(1)] = int(match.group(2))
            return f"{strategy.engine}:{strategy.name}", params
    
    return None, {}


def plot_benchmark(
    benchmark: Benchmark,
    results_dir: str = "results",
    output_dir: str = "plots",
    engines: list[str] | None = None,
    strategies_filter: list[str] | None = None,
    fmt: str = "png",
    x_param: str | None = None,
    fixed_params: dict[str, int] | None = None,
    xlim: tuple[float, float] | None = None,
    ylim: tuple[float, float] | None = None,
    log_y: bool = False,
    log_x: bool = False,
    title: str | None = None,
    show_failures: bool = False,
):
    """
    Plot benchmark results.
    
    Args:
        benchmark: The benchmark definition.
        results_dir: Directory containing results JSON.
        output_dir: Directory to save plots.
        engines: Only plot these engines (None = all).
        strategies_filter: Only plot these strategy names (None = all).
        fmt: Output format (png, pdf, svg).
        x_param: Which parameter to use as x-axis (default: first param).
        fixed_params: Fix other parameters to these values (for multi-param benchmarks).
        xlim, ylim: Axis limits.
        log_y, log_x: Use log scale.
        title: Custom title (default: benchmark description).
        show_failures: Mark failed/timeout points on the plot.
    """
    plt.rcParams.update(PAPER_STYLE)

    results_path = os.path.join(results_dir, f"{benchmark.name}.json")
    results = load_results(results_path)

    if not results:
        print(f"No results found at {results_path}")
        return

    # Determine x-axis parameter
    if x_param is None:
        x_param = benchmark.params[0].name

    # Build strategy lookup
    strategy_map: dict[str, Strategy] = {}
    for s in benchmark.strategies:
        sid = f"{s.engine}:{s.name}"
        strategy_map[sid] = s

    # Collect data points per strategy
    data: dict[str, list[tuple[int, float]]] = {}
    failure_data: dict[str, list[tuple[int, float]]] = {}

    for key, value in results.items():
        strat_id, params = parse_result_key(key, benchmark)
        if strat_id is None:
            continue

        strategy = strategy_map.get(strat_id)
        if strategy is None:
            continue

        # Filter by engine
        if engines and strategy.engine not in engines:
            continue

        # Filter by strategy name
        if strategies_filter and strategy.name not in strategies_filter:
            continue

        # Filter by fixed params
        if fixed_params:
            skip = False
            for pk, pv in fixed_params.items():
                if pk in params and params[pk] != pv:
                    skip = True
                    break
            if skip:
                continue

        x_val = params.get(x_param)
        if x_val is None:
            continue

        # Include both successful results and soft-timeout results (which completed with a time)
        if value.get("success") or value.get("soft_timeout"):
            data.setdefault(strat_id, []).append((x_val, value["time_taken"]))
        elif show_failures:
            failure_data.setdefault(strat_id, []).append((x_val, value["time_taken"]))

    if not data:
        print(f"No successful data points to plot for {benchmark.name}")
        return

    # Plot
    fig, ax = plt.subplots()

    for strat_id, points in sorted(data.items()):
        strategy = strategy_map[strat_id]
        points.sort()
        xs, ys = zip(*points)
        ax.plot(
            xs, ys,
            label=strategy.label,
            color=strategy.color,
            marker=strategy.marker,
            alpha=0.8,
            linestyle=(0, (5, 5)),
            linewidth=1.5,
            markersize=5,
            zorder=2,
        )

    if show_failures:
        for strat_id, points in failure_data.items():
            strategy = strategy_map[strat_id]
            points.sort()
            xs, ys = zip(*points)
            ax.scatter(
                xs, ys,
                color=strategy.color,
                marker="X",
                s=60,
                alpha=0.5,
                zorder=1,
                label=f"{strategy.label} (failed)",
            )

    ax.set_xlabel(benchmark.x_label)
    ax.set_ylabel(benchmark.y_label)

    if title:
        ax.set_title(title)

    if xlim:
        ax.set_xlim(xlim)
    if ylim:
        ax.set_ylim(ylim)
    if log_y:
        ax.set_yscale("log")
    if log_x:
        ax.set_xscale("log")

    ax.legend()
    ax.grid(True, which="major", axis="both", linestyle="--", linewidth=0.5)
    fig.tight_layout()

    # Save
    os.makedirs(output_dir, exist_ok=True)

    # Build filename with fixed params if any
    suffix = ""
    if fixed_params:
        suffix = "_" + "_".join(f"{k}{v}" for k, v in sorted(fixed_params.items()))

    output_path = os.path.join(output_dir, f"{benchmark.name}{suffix}.{fmt}")
    fig.savefig(output_path)
    plt.close(fig)
    print(f"Plot saved to {output_path}")


def plot_all_variants(
    benchmark: Benchmark,
    results_dir: str = "results",
    output_dir: str = "plots",
    fmt: str = "png",
    **kwargs,
):
    """
    For multi-parameter benchmarks, generate one plot per combination
    of fixed parameters, varying the first parameter as x-axis.
    """
    if len(benchmark.params) <= 1:
        plot_benchmark(benchmark, results_dir, output_dir, fmt=fmt, **kwargs)
        return

    # For multi-param: vary first param, fix all others.
    # Find all values of the secondary params from results.
    results_path = os.path.join(results_dir, f"{benchmark.name}.json")
    results = load_results(results_path)
    if not results:
        print(f"No results found at {results_path}")
        return

    secondary_params = benchmark.params[1:]
    secondary_values: dict[str, set[int]] = {p.name: set() for p in secondary_params}

    for key in results:
        _, params = parse_result_key(key, benchmark)
        for p in secondary_params:
            if p.name in params:
                secondary_values[p.name].add(params[p.name])

    # Generate plots for each combination
    combos = list(itertools.product(*(sorted(secondary_values[p.name]) for p in secondary_params)))
    for combo in combos:
        fixed = dict(zip([p.name for p in secondary_params], combo))
        plot_benchmark(benchmark, results_dir, output_dir, fmt=fmt,
                      fixed_params=fixed, **kwargs)


# Need itertools for plot_all_variants
import itertools
