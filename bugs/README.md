# Known segfaults: induction over computational goals

These two minimal examples crash the engine (SIGSEGV). They are **pre-existing**
kernel issues, not caused by the induction-principle generator change on this
branch — that change only fixed the *type* of the generated `_ind`. They surface
now because a correct, IH-bearing induction principle can finally be applied to a
goal stated with a `Fixpoint`.

Both involve the same shape: an induction principle whose motive applies a
fixpoint (`add`) to a constructor-headed *symbolic* argument, so type-checking the
step case must reduce/convert `add (S n) O` with `n` a variable. (MEngine's
reduction is ground-only — `add (S n) O` does not reduce when `n` is symbolic; see
`bugs/note_clean_conversion_failure.me`, which fails *cleanly* with exit 1 rather
than crashing.) The crash is specific to the eliminator-*application* paths below.

Run them with:

```bash
./build/mengine -q bugs/segfault_apply_fixpoint_motive.me      # exit 139
./build/mengine -q bugs/segfault_exact_eliminator_fixpoint.me  # exit 139
```

## 1. `segfault_apply_fixpoint_motive.me` — the `apply` / unifier path

`apply (nat_ind <motive>)`, where the motive mentions the `add` fixpoint, crashes
while the tactic unifies the principle against the goal and builds the subgoals.
This is the direct route a real induction proof would take, so it currently blocks
proving e.g. `forall n, add n O = n` by induction.

## 2. `segfault_exact_eliminator_fixpoint.me` — the kernel type-check path

`Check` (equivalently `exact`) of a fully explicit eliminator proof term crashes
while type-checking the step case, where `add (S n) O` must be converted to
`S (add n O)` under the `n` binder. This isolates the crash to the kernel's
handling of the eliminator application itself: the bare conversion alone does not
crash (it fails cleanly), but wrapping it in the eliminator application does.

## Still not fixed here (the two reproducers above)

Both reproducers above still crash. Fixing them is the remaining "Tier 2" work:
make iota/fix reduction fire on symbolic constructor-headed arguments, and harden
the eliminator application / conversion path against it. The shared root cause is
non-termination in `conversion_whnf`/`conversion_derivable` when a fixpoint is
unfolded on a symbolic (variable) recursive argument (`is_fix_reducible` is
unconditional), so the standard guard — reduce a fix only when its decreasing
argument is in constructor head normal form — is the proper repair.

## Fixed on the stdlib-benchmark branch (related, but distinct)

Two adjacent crashes/limitations *were* fixed to unblock the Rocq-stdlib
benchmark (`benchmarks/stdlib/`); they are independent of the symbolic-fixpoint
crash above and all 431 kernel tests still pass:

1. **`cbv` did not reduce applied fixpoints.** `_normalize_cbv` only tried beta
   on an application spine, so `cbv`/`Eval` left `add (S O) O` stuck even though
   the conversion checker could reduce it. `src/kernel/normalize.c` now unfolds
   `(fix …) arg` in the APP case, mirroring `normalize_whnf`.

2. **GC double-free on *ground* computational eliminators.** Type-checking a
   fully ground eliminator whose motive needs reduction (e.g. `destruct b` on
   `negb (negb b) = b`) produced a correct proof term but crashed at shutdown:
   `MatchBranch` arrays are shared by pointer across arena nodes (normalize/
   conversion rebuild a match with a new scrutinee but reuse its branches), and
   `expression_gc_shutdown` freed them per-node. It now frees each shared
   allocation exactly once (`src/kernel/expression.c`). This is shutdown-only
   memory hygiene and cannot affect proof soundness.
