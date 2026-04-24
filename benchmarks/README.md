# MEngine Benchmark Suite

Benchmarks comparing MEngine, Rocq (Coq), and Lean on proof-engine tasks.

## Quick Start

```bash
# 1. Edit config.json with your engine paths
vim config.json

# 2. List available benchmarks
python3 bench.py list

# 3. Run a specific benchmark (adaptive stopping handles the rest)
python3 bench.py run rewrite_fa

# 4. Generate plots
python3 bench.py plot rewrite_fa --format pdf
```

## Key Design: Adaptive Stopping

Unlike the old suite where you had to manually specify different parameter
ranges per engine (mengine: n=1..4000, coq: n=1..300), this suite uses
**one global parameter range** and automatically retires an engine after
it hits N consecutive timeouts. No manual intervention needed.

```bash
# The runner will stop coq early while mengine keeps going:
python3 bench.py run rewrite_fa --timeout 30

# Adjust the retirement threshold:
python3 bench.py run rewrite_fa --max-timeouts 5
```

## Commands

### `bench.py list`
Show all benchmarks with their parameters and strategies.

### `bench.py status [BENCHMARK]`
Show what results exist, broken down by engine.

### `bench.py run [BENCHMARK] [OPTIONS]`
Run benchmarks. Results are saved incrementally to `results/`.

Options:
- `--engine mengine,coq,lean` - restrict to specific engines
- `--override n=1:500:10` - override parameter range
- `--timeout 60` - per-run timeout in seconds
- `--max-timeouts 5` - consecutive timeouts before retiring a strategy
- `--force` - re-run even if results already exist

### `bench.py plot [BENCHMARK] [OPTIONS]`
Generate plots from results.

Options:
- `--engine mengine,coq` - only plot certain engines
- `--format pdf` - output format (png/pdf/svg)
- `--fixed m=3` - for multi-parameter benchmarks, fix a parameter
- `--xlim 0:1000`, `--ylim 0:10` - axis limits
- `--log-y`, `--log-x` - use log scale
- `--title "Custom Title"` - override title
- `--show-failures` - mark timeout/failed points on plot

## Benchmarks

| Name | Description | Engines |
|------|-------------|---------|
| `rewrite_fa` | Rewriting f(f(...f(a)))=a | mengine, coq, lean |
| `repeat_mod` | Nested modulo chain rewriting | mengine, coq, lean |
| `addr0_let_in` | Rewriting inside nested let-bindings | mengine, coq, lean |
| `rewrite_nm` | N-ary function + M-deep let-bindings | mengine, coq, lean |
| `separation_logic` | Sep logic predicate cancellation | mengine, coq |
| `symbolic_execution` | Imperative program verification | mengine, coq, lean |

### Multi-parameter benchmarks

`rewrite_nm` has two parameters (n, m). To reproduce the old suite's plots:

```bash
# Old: rewrite_nm_fixedm3 → fix m=3, vary n
python3 bench.py run rewrite_nm --override n=1:4000:25 --override m=3:4:1
python3 bench.py plot rewrite_nm --fixed m=3

# Old: rewrite_nm_fixedm5 → fix m=5, vary n
python3 bench.py run rewrite_nm --override n=1:4000:25 --override m=5:6:1
python3 bench.py plot rewrite_nm --fixed m=5

# Old: rewrite_nm_fixedn3 → fix n=3, vary m
python3 bench.py run rewrite_nm --override n=3:4:1 --override m=1:3000:25
python3 bench.py plot rewrite_nm --fixed n=3
```

## Paper Export

```bash
# Generate all plots as PDF for LaTeX inclusion:
python3 bench.py plot --format pdf

# Generate a specific plot with custom settings:
python3 bench.py plot rewrite_fa --format pdf --ylim 0:5 --title "Rewriting Performance"
```

## Adding a New Benchmark

1. Create `benchmarks/my_new_benchmark.py` with a concrete class extending `Benchmark`
2. That's it - the registry auto-discovers benchmark subclasses in `benchmarks/`, and the CLI and plotter pick them up automatically
