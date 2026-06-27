# Plan: Rocq Standard Library Benchmark Suite for MEngine

Status: plan (not yet implemented). Scope decisions locked with the user:
- **Granularity:** per-file first; per-proof is a later optional phase.
- **Corpus scope:** Tier A only — files the translator handles with **zero manual edits**.
  Tier B (manual/aligned) and Tier C (out of scope) are documented but not built initially.
- **Constraint:** do **not** edit the MEngine implementation (`src/`, `Makefile`). All work is
  new files under `benchmarks/stdlib/`.

---

## 1. Feasibility verdict (read this first)

A pure text-level translator of *arbitrary* stdlib files is **not** feasible. Two gaps dominate:

- **Notation.** MEngine has no notation system. Rocq stdlib is saturated with infix: `A -> B`,
  `x = y`, `n + m`, `/\`, `\/`, `<=`, decimal literals (`0`, `42`), list `::`/`[]`. MEngine wants
  `forall (_:A), B`, `eq nat x y`, prefix application, and `S (S O)` constructors. Every one must
  be desugared.
- **Tactics.** MEngine has ~14 primitives (`intro/intros/apply/eapply/exact/rewrite/cbv/
  reflexivity/split/left/right/exists/assumption`) plus a thin prelude. It has **no** `induction`,
  `destruct`, `constructor`, `simpl`, `lia`, `ring`, real `auto`, `symmetry`, `trivial`,
  `inversion`. Most real stdlib proofs use these on line one.

Structural gaps too: no `Require`/`Import`/`Module`/`Section`/`Notation`/`Record`/`Class`/`Scope`,
no implicit args `{A}`, single-binder `forall (x:T),`, mandatory `{struct n}` on `Fixpoint`, no
mutual inductives.

Grounding facts verified in-tree:
- `src/runtime/core.c` provides only `eq`, `and`, `or`, `ex`, `Reflexive`. **No** `nat`, `bool`,
  `list`, `True`/`False` — these must come from a compat prelude or each unit.
- `src/main.c` exposes `-q/--quiet` and `--time` (whole-run *subsystem* totals on exit, **not**
  per-proof). So there is no per-proof timer we can use; per-proof requires file-splitting.

**Consequence:** viable benchmark over a *curated Tier-A subset* — the computational/equational
corners of the stdlib (Bool, basic Arith provable by `rewrite`/`reflexivity`/`cbv`, structural
list lemmas) — reached through a layered pipeline (mechanical translator + hand-written compat
prelude). `bench`/runner only builds Rocq files that successfully translate.

## 2. Granularity

- **Per-file (primary, build now):** time `coqc rocq.v` vs `mengine mengine.me` end to end.
  Robust, simple, symmetric.
- **Per-proof (optional, later):** no cross-engine per-proof timer exists (MEngine has none;
  Rocq's `coqc -time` is per-sentence but has no MEngine analog). Portable route = **file
  splitting**: one unit per theorem = `shared preamble + single Theorem`, time each in both
  engines, subtract a measured **preamble-only baseline** to isolate proof cost. `coqc -time`
  output captured as a Rocq-side cross-check.

## 3. Why a sibling driver (not the existing framework)

`benchmarks/framework/` is built for parametric scaling sweeps: `ParamSpec` integer ranges,
adaptive step sizing (`runner._adaptive_step`), auto-retirement on consecutive failures. A fixed
corpus is a different shape. We will **not** subclass `Benchmark`; we add a sibling driver that
**reuses the timing core** (`framework/runner.run_single`: process-group kill on timeout, N-trials
keep-min, soft/hard timeouts) but iterates the manifest instead of sweeping a range. Existing
`framework/` and `benchmarks/` packages are untouched.

## 4. Directory layout (all new, under `benchmarks/`)

```
benchmarks/stdlib/
  PLAN.md                    # this document
  README.md                  # how the suite works, how to add a file
  translate.py               # mechanical Rocq .v -> MEngine .me translator
  stdlib_bench.py            # corpus runner (list/test/run/report subcommands)
  report.py                  # markdown table + scatter plot (Rocq vs MEngine)
  compat/
    stdlib_compat.me         # compat prelude: nat/bool/list defs + emulated tactics
  corpus/
    manifest.json            # in-scope units: source path+sha, tier, deps, notes
    <unit>/
      rocq.v                 # benchmarked Rocq version (Requires installed stdlib)
      mengine.me             # MEngine version (Tier A: auto-translated)
  results/                   # per-unit timings (json), framework-style schema
  plots/                     # generated comparison artifacts
```

No edits to `src/`, `Makefile`, or existing `benchmarks/framework|benchmarks`, except an optional
one-line pointer added to `benchmarks/README.md`.

## 5. Translator (`translate.py`)

Single readable module. `translate.py file.v > out.me`; plus `--report` mode listing handled vs
unhandled constructs (drives triage). It is a **lightweight token/sentence-aware rewriter** (split
on `.` respecting `(* *)` comments and strings), not a full Coq parser. Ordered rules:

**Structural (drop / unwrap):**
- Strip `Require`/`Import`/`Export`/`Open Scope`/`Hint`/`Set`/`Unset`/`Arguments`/`#[...]`.
- Drop `Section ... End`, `Module ... End` wrappers (keep bodies; flag `Context`/`Variable`).
- Remove `Proof.`/`Qed.`/`Defined.`/`Admitted.` framing (MEngine ends a proof when goals close).
- `Parameter`/`Conjecture` -> `Axiom`.

**Term desugaring (the bulk):**
- Non-dependent `A -> B` -> `forall (_ : A), B`.
- Multi-binder expansion: `forall x y z, P` / `forall (x y : T), P` -> nested single binders;
  same for `fun`.
- `fun x : T => e` -> `fun (x : T) => e`; normalize `=>` / `,`.
- Notation -> prefix application via configurable table: `=`->`eq`, `/\`->`and`, `\/`->`or`,
  `exists x, P`->`ex ...`, etc. (`eq`/`and`/`or`/`ex` heads exist in `core.c`.) Operators needing
  a type argument (`eq` is `eq A x y`) handled where recoverable, else flagged.
- Numeric literals `0,1,2,...` -> `O, S O, ...` (bounded; large literals flagged).
- `Fixpoint`: inject `{struct <first arg>}` if absent (flag if decreasing arg isn't first).
- `match ... end`: pass through; flag `as`/`in`/`return` clauses and nested patterns as manual.

**Tactics (line-by-line, conservative):**
- Pass through the supported set (`intro(s)/apply/eapply/exact/rewrite/reflexivity/cbv/split/
  left/right/exists/assumption`).
- Map known aliases to compat-prelude tactics: `simpl`->`cbv`, `symmetry`/`trivial`/`now`/`easy`
  -> compat equivalents (sec. 6).
- Any unsupported tactic (`induction/destruct/lia/ring/inversion/auto/...`) -> **hard stop**:
  unit marked `manual`, excluded from Tier A; `--report` lists the offending tactic.

Principle: **flag, never guess.** A wrong translation that happens to compile would corrupt the
benchmark, so the translator refuses rather than emit an unjustified rewrite.

## 6. Compat prelude (`compat/stdlib_compat.me`)

Hand-written MEngine, loaded ahead of every translated unit. Contents:
- **Base datatypes** stdlib assumes but `core.c` lacks: `nat` (`O`/`S`), `bool`, `list`, `option`,
  `True`/`False`, plus basic `Fixpoint`s (`add`, `mul`, `app`, ...), matching Rocq's definitions
  exactly so theorem statements line up.
- **Emulated tactics** in the tactic language (same pattern as the separation-logic benchmark's
  `.me` tactic defs):
  - `symmetry` := `apply eq_sym` (with a provided `eq_sym`).
  - `trivial`/`easy`/`now` := `try reflexivity; try assumption` (+ `intros`).
  - `simpl` := `cbv beta delta iota fix`.
  - limited `destruct`/`induction` helpers: `match Goal` to find the inductive head, then `apply`
    the auto-generated induction principle (MEngine builds these for `Inductive` decls). Clean
    structural cases only; complex eliminations stay Tier B.
- Each emulated tactic carries a comment stating exactly how it differs from Rocq's, so divergence
  is auditable and lives in one reviewable file.

## 7. Faithfulness validation (integrity)

`stdlib_bench.py test` verifies, per unit, before any timing:
1. `rocq.v` compiles against installed stdlib (`coqc -time`, with `-Q`/`-R` from config).
2. `mengine.me` runs clean under `mengine -q`.
3. **Statement correspondence:** translator emits a normalized statement digest; the Rocq and
   MEngine statements must correspond. Guards against benchmarking two engines on *different*
   theorems. Only units passing all three enter the timed corpus.

## 8. Runner & Rocq instrumentation (`stdlib_bench.py`)

Reuses `framework/runner.run_single` timing logic; subcommands mirror `bench.py`: `list`, `test`,
`run [unit]`, `report`.
- **MEngine:** `mengine -q <unit>/mengine.me`, run from `mengine_root` so `prelude/` resolves;
  compat prelude prepended.
- **Rocq:** `coqc -time <unit>/rocq.v` with load paths from config. Rely on the **installed
  stdlib** for `Require` (deps are *not* recompiled — we time only the translated unit's work,
  the fair comparison). `-time` per-sentence output stored alongside wall-clock.
- **Config:** extend `benchmarks/config.json` with a `stdlib` block (rocq stdlib source path for
  translation input, coqc load-path args, per-unit timeout). Reuse existing
  `mengine_path`/`coq_path`/`trials`/timeout keys.
- **Results:** `results/stdlib.json`, keyed `{engine}_{unit}` (and `_{proof}` in per-proof mode),
  same value schema (`time_taken`/`success`/`trials`) as the framework.

## 9. Reporting (`report.py`)

- **Markdown table:** per unit — Rocq time, MEngine time, ratio, tier, status — drop-in for
  `README.md` like the existing benchmark table.
- **Scatter plot:** Rocq time (x) vs MEngine time (y), log-log, parity diagonal, colored by tier.
- **Summary:** geometric-mean speedup over Tier A; counts (translated / built-in-both / excluded).

## 10. Phased roadmap (per-file, Tier A)

1. **Skeleton + one unit end to end.** Layout, `config.json` `stdlib` block, compat prelude with
   `nat`/`bool` + 2-3 tactics, `stdlib_bench.py` (`test`/`run`), one hand-made Tier-A unit (e.g.
   `add_0_r`). Proves translate->validate->time->report before scaling.
2. **Translator core.** Structural stripping + term desugaring + supported-tactic passthrough +
   `--report`. Sanity-check against `examples/*.me` and the first units.
3. **Triage.** Run `--report` over a Rocq stdlib checkout; populate `manifest.json`; lock the
   Tier-A set (Tier B/C recorded but not built).
4. **Corpus build-out.** Auto-translate Tier A; expand compat prelude as recurring needs surface.
5. **Full run + report.** `stdlib_bench.py run`; generate table + scatter; optional row in
   `benchmarks/README.md`.
6. **Per-proof mode (optional, later).** File-splitting + preamble-baseline subtraction; `-time`
   cross-check.

## 11. Key risks

- **Tier A is small.** Likely, given the tactic gap. Mitigation: lean on the compat prelude;
  accept "useful benchmark" = tens of representative equational/structural proofs, not breadth.
  Tier B (manual, deferred) is the growth path if more coverage is wanted later.
- **Mistranslation that compiles** (silent unfaithfulness — the worst failure). Mitigation:
  flag-not-guess translator + sec. 7 statement-correspondence check.
- **Fairness of the Rocq baseline.** One-theorem `.v` against installed stdlib includes Rocq
  process/Require startup. Mitigation: symmetric treatment (MEngine pays prelude startup too) +
  preamble-baseline subtraction in per-proof mode.
