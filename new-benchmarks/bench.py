#!/usr/bin/env python3
"""
bench.py — Unified benchmark CLI for MEngine vs Rocq vs Lean.

Usage:
  bench.py list                           List all benchmarks
  bench.py status [BENCHMARK]             Show what results exist
  bench.py run [BENCHMARK] [OPTIONS]      Run benchmarks
  bench.py plot [BENCHMARK] [OPTIONS]     Generate plots
  bench.py render BENCHMARK ENGINE [PARAMS] Render a single instance to stdout

Run options:
  --engine ENGINE[,ENGINE,...]   Only run these engines (mengine, coq, lean)
  --override PARAM=START:STOP:STEP  Override a parameter range
  --timeout SECONDS              Override default timeout
  --max-timeouts N               Consecutive timeouts before retiring (default: 3)
  --force                        Re-run even if results exist

Plot options:
  --engine ENGINE[,ENGINE,...]   Only plot these engines
  --format FMT                   Output format: png, pdf, svg (default: png)
  --fixed PARAM=VALUE            Fix a parameter (for multi-param benchmarks)
  --xlim MIN:MAX                 Set x-axis limits
  --ylim MIN:MAX                 Set y-axis limits
  --log-y                        Use log scale on y-axis
  --log-x                        Use log scale on x-axis
  --title TITLE                  Custom plot title
  --show-failures                Mark failed/timeout points

Examples:
  # Run everything with adaptive stopping:
  bench.py run

  # Run just rewrite_fa for mengine:
  bench.py run rewrite_fa --engine mengine

  # Run rewrite_nm with specific parameter slice:
  bench.py run rewrite_nm --override n=1:4000:25 --override m=3:4:1

  # Plot rewrite_nm with m fixed to 3:
  bench.py plot rewrite_nm --fixed m=3 --format pdf

  # Plot everything for the paper:
  bench.py plot --format pdf

  # Render a single instance for testing:
  bench.py render symbolic_execution mengine n=5
  bench.py render rewrite_nm coq n=10 m=3
"""

import argparse
import json
import os
import sys
import tempfile

# Make imports work when running from the new-benchmarks directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

from benchmarks.registry import ALL_BENCHMARKS
from framework.benchmark import ParamSpec
from framework.runner import RunConfig, run_benchmark, load_results
from framework.plotter import plot_benchmark, plot_all_variants


DEFAULT_CONFIG_PATH = os.path.join(BASE_DIR, "config.json")

DEFAULT_CONFIG = {
    "mengine_path": "~/mengine/mengine",
    "mengine_root": "~/mengine",
    "coq_path": "coqc",
    "lean_path": "lean",
    "coqutil_root": "~/coqutil",
    "results_dir": "results",
    "plots_dir": "plots",
    "default_timeout": 30,
    "max_consecutive_timeouts": 3,
    "max_consecutive_failures": 5,
}

_PATH_KEYS = {"mengine_path", "mengine_root", "coqutil_root", "results_dir", "plots_dir"}

def load_config():
    cfg = dict(DEFAULT_CONFIG)
    if os.path.exists(DEFAULT_CONFIG_PATH):
        with open(DEFAULT_CONFIG_PATH) as f:
            loaded = json.load(f)
        cfg.update(loaded)

    # Normalize path-like config values once so all commands behave identically
    # no matter where bench.py is launched from.
    for key in _PATH_KEYS:
        value = cfg.get(key, "")
        if not value:
            continue
        if key == "mengine_path":
            cfg[key] = resolve_path_or_cmd(value)
        else:
            cfg[key] = resolve_path_from_base(value)

    return cfg


def resolve_path_from_base(path_str):
    """Resolve a potentially-relative path against this script's directory."""
    expanded = os.path.expanduser(path_str)
    if os.path.isabs(expanded):
        return expanded
    return os.path.normpath(os.path.join(BASE_DIR, expanded))


def resolve_path_or_cmd(path_or_cmd):
    """Keep bare executable names as-is; resolve path-like values from BASE_DIR."""
    if not path_or_cmd:
        return path_or_cmd
    if os.path.sep not in path_or_cmd and not path_or_cmd.startswith("~") and not path_or_cmd.startswith("."):
        return path_or_cmd
    return resolve_path_from_base(path_or_cmd)


def save_default_config():
    with open(DEFAULT_CONFIG_PATH, "w") as f:
        json.dump(DEFAULT_CONFIG, f, indent=2)
    print(f"Default config written to {DEFAULT_CONFIG_PATH}")
    print("Edit it with your engine paths before running benchmarks.")


def make_run_config(cfg, args):
    return RunConfig(
        mengine_path=cfg["mengine_path"],
        coq_path=cfg["coq_path"],
        lean_path=cfg["lean_path"],
        coqutil_root=cfg.get("coqutil_root", ""),
        mengine_root=cfg.get("mengine_root", ""),
        results_dir=cfg["results_dir"],
        default_timeout=args.timeout or cfg["default_timeout"],
        max_consecutive_timeouts=args.max_timeouts if hasattr(args, "max_timeouts") and args.max_timeouts else cfg["max_consecutive_timeouts"],
        max_consecutive_failures=cfg["max_consecutive_failures"],
    )


def parse_override(s):
    """Parse 'param=start:stop:step' into (param_name, ParamSpec)."""
    name, range_str = s.split("=", 1)
    parts = range_str.split(":")
    if len(parts) != 3:
        raise ValueError(f"Override must be PARAM=START:STOP:STEP, got '{s}'")
    return name, ParamSpec(name, int(parts[0]), int(parts[1]), int(parts[2]))


def parse_limit(s):
    """Parse 'min:max' into (float, float)."""
    parts = s.split(":")
    return (float(parts[0]), float(parts[1]))


def parse_fixed(s):
    """Parse 'param=value' into (str, int)."""
    name, val = s.split("=", 1)
    return name, int(val)


def cmd_list(args):
    print(f"\n{'Name':<25} {'Description'}")
    print(f"{'─'*25} {'─'*50}")
    for name, bench in ALL_BENCHMARKS.items():
        params_str = ", ".join(f"{p.name}=[{p.start}..{p.stop})" for p in bench.params)
        engines = sorted(set(s.engine for s in bench.strategies))
        print(f"{name:<25} {bench.description}")
        print(f"{'':25} params: {params_str}")
        print(f"{'':25} engines: {', '.join(engines)}")
        print(f"{'':25} strategies: {len(bench.strategies)}")
        print()


def cmd_status(args):
    cfg = load_config()
    benchmarks = get_benchmarks(args)
    results_dir = cfg["results_dir"]

    for name, bench in benchmarks.items():
        results_path = os.path.join(results_dir, f"{bench.name}.json")
        results = load_results(results_path)
        
        total = len(results)
        success = sum(1 for v in results.values() if v.get("success"))
        timeout = sum(1 for v in results.values() if v.get("timeout"))
        failed = total - success - timeout

        print(f"\n{name}:")
        if total == 0:
            print("  No results yet.")
        else:
            print(f"  {total} results: {success} success, {timeout} timeout, {failed} failed")

            # Show per-engine breakdown
            by_engine = {}
            for key, val in results.items():
                engine = key.split("_")[0]
                by_engine.setdefault(engine, {"success": 0, "timeout": 0, "failed": 0})
                if val.get("success"):
                    by_engine[engine]["success"] += 1
                elif val.get("timeout"):
                    by_engine[engine]["timeout"] += 1
                else:
                    by_engine[engine]["failed"] += 1

            for engine, counts in sorted(by_engine.items()):
                print(f"    {engine}: {counts['success']} ok, {counts['timeout']} timeout, {counts['failed']} fail")


def cmd_run(args):
    cfg = load_config()
    
    if not os.path.exists(DEFAULT_CONFIG_PATH):
        save_default_config()
        return

    run_cfg = make_run_config(cfg, args)
    benchmarks = get_benchmarks(args)
    engines = args.engine.split(",") if args.engine else None

    # Parse overrides
    param_overrides = {}
    if args.override:
        for ov in args.override:
            name, spec = parse_override(ov)
            param_overrides[name] = spec

    for name, bench in benchmarks.items():
        run_benchmark(
            bench,
            run_cfg,
            engines=engines,
            param_overrides=param_overrides if param_overrides else None,
            timeout_override=args.timeout,
            force=args.force,
        )


def cmd_plot(args):
    cfg = load_config()
    benchmarks = get_benchmarks(args)
    engines = args.engine.split(",") if args.engine else None
    results_dir = cfg["results_dir"]
    plots_dir = cfg.get("plots_dir", os.path.join(BASE_DIR, "plots"))
    
    fixed_params = {}
    if args.fixed:
        for fp in args.fixed:
            name, val = parse_fixed(fp)
            fixed_params[name] = val

    for name, bench in benchmarks.items():
        kwargs = {
            "results_dir": results_dir,
            "output_dir": plots_dir,
            "engines": engines,
            "fmt": args.format,
            "log_y": args.log_y,
            "log_x": args.log_x,
            "title": args.title,
            "show_failures": args.show_failures,
        }
        
        if args.xlim:
            kwargs["xlim"] = parse_limit(args.xlim)
        if args.ylim:
            kwargs["ylim"] = parse_limit(args.ylim)

        if fixed_params:
            kwargs["fixed_params"] = fixed_params
            plot_benchmark(bench, **kwargs)
        elif len(bench.params) > 1:
            # Multi-param benchmark: generate all variants
            plot_all_variants(bench, **kwargs)
        else:
            plot_benchmark(bench, **kwargs)


def cmd_render(args):
    name = args.benchmark
    if name not in ALL_BENCHMARKS:
        print(f"Unknown benchmark: {name}", file=sys.stderr)
        print(f"Available: {', '.join(ALL_BENCHMARKS.keys())}", file=sys.stderr)
        sys.exit(1)

    bench = ALL_BENCHMARKS[name]
    engine = args.engine

    # List engines if none given
    if engine is None:
        engines = sorted(set(s.engine for s in bench.strategies))
        print(f"Engines for '{name}': {', '.join(engines)}")
        for s in bench.strategies:
            print(f"  {s.engine:12s}  {s.name}")
        params_desc = ", ".join(f"{p.name}={p.start}" for p in bench.params)
        print(f"Params (defaults): {params_desc}")
        return

    # If args.strategy looks like a key=value param, treat it as the first param
    strategy_arg = args.strategy
    extra_params = list(args.params or [])
    if strategy_arg is not None and "=" in strategy_arg:
        extra_params.insert(0, strategy_arg)
        strategy_arg = None

    # Find matching strategy (first one for this engine, or by name)
    strategy = None
    for s in bench.strategies:
        if s.engine == engine:
            if strategy_arg is None or s.name == strategy_arg:
                strategy = s
                break
    if strategy is None:
        strats_for_engine = [s.name for s in bench.strategies if s.engine == engine]
        all_strats = [f"{s.engine}/{s.name}" for s in bench.strategies]
        if strats_for_engine:
            print(f"No strategy '{strategy_arg}' for engine '{engine}'", file=sys.stderr)
            print(f"Available for '{engine}': {', '.join(strats_for_engine)}", file=sys.stderr)
        else:
            print(f"No strategy for engine '{engine}'", file=sys.stderr)
            print(f"All available: {', '.join(all_strats)}", file=sys.stderr)
        sys.exit(1)

    # Parse params from positional key=value args
    params = {}
    for pv in extra_params:
        if "=" not in pv:
            print(f"Invalid param '{pv}': expected key=value format (e.g. n=5)", file=sys.stderr)
            sys.exit(1)
        k, v = pv.split("=", 1)
        params[k] = int(v)

    # Fill in defaults for missing params
    for p in bench.params:
        if p.name not in params:
            params[p.name] = p.start

    with tempfile.TemporaryDirectory() as tmpdir:
        path = bench.generate(strategy, params, tmpdir)
        with open(path) as f:
            print(f.read(), end="")


def cmd_init(args):
    save_default_config()


def get_benchmarks(args):
    if hasattr(args, "benchmark") and args.benchmark:
        name = args.benchmark
        if name not in ALL_BENCHMARKS:
            print(f"Unknown benchmark: {name}")
            print(f"Available: {', '.join(ALL_BENCHMARKS.keys())}")
            sys.exit(1)
        return {name: ALL_BENCHMARKS[name]}
    return ALL_BENCHMARKS


def main():
    parser = argparse.ArgumentParser(
        description="MEngine benchmark suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    subparsers = parser.add_subparsers(dest="command")

    # list
    subparsers.add_parser("list", help="List all benchmarks")

    # status
    p_status = subparsers.add_parser("status", help="Show benchmark status")
    p_status.add_argument("benchmark", nargs="?", help="Specific benchmark")

    # init
    subparsers.add_parser("init", help="Create default config.json")

    # render
    p_render = subparsers.add_parser("render", help="Render a single benchmark instance to stdout")
    p_render.add_argument("benchmark", help="Benchmark name")
    p_render.add_argument("engine", nargs="?", help="Engine (mengine, coq, lean)")
    p_render.add_argument("strategy", nargs="?", help="Strategy name (default: first for engine)")
    p_render.add_argument("params", nargs="*", help="Parameters as key=value (e.g. n=5 m=3)")

    # run
    p_run = subparsers.add_parser("run", help="Run benchmarks")
    p_run.add_argument("benchmark", nargs="?", help="Specific benchmark")
    p_run.add_argument("--engine", help="Engines to run (comma-separated)")
    p_run.add_argument("--override", action="append", help="Override param range: PARAM=START:STOP:STEP")
    p_run.add_argument("--timeout", type=float, help="Timeout in seconds")
    p_run.add_argument("--max-timeouts", type=int, help="Max consecutive timeouts before retiring")
    p_run.add_argument("--force", action="store_true", help="Re-run existing results")

    # plot
    p_plot = subparsers.add_parser("plot", help="Generate plots")
    p_plot.add_argument("benchmark", nargs="?", help="Specific benchmark")
    p_plot.add_argument("--engine", help="Engines to plot (comma-separated)")
    p_plot.add_argument("--format", default="png", help="Output format (png, pdf, svg)")
    p_plot.add_argument("--fixed", action="append", help="Fix parameter: PARAM=VALUE")
    p_plot.add_argument("--xlim", help="X-axis limits: MIN:MAX")
    p_plot.add_argument("--ylim", help="Y-axis limits: MIN:MAX")
    p_plot.add_argument("--log-y", action="store_true", help="Log scale y-axis")
    p_plot.add_argument("--log-x", action="store_true", help="Log scale x-axis")
    p_plot.add_argument("--title", help="Custom title")
    p_plot.add_argument("--show-failures", action="store_true", help="Show failed points")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        sys.exit(0)

    commands = {
        "list": cmd_list,
        "status": cmd_status,
        "run": cmd_run,
        "plot": cmd_plot,
        "init": cmd_init,
        "render": cmd_render,
    }
    commands[args.command](args)


if __name__ == "__main__":
    main()
