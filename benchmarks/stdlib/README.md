# Rocq standard-library benchmark for MEngine

A per-module benchmark comparing **MEngine** against **Rocq** on a curated,
auto-translated subset of the Rocq standard library.  See [`PLAN.md`](PLAN.md)
for the full design and rationale; this README is the operator's guide.

## What it measures

Each *unit* is one stdlib **module** — a single file grouping the Tier-A lemmas
drawn from that module, mirroring the standard library's own file structure
(`coqc` compiles a `.v` file, not a lemma) rather than splitting each lemma into
its own file.  The corpus currently has four modules:

| Module  | Source                                  | Lemmas |
|---------|-----------------------------------------|--------|
| `Bool`  | `Coq.Bool.Bool` (ops from `Init.Datatypes`) | 18 |
| `Logic` | `Coq.Init.Logic` (eq, and/or, ex)       | 13 |
| `Nat`   | `Coq.Init.Nat` (ground + inductive arithmetic) | 13 |
| `Peano` | `Coq.Init.Peano` (the `le` order)       | 4  |

Computational induction over the `add`/`mul` fixpoints (`n + 0 = n`,
`n + m = m + n`, `(n+m)+p = n+(m+p)`, …) is now in Tier A: the kernel reduces a
fixpoint applied to a *symbolic* constructor-headed argument (`add (S n) m ↝
S (add n m)`) while leaving a stuck recursive call constant-headed, and `rewrite`
on a quantified induction hypothesis works.  The translator emits these as an
`apply (nat_ind <motive>)` with one focused subgoal per constructor.  Remaining
out-of-scope cases are listed under `excluded` in `corpus/manifest.json`
(multi-variable/nested case analysis, parametric `list` induction, and induction
over an inductive relation).

Each module has two forms:

- `corpus/<Module>/rocq.v` — the Rocq source (compiles against the installed
  stdlib; `Coq.Init` is auto-loaded, so the corpus needs no `Require`).
- `corpus/<Module>/mengine.me` — the MEngine source, produced **mechanically** by
  [`translate.py`](translate.py) with zero manual edits (Tier A).

Grouping by module also lifts the proof work above each engine's fixed startup
cost (see "Startup-subtracted times" below): a one-lemma file's proof is far
smaller than process startup, so its timing is pure startup noise; a whole
module's proofs are measurable.

### How statements are translated (`Set Printing All`)

MEngine has no notation system and no elaboration, so notation, implicit
arguments, and numeric literals in a Rocq statement must all be made explicit.
Rather than re-implement Rocq's elaborator (the old translator hand-desugared
notation and *synthesised* the implicit type argument of `=`, which failed for
any non-`nat`/`bool` equality and for polymorphic lists), `translate.py
--elaborate` replays the unit through Rocq with `Set Printing All` and a `Check`
per statement, and translates the **fully-explicit, notation-free** form it
prints back:

```
(* surface *)   forall (A:Type) (l:list A), nil ++ l = l
(* Printing All *)  forall (A : Type) (l : list A), @eq (list A) (@app A (@nil A) l) l
(* MEngine *)   forall (A : Type), forall (l : (list A)),
                  (((eq (list A)) (((app A) (nil A)) l)) l)
```

Because every implicit is already supplied, no type synthesis is needed: the
element type of an equality or a list comes straight from Rocq.  The only
qualified heads `Set Printing All` emits are the `Nat.*` arithmetic ops, mapped
to the compat-prelude names; any *other* qualified head is flagged, never
guessed.  This covers **definition and theorem statements**; terms appearing
inside *tactics* are still translated from the surface source (they are not
always elaborable), which is why the proof side keeps the surface rules below.

`stdlib_bench.py run` times each unit end-to-end in both engines (whole process,
best of N trials).  Both engines pay their own fixed startup: MEngine loads
`prelude/tactics.me` plus the compat prelude (~4 ms); Rocq starts its process and
loads its auto-loaded `Prelude` (~65 ms).  At this problem size the *whole-file*
number is almost entirely this per-invocation cost, not the proof.

**Startup-subtracted ("proof") times.**  To compare proof cost fairly, `run`
first times each engine's preamble *alone* — an empty `.v` (the corpus has no
`Require`, so this loads exactly the same `Prelude` every unit does) and the
compat prelude with no unit appended — and records it as a startup baseline.
`report` subtracts that floor from each unit's whole-file time (clamped at 0) to
get the marginal statement+proof cost, and reports a residual at or below the
baseline's own run-to-run jitter as `~0` (indistinguishable from startup).  For
the current Tier-A corpus every proof is trivial enough that its
startup-subtracted cost lands below the noise floor on both engines — so the
headline ~30× whole-file ratio is a *process-startup* ratio, and measuring proof
speed above the floor needs heavier units.

## Layout

```
benchmarks/stdlib/
  PLAN.md                  design document
  README.md                this file
  translate.py             Rocq .v -> MEngine .me translator (+ --report triage)
  stdlib_bench.py          corpus runner: list / test / run / report / manifest / triage
  report.py                markdown table + log-log scatter plot
  compat/stdlib_compat.me  compat prelude: nat/bool/list/option + le + emulated tactics
  corpus/
    manifest.json          locked Tier-A set + per-statement digests + excluded boundary
    <Module>/rocq.v        benchmarked Rocq source (one stdlib module per file)
    <Module>/mengine.me    auto-translated MEngine source
  results/stdlib.json      per-module timings + per-engine startup baselines
  results/REPORT.md        generated table + geometric-mean summary
  plots/stdlib_scatter.png generated scatter (Rocq x vs MEngine y, log-log)
```

## Usage

```bash
cd benchmarks
python3 stdlib/stdlib_bench.py list      # show the corpus by category
python3 stdlib/stdlib_bench.py test      # faithfulness gate (see below)
python3 stdlib/stdlib_bench.py run       # time both engines, write results
python3 stdlib/stdlib_bench.py report    # regenerate REPORT.md + scatter plot
python3 stdlib/stdlib_bench.py manifest  # regenerate corpus/manifest.json
python3 stdlib/stdlib_bench.py triage    # translate.py --report over the stdlib
```

Run a single unit: `… run le_0_n`, `… test bool_negb_involutive`.

## Faithfulness (`test`)

Before any timing, `test` verifies per unit (plan §7):

1. `rocq.v` compiles under `coqc`.
2. `mengine.me` runs clean under `mengine -q` (compat prelude prepended).
3. `mengine.me` is exactly what `translate.py --elaborate` re-emits from `rocq.v`
   (no drift), and the **theorem names match** between the two sides.  Because
   step 3 elaborates through Rocq (`Set Printing All`), the gate needs `coqc` on
   `PATH` (or `coq_path` in `config.json`).

The guiding principle of the translator is **flag, never guess**: any construct
it cannot translate soundly is reported and the unit is excluded, rather than
mistranslated.  A wrong translation that happened to compile would silently
benchmark two *different* theorems — the worst failure — so the translator
refuses instead.

## Scope (Tier A) and the engine boundary

Tier A is the computational/structural corner reachable today by MEngine + the
compat prelude:

- **Bool** — identities by ground reduction and by case analysis (`destruct`).
- **Nat (ground)** — closed `add`/`mul`/`sub` computations and reductions like
  `0 + n = n`.
- **`le` / order** — structural induction with a fixpoint-free motive, and
  constructor chains.
- **Logic** — propositional introduction (`split`/`left`/`right`/`apply`).
- **Polymorphic equality** — `eq_refl`/`eq_sym`/`eq_trans`/`f_equal`/`f_equal2`
  over an arbitrary type `A`, proved by `reflexivity`/`symmetry`/`rewrite`.
  These are exactly what the `Set Printing All` elaboration unlocked: the old
  surface translator had to *synthesise* the implicit type argument of `=` and
  gave up on any non-`nat`/`bool` equality, so a generic `forall (A:Type) (x:A),
  x = x` was untranslatable.  Now Rocq supplies the `T` of `@eq T x y` directly
  (and the `B` of `@eq B (f x) (f y)`), so these statements translate with no
  inference — and their proofs need no fixpoint reduction, so they prove today.
- **Polymorphic existentials** — `ex_intro` (`P x -> exists y, P y`) and
  `exists y, y = x`, proved by `exists`/`exact`/`reflexivity`.  Same unlock: the
  implicit type argument of `@ex A P` is now supplied.  Emitting these surfaced a
  translator bug — a binder-headed application argument (the predicate `fun y =>
  …` of `ex A (fun y => …)`) was printed without parentheses, which MEngine's
  parser rejects; `emit` now parenthesizes `fun`/`forall`/`arrow` arguments.

Several engine fixes on this branch enable the computational part (all are pure,
soundness-preserving kernel changes; all 431 kernel tests + every `examples/*.me`
still pass):

1. **`cbv` reduces applied fixpoints** (`src/kernel/normalize.c`).  `_normalize_cbv`
   now unfolds `(fix …) arg` like `normalize_whnf` already did, so `cbv`/`Eval`
   actually compute (e.g. `add (S O) O` → `S O`).
2. **GC shutdown no longer double-frees shared match branches**
   (`src/kernel/expression.c`).  `MatchBranch` arrays are shared by pointer
   across arena nodes (normalize/conversion rebuild a match with a new scrutinee
   but reuse its branches); shutdown now frees each exactly once.  This removes a
   crash that fired whenever a computational eliminator was type-checked
   (e.g. `destruct b` on `negb (negb b) = b`).
3. **Symbolic-fixpoint reduction leaves stuck calls constant-headed**
   (`src/kernel/fix_reduction.c`, `src/kernel/normalize.c`).  `cbv`/`whnf` hold a
   fixpoint *constant* folded until its decreasing argument is constructor-headed
   (`fix_reduce_app` unfolds and fires it), so `add (S n) m` reduces to
   `S (add n m)` while the stuck `add n m` stays headed by the `add` constant —
   which is what lets `rewrite`/congruence match a recursive call after `simpl`.
4. **`rewrite` works with a quantified induction hypothesis**
   (`src/engine/unify.c`, `src/engine/rewrite_internal.c`,
   `src/tacticlanguage/tactic_interp.c`, `src/runtime/core.c`).  An IH's stored
   type is the eliminator's beta-redex `(motive) n`; the rewrite path now
   weak-head-normalizes it before reading off / instantiating the equality, and
   `_get_lhs_eq` returns `NULL` (clean failure) instead of dereferencing a
   non-eq type.
5. **Applying a function whose type is a Pi only up to reduction**
   (`src/kernel/expression.c`).  `init_app_expression_wc` no longer pre-rejects a
   non-syntactic-`forall` function type; `_construct_app_type` already
   weak-head-normalizes, so a redex-typed IH (`(motive) n`) can be applied.
6. **The eliminator's index hole is recorded during `fill`**
   (`src/kernel/type_compat.c`).  With a quantified motive the induction index is
   left as an evar inside the term's type; `_open_compat`'s actual-side hole branch
   now records the assignment (symmetric to the expected side) so `fill_hole`
   cascade-fills it instead of leaving a stray open goal.

Now **in Tier A** (previously excluded, unblocked by the kernel work below):

- **Computational induction over a `Fixpoint`** (`add n 0 = n`, `n + m = m + n`,
  `(n+m)+p = n+(m+p)`, `mul` lemmas): the kernel now reduces a fixpoint applied to
  a *symbolic* constructor-headed argument (`add (S n) m ↝ S (add n m)`) while
  leaving a stuck recursive call headed by the `add`/`mul` *constant* (not a bare
  fix node), so `simpl` makes progress and `rewrite` matches it.  `rewrite` on a
  *quantified* induction hypothesis (whose type is the eliminator's beta-redex
  `(motive) n`) works too.  The old `bugs/segfault_*` reproducers run to
  completion, and `examples/computational_induction_rewrite.me` is the regression.
- **Single-variable `destruct`/`case` with a constant RHS** (`andb b false =
  false`): proved by `apply (bool_ind <motive>)` + per-case `simpl`/`reflexivity`.

What stays **out of Tier A** (documented in `corpus/manifest.json` → `excluded`):

- **Multi-variable / nested case analysis** (`andb_comm`, `orb b1 b2 = orb b2 b1`,
  de Morgan): the translator emits one `apply (<T>_ind motive)` and segments a
  single level of cases; a second `destruct` *inside* a case is not yet generated.
- **Polymorphic / parametric induction** (`app`/`length`/`rev` over `list A`): the
  *statement* translates faithfully (`Set Printing All` supplies the element type),
  but applying the generated parametric `list_ind` — whose motive ranges over the
  type parameter — currently fails to type-check the eliminator application.
- **Induction over an inductive relation** (`le_trans`, `le_n_S` via `le_ind`):
  the eliminator's motive is dependent on the derivation; the translator only
  builds non-dependent `fun (x:T) => <body>` motives.

## Why whole stdlib files don't translate

`stdlib_bench.py triage` runs the translator over the installed stdlib and
reports, per file, the first blocking construct.  Real files are saturated with
`Notation`/`Ltac`/`Variant`/`Register`/multi-scrutinee `match`/qualified names,
so essentially none translate as a whole file (≈1/16 even in `Coq.Init`).  This
is exactly the feasibility verdict in `PLAN.md §1`, and the reason the corpus is
built from curated lemmas drawn from stdlib content rather than from verbatim
stdlib files.  Those curated lemmas are then grouped one file per stdlib module
(`Bool`, `Logic`, `Nat`, `Peano`), so each benchmark file matches the library's
own file structure even though it holds only the Tier-A-provable subset of that
module's lemmas.
