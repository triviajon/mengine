#!/usr/bin/env python3
"""
Exhaustive compile-time parameter tuner for mengine.

This script evaluates the cartesian product of a bounded parameter grid and
reports the fastest TUNE_FLAGS for symbolic execution at a fixed size.

Default scoring strategy:
- Repeat each benchmark point 5 times.
- Use the minimum observed time as the score (treating min as the best
    estimate of machine noise-free runtime).

Usage examples:
  python3 scripts/tune_params.py
    python3 scripts/tune_params.py --n 100
    python3 scripts/tune_params.py --n 100 --repeats 5
  python3 scripts/tune_params.py --profile tiny
  python3 scripts/tune_params.py --profile small --dry-run

Notes:
- The default profile is intentionally small so full cartesian search remains practical.
- We force rebuilds for each parameter point (make -B) to ensure correctness.
"""

import argparse
import itertools
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from typing import Dict, Iterable, List, Tuple


@dataclass(frozen=True)
class TuneParam:
    name: str
    kind: str  # "value" or "define"
    candidates: Tuple[object, ...]


# All currently supported tunable compile-time knobs.
# Keep this list aligned with code-level #ifndef defaults and Makefile docs.
ALL_PARAMS: Tuple[TuneParam, ...] = (
    TuneParam("HCMAP_INITIAL_CAPACITY", "value", (1024, 2048)),
    TuneParam("HCMAP_LOAD_FACTOR_NUM", "value", (7,)),
    TuneParam("HCMAP_LOAD_FACTOR_DEN", "value", (10,)),
    TuneParam("MAP_INITIAL_CAPACITY", "value", (16, 32)),
    TuneParam("MAP_LOAD_FACTOR_NUM", "value", (7, 8)),
    TuneParam("MAP_LOAD_FACTOR_DEN", "value", (10,)),
    TuneParam("MAP_HASH_SEED", "value", (0x6D656E67,)),
    TuneParam("MAX_RELATIONS", "value", (64, 128)),
    TuneParam("MAX_PATTERN_BINDINGS", "value", (64, 128)),
    TuneParam("TAC_CALL_MAX_DEPTH", "value", (1000,)),
    TuneParam("DISABLE_HASH_CONSING", "define", (False,)),
    TuneParam("DISABLE_REWRITE_CACHE", "define", (False,)),
)


PROFILE_OVERRIDES = {
    # Fast smoke profile.
    "tiny": {
        "HCMAP_INITIAL_CAPACITY": (1024,),
        "MAP_INITIAL_CAPACITY": (16,),
        "MAP_LOAD_FACTOR_NUM": (7,),
        "MAX_RELATIONS": (64,),
        "MAX_PATTERN_BINDINGS": (64,),
    },
    # Practical default: 32 combinations.
    "small": {
        "HCMAP_INITIAL_CAPACITY": (1024, 2048),
        "MAP_INITIAL_CAPACITY": (16, 32),
        "MAP_LOAD_FACTOR_NUM": (7, 8),
        "MAX_RELATIONS": (64, 128),
        "MAX_PATTERN_BINDINGS": (64, 128),
    },
}


def apply_profile(base: Iterable[TuneParam], profile: str) -> List[TuneParam]:
    overrides = PROFILE_OVERRIDES.get(profile, {})
    out = []
    for p in base:
        cand = overrides.get(p.name, p.candidates)
        out.append(TuneParam(p.name, p.kind, tuple(cand)))
    return out


def format_tune_flags(point: Dict[str, object], params: List[TuneParam]) -> str:
    parts: List[str] = []
    for p in params:
        val = point[p.name]
        if p.kind == "value":
            parts.append(f"-D{p.name}={val}")
        else:
            if bool(val):
                parts.append(f"-D{p.name}")
    return " ".join(parts)


def product_size(params: List[TuneParam]) -> int:
    total = 1
    for p in params:
        total *= len(p.candidates)
    return total


def render_symbolic_input(root: str, n: int) -> str:
    fd, out_path = tempfile.mkstemp(prefix="mengine_symexec_", suffix=f"_n{n}.me")
    os.close(fd)
    cmd = [
        sys.executable,
        os.path.join(root, "new-benchmarks", "bench.py"),
        "render",
        "symbolic_execution",
        "mengine",
        f"n={n}",
    ]
    with open(out_path, "w", encoding="utf-8") as f:
        proc = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"Failed to render symbolic_execution input: {proc.stderr.strip()}")
    return out_path


def build_mengine(root: str, tune_flags: str, jobs: int, dry_run: bool = False) -> bool:
    cmd = [
        "make",
        "-C",
        root,
        "-B",
        "mengine",
        f"TUNE_FLAGS={tune_flags}",
        "--no-print-directory",
        f"-j{jobs}",
    ]
    if dry_run:
        print("[dry-run]", " ".join(cmd))
        return True

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print("BUILD FAILED:", proc.stderr[-600:], file=sys.stderr)
        return False
    return True


def time_run(binary: str, input_file: str, timeout_s: float) -> float:
    t0 = time.perf_counter()
    proc = subprocess.run([binary, input_file], capture_output=True, timeout=timeout_s)
    t1 = time.perf_counter()
    if proc.returncode != 0:
        raise RuntimeError("mengine run failed")
    return t1 - t0


def evaluate_point(
    root: str,
    params: List[TuneParam],
    point: Dict[str, object],
    input_file: str,
    repeats: int,
    timeout_s: float,
    jobs: int,
    dry_run: bool,
) -> float:
    tune_flags = format_tune_flags(point, params)
    if not build_mengine(root, tune_flags, jobs=jobs, dry_run=dry_run):
        return float("inf")

    if dry_run:
        return 1.0

    binary = os.path.join(root, "mengine")
    times = []
    for _ in range(repeats):
        try:
            times.append(time_run(binary, input_file, timeout_s))
        except Exception:
            return float("inf")
    return min(times)


def iter_points(params: List[TuneParam]):
    names = [p.name for p in params]
    ranges = [p.candidates for p in params]
    for values in itertools.product(*ranges):
        yield dict(zip(names, values))


def main() -> int:
    parser = argparse.ArgumentParser(description="Exhaustive cartesian tuner for mengine compile-time parameters")
    parser.add_argument("--root", default=".", help="Path to mengine repo root (default: .)")
    parser.add_argument("--n", type=int, default=100, help="symbolic_execution size n (default: 100)")
    parser.add_argument(
        "--repeats",
        type=int,
        default=5,
        help="Timing repeats per point; minimum time is used as score (default: 5)",
    )
    parser.add_argument("--timeout", type=float, default=120.0, help="Per-run timeout seconds (default: 120)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="Parallel build jobs (default: nproc)")
    parser.add_argument("--profile", choices=["tiny", "small"], default="small", help="Candidate profile size")
    parser.add_argument("--max-combinations", type=int, default=256, help="Safety cap for cartesian size")
    parser.add_argument("--dry-run", action="store_true", help="Do not build/run; print planned search")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    params = apply_profile(ALL_PARAMS, args.profile)
    total = product_size(params)

    print(f"Root: {root}")
    print(f"Benchmark: symbolic_execution n={args.n}")
    print(f"Profile: {args.profile}")
    print(f"Cartesian points: {total}")

    if total > args.max_combinations:
        print(
            f"Refusing to run: {total} combinations exceeds --max-combinations={args.max_combinations}.",
            file=sys.stderr,
        )
        return 2

    for p in params:
        print(f"  {p.name}: {list(p.candidates)}")

    input_file = render_symbolic_input(root, args.n)

    best_score = float("inf")
    best_point: Dict[str, object] = {}
    leaderboard: List[Tuple[float, Dict[str, object]]] = []

    try:
        for idx, point in enumerate(iter_points(params), start=1):
            score = evaluate_point(
                root=root,
                params=params,
                point=point,
                input_file=input_file,
                repeats=args.repeats,
                timeout_s=args.timeout,
                jobs=args.jobs,
                dry_run=args.dry_run,
            )

            tune_flags = format_tune_flags(point, params)
            if score == float("inf"):
                print(f"[{idx:>3}/{total}] FAIL  {tune_flags}")
                continue

            print(f"[{idx:>3}/{total}] {score:8.4f}s  {tune_flags}")
            leaderboard.append((score, dict(point)))
            leaderboard.sort(key=lambda x: x[0])
            leaderboard = leaderboard[:10]

            if score < best_score:
                best_score = score
                best_point = dict(point)

        if best_score == float("inf"):
            print("No successful parameter point found.", file=sys.stderr)
            return 1

        best_flags = format_tune_flags(best_point, params)
        print("\n=== BEST ===")
        print(f"time: {best_score:.4f}s")
        print(f"TUNE_FLAGS=\"{best_flags}\"")

        print("\n=== TOP 10 ===")
        for rank, (score, point) in enumerate(leaderboard, start=1):
            print(f"{rank:>2}. {score:8.4f}s  {format_tune_flags(point, params)}")

        return 0
    finally:
        try:
            os.remove(input_file)
        except OSError:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
