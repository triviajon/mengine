# mengine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.md)
[![test](https://github.com/triviajon/mengine/actions/workflows/test.yaml/badge.svg)](https://github.com/triviajon/mengine/actions/workflows/test.yaml)
[![examples](https://github.com/triviajon/mengine/actions/workflows/examples.yaml/badge.svg)](https://github.com/triviajon/mengine/actions/workflows/examples.yaml)

MEngine is a dependently typed kernel with explicit context management, a proof engine, a tactic language, and a theorem prover, optimized for highly automated proof scripts. 

The original ideas and design of MEngine were detailed in my master's thesis[^1], but have since been refined.

## How it works

MEngine represents proof terms as λ-DAGs [^2]: directed acyclic graphs where subterms are shared by reference rather than duplicated. Variable nodes are pointer-unique: one heap allocation per variable, so every reference to `x` is literally the same pointer. Applications, abstractions, and other compound nodes hold references to their children, and every node keeps  pointers its type, minimal context, and incoming references. All terms manipulated by a tactic live in the same DAG. Holes (e-vars/metavariables) are also first-class nodes. 

The intended workflow: write proofs in MEngine's scripting language (`.me` files) using built-in tactics (`exact`, `rewrite`, `induction`, `match_goal`, `apply`, etc.) or define new tactics directly in the language.

[^1]: https://dspace.mit.edu/handle/1721.1/162908
[^2]: https://link.springer.com/chapter/10.1007/978-3-540-31987-0_16

## Quick Start

```bash
# Install dependencies (if on macOS)
make install-tools
# Build CLI `mengine` and library `libmengine.a`
make
# Run examples
make examples
```

For development, it is helpful to generate a `.clangd`:
```bash
make clangd
```

**Usage:**

```bash
./mengine
./mengine path/to/script.me
./mengine --help  # for options
```

Check out `examples/` for scripts showing the syntax.

## Status

This is a prototype! My goals include:
- examples of verifying large imperative programs
- all proofs continue to generate Coq-checkable proof terms
- a tactic scripting language for writing new tactics

## Benchmark Results

<!-- BENCHMARK_RESULTS_START -->

| Benchmark | Description | Plot |
| --- | --- | --- |
| `addr0_let_in` | Rewriting add_r_O inside nested let-bindings | <img src="benchmarks/plots/addr0_let_in.png" alt="addr0_let_in" width="320"><br><sub>overview</sub> |
| `context_order_validity` | Deep-context valid_in_context queries for order-backend comparison | <img src="benchmarks/plots/context_order_validity.png" alt="context_order_validity" width="320"><br><sub>overview</sub> |
| `evar_free_filling` | Hole filling with a large evar-free proof term | <img src="benchmarks/plots/evar_free_filling.png" alt="evar_free_filling" width="320"><br><sub>overview</sub> |
| `repeat_mod` | Rewriting nested modulo chains (mod (mod ... b p) p) = (mod b p) | <img src="benchmarks/plots/repeat_mod.png" alt="repeat_mod" width="320"><br><sub>overview</sub> |
| `rewrite_fa` | Rewriting f(f(...f(a))) = a with n nested applications | <img src="benchmarks/plots/rewrite_fa.png" alt="rewrite_fa" width="320"><br><sub>overview</sub> |
| `rewrite_nm` | Rewriting f(x,...,x)=x in let x1:=f x0..x0 in ... let xm:=f x(m-1)..x(m-1) in xm=x0 | <img src="benchmarks/plots/rewrite_nm_m1.png" alt="rewrite_nm_m1" width="320"><br><sub>m1</sub><br><br><img src="benchmarks/plots/rewrite_nm_m2.png" alt="rewrite_nm_m2" width="320"><br><sub>m2</sub><br><br><img src="benchmarks/plots/rewrite_nm_m3.png" alt="rewrite_nm_m3" width="320"><br><sub>m3</sub><br><br><img src="benchmarks/plots/rewrite_nm_m4.png" alt="rewrite_nm_m4" width="320"><br><sub>m4</sub><br><br><img src="benchmarks/plots/rewrite_nm_m5.png" alt="rewrite_nm_m5" width="320"><br><sub>m5</sub><br><br><img src="benchmarks/plots/rewrite_nm_m6.png" alt="rewrite_nm_m6" width="320"><br><sub>m6</sub><br><br><img src="benchmarks/plots/rewrite_nm_m7.png" alt="rewrite_nm_m7" width="320"><br><sub>m7</sub><br><br><img src="benchmarks/plots/rewrite_nm_m8.png" alt="rewrite_nm_m8" width="320"><br><sub>m8</sub><br><br><img src="benchmarks/plots/rewrite_nm_m9.png" alt="rewrite_nm_m9" width="320"><br><sub>m9</sub><br><br><img src="benchmarks/plots/rewrite_nm_m10.png" alt="rewrite_nm_m10" width="320"><br><sub>m10</sub> |
| `separation_logic` | Separation logic predicate cancellation (reordering sep predicates) | _pending_ |
| `substitution_sharing` | Substitution through a dependent type with repeated references to one shared let-bound subtree | _pending_ |
| `symbolic_execution` | Symbolic execution of imperative programs with partial maps | _pending_ |

<!-- BENCHMARK_RESULTS_END -->
