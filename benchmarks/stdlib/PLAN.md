# Plan: Rocq Standard Library Benchmark Suite for MEngine

Status: **implemented** (Tier A). See `README.md` for the operator's guide and
the realized scope; the per-proof mode (phase 6) remains optional/future. The
sections below are the original design; where the implementation refined a
decision (e.g. corpus drawn from curated lemmas — grouped one file per stdlib
module, mirroring the library's file structure — rather than verbatim stdlib
files, two targeted kernel fixes landed, the symbolic-fixpoint induction
crash deferred, **statement types elaborated through Rocq's `Set Printing All`
instead of hand-desugared** — see §5), `README.md` is authoritative.

Scope decisions locked with the user:
- **Granularity:** per-file first; per-proof is a later optional phase.
- **Corpus scope:** Tier A only — files the translator handles with **zero manual edits**.
  Tier B (manual/aligned) and Tier C (out of scope) are documented but not built initially.
- **Engine baseline:** this branch is rebased on `induction-principle-with-ih`, so the benchmark
  targets a MEngine that **generates proper induction principles** — `<T>_ind` with induction
  hypotheses, for both parametric and non-parametric inductives. The original "do not touch the
  engine" rule has been relaxed: the user is willing to make targeted kernel fixes that unblock the
  benchmark. Remaining engine gaps are tracked as **Tier 2** (sec. 5a); taking them on
  substantially widens the corpus. The benchmark *machinery itself* still lives entirely under
  `benchmarks/stdlib/`.

---

## 1. Feasibility verdict (read this first)

A pure text-level translator of *arbitrary* stdlib files is **not** feasible. Two gaps dominate:

- **Notation.** MEngine has no notation system. Rocq stdlib is saturated with infix: `A -> B`,
  `x = y`, `n + m`, `/\`, `\/`, `<=`, decimal literals (`0`, `42`), list `::`/`[]`. MEngine wants
  `forall (_:A), B`, `eq nat x y`, prefix application, and `S (S O)` constructors. Every one must
  be desugared.
- **Tactics.** MEngine has ~14 primitives (`intro/intros/apply/eapply/exact/rewrite/cbv/
  reflexivity/split/left/right/exists/assumption`) plus a thin prelude. There is still no native
  `induction`/`destruct` *tactic*, but the engine now **generates the induction principle**
  `<T>_ind` (with IH) for every inductive, so `induction`/`destruct` can be *emulated* by applying
  it (sec. 5a). `constructor` is emulated in the compat prelude (sec. 6a). Still missing: `simpl`,
  `lia`, `ring`, real `auto`, `symmetry`, `trivial`, `inversion`. Most real stdlib proofs use
  several of these on line one.

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
list lemmas) plus **structural induction proofs** now reachable through the generated `<T>_ind`
(sec. 5a) — reached through a layered pipeline (mechanical translator + hand-written compat
prelude). Two engine gaps (sec. 5a) still block *computational* induction (the bulk of stdlib
arithmetic); closing them is the main lever for growing the corpus. `bench`/runner only builds Rocq
files that successfully translate.

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

> **Refinement (implemented): statement types via `Set Printing All`.** The
> term-desugaring rules below describe the original *surface* rewriter, which had
> to synthesise the implicit type argument of `=` (failing for any non-`nat`/`bool`
> equality and for polymorphic lists). `translate.py --elaborate` instead replays
> the unit through Rocq (`Set Printing All` + one `Check` per statement) and
> translates the **fully-explicit, notation-free** type Rocq prints — every
> implicit argument, including the `T` of `@eq T x y` and a list's element type,
> is already supplied, so no synthesis is needed and notation/literals are gone.
> This applies to definition and theorem *statements*; the surface rules still
> govern *tactic* terms (not always elaborable) and the `--report`/`--dir` triage
> mode (over uncompilable stdlib files). See `README.md` → "How statements are
> translated".

**Term desugaring (the bulk) — surface rewriter, superseded for statements:**
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
- `constructor` / `constructor n` -> pass through to the compat-prelude `constructor` tactic
  (sec. 6a), but only when the goal head is one the prelude enumerates; otherwise flag.
- `induction x` / `destruct x` -> emit the generated-principle application (sec. 5a), but only when
  it is safe (motive free of fixpoints; step case needs no symbolic reduction). Otherwise flag.
- Remaining unsupported tactics (`lia/ring/inversion/auto/...`) -> **hard stop**:
  unit marked `manual`, excluded from Tier A; `--report` lists the offending tactic.

Principle: **flag, never guess.** A wrong translation that happens to compile would corrupt the
benchmark, so the translator refuses rather than emit an unjustified rewrite.

## 5a. Induction (generated `<T>_ind`)

The engine now generates, for every inductive `T`, the principle `T_ind` carrying an induction
hypothesis for each recursive argument — e.g.

```
nat_ind  : forall (P : nat -> Prop), P O -> (forall n, P n -> P (S n)) -> forall n, P n
list_ind : forall (A : Type) (P : list A -> Prop),
             P (nil A) -> (forall a l, P l -> P (cons A a l)) -> forall l, P l
```

**Translating `induction x`.** Emit an explicit application of the principle:

```
intro x.
apply (T_ind (fun (x : T) => <goal body>)).
<case_0> ... <case_n>        (* one focused subgoal per constructor, in declaration order *)
```

Verified against the engine:
- The `intro x` **then** `apply (T_ind motive)` order matters. MEngine's first-order `apply` can't
  match the principle's conclusion under the goal's outer `forall`, so `x` is introduced first and
  the conclusion then unifies at the concrete `x`. (`apply` of the bare principle, or with a motive
  but no prior `intro`, fails with `fill: hole fill failed`.)
- The motive `fun (x:T) => <goal body>` is computed textually. The clean case is `induction x` on a
  **leading** universally-quantified variable (goal `forall (x:T), Body`, so the body is `Body`).
  `induction` after intros, on a non-leading variable, or with hypotheses depending on `x` (which
  Rocq auto-reverts) is Tier B / out of scope.
- Each constructor yields one focused subgoal in declaration order; the step case exposes the
  recursive arguments and the IH. The translator introduces them and names the IH as Rocq does
  (`IH<x>`) so the remaining ported lines line up.
- `destruct x` is the same application, ignoring the IH in the step case(s).

**Tier-2 caveats (these gate the computational majority).** Two engine issues — each with a minimal
reproducer in `bugs/` on the engine branch — block induction over goals stated with a `Fixpoint`:

1. **`apply` with a fixpoint-mentioning motive segfaults.** When `<goal body>` applies a defined
   `Fixpoint` to `x` (e.g. `eq nat (add x O) x`), the `apply (T_ind motive)` step crashes. So any
   equational arithmetic/list lemma whose *statement* uses a recursive function is unreachable by
   induction right now.
2. **Ground-only iota.** `simpl`/`cbv` do not reduce a fixpoint applied to a constructor-headed
   *symbolic* term (`add (S n) m`, `app (cons a l) m`), so step cases that must unfold a recursive
   function on `S n` / `cons a l` cannot progress.

Until (1) and (2) are fixed, the translator must **detect and exclude** induction whose motive
mentions a defined fixpoint or whose step case needs symbolic reduction, keeping such units out of
Tier A. What works **today**: structural induction with a fixpoint-free motive whose cases close via
the IH, constructors, `apply`/`rewrite` with lemmas, and `assumption` — i.e. relational/predicate
lemmas rather than computational equalities. Fixing (1) and (2) is the highest-leverage corpus
expansion and is now in scope (the engine is already being modified on the parent branch).

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
  - `destruct`/`induction` support built on the generated `<T>_ind` (now IH-bearing, sec. 5a).
    Whether this lives as a compat tactic or as direct translator-emitted `apply (T_ind motive)`
    is decided in implementation; either way, clean structural cases only — computational and
    complex eliminations stay Tier 2 / Tier B.
- Each emulated tactic carries a comment stating exactly how it differs from Rocq's, so divergence
  is auditable and lives in one reviewable file.

## 6a. `constructor` (compat prelude, no engine change)

`constructor` is **not** a static-translation problem and does not need a new engine primitive. It
is the generalization of `split`/`left`/`right`, which the existing prelude already implements as
runtime tactics via `match Goal with … => eapply <ctor>`. The tactic language supplies the three
primitives needed: `match Goal with` (read the goal head), `first [ … | … ]` (try constructors in
declaration order), and `eapply` (which leaves a multi-arg constructor's arguments as subgoals/evars,
exactly as `split` leaves two subgoals).

Because the compat prelude already *defines* every inductive the corpus uses, we enumerate their
constructors once in a single dispatch tactic, e.g.

```
Tactic constructor := match Goal with
| [ |- True ]          => exact I
| [ |- ((and ?A) ?B) ] => eapply ((conj A) B)
| [ |- ((or  ?A) ?B) ] => first [ eapply (or_introl A B) | eapply (or_intror A B) ]
| [ |- ((le  ?n) ?m) ] => first [ eapply (le_n n)        | eapply (le_S n ...) ]
| ...                              (* one arm per relational predicate in the corpus *)
end.
```

`first` over the constructors reproduces Rocq's "try each in declaration order" semantics, and each
arm builds a full proof term, so this stays sound under **flag, never guess** — the arms are
hand-verified, exactly like `split`. `constructor n` translates directly to `eapply C_n`, no
dispatch needed.

This is high leverage: the lemmas already reachable today (sec. 5a: relational/predicate lemmas
such as `le`, ordering, custom inductive predicates) are precisely the ones whose proofs lead with
`constructor`, so adding it promotes a chunk of current hard-stops into Tier A at near-zero cost. It
is orthogonal to the two Tier-2 engine caveats (those gate computational induction, not `constructor`).

Limitations, kept honest: witness-guessing (`constructor` on `ex`, i.e. `eexists`) stays out, the
same boundary as today; and a goal whose head the prelude does not enumerate is still flagged — but
the supported set is now "every inductive in the compat prelude," not "none." A fully generic
`constructor` over *any* inductive would want a small engine primitive that reads the goal's
inductive and tries each stored constructor (the engine already keeps that list — it generates
`<T>_ind` from it); that is the "do it once properly" path, deferred and out of scope here.

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
   the `induction`/`destruct` rule (sec. 5a) with fixpoint-motive exclusion + `--report`.
   Sanity-check against `examples/*.me` and the first units (include one structural induction unit).
3. **Triage.** Run `--report` over a Rocq stdlib checkout; populate `manifest.json`; lock the
   Tier-A set (Tier B/C recorded but not built).
4. **Corpus build-out.** Auto-translate Tier A; expand compat prelude as recurring needs surface.
5. **Full run + report.** `stdlib_bench.py run`; generate table + scatter; optional row in
   `benchmarks/README.md`.
6. **Per-proof mode (optional, later).** File-splitting + preamble-baseline subtraction; `-time`
   cross-check.

## 11. Key risks

- **Tier A is small.** Likely, given the tactic gap. Induction principles widen it to structural
  inductive proofs, but the Tier-2 gaps (sec. 5a) keep *computational* induction — most stdlib
  arithmetic — out for now. Mitigations, in leverage order: (1) take on the Tier-2 engine fixes
  (the single biggest corpus multiplier, now in scope); (2) lean on the compat prelude; (3) accept
  "useful benchmark" = tens of representative proofs, not breadth. Tier B (manual, deferred) is a
  further growth path.
- **Mistranslation that compiles** (silent unfaithfulness — the worst failure). Mitigation:
  flag-not-guess translator + sec. 7 statement-correspondence check.
- **Fairness of the Rocq baseline.** One-theorem `.v` against installed stdlib includes Rocq
  process/Require startup. Mitigation: symmetric treatment (MEngine pays prelude startup too) +
  preamble-baseline subtraction in per-proof mode.
