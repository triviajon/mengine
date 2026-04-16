#!/usr/bin/env python3
"""
Compile-time parameter tuner for mengine.

Searches for optimal values of the performance constants in the mengine kernel
using coordinate descent with random restarts. After each restart the script
hill-climbs by cycling through parameters one at a time and picking the value
that minimises the benchmark time. It repeats until no parameter improves.

Usage
-----
    # From the mengine root directory:
    python3 scripts/tune_params.py

    # Override benchmark targets (default: all examples/*.me files):
    python3 scripts/tune_params.py --inputs examples/rewrite_expo.me examples/repeat_mod.me

    # Control search budget:
    python3 scripts/tune_params.py --restarts 5 --rounds 10

    # Dry-run: just print what would be built/run:
    python3 scripts/tune_params.py --dry-run

Output
------
Best parameters are printed as a TUNE_FLAGS string you can paste into make:
    make TUNE_FLAGS="-DHCMAP_INITIAL_CAPACITY=2048 -DMAP_LOAD_FACTOR_NUM=8"
"""

import argparse
import glob
import math
import os
import random
import subprocess
import sys
import time
from copy import deepcopy
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

# ---------------------------------------------------------------------------
# Parameter space
# Each entry: (flag_name, default, candidates)
# candidates is the ordered list of values to try during the search.
# Add new entries here as more constants are identified.
# ---------------------------------------------------------------------------

PARAMS = [
    # hashcons intern table
    ("HCMAP_INITIAL_CAPACITY", 1024, [256, 512, 1024, 2048, 4096, 8192]),
    ("HCMAP_LOAD_FACTOR_NUM",     7, [5, 6, 7, 8, 9]),
    # generic map
    ("MAP_INITIAL_CAPACITY",      16, [8, 16, 32, 64, 128]),
    ("MAP_LOAD_FACTOR_NUM",        7, [5, 6, 7, 8, 9]),
    # rewrite / tactic limits — correctness constraints, but exposing for tuning
    ("MAX_RELATIONS",              64, [32, 64, 128, 256]),
    ("MAX_PATTERN_BINDINGS",       64, [32, 64, 128]),
    ("TAC_CALL_MAX_DEPTH",       1000, [500, 1000, 2000]),
]

# HCMAP_LOAD_FACTOR_DEN and MAP_LOAD_FACTOR_DEN are fixed at 10 so the ratio
# NUM/DEN directly represents the target occupancy (e.g. 7/10 = 70 %).
# MAP_HASH_SEED affects hash distribution in subtle ways; random search over
# it is rarely productive so it is left as a separate manual exercise.


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def make_tune_flags(params_dict: dict) -> str:
    return " ".join(f"-D{k}={v}" for k, v in params_dict.items())


def build(tune_flags: str, dry_run: bool = False, root: str = ".") -> bool:
    """(Re)build mengine with the given TUNE_FLAGS. Returns True on success."""
    cmd = ["make", "-C", root, "mengine", f"TUNE_FLAGS={tune_flags}", "--no-print-directory"]
    if dry_run:
        print(f"  [dry-run] {' '.join(cmd)}")
        return True
    result = subprocess.run(cmd, capture_output=True)
    if result.returncode != 0:
        print("  BUILD FAILED:", result.stderr.decode()[-400:], file=sys.stderr)
        return False
    return True


def run_benchmark(inputs: List[str], binary: str, dry_run: bool = False) -> Optional[float]:
    """
    Run the mengine binary on each input file and return the total wall time
    in seconds. Returns None if any run fails.
    """
    if dry_run:
        return random.uniform(0.1, 2.0)

    total = 0.0
    for inp in inputs:
        try:
            t0 = time.perf_counter()
            result = subprocess.run(
                [binary, inp],
                capture_output=True,
                timeout=60,
            )
            t1 = time.perf_counter()
            if result.returncode != 0:
                print(f"  RUNTIME ERROR on {inp}:\n{result.stderr.decode()[-300:]}", file=sys.stderr)
                return None
            total += t1 - t0
        except subprocess.TimeoutExpired:
            print(f"  TIMEOUT on {inp}", file=sys.stderr)
            return None
    return total


# ---------------------------------------------------------------------------
# Search: coordinate descent with random restarts
# ---------------------------------------------------------------------------

def default_point() -> dict:
    return {name: default for name, default, _ in PARAMS}


def random_point() -> dict:
    return {name: random.choice(candidates) for name, _, candidates in PARAMS}


def candidates_for(name: str) -> list:
    for n, _, c in PARAMS:
        if n == name:
            return c
    raise KeyError(name)


def evaluate(point: dict, inputs: List[str], binary: str,
             repeats: int, dry_run: bool) -> Optional[float]:
    tune_flags = make_tune_flags(point)
    if not build(tune_flags, dry_run=dry_run):
        return None
    times = []
    for _ in range(repeats):
        t = run_benchmark(inputs, binary, dry_run=dry_run)
        if t is None:
            return None
        times.append(t)
    # Use median to reduce noise
    times.sort()
    return times[len(times) // 2]


def coordinate_descent(start: dict, inputs: List[str], binary: str,
                        repeats: int, dry_run: bool, verbose: bool) -> Tuple[dict, float]:
    current = deepcopy(start)
    current_score = evaluate(current, inputs, binary, repeats, dry_run)
    if current_score is None:
        raise RuntimeError("Initial evaluation failed for starting point.")
    if verbose:
        print(f"  start score={current_score:.4f}s  flags={make_tune_flags(current)}")

    improved = True
    while improved:
        improved = False
        for name, _, _ in PARAMS:
            best_val = current[name]
            best_score = current_score
            for val in candidates_for(name):
                if val == current[name]:
                    continue
                candidate = deepcopy(current)
                candidate[name] = val
                score = evaluate(candidate, inputs, binary, repeats, dry_run)
                if score is None:
                    continue
                if verbose:
                    print(f"    {name}={val}  score={score:.4f}s", end="")
                if score < best_score:
                    best_score = score
                    best_val = val
                    if verbose:
                        print(" *", end="")
                if verbose:
                    print()
            if best_val != current[name]:
                current[name] = best_val
                current_score = best_score
                improved = True
                if verbose:
                    print(f"  -> improved {name}={best_val}  score={current_score:.4f}s")

    return current, current_score


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Tune mengine compile-time performance constants via coordinate descent.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--inputs", nargs="+", metavar="FILE",
        help="Input .me files to benchmark (default: all examples/*.me)",
    )
    parser.add_argument(
        "--restarts", type=int, default=3,
        help="Number of random restarts (default: 3). More restarts = better coverage.",
    )
    parser.add_argument(
        "--repeats", type=int, default=3,
        help="Number of timed repetitions per evaluation (median is taken, default: 3).",
    )
    parser.add_argument(
        "--root", default=".",
        help="Path to the mengine root directory (default: current directory).",
    )
    parser.add_argument(
        "--binary", default=None,
        help="Path to the mengine binary (default: <root>/mengine).",
    )
    parser.add_argument(
        "--seed", type=int, default=None,
        help="RNG seed for reproducibility.",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Print per-step scores.",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Simulate builds and benchmarks without actually running anything.",
    )
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    root = os.path.abspath(args.root)
    binary = args.binary or os.path.join(root, "mengine")

    if args.inputs:
        inputs = [os.path.abspath(p) for p in args.inputs]
    else:
        pattern = os.path.join(root, "examples", "*.me")
        inputs = sorted(glob.glob(pattern))
        if not inputs:
            sys.exit(f"No *.me files found under {root}/examples/. Use --inputs.")

    print(f"Benchmark inputs ({len(inputs)} files):")
    for f in inputs:
        print(f"  {f}")
    print(f"Restarts: {args.restarts}, Repeats per eval: {args.repeats}")
    print()

    global_best_point = default_point()
    global_best_score = math.inf

    # Always include the default as one of the starting points
    starting_points = [default_point()] + [random_point() for _ in range(args.restarts - 1)]

    for restart_idx, start in enumerate(starting_points):
        label = "defaults" if restart_idx == 0 else f"random restart {restart_idx}"
        print(f"=== Restart {restart_idx + 1}/{args.restarts} ({label}) ===")
        try:
            point, score = coordinate_descent(
                start, inputs, binary, args.repeats, args.dry_run, args.verbose
            )
        except RuntimeError as e:
            print(f"  Skipping: {e}", file=sys.stderr)
            continue

        print(f"  Local best: {score:.4f}s  flags={make_tune_flags(point)}")
        if score < global_best_score:
            global_best_score = score
            global_best_point = point
        print()

    print("=" * 60)
    print(f"GLOBAL BEST SCORE: {global_best_score:.4f}s")
    tune_flags_str = make_tune_flags(global_best_point)
    print(f"TUNE_FLAGS=\"{tune_flags_str}\"")
    print()
    print("To build mengine with these settings:")
    print(f"  make TUNE_FLAGS=\"{tune_flags_str}\"")
    print()
    print("To make permanent, add to Makefile or your environment:")
    print(f"  export TUNE_FLAGS=\"{tune_flags_str}\"")
    print()

    changes = {k: v for k, v in global_best_point.items()
               if v != dict(zip((n for n, _, _ in PARAMS), (d for _, d, _ in PARAMS)))[k]}
    if changes:
        print("Changed from defaults:", changes)
    else:
        print("No change from defaults — defaults are already optimal for this workload.")


if __name__ == "__main__":
    main()
