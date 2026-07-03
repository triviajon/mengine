# Rocq standard-library benchmark for MEngine

Per-module benchmark comparing **MEngine** against **Rocq** on a curated,
mechanically-translated subset of the Rocq standard library. Each unit is one
stdlib **module** — a `.v` file grouping that module's lemmas, mirroring the
library's own file structure — so the proof work rises above each engine's
fixed process-startup cost. Current corpus: 5 modules, 77 lemmas.

| Module  | Source                                              | Lemmas |
|---------|-----------------------------------------------------|--------|
| `Bool`  | `Coq.Bool.Bool`                                     | 22 |
| `Lists` | `Coq.Lists.List` (app/length/map/rev over `list A`) | 11 |
| `Logic` | `Coq.Init.Logic` (eq, and/or, ex)                   | 10 |
| `Nat`   | `Coq.Init.Nat` (inductive arithmetic, max/min)      | 31 |
| `Peano` | `Coq.Init.Peano` (the `le` order)                   | 3  |

Every lemma is a **named** stdlib lemma that lives in its module's file — the
corpus carries no bespoke facts. `fidelity` (below) enforces this.

## Layout

```
benchmarks/stdlib/
  translate.py             Rocq .v -> MEngine .me translator (via Rocq `Set Printing All`)
  stdlib_bench.py          runner: test / fidelity / clean / regen
  report.py                markdown table + log-log scatter plot
  fidelity.py              statement-vs-stdlib correspondence check (via Rocq's kernel)
  compat/stdlib_compat.me  compat prelude: nat/bool/list/option + pred/max/min/le + emulated tactics
  corpus/
    manifest.json          locked corpus + per-statement digests + excluded boundary
    stdlib_map.json         each curated lemma -> its real stdlib counterpart + per-module file(s)
    <Module>/rocq.v         benchmarked Rocq source (hand-curated)
    <Module>/mengine.me     auto-translated MEngine source
  results/                 generated: stdlib.json (timings) + REPORT.md (table)
  plots/stdlib_scatter.png generated scatter
```

## Usage

```bash
cd benchmarks
python3 stdlib/stdlib_bench.py regen     # rebuild every generated file, in dependency order
python3 stdlib/stdlib_bench.py clean     # remove every generated file (keep sources)
python3 stdlib/stdlib_bench.py test      # faithfulness gate (see below)
python3 stdlib/stdlib_bench.py fidelity  # check each statement vs the real stdlib
```

`regen` rebuilds every generated file from source in order (mengine.me ->
manifest -> run -> report); `clean` removes them. Hand-authored sources
(`rocq.v`, `stdlib_map.json`, the compat prelude) are never touched. `test` and
`fidelity` are the correctness gates, run by hand; the run/report/manifest steps
have no standalone command (they run inside `regen`).

## Translation (`Set Printing All`)

MEngine has no notation system and no elaboration, so notation, implicits, and
numeric literals must all be made explicit. Rather than re-implement Rocq's
elaborator, `translate.py` replays each unit through Rocq with `Set Printing All`
and translates the fully-explicit, notation-free form it prints (so a working
`coqc`/`rocq`, `--coq`, is required):

```
(* surface *)      forall (A:Type) (l:list A), nil ++ l = l
(* Printing All *) forall (A : Type) (l : list A), @eq (list A) (@app A (@nil A) l) l
(* MEngine *)      forall (A : Type), forall (l : (list A)), (((eq (list A)) (((app A) (nil A)) l)) l)
```

Every implicit is supplied by Rocq, so no type synthesis is needed. Terms inside
*tactics* are still translated from surface source. The guiding principle is
**flag, never guess**: any construct it cannot translate soundly is reported and
the unit excluded, never mistranslated (a wrong translation that happened to
compile would silently benchmark two *different* theorems).

## Timing

`run` times each unit end-to-end in both engines (whole process, best of N).
Both pay a fixed startup — MEngine ~4 ms (loading `prelude/tactics.me` + compat),
Rocq ~65 ms — which at this problem size dominates the whole-file number. To
isolate proof cost, `run` also times each engine's preamble *alone* as a startup
floor: for Rocq, **each module's own Require/Import preamble** (`Lists` requires
`Coq.Lists.List`, ~138 ms, so that one-time library load is subtracted, not
charged as proof). `report` subtracts each module's floor (clamped at 0); a
residual at or below the floor's run-to-run jitter is reported `~0`.

`plots/stdlib_scatter.png` plots each module's **own-floor-subtracted proof
time** (Rocq x vs MEngine y, log-log, parity `y = x`) — the same two numbers as
its REPORT.md row. Whole-file time would be dishonest: `Lists`' one-time `List`
load would drop it below any single parity line and read as an MEngine win, when
on proof cost MEngine is actually ~2× *slower* on `map`/`rev` induction. Whiskers
run to the slowest trial; shaded bands at each engine's startup-noise floor (the
std-dev of its baseline trials) mark where a residual stops being trustworthy.

## `test` — faithfulness gate

Per unit, before any timing:

1. `rocq.v` compiles under `coqc`.
2. `mengine.me` runs clean under `mengine -q` (compat prelude prepended).
3. `mengine.me` is exactly what `translate.py` re-emits from `rocq.v` (no drift),
   with matching theorem names.

Needs `coqc` on `PATH` (or `coq_path` in `config.json`).

## `fidelity` — statement vs the real stdlib

`test` checks that `.me` faithfully follows `.v`; it does **not** check that the
hand-curated `.v` statement matches the stdlib lemma it claims to be — the one
place a statement could silently drift. `fidelity` closes that gap with **Rocq's
own kernel**: `corpus/stdlib_map.json` records each module's source file(s) and
maps every lemma to a stdlib ref. The ref is qualified with the file (`andb_diag`
→ `Stdlib.Bool.Bool.andb_diag`) and checked by `Check (<file>.<ref> :
<curated statement>).`, which passes iff the lemma both belongs to that file and
is convertible to the curated one. An unmapped lemma, a stale map entry, a
missing/absent ref, or a non-convertible match all fail (exit 1). Because it
shells out to `coqc` (~6 s for the corpus) it is separate from `test`; run it
after editing any statement or the map.

## Scope

The computational/structural corner reachable by MEngine + the compat prelude:

- **Bool** — identities by ground reduction and single-variable `destruct`.
- **Nat** — `add`/`mul`/`sub` reductions *and* computational induction over the
  `add`/`mul` fixpoints (the full additive/multiplicative theory up to
  `mul_assoc` and both distributive laws), plus the `max`/`min` identities.
- **Lists** — parametric `list` induction: `app`/`length` plus the `map`/`rev`
  theory (`map_app`, `map_map`, `rev_app_distr`, `rev_involutive`, …).
- **`le` / order** — structural induction with a fixpoint-free motive.
- **Logic** — propositional introduction (`split`/`left`/`right`/`apply`).
- **Polymorphic `eq` / `ex`** — over an arbitrary type, unlocked by the
  `Set Printing All` elaboration supplying the implicit type argument.

Out of scope (see `corpus/manifest.json` → `excluded`): multi-variable / nested
case analysis (`andb_comm`, de Morgan) and induction over an inductive relation
(`le_trans`/`le_n_S` via `le_ind`, whose motive is dependent on the derivation).

## Why not verbatim stdlib files

Running the translator over the installed stdlib per file, essentially none
translate whole (≈1/16 even in `Coq.Init`): real files are saturated with
`Notation`/`Ltac`/`Variant`/`Register`/multi-scrutinee `match`/qualified names.
Hence the corpus is curated lemmas drawn from stdlib content, grouped one file
per module to mirror the library's structure.
