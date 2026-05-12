# mengine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.md)
[![test](https://github.com/triviajon/mengine/actions/workflows/test.yaml/badge.svg)](https://github.com/triviajon/mengine/actions/workflows/test.yaml)
[![examples](https://github.com/triviajon/mengine/actions/workflows/examples.yaml/badge.svg)](https://github.com/triviajon/mengine/actions/workflows/examples.yaml)

MEngine is a dependently typed kernel with explicit context management, a proof engine, a tactic language, and a theorem prover, optimized for highly automated proof scripts. 

The original ideas of MEngine can be found in my master's thesis[^1], but since been refined.

## How it works

MEngine represents proof terms as λ-DAGs [^2]: directed acyclic graphs where subterms are shared by reference rather than duplicated. Variable nodes are pointer-unique: one heap allocation per variable, so every reference to `x` is literally the same pointer. Applications, abstractions, and other compound nodes hold references to their children, and every node keeps  pointers its type, minimal context, and incoming references. All terms manipulated by a tactic live in the same DAG. Holes (e-vars/metavariables) are also first-class nodes. 

The intended workflow: write proofs in MEngine's scripting language (`.me` files) using built-in tactics (`exact`, `rewrite`, `induction`, `match_goal`, `apply`, etc.) or define new tactics directly in the language.

[^1]: https://dspace.mit.edu/handle/1721.1/16290
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
