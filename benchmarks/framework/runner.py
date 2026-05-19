"""
Adaptive benchmark runner.

Key feature: instead of requiring manually-tuned per-engine parameter ranges,
the runner sweeps the global parameter range and automatically stops running
an engine/strategy after it hits `max_consecutive_timeouts` consecutive
timeouts. This eliminates the need for manual intervention when engines have
vastly different performance characteristics.

Results are stored incrementally so runs can be resumed.
"""

import itertools
import json
import math
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import replace
from pathlib import Path

from .benchmark import Benchmark, Strategy, ParamSpec


_ADAPTIVE_WINDOW = 4
_ADAPTIVE_STEP_UP_R2   = 0.999
_ADAPTIVE_STEP_DOWN_R2 = 0.95
_ADAPTIVE_MAX_FACTOR = 8


def _log_linear_r2(recent: list[tuple[int, float]]) -> float:
    """R² of a log-log power-law fit over recent (x, time) pairs."""
    if len(recent) < 2:
        return 0.0
    try:
        lxs = [math.log(x) for x, _ in recent]
        lys = [math.log(t) for _, t in recent]
    except ValueError:
        return 0.0
    n = len(lxs)
    mx = sum(lxs) / n
    my = sum(lys) / n
    ss_xx = sum((lx - mx) ** 2 for lx in lxs)
    if ss_xx == 0:
        return 1.0
    ss_xy = sum((lx - mx) * (ly - my) for lx, ly in zip(lxs, lys))
    slope = ss_xy / ss_xx
    intercept = my - slope * mx
    ss_res = sum((ly - (slope * lx + intercept)) ** 2 for lx, ly in zip(lxs, lys))
    ss_tot = sum((ly - my) ** 2 for ly in lys)
    return 1.0 - ss_res / ss_tot if ss_tot > 0 else 1.0


def _adaptive_step(
    recent: list[tuple[int, float]],
    current_step: int,
    base_step: int,
) -> int:
    """Double step when recent points fit a power law well; halve only if they clearly deviate."""
    if len(recent) < _ADAPTIVE_WINDOW:
        return current_step
    r2 = _log_linear_r2(recent[-_ADAPTIVE_WINDOW:])
    if r2 >= _ADAPTIVE_STEP_UP_R2:
        return min(current_step * 2, base_step * _ADAPTIVE_MAX_FACTOR)
    if r2 < _ADAPTIVE_STEP_DOWN_R2:
        return max(current_step // 2, base_step)
    return current_step


_DEFAULT_VARIANT_STYLES = {
    "baseline":          {"color": "steelblue",   "marker": "o"},
    "no_evar_free_fill": {"color": "deepskyblue",  "marker": "^"},
    "order_ll":          {"color": "royalblue",   "marker": "D"},
    "order_demain":      {"color": "navy",        "marker": "v"},
}


def load_results(path: str) -> dict:
    if os.path.exists(path):
        with open(path, "r") as f:
            return json.load(f)
    return {}


def save_results(results: dict, path: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(results, f, indent=2)


class RunConfig:
    """Configuration for a benchmark run."""
    def __init__(
        self,
        mengine_path: str = "./mengine",
        coq_path: str = "coqc",
        lean_path: str = "lean",
        coqutil_root: str = "",
        mengine_root: str = "",
        results_dir: str = "results",
        default_timeout: float = 30.0,
        max_consecutive_timeouts: int = 3,
        max_consecutive_failures: int = 5,
        mengine_variants: dict | None = None,
    ):
        self.mengine_path = os.path.expanduser(mengine_path)
        self.coq_path = os.path.expanduser(coq_path)
        self.lean_path = os.path.expanduser(lean_path)
        self.coqutil_root = os.path.expanduser(coqutil_root)
        self.mengine_root = os.path.expanduser(mengine_root) if mengine_root else ""
        self.results_dir = results_dir
        self.default_timeout = default_timeout
        self.max_consecutive_timeouts = max_consecutive_timeouts
        self.max_consecutive_failures = max_consecutive_failures
        self.mengine_variants = {}
        for name, spec in (mengine_variants or {}).items():
            if isinstance(spec, str):
                path = spec
                root = self.mengine_root
                label = name
                color = None
                marker = None
            else:
                path = spec.get("path", self.mengine_path)
                root = spec.get("root", self.mengine_root)
                label = spec.get("label", name)
                color = spec.get("color")
                marker = spec.get("marker")
            style = _DEFAULT_VARIANT_STYLES.get(name, {})
            self.mengine_variants[name] = {
                "path": os.path.expanduser(path),
                "root": os.path.expanduser(root) if root else "",
                "label": label,
                "color": color or style.get("color"),
                "marker": marker or style.get("marker"),
            }

    def engine_path(self, engine: str, variant: str | None = None) -> str:
        if engine == "mengine" and variant:
            return self.mengine_variants[variant]["path"]
        return {"mengine": self.mengine_path, "coq": self.coq_path, "lean": self.lean_path}[engine]

    def engine_cwd(self, engine: str, variant: str | None = None, default: str | None = None) -> str:
        if engine != "mengine":
            return default or os.getcwd()
        if variant:
            root = self.mengine_variants[variant]["root"]
        else:
            root = self.mengine_root
        return root or (default or os.getcwd())

    def expand_strategies(self, strategies: list[Strategy]) -> list[Strategy]:
        if not self.mengine_variants:
            return strategies
        expanded = []
        for strategy in strategies:
            if strategy.engine != "mengine":
                expanded.append(strategy)
                continue
            for variant, spec in self.mengine_variants.items():
                expanded.append(
                    replace(
                        strategy,
                        name=f"{strategy.name}_{variant}",
                        label=f"{strategy.label} ({spec['label']})",
                        color=spec["color"] or strategy.color,
                        marker=spec["marker"] or strategy.marker,
                        variant=variant,
                    )
                )
        return expanded


def run_single(
    benchmark: Benchmark,
    strategy: Strategy,
    params: dict[str, int],
    config: RunConfig,
    timeout: float,
) -> dict:
    """
    Run a single benchmark point with soft/hard timeout limits.
    
    Soft timeout (at 'timeout'): counts as a failure toward auto-retire, but process continues.
    Hard timeout (at 2x 'timeout'): actually terminates the process.
    
    Returns {"time_taken": float, "success": bool, "soft_timeout": bool (optional)}.
    """
    with tempfile.TemporaryDirectory() as workdir:
        generated_file = benchmark.generate(strategy, params, workdir)
        engine_path = config.engine_path(strategy.engine, strategy.variant)
        cmd = benchmark.get_command(strategy, params, engine_path, generated_file, config=config)

        # mengine needs to run from its root dir to find prelude/tactics.me
        cwd = config.engine_cwd(strategy.engine, strategy.variant, workdir)

        soft_timeout = timeout
        hard_timeout = timeout * 2

        start = time.perf_counter()
        soft_timeout_hit = False
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=hard_timeout,
                cwd=cwd,
            )
            elapsed = time.perf_counter() - start
            
            # Check if we exceeded soft timeout but finished before hard timeout
            if elapsed > soft_timeout:
                soft_timeout_hit = True
            
            success = proc.returncode == 0
            result = {"time_taken": elapsed, "success": success}
            if soft_timeout_hit:
                result["soft_timeout"] = True
            if not success and proc.stderr:
                result["stderr"] = proc.stderr[:500]
            return result
        except subprocess.TimeoutExpired:
            elapsed = time.perf_counter() - start
            # Hard timeout hit - process was actually killed
            return {"time_taken": elapsed, "success": False, "timeout": True}
        except FileNotFoundError:
            return {"time_taken": 0, "success": False, "error": f"Engine not found: {cmd[0]}"}


def run_benchmark(
    benchmark: Benchmark,
    config: RunConfig,
    engines: list[str] | None = None,
    param_overrides: dict[str, ParamSpec] | None = None,
    timeout_override: float | None = None,
    force: bool = False,
    verbose: bool = True,
):
    """
    Run a benchmark with adaptive stopping.
    
    Args:
        benchmark: The benchmark to run.
        config: Run configuration (paths, timeouts, etc.).
        engines: Filter to these engines only (None = all).
        param_overrides: Override default parameter ranges.
        timeout_override: Override default timeout.
        force: Re-run even if results already exist.
        verbose: Print progress.
    """
    results_path = os.path.join(config.results_dir, f"{benchmark.name}.json")
    # Always load existing results so --force can selectively overwrite only
    # the keys being rerun, preserving other engines/strategies.
    results = load_results(results_path)

    timeout = timeout_override or config.default_timeout

    # Determine parameter ranges
    param_specs = list(benchmark.params)
    if param_overrides:
        param_specs = [
            param_overrides.get(p.name, p) for p in param_specs
        ]

    # Determine which strategies to run
    strategies = config.expand_strategies(benchmark.strategies)
    if engines:
        strategies = [s for s in strategies if s.engine in engines]

    if not strategies:
        print(f"No strategies to run for {benchmark.name}")
        return

    # Track consecutive timeouts/failures per strategy
    consecutive_timeouts: dict[str, int] = {}
    consecutive_failures: dict[str, int] = {}
    retired: set[str] = set()

    # Split into primary param (adaptive step) and secondary params (fixed grid)
    primary_spec = param_specs[0]
    secondary_specs = param_specs[1:]
    secondary_names = [p.name for p in secondary_specs]
    secondary_ranges = [list(p.range()) for p in secondary_specs]

    total_points = len(list(primary_spec.range()))
    for r in secondary_ranges:
        total_points *= len(r)

    if verbose:
        print(f"\n{'='*60}")
        print(f"  {benchmark.name}: {benchmark.description}")
        print(f"  ~{total_points} parameter points × {len(strategies)} strategies (adaptive step)")
        print(f"  Soft timeout: {timeout}s | Hard timeout: {timeout*2}s")
        print(f"  Auto-retire after {config.max_consecutive_timeouts} consecutive timeouts")
        print(f"{'='*60}")

    run_count = 0
    skip_count = 0

    for strategy in strategies:
        strat_id = f"{strategy.engine}:{strategy.name}"

        for secondary_vals in (itertools.product(*secondary_ranges) if secondary_ranges else [()]):
            if strat_id in retired:
                break

            recent_times: list[tuple[int, float]] = []
            current_step = primary_spec.step
            x = primary_spec.start

            while x < primary_spec.stop:
                if strat_id in retired:
                    break

                params: dict[str, int] = {primary_spec.name: x}
                if secondary_specs:
                    params.update(dict(zip(secondary_names, secondary_vals)))

                key = benchmark.result_key(strategy, params)

                if key in results and not force:
                    skip_count += 1
                    v = results[key]
                    if v.get("success") or v.get("soft_timeout"):
                        recent_times.append((x, v["time_taken"]))
                        if len(recent_times) > 8:
                            recent_times = recent_times[-8:]
                    current_step = _adaptive_step(recent_times, current_step, primary_spec.step)
                    x += current_step
                    continue

                result = run_single(benchmark, strategy, params, config, timeout)
                results[key] = result
                save_results(results, results_path)
                run_count += 1

                if result.get("timeout"):
                    consecutive_timeouts[strat_id] = consecutive_timeouts.get(strat_id, 0) + 1
                    consecutive_failures[strat_id] = 0
                    if verbose:
                        print(f"  TIMEOUT  {strat_id} {params} "
                              f"({consecutive_timeouts[strat_id]}/{config.max_consecutive_timeouts})")
                elif result.get("soft_timeout"):
                    consecutive_timeouts[strat_id] = consecutive_timeouts.get(strat_id, 0) + 1
                    consecutive_failures[strat_id] = 0
                    if verbose:
                        print(f"  SOFT_TO  {strat_id} {params} -> {result['time_taken']:.4f}s "
                              f"({consecutive_timeouts[strat_id]}/{config.max_consecutive_timeouts})")
                elif not result["success"]:
                    consecutive_failures[strat_id] = consecutive_failures.get(strat_id, 0) + 1
                    consecutive_timeouts[strat_id] = 0
                    if verbose:
                        err = result.get("error", result.get("stderr", "")[:80])
                        print(f"  FAIL     {strat_id} {params}: {err}")
                else:
                    consecutive_timeouts[strat_id] = 0
                    consecutive_failures[strat_id] = 0
                    if verbose:
                        print(f"  OK       {strat_id} {params} -> {result['time_taken']:.4f}s")

                if result.get("success") or result.get("soft_timeout"):
                    recent_times.append((x, result["time_taken"]))
                    if len(recent_times) > 8:
                        recent_times = recent_times[-8:]
                    new_step = _adaptive_step(recent_times, current_step, primary_spec.step)
                    if new_step != current_step and verbose:
                        print(f"  STEP     {strat_id} {primary_spec.name}: {current_step} → {new_step}")
                    current_step = new_step

                if consecutive_timeouts.get(strat_id, 0) >= config.max_consecutive_timeouts:
                    retired.add(strat_id)
                    if verbose:
                        print(f"  RETIRED  {strat_id} after {config.max_consecutive_timeouts} consecutive timeouts")

                if consecutive_failures.get(strat_id, 0) >= config.max_consecutive_failures:
                    retired.add(strat_id)
                    if verbose:
                        print(f"  RETIRED  {strat_id} after {config.max_consecutive_failures} consecutive failures")

                x += current_step

    if verbose:
        print(f"\nDone: {run_count} runs, {skip_count} skipped, {len(retired)} strategies retired")
        print(f"Results saved to {results_path}")
