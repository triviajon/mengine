# mengine

a lambda-DAG based proof engine

the main idea: https://dspace.mit.edu/handle/1721.1/162908

## building

to build, you need `clang` and `make`. on macOS, you'll also need argp-standalone:

```bash
brew install argp-standalone
make
```

on linux, argp is part of glibc so just:

```bash
make
```

if you use clangd as your language server, you can also generate a `.clangd`:
```bash
make clangd
```

## usage

```bash
./mengine
./mengine path/to/script.me
./mengine --help  # for options
```

check out `examples/` for example scripts showing the syntax.

## status

this is a prototype! my goals include:
- examples of verifying large imperative programs
- all proofs continue to generate Coq-checkable proof terms
- a tactic scripting language for writing new tactics
