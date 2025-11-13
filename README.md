# Mengine

A prototype for a lambda-DAG based proof engine optimized for highly automated scripted proofs. Mengine addresses the performance challenges of existing type-theoretic proof assistants when handling large proof goals (e.g., from verifying imperative programs with thousands of lines). 

This prototype outputs Rocq (Coq) proof terms you can check with the standard proof checker.

## Building

You need `clang` and `make`.

```bash
make
# The binary will be created as `./main`.
```


## Running Examples

```bash
./main [OPTIONS] <example> [args...]
```

Options:
- `--proof=[0|1]` — output Coq-checkable proof (default: 1)
- `--withlet=[0|1]` — use let bindings in output (default: 1)  
- `--debug=[0|1]` — debug output (default: 0, currently does nothing)

There's no tactic language yet, so the examples are just C programs calling the kernel directly. Here's what's available:

**gfa** — nested function applications with axiom `f(a) = a`
```bash
./main gfa <f_length> <g_wrap>
# ./main gfa 2 1  rewrites  g(f(f(a))) -> g(a)
```

**haa** — nested binary applications with axiom `h(x, x) = x`
```bash
./main haa <h_depth>
# ./main haa 2  rewrites  h(h(h(a,a), h(a,a)), h(h(a,a), h(a,a))) -> a
```

**addr0** — exponentially-sized addition terms with axiom `add x 0 = x`
```bash
./main addr0 <native|letin|tree> <n_depth>
# Three variants: native (maximal sharing), letin (explicit lets), tree (no sharing)
# ./main addr0 native 2
```

**mod** — chained modulo with axiom `mod (mod a n) n = mod a n`
```bash
./main mod <n_depth>
# ./main mod 3  rewrites  mod (mod (mod b p) p) p -> mod b p
```

**lambda** — rewriting under lambda binders
```bash
./main lambda
```

**open** — test with open holes (uninstantiated evars)
```bash
./main open
```

**fillhole** — test hole instantiation
```bash
./main fillhole
```

**sym** — symbolic execution for imperative programs
```bash
./main sym <n>
# ./main sym 2  does symbolic exec with n*(a := a + a; a := a - x) loops
```

**sep1** — separation logic predicate reordering
```bash
./main sep1 <n>
# ./main sep1 5  solves  (P1 * P2 * P3 * P4 * P5) <-> (P5 * P4 * P3 * P2 * P1)
```

**nm** — scalability test with n-ary function and m let-bindings
```bash
./main nm <n> <m>
# ./main nm 5 10
```