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
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from .benchmark import Benchmark, Strategy, ParamSpec


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

    def engine_path(self, engine: str) -> str:
        return {
            "mengine": self.mengine_path,
            "coq": self.coq_path,
            "lean": self.lean_path,
        }[engine]


def run_single(
    benchmark: Benchmark,
    strategy: Strategy,
    params: dict[str, int],
    config: RunConfig,
    timeout: float,
) -> dict:
    """
    Run a single benchmark point. Returns {"time_taken": float, "success": bool}.
    """
    with tempfile.TemporaryDirectory() as workdir:
        generated_file = benchmark.generate(strategy, params, workdir)
        engine_path = config.engine_path(strategy.engine)
        cmd = benchmark.get_command(strategy, params, engine_path, generated_file, config=config)

        # mengine needs to run from its root dir to find prelude/tactics.me
        if strategy.engine == "mengine" and config.mengine_root:
            cwd = config.mengine_root
        else:
            cwd = workdir

        start = time.perf_counter()
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                cwd=cwd,
            )
            elapsed = time.perf_counter() - start
            success = proc.returncode == 0
            result = {"time_taken": elapsed, "success": success}
            if not success and proc.stderr:
                result["stderr"] = proc.stderr[:500]
            return result
        except subprocess.TimeoutExpired:
            elapsed = time.perf_counter() - start
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
    results = {} if force else load_results(results_path)

    timeout = timeout_override or config.default_timeout

    # Determine parameter ranges
    param_specs = list(benchmark.params)
    if param_overrides:
        param_specs = [
            param_overrides.get(p.name, p) for p in param_specs
        ]

    # Determine which strategies to run
    strategies = benchmark.strategies
    if engines:
        strategies = [s for s in strategies if s.engine in engines]

    if not strategies:
        print(f"No strategies to run for {benchmark.name}")
        return

    # Track consecutive timeouts/failures per strategy
    consecutive_timeouts: dict[str, int] = {}
    consecutive_failures: dict[str, int] = {}
    retired: set[str] = set()  # strategies that have been retired

    # Build sorted parameter grid
    ranges = [list(p.range()) for p in param_specs]
    param_names = [p.name for p in param_specs]

    total_points = 1
    for r in ranges:
        total_points *= len(r)

    if verbose:
        print(f"\n{'='*60}")
        print(f"  {benchmark.name}: {benchmark.description}")
        print(f"  {total_points} parameter points × {len(strategies)} strategies")
        print(f"  Timeout: {timeout}s | Auto-retire after {config.max_consecutive_timeouts} consecutive timeouts")
        print(f"{'='*60}")

    run_count = 0
    skip_count = 0

    for param_values in itertools.product(*ranges):
        params = dict(zip(param_names, param_values))

        for strategy in strategies:
            strat_id = f"{strategy.engine}:{strategy.name}"

            if strat_id in retired:
                continue

            key = benchmark.result_key(strategy, params)

            # Skip if already have results (unless force)
            if key in results and not force:
                skip_count += 1
                continue

            # Run it
            result = run_single(benchmark, strategy, params, config, timeout)
            results[key] = result
            save_results(results, results_path)
            run_count += 1

            # Track timeouts/failures for adaptive stopping
            if result.get("timeout"):
                consecutive_timeouts[strat_id] = consecutive_timeouts.get(strat_id, 0) + 1
                consecutive_failures[strat_id] = 0
                if verbose:
                    print(f"  TIMEOUT  {strat_id} {params} "
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

            # Check retirement
            if consecutive_timeouts.get(strat_id, 0) >= config.max_consecutive_timeouts:
                retired.add(strat_id)
                if verbose:
                    print(f"  RETIRED  {strat_id} after {config.max_consecutive_timeouts} consecutive timeouts")

            if consecutive_failures.get(strat_id, 0) >= config.max_consecutive_failures:
                retired.add(strat_id)
                if verbose:
                    print(f"  RETIRED  {strat_id} after {config.max_consecutive_failures} consecutive failures")

    if verbose:
        print(f"\nDone: {run_count} runs, {skip_count} skipped, {len(retired)} strategies retired")
        print(f"Results saved to {results_path}")
