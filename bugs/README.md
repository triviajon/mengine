# Fixed: induction over computational goals (symbolic-fixpoint reduction)

The three examples here exercise induction over a goal stated with a `Fixpoint`.
Two of them (`segfault_apply_fixpoint_motive.me`, `segfault_exact_eliminator_fixpoint.me`)
used to crash the engine with SIGSEGV (exit 139); the third
(`note_clean_conversion_failure.me`) used to fail cleanly with a type error. **All
three now run to completion** (exit 0). They are kept as minimal reproducers and
regression notes.

All involve the same shape: an induction principle whose motive applies a fixpoint
(`add`) to a *symbolic* recursive argument, so type-checking the step case must
convert `add (S n) O` to `S (add n O)` with `n` a variable.

Run them with:

```bash
./build/mengine -q bugs/segfault_apply_fixpoint_motive.me      # exit 0
./build/mengine -q bugs/segfault_exact_eliminator_fixpoint.me  # exit 0
./build/mengine -q bugs/note_clean_conversion_failure.me       # exit 0
```

A complete worked proof of `forall n, add n O = n` by induction now lives in
`examples/computational_eliminator.me` as permanent regression coverage.

**Follow-on (stdlib-benchmark branch):** the reproducers here use `apply`/`exact`
of the eliminator with `f_equal`-style step cases.  The standard stdlib idiom —
`rewrite` on the induction hypothesis, including a *quantified* IH whose type is
the eliminator's beta-redex `(motive) n` — also works now; see
`examples/computational_induction_rewrite.me` (`add_succ_r`, `add_comm`) and the
`Nat` module of `benchmarks/stdlib/`.  The extra fixes that idiom needed are
listed in `benchmarks/stdlib/README.md` (symbolic-fixpoint reductions stay
constant-headed; the rewrite path whnf-normalizes a redex-typed IH; the
eliminator index hole is recorded during `fill_hole`).

## Root cause

The shared root cause was non-termination when a fixpoint is unfolded on a
*symbolic* (variable) recursive argument. Three issues conspired:

1. **Unconditional fix reduction.** `conversion_whnf` / `normalize_whnf` / `cbv`
   reduced a fixpoint application regardless of its decreasing argument. With `n`
   a variable, `add n O` unfolded to `match n with O => O | S p => S (add p O)`;
   `conversion_derivable` then recursed into the `S` branch (`add p O`), which
   unfolded again, forever — a stack overflow / SIGSEGV in the eliminator paths.

2. **A fixpoint constant was delta-reducible to its eta-expanded lambda form.**
   `Fixpoint add …` registered the recursive variable's body as
   `λn. λm. body`, so `add` unfolded unconditionally via delta+beta and a fix
   node never actually appeared during reduction — the guard in (3) had nothing to
   bite on.

3. **`fix_reduce` captured shared binders.** It substituted the fix node inline
   for the recursive variable, but the fix shares its argument binders with the
   body, so re-wrapping them in lambdas double-bound those variables.

## The fix

- `src/kernel/fix_reduction.c` adds `fix_reduce_app`, the guarded fix rule: an
  application whose head is a fix reduces only once its decreasing argument is in
  constructor head normal form. `conversion_whnf`, `normalize_whnf`, and `cbv`
  call it instead of reducing fixpoints unconditionally; a bare fix is now a
  value. So `add n O` (symbolic `n`) stays stuck and conversion compares it
  structurally — terminating.

- `src/kernel/expression.c` registers the **fix node itself** as the recursive
  variable's definitional body (not the eta-expanded lambda). Unfolding the
  constant now yields a fix node, so the guard governs it. Recursive calls inside
  the body resolve through the recursive variable (via delta), so `fix_reduce`
  no longer substitutes inline — it just strips the fix to its lambda abstraction,
  avoiding the binder capture in (3).

- `src/commandlanguage/command_exec.c` makes the `Definition` command accept a
  declared type that is *convertible* (not merely structurally congruent) to the
  inferred type, so computational types such as `add O O = O` are accepted.

All 431 kernel tests and every `examples/*.me` still pass.

## Previously fixed on this branch (related, but distinct)

Two adjacent crashes/limitations were fixed earlier to unblock the Rocq-stdlib
benchmark (`benchmarks/stdlib/`); they are independent of the symbolic-fixpoint
crash above:

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
