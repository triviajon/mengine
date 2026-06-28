# Rocq standard-library benchmark for MEngine

A per-unit benchmark comparing **MEngine** against **Rocq** on a curated,
auto-translated subset of the Rocq standard library.  See [`PLAN.md`](PLAN.md)
for the full design and rationale; this README is the operator's guide.

## What it measures

Each *unit* is one stdlib-style lemma in two forms:

- `corpus/<unit>/rocq.v` — the Rocq source (compiles against the installed
  stdlib; `Coq.Init` is auto-loaded, so most units need no `Require`).
- `corpus/<unit>/mengine.me` — the MEngine source, produced **mechanically** by
  [`translate.py`](translate.py) with zero manual edits (Tier A).

`stdlib_bench.py run` times each unit end-to-end in both engines (whole process,
best of N trials).  Both engines pay their own fixed startup: MEngine loads
`prelude/tactics.me` plus the compat prelude; Rocq starts its process and loads
`Coq.Init`.  At this problem size the comparison reflects per-invocation cost,
not asymptotics.

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
    <unit>/rocq.v          benchmarked Rocq source
    <unit>/mengine.me      auto-translated MEngine source
  results/stdlib.json      per-unit timings
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
3. `mengine.me` is exactly what `translate.py` re-emits from `rocq.v` (no drift),
   and the **theorem names match** between the two sides.

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

Two engine fixes on this branch enable the computational part (both are pure,
soundness-preserving kernel changes; all 431 kernel tests still pass):

1. **`cbv` reduces applied fixpoints** (`src/kernel/normalize.c`).  `_normalize_cbv`
   now unfolds `(fix …) arg` like `normalize_whnf` already did, so `cbv`/`Eval`
   actually compute (e.g. `add (S O) O` → `S O`).
2. **GC shutdown no longer double-frees shared match branches**
   (`src/kernel/expression.c`).  `MatchBranch` arrays are shared by pointer
   across arena nodes (normalize/conversion rebuild a match with a new scrutinee
   but reuse its branches); shutdown now frees each exactly once.  This removes a
   crash that fired whenever a computational eliminator was type-checked
   (e.g. `destruct b` on `negb (negb b) = b`).

What stays **out of Tier A** (documented in `corpus/manifest.json` → `excluded`):

- **Computational induction over a `Fixpoint`** (`add n 0 = n`, `n + 0 = n`):
  needs symbolic-argument fixpoint conversion, which hits a pre-existing kernel
  stack-overflow (`bugs/segfault_apply_fixpoint_motive.me`,
  `bugs/segfault_exact_eliminator_fixpoint.me`).  This is the highest-leverage
  remaining expansion (plan §5a, §11) and is genuinely hard kernel work.
- **`destruct`/`case` with a constant RHS** (`andb b false = false`): MEngine's
  first-order `apply` cannot solve the eliminator's scrutinee evar when the
  scrutinee variable has no *bare* occurrence in the goal, because the unifier
  whnf-folds the function application into a stuck `match`.
- **Polymorphic list reasoning** (`app`/`length` lemmas): needs element-type
  inference in the translator (Tier B).

## Why whole stdlib files don't translate

`stdlib_bench.py triage` runs the translator over the installed stdlib and
reports, per file, the first blocking construct.  Real files are saturated with
`Notation`/`Ltac`/`Variant`/`Register`/multi-scrutinee `match`/qualified names,
so essentially none translate as a whole file (≈1/16 even in `Coq.Init`).  This
is exactly the feasibility verdict in `PLAN.md §1`, and the reason the corpus is
built from curated, single-lemma units drawn from stdlib content rather than from
whole files.
