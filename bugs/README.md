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

## Not fixed here

Per the branch's scope, these crashes are documented, not fixed. Fixing them is
the "Tier 2" work: make iota/fix reduction fire on symbolic constructor-headed
arguments, and harden the eliminator application / conversion path against it.
