"""
Adaptive benchmark runner.

The runner sweeps the global parameter range and automatically stops running
an engine/strategy after it hits `max_consecutive_timeouts` consecutive
timeouts. Results are stored incrementally so runs can be resumed.
"""

import itertools
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import replace
from pathlib import Path

from .benchmark import Benchmark, Strategy, ParamSpec


_ADAPTIVE_MAX_FACTOR = 16
_SMALL_GROWTH_PER_BASE_STEP = 0.04
_LARGE_GROWTH_PER_BASE_STEP = 0.20


def _adaptive_step(
    recent: list[tuple[int, float]],
    current_step: int,
    base_step: int,
    min_step: int,
    max_step: int,
) -> int:
    """Adjust by the local derivative instead of a global correlation fit."""
    if len(recent) < 2:
        return current_step

    (x0, t0), (x1, t1) = recent[-2], recent[-1]
    dx = x1 - x0
    if dx <= 0 or t1 <= 0:
        return current_step

    # Fractional time increase we would expect over one base step.  Small means
    # the curve is locally flat; large means the cliff is probably close.
    growth = max(0.0, (t1 - t0) / dx) * base_step / max(t1, 1e-9)
    if growth <= _SMALL_GROWTH_PER_BASE_STEP:
        return min(max(current_step * 2, base_step), max_step)
    if growth >= _LARGE_GROWTH_PER_BASE_STEP:
        return max(current_step // 2, min_step)
    return current_step


def _successful_time(result: dict) -> float | None:
    if result.get("success") or result.get("soft_timeout"):
        return result.get("time_taken")
    return None


def _result_is_complete(result: dict, target_trials: int) -> bool:
    """Return true if a stored result should be skipped unless --force is used."""
    if result.get("timeout") or result.get("error") or result.get("success") is False:
        return True
    if result.get("soft_timeout") or result.get("repeat_stopped"):
        return True
    return len(result.get("trials", [])) >= target_trials


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
        max_consecutive_timeouts: int = 2,
        max_consecutive_failures: int = 5,
        trials: int = 2,
        coq_timeout_multiplier: float = 1.5,
        repeat_trial_cutoff: float = 0.5,
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
        self.trials = max(1, int(trials))
        self.coq_timeout_multiplier = max(1.0, float(coq_timeout_multiplier))
        self.repeat_trial_cutoff = max(0.0, min(1.0, float(repeat_trial_cutoff)))
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
    Run a single benchmark point, repeating trials and keeping the minimum.
    
    Timeout means the whole process group is terminated.  Slow completed
    points are still flagged as soft timeouts for auto-retirement.
    
    Returns {"time_taken": min_success_time, "success": bool, "trials": [...]}.
    """
    timeout_multiplier = config.coq_timeout_multiplier if strategy.engine == "coq" else 1.0
    soft_timeout = timeout * timeout_multiplier
    hard_timeout = soft_timeout

    trials = []
    best_idx = None
    best_time = None
    repeat_stopped = None

    for trial_index in range(config.trials):
        with tempfile.TemporaryDirectory() as workdir:
            generated_file = benchmark.generate(strategy, params, workdir)
            engine_path = config.engine_path(strategy.engine, strategy.variant)
            cmd = benchmark.get_command(strategy, params, engine_path, generated_file, config=config)

            # mengine needs to run from its root dir to find prelude/tactics.me
            cwd = config.engine_cwd(strategy.engine, strategy.variant, workdir)

            start = time.perf_counter()
            try:
                proc = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    cwd=cwd,
                    start_new_session=True,
                )
                stdout, stderr = proc.communicate(timeout=hard_timeout)
                elapsed = time.perf_counter() - start
                trial = {"time_taken": elapsed, "success": proc.returncode == 0}
                if elapsed > soft_timeout:
                    trial["soft_timeout"] = True
                if not trial["success"]:
                    trial["returncode"] = proc.returncode
                    if stderr:
                        trial["stderr"] = stderr[:500]
                trials.append(trial)
            except subprocess.TimeoutExpired:
                elapsed = time.perf_counter() - start
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                    proc.communicate(timeout=1)
                except Exception:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except Exception:
                        pass
                trials.append({"time_taken": elapsed, "success": False, "timeout": True})
            except FileNotFoundError:
                trials.append({"time_taken": 0, "success": False, "error": f"Engine not found: {cmd[0]}"})
                break

        trial = trials[-1]
        if trial.get("success"):
            elapsed = trial["time_taken"]
            if best_time is None or elapsed < best_time:
                best_time = elapsed
                best_idx = len(trials) - 1

        if trial.get("timeout") or trial.get("error") or not trial.get("success"):
            repeat_stopped = "failed"
            break
        if trial.get("soft_timeout"):
            repeat_stopped = "soft_timeout"
            break
        if trial_index == 0 and trial.get("time_taken", 0) > soft_timeout * config.repeat_trial_cutoff:
            repeat_stopped = "slow_first_trial"
            break

    if best_idx is not None:
        best = trials[best_idx]
        result = {
            "time_taken": best["time_taken"],
            "success": True,
            "trials": trials,
            "best_trial": best_idx,
        }
        if best.get("soft_timeout"):
            result["soft_timeout"] = True
        if repeat_stopped:
            result["repeat_stopped"] = repeat_stopped
        return result

    first_error = next((t for t in trials if t.get("error")), None)
    slowest = max(trials, key=lambda t: t.get("time_taken", 0), default={"time_taken": 0})
    result = {
        "time_taken": slowest.get("time_taken", 0),
        "success": False,
        "trials": trials,
    }
    if any(t.get("timeout") for t in trials):
        result["timeout"] = True
    if repeat_stopped:
        result["repeat_stopped"] = repeat_stopped
    if first_error:
        result["error"] = first_error["error"]
    else:
        failed = next((t for t in trials if t.get("stderr")), None)
        if failed:
            result["stderr"] = failed["stderr"]
            result["returncode"] = failed.get("returncode")
    return result


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

    # Track consecutive timeouts/failures per plotted series.  Multi-parameter
    # benchmarks should not let one fixed-parameter slice retire the others.
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
        print(f"  Timeout: {timeout}s")
        if config.coq_timeout_multiplier != 1.0:
            print(f"  Rocq timeout: {timeout * config.coq_timeout_multiplier:g}s")
        print(f"  Trials per point: up to {config.trials}; slow/failed points stop after one trial")
        print(f"  Auto-retire after {config.max_consecutive_timeouts} consecutive timeouts")
        print(f"{'='*60}")

    run_count = 0
    skip_count = 0

    for strategy in strategies:
        strat_id = f"{strategy.engine}:{strategy.name}"

        for secondary_vals in (itertools.product(*secondary_ranges) if secondary_ranges else [()]):
            secondary_suffix = ""
            if secondary_specs:
                secondary_suffix = " " + " ".join(
                    f"{name}={value}" for name, value in zip(secondary_names, secondary_vals)
                )
            series_id = f"{strat_id}{secondary_suffix}"
            if series_id in retired:
                continue

            recent_times: list[tuple[int, float]] = []
            current_step = primary_spec.step
            min_step = max(1, primary_spec.step // 5) if strategy.engine == "coq" else primary_spec.step
            primary_span = max(primary_spec.stop - primary_spec.start, primary_spec.step)
            max_step = max(primary_spec.step, min(primary_spec.step * _ADAPTIVE_MAX_FACTOR, primary_span // 12 or primary_spec.step))
            x = primary_spec.start
            last_success_x: int | None = None

            while x < primary_spec.stop:
                if series_id in retired:
                    break

                params: dict[str, int] = {primary_spec.name: x}
                if secondary_specs:
                    params.update(dict(zip(secondary_names, secondary_vals)))

                key = benchmark.result_key(strategy, params)

                existing = results.get(key)
                if existing is not None and not force and _result_is_complete(existing, config.trials):
                    skip_count += 1
                    v = results[key]
                    if _successful_time(v) is not None:
                        recent_times.append((x, v["time_taken"]))
                        if len(recent_times) > 8:
                            recent_times = recent_times[-8:]
                        last_success_x = x
                        if v.get("soft_timeout"):
                            consecutive_timeouts[series_id] = consecutive_timeouts.get(series_id, 0) + 1
                            consecutive_failures[series_id] = 0
                        else:
                            consecutive_timeouts[series_id] = 0
                            consecutive_failures[series_id] = 0
                    elif v.get("timeout"):
                        consecutive_timeouts[series_id] = consecutive_timeouts.get(series_id, 0) + 1
                        consecutive_failures[series_id] = 0
                    else:
                        consecutive_failures[series_id] = consecutive_failures.get(series_id, 0) + 1
                        consecutive_timeouts[series_id] = 0

                    if consecutive_timeouts.get(series_id, 0) >= config.max_consecutive_timeouts:
                        retired.add(series_id)
                        if verbose:
                            print(f"  RETIRED  {strat_id} after {config.max_consecutive_timeouts} saved timeouts")
                        break

                    if consecutive_failures.get(series_id, 0) >= config.max_consecutive_failures:
                        retired.add(series_id)
                        if verbose:
                            print(f"  RETIRED  {strat_id} after {config.max_consecutive_failures} saved failures")
                        break

                    current_step = _adaptive_step(recent_times, current_step, primary_spec.step, min_step, max_step)
                    if _successful_time(v) is None and strategy.engine == "coq" and current_step > min_step and last_success_x is not None:
                        current_step = max(current_step // 2, min_step)
                        next_x = last_success_x + current_step
                        if next_x < x:
                            x = next_x
                            continue
                    x += current_step
                    continue

                result = run_single(benchmark, strategy, params, config, timeout)
                results[key] = result
                save_results(results, results_path)
                run_count += 1

                if result.get("timeout"):
                    consecutive_timeouts[series_id] = consecutive_timeouts.get(series_id, 0) + 1
                    consecutive_failures[series_id] = 0
                    if verbose:
                        print(f"  TIMEOUT  {strat_id} {params} "
                              f"({consecutive_timeouts[series_id]}/{config.max_consecutive_timeouts})")
                elif result.get("soft_timeout"):
                    consecutive_timeouts[series_id] = consecutive_timeouts.get(series_id, 0) + 1
                    consecutive_failures[series_id] = 0
                    if verbose:
                        print(f"  SOFT_TO  {strat_id} {params} -> {result['time_taken']:.4f}s "
                              f"({consecutive_timeouts[series_id]}/{config.max_consecutive_timeouts})")
                elif not result["success"]:
                    consecutive_failures[series_id] = consecutive_failures.get(series_id, 0) + 1
                    consecutive_timeouts[series_id] = 0
                    if verbose:
                        err = result.get("error", result.get("stderr", "")[:80])
                        print(f"  FAIL     {strat_id} {params}: {err}")
                else:
                    consecutive_timeouts[series_id] = 0
                    consecutive_failures[series_id] = 0
                    if verbose:
                        print(f"  OK       {strat_id} {params} -> {result['time_taken']:.4f}s")

                if _successful_time(result) is not None:
                    recent_times.append((x, result["time_taken"]))
                    if len(recent_times) > 8:
                        recent_times = recent_times[-8:]
                    last_success_x = x
                    new_step = _adaptive_step(recent_times, current_step, primary_spec.step, min_step, max_step)
                    current_step = new_step
                elif consecutive_timeouts.get(series_id, 0) >= config.max_consecutive_timeouts:
                    pass
                elif consecutive_failures.get(series_id, 0) >= config.max_consecutive_failures:
                    pass
                elif strategy.engine == "coq" and current_step > min_step and last_success_x is not None:
                    current_step = max(current_step // 2, min_step)
                    next_x = last_success_x + current_step
                    if next_x < x:
                        x = next_x
                        continue

                if consecutive_timeouts.get(series_id, 0) >= config.max_consecutive_timeouts:
                    retired.add(series_id)
                    if verbose:
                        print(f"  RETIRED  {strat_id} after {config.max_consecutive_timeouts} consecutive timeouts")

                if consecutive_failures.get(series_id, 0) >= config.max_consecutive_failures:
                    retired.add(series_id)
                    if verbose:
                        print(f"  RETIRED  {strat_id} after {config.max_consecutive_failures} consecutive failures")

                x += current_step

    if verbose:
        print(f"\nDone: {run_count} runs, {skip_count} skipped, {len(retired)} strategies retired")
        print(f"Results saved to {results_path}")
