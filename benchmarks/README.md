# MEngine Benchmark Suite

Benchmarks comparing MEngine, Rocq, and Lean on proof-engine tasks.

## Quick Start

```bash
# Edit config.json with your engine paths
vim config.json

# Run a benchmark (stops each engine automatically after N consecutive timeouts)
python3 bench.py run rewrite_fa

# Generate plots
python3 bench.py plot rewrite_fa --format pdf
```

## Commands

```bash
python3 bench.py list                        # list all benchmarks
python3 bench.py status [BENCHMARK]          # show what results exist
python3 bench.py run [BENCHMARK] [OPTIONS]   # run benchmarks
python3 bench.py plot [BENCHMARK] [OPTIONS]  # generate plots
python3 bench.py test [BENCHMARK] [OPTIONS]  # smoke-test each strategy once
```

**Run options:** `--engine mengine,coq,lean` · `--override n=1:500:10` · `--timeout 60` · `--max-timeouts 5` · `--force`

**Plot options:** `--engine mengine,coq` · `--format png|pdf|svg` · `--fixed m=3` · `--xlim 0:1000` · `--ylim 0:10` · `--log-y` · `--log-x` · `--smooth N` · `--show-failures`

## Benchmarks

| Name | Description | Engines |
|------|-------------|---------|
| `rewrite_fa` | Rewriting f(f(...f(a)))=a | mengine, coq, lean |
| `repeat_mod` | Nested modulo chain rewriting | mengine, coq, lean |
| `addr0_let_in` | Rewriting inside nested let-bindings | mengine, coq, lean |
| `rewrite_nm` | N-ary function + M-deep let-bindings | mengine, coq, lean |
| `substitution_sharing` | Substitution through repeated references to one shared subtree | mengine |
| `evar_free_filling` | Hole filling with a large evar-free proof term | mengine |
| `context_order_validity` | Deep-context validity checks for order backend comparison | mengine |
| `separation_logic` | Sep logic predicate cancellation | mengine, coq |
| `symbolic_execution` | Imperative program verification | mengine, coq, lean |

`rewrite_nm` takes two parameters. Fix one to vary the other:

```bash
python3 bench.py run rewrite_nm --override n=1:4000:25 --override m=3:4:1
python3 bench.py plot rewrite_nm --fixed m=3
```

## Adding a Benchmark

Create `benchmarks/my_benchmark.py` with a class extending `Benchmark`. The registry, CLI, and plotter pick it up automatically.
