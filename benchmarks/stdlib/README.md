# Rocq standard-library benchmark for MEngine

A per-module benchmark comparing **MEngine** against **Rocq** on a curated,
auto-translated subset of the Rocq standard library.  This README is both the
operator's guide and the design reference.

## What it measures

Each *unit* is one stdlib **module** — a single file grouping the Tier-A lemmas
drawn from that module, mirroring the standard library's own file structure
(`coqc` compiles a `.v` file, not a lemma) rather than splitting each lemma into
its own file.  The corpus currently has five modules (77 lemmas):

| Module  | Source                                  | Lemmas |
|---------|-----------------------------------------|--------|
| `Bool`  | `Coq.Bool.Bool` (ops from `Init.Datatypes`) | 22 |
| `Lists` | `Coq.Lists.List` (app/length/map/rev over `list A`) | 11 |
| `Logic` | `Coq.Init.Logic` (eq, and/or, ex)       | 10 |
| `Nat`   | `Coq.Init.Nat` (inductive arithmetic, max/min) | 31 |
| `Peano` | `Coq.Init.Peano` (the `le` order)       | 3  |

Every lemma is a **named** standard-library lemma that lives in its module's
stdlib file — the corpus carries no bespoke facts (ground computations,
nested-conjunction intros, …) with no library counterpart; `fidelity` enforces
this (see "Statement fidelity" below).

Computational induction over the `add`/`mul` fixpoints is now in Tier A, and the
`Nat` module carries the full additive and multiplicative theory up to
`mul_assoc` and both distributive laws: `add_comm`/`add_assoc`,
`mul_comm`/`mul_succ_r`, `mul_add_distr_r`/`mul_add_distr_l`, `mul_assoc`, plus
the `sub`/`pred` reductions (`n - n = 0`, `S n - S m = n - m`, `pred (S n) = n`)
and the `max`/`min` identities (`max 0 n = n`, `max n 0 = n`, `max n n = n`, and
their `min` duals — the `_0_r` cases by `destruct`, the idempotents by induction).
The kernel reduces a
fixpoint applied to a *symbolic* constructor-headed argument (`add (S n) m ↝
S (add n m)`) while leaving a stuck recursive call constant-headed, and `rewrite`
on a quantified induction hypothesis works.  The translator emits these as an
`apply (nat_ind <motive>)` with one focused subgoal per constructor.  Because the
scripted `rewrite` only fires on the goal equation's **left** side, the multi-step
arithmetic proofs (`mul_comm`, `mul_assoc`, …) interleave `symmetry` to bring each
rewrite target to the left and use *fully-applied* lemma instances (e.g.
`rewrite (mul_comm m n)`) where the leftmost occurrence would otherwise be wrong —
mirroring the standard library's reassociation, just spelled out.  Remaining
out-of-scope cases are listed under `excluded` in `corpus/manifest.json`
(multi-variable/nested case analysis and induction over an inductive relation).
**Parametric `list` induction is now in Tier A** (the `Lists` module): the kernel
reduces a fixpoint over a parametric constructor and `induction l` translates to
`apply (list_ind A <motive>)`.  The `Lists` module covers `app`/`length` plus the
`map`/`rev` theory — `map_app`, `length_map`, `map_map`, `rev_app_distr`,
`rev_involutive`, `rev_unit` — the `rev` lemmas by `rewrite`ing the (folded) `rev`
recursive call with the induction hypothesis, exactly as on `nat` (`rev_unit`
straight-line via `rev_app_distr`), and `map_map` by `f_equal` peeling the shared
`cons` head down to the induction hypothesis.

Each module has two forms:

- `corpus/<Module>/rocq.v` — the Rocq source (compiles against the installed
  stdlib).  Four modules need no `Require` (`Coq.Init` is auto-loaded); `Lists`
  alone `Require`s `Coq.Lists.List` for `map`/`rev`, and is timed against its own
  preamble baseline (see "Startup-subtracted times").
- `corpus/<Module>/mengine.me` — the MEngine source, produced **mechanically** by
  [`translate.py`](translate.py) with zero manual edits (Tier A).

Grouping by module also lifts the proof work above each engine's fixed startup
cost (see "Startup-subtracted times" below): a one-lemma file's proof is far
smaller than process startup, so its timing is pure startup noise; a whole
module's proofs are measurable.

### How statements are translated (`Set Printing All`)

MEngine has no notation system and no elaboration, so notation, implicit
arguments, and numeric literals in a Rocq statement must all be made explicit.
Rather than re-implement Rocq's elaborator (an earlier translator hand-desugared
notation and *synthesised* the implicit type argument of `=`, which failed for
any non-`nat`/`bool` equality and for polymorphic lists), `translate.py` always
replays the unit through Rocq with `Set Printing All` and a `Check` per
statement, and translates the **fully-explicit, notation-free** form it prints
back (so a working `coqc`/`rocq`, `--coq`, is required):

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
first times each engine's preamble *alone* and records it as a startup baseline:
for MEngine the compat prelude with no unit appended; for Rocq **each module's
own Require/Import preamble** (a `.v` holding just its leading directives).  Four
modules `Require` nothing, so their Rocq baseline is an empty `.v` loading the
same auto-loaded `Prelude` they do; `Lists` `Require`s `Coq.Lists.List`, so its
baseline loads `List` too (~138 ms vs the ~64 ms empty floor) — that library load
is therefore subtracted from Lists' time, not mistaken for proof cost.
`report` subtracts each module's floor from its whole-file time (clamped at 0) to
get the marginal statement+proof cost, and reports a residual at or below the
baseline's own run-to-run jitter as `~0` (indistinguishable from startup).  The
trivial modules (`Bool`, `Logic`, `Peano`) sit just above their MEngine floor and
the lighter side of Rocq's; the two computational modules clear it comfortably —
`Nat` (~46 ms MEngine / ~68 ms Rocq) and `Lists` (~102 ms MEngine / ~48 ms Rocq) —
so proof cost is now measurable, not pure startup noise.  The whole-file ratio
remains a *process-startup* ratio and is reported only as the raw `total` columns.

**The scatter plots proof time, not whole-file time — deliberately.**  A
whole-file scatter is *dishonest* here, and `Lists` is exactly why.  Because
`Lists` `Require`s `Coq.Lists.List`, its Rocq startup floor (~128 ms) is double
every other module's (~62 ms).  Plotting whole-file time charges that one-time
library load to Lists' x-coordinate, and any *single* parity line — necessarily
anchored at the *global* Rocq floor — credits Rocq back only ~62 ms of it.  So
Lists drops below the line and reads as an MEngine win, when on actual proof cost
MEngine is ~2× **slower** there (`map`/`rev` induction is genuinely heavier in
MEngine than in Rocq).  No single line over whole-file points can be honest when
the per-module floors differ.  `plots/stdlib_scatter.png` therefore plots each
module's **own-floor-subtracted proof time** (Rocq x vs MEngine y, log-log), where
the honest parity is simply `y = x`: points in the shaded lower-right are MEngine
proof-faster, points above the line (Lists) are MEngine proof-slower, and a band
at the MEngine noise floor marks where a residual stops being trustworthy.  The
whole-file numbers are not discarded — they stay in REPORT.md's `total` columns —
they are just not the axis a proof-speed claim is read off of.

**The scatter carries error bars, because the residual is a difference of two
noisy numbers.**  A proof residual is `whole-file time - startup floor`, and both
inputs jitter run-to-run (the Rocq floor alone swings ±10–30 ms between
invocations — often larger than the proof residual itself).  Reporting only the
best-of-N residual would hide that.  Each point is therefore drawn as a cross:
the dot is the *median* residual, and the whiskers span the 10th–90th percentile
of the resampled residual — every `total_trial − startup_trial` difference,
treating the two as independent, so the bar folds in *both* the whole-file jitter
and the startup-floor jitter at once.  Read it directly: a whisker that does not
cross `y = x` is a result robust to the noise (Nat is clearly faster, Lists
clearly slower); a cross straddling the line (Logic) or running off toward the
noise floor (Peano) is a module where the per-invocation jitter swamps the proof
cost and no speed claim should be made.

## Layout

```
benchmarks/stdlib/
  README.md                this file
  translate.py             Rocq .v -> MEngine .me translator (statements via Rocq `Set Printing All`)
  stdlib_bench.py          corpus runner: list / test / run / report / manifest / fidelity / proof-fidelity
  report.py                markdown table + log-log scatter plot
  fidelity.py              statement-vs-stdlib correspondence check (via Rocq's kernel)
  proof_fidelity.py        proof-script vs stdlib-proof gap (re-extracted from installed stdlib)
  compat/stdlib_compat.me  compat prelude: nat/bool/list/option + pred/max/min/le + emulated tactics
  corpus/
    manifest.json          locked Tier-A set + per-statement digests + excluded boundary
    stdlib_map.json        each curated lemma -> its real stdlib counterpart + per-module stdlib file(s)
    PROOF_FIDELITY.md      generated: each corpus proof vs the real stdlib proof + why they diverge
    <Module>/rocq.v        benchmarked Rocq source (one stdlib module per file)
    <Module>/mengine.me    auto-translated MEngine source
  results/stdlib.json      per-module timings + per-engine startup baselines
  results/REPORT.md        generated table + geometric-mean summary
  plots/stdlib_scatter.png generated scatter: per-module startup-subtracted proof time (Rocq x vs MEngine y, log-log, y=x parity, 10-90% jitter whiskers)
```

## Usage

```bash
cd benchmarks
python3 stdlib/stdlib_bench.py list      # show the corpus by category
python3 stdlib/stdlib_bench.py test      # faithfulness gate (see below)
python3 stdlib/stdlib_bench.py run       # time both engines, write results
python3 stdlib/stdlib_bench.py report    # regenerate REPORT.md + scatter plot
python3 stdlib/stdlib_bench.py manifest  # regenerate corpus/manifest.json
python3 stdlib/stdlib_bench.py fidelity  # check each statement vs the real stdlib
python3 stdlib/stdlib_bench.py proof-fidelity  # regenerate corpus/PROOF_FIDELITY.md
```

Run a single unit: `… run le_0_n`, `… test bool_negb_involutive`.

## Faithfulness (`test`)

Before any timing, `test` verifies per unit (plan §7):

1. `rocq.v` compiles under `coqc`.
2. `mengine.me` runs clean under `mengine -q` (compat prelude prepended).
3. `mengine.me` is exactly what `translate.py` re-emits from `rocq.v` (no drift),
   and the **theorem names match** between the two sides.  Because the translator
   elaborates statement types through Rocq (`Set Printing All`), the gate needs
   `coqc` on `PATH` (or `coq_path` in `config.json`).

The guiding principle of the translator is **flag, never guess**: any construct
it cannot translate soundly is reported and the unit is excluded, rather than
mistranslated.  A wrong translation that happened to compile would silently
benchmark two *different* theorems — the worst failure — so the translator
refuses instead.

## Statement fidelity vs the real stdlib (`fidelity`)

`test` checks that the MEngine `.me` faithfully follows the curated `.v`
(`.me` ← `.v`).  It does **not** check that the curated `.v` statement matches
the standard-library lemma it claims to be — and that hand-curation (`.v` ←
stdlib) is the one place a corpus statement could silently drift from the
library, because the corpus is re-stated by hand rather than extracted verbatim
(see "Why whole stdlib files don't translate").

`stdlib_bench.py fidelity` closes that gap by letting **Rocq's own kernel**
compare each curated statement to its real counterpart in the *installed*
stdlib.  Every curated lemma **must** name a real stdlib lemma that lives in its
module's standard-library file — the corpus deliberately holds no bespoke
theorems (ground computations like `2 + 2 = 4`, nested-conjunction intros, …)
that do not exist in the library.  `corpus/stdlib_map.json` records, per module,
the stdlib file(s) its lemmas are drawn from (`module_files`) and maps every
curated lemma to one of:

- a stdlib lemma — the ref is **qualified** with the module's associated file
  (`andb_diag` → `Stdlib.Bool.Bool.andb_diag`) and checked by
  `Check (<file>.<stdlib_ref> : <curated statement>).`; Rocq accepts it iff the
  lemma both **belongs to that file** and has a type **convertible** to the
  curated one (notation, implicit arguments, and numerals are handled by Rocq, so
  there is nothing to guess).  Reported `match`.
- a stdlib lemma whose statement is the **mirror** of the curated one
  (`"relation": "symmetry"`) — verified by `Goal <curated>. Proof. intros;
  symmetry; apply <file>.<stdlib_ref>. Qed.`, confirming they are equivalent up
  to `eq_sym`.  Reported `symmetry`.  No corpus lemma currently uses this: the
  whole corpus is stated to **exactly match** stdlib (the `assoc` lemmas, which
  stdlib states in the opposite orientation, were flipped to match), so the
  mechanism is kept for future curation but every current lemma is `match`.

The map is the single source of truth and must cover **every** curated lemma: an
unmapped lemma, a map entry with no curated lemma, an entry with no `stdlib` ref
(`NO_STDLIB`), a ref that resolves in none of the module's associated files
(`ABSENT`), or a non-convertible `match` claim (`MISMATCH`) all fail the check
(exit 1).  Like the translator, the map is **flag, never guess** — every ref,
relation, and file was confirmed against the installed Rocq before being
recorded.  Because the check shells out to `coqc` (one invocation per mapped
lemma, ~6 s for the whole corpus), it is a separate command rather than part of
the frequently-run `test` gate; run it after editing any `rocq.v` statement or
`stdlib_map.json`.  It needs `coqc` on `PATH` (or `coq_path` in `config.json`)
and the modules named in the map's `preamble` (`Bool.Bool`, `Arith.PeanoNat`,
`Lists.List`) installed.

## Proof-script fidelity vs the real stdlib (`proof-fidelity`)

`fidelity` settles the *statements*: every curated `.v` statement is convertible
to its stdlib counterpart, so the two engines are benchmarked on the **same
theorem**.  The *proof scripts*, however, are deliberately **not** the stdlib's
verbatim proofs — and for almost every lemma they structurally **cannot** be.
`stdlib_bench.py proof-fidelity` documents that gap: it regenerates
`corpus/PROOF_FIDELITY.md`, re-extracting each lemma's real proof straight from
the **installed** stdlib (it asks `coqc` `About <ref>` for the exact source
location, then reads back the verbatim `Proof … Qed` block — never guessed) and
classifying *why* the corpus proof differs:

- `near-match` — the library proof is `reflexivity`/`trivial` and the corpus
  proof has the same shape (the only category that *could* be verbatim).
- `untranslatable` — a concrete script exists but uses *leaf* tactics MEngine
  lacks (the `destr_bool` Ltac macro — all of `Bool`; `destruct N` on a premise —
  `Logic` `eq_sym`/`f_equal`) or only approximates/emulates more weakly
  (`f_equal`, now a single-layer compat-prelude tactic — `Lists` `app_*`; `auto`
  — `Lists` `length_app`).  Why `destr_bool`/`destruct_all` in particular cannot
  be written as MEngine tactics (missing goal-abstraction primitive, no
  higher-order motive inference, no hypothesis clearing) is documented in
  [`DESTR_BOOL_OBSTACLES.md`](DESTR_BOOL_OBSTACLES.md).  This is **not** about sequencing: MEngine's
  *implemented* tactic combinators are `;`, `||`, `try`, `repeat`,
  `first […]`, and `match Goal` (plus emulated
  `auto`/`trivial`/`simpl`/`symmetry`/`f_equal`), and the corpus proofs use `;`
  and `repeat` themselves (`split; assumption`, `repeat constructor`).  The
  grammar additionally *parses* the dispatch selector `… ; [ t1 | t2 | … ]` and
  `shelve`, but the interpreter leaves both as unimplemented stubs (they raise
  `Unknown tactic expression` — see `tactic_interp.c`); neither is needed by the
  corpus, since no lemma's case split closes its branches with *different*
  tactics.
- `functor` — the lemma (most of the arithmetic: `add_0_r`, `add_comm`,
  `add_assoc`, `mul_*`, `eqb_refl`, `leb_refl`, `Nat.le_0_l`) is produced by
  module-functor `Include` over Rocq's abstract `Numbers`/`Structures`
  framework.  Its only script lives in the abstract functor, over setoid
  equality `==` with custom Ltac (`nzinduct`/`nzsimpl`); **no concrete-`nat`
  proof script exists to copy at all**.
- `constructor` — the lemma maps to an inductive **constructor** (`conj`,
  `or_introl`, `ex_intro`, `eq_refl`, `le_n`): the library has no proof script
  for it; the corpus builds the same term with `split`/`left`/`right`/`exists`/
  `reflexivity`/`constructor`.

This is the concrete answer to "why not generate the corpus `.v` from the stdlib
verbatim": the *statements* are (and are kernel-checked to be) the library's, but
the *proofs* must be re-derived in MEngine's tactic subset, because the
library's own proofs are Ltac macros, functor-generated abstract proofs with no
concrete script, or constructors with no script.  Like `fidelity`, it shells out
to `coqc` (one `About` per mapped lemma) and so is a separate command from the
fast `test` gate; re-run it after editing any `rocq.v` proof or `stdlib_map.json`.

## Scope (Tier A) and the engine boundary

Tier A is the computational/structural corner reachable today by MEngine + the
compat prelude:

- **Bool** — identities by ground reduction and by case analysis (`destruct`).
- **Nat (reductions)** — definitional `add`/`mul`/`sub` reductions like
  `0 + n = n`, `S n + m = S (n + m)`, `S n - S m = n - m`.
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
7. **Substitution preserves a match parameter slot's delta alias**
   (`src/kernel/mixed_subst.c`).  When matching on a *parametric* inductive
   (`match l with | cons _ x xs => …` on `l : list A`), the parameter-slot pattern
   variable (`_`) is built as a delta-reducible alias `_ := A`, which is what makes
   the branch body's applications (`cons A x …`, the recursive `app A xs …`)
   type-check.  Both substitution rebuilders (`_simple_topdown_psubst` and the
   spine-rebuild path) dropped that alias when reconstructing the branch under a
   substitution, so reducing a fixpoint over a parametric constructor (`app A
   (cons A x xs) k`) hit "Application does not type check" and then a NULL-deref
   crash.  They now carry the (substituted) body forward via
   `init_var_expression_wc_with_body`.  Without this, *no* list computation
   reduces.
8. **`map_new_with_capacity` rounds up to a power of two**
   (`src/common/map.c`).  The open-addressing map indexes with `hash & (capacity -
   1)`, which is only a valid modulo when the capacity is a power of two.
   `map_new` and resize keep that invariant, but `map_new_with_capacity` took a
   caller-supplied size verbatim.  `iota_reduce` sizes the pattern-substitution map
   to the constructor's argument count — **3** for `cons` — so a `cons` match built
   a capacity-3 map; a lookup of an absent key in the full table then probed the
   same two reachable slots forever (an infinite hang, masked as nondeterministic
   under a normal build).  nat's `S` (one argument → resizes to a power of two)
   never hit it.  Rounding the requested capacity up to the next power of two fixes
   it for every caller.
9. **A match branch may return the matched parametric inductive**
   (`src/kernel/expression.c`).  `init_match_expression_wc` seeded the match's
   result type from the *first* branch's body type, keeping that branch's
   pattern-variable context, then checked the other branches against it.  When the
   seed type mentions the inductive's parameter — e.g. a `nil A` branch of type
   `list A` — that context is not an ancestor of a sibling branch's context, so
   the (sound) `list A` ≡ `list A` convertibility check spuriously failed with
   "Branch body types do not match" (`app`/`length` escaped only because their
   first branch is a variable/`O`, whose type already lives in the outer context).
   The seed type is now transported to the outer context by substituting away the
   branch's parameter-slot pattern variables — each a delta-alias to a scrutinee
   type argument, so the substitution is meaning-preserving (it relocates the
   type, never changes it).  Without this, the direct `Fixpoint map`/`Fixpoint rev`
   the proofs rely on could not be defined (their base branch is `nil <T>`); the
   work-around of routing through `fold_right` makes `map`/`rev` *Definitions*,
   which `simpl` then delta-unfolds, breaking the `rewrite <IH>` the rev proofs
   need.  All 431 kernel tests and every `examples/*.me` still pass.

Now **in Tier A** (previously excluded, unblocked by the kernel work below):

- **Computational induction over a `Fixpoint`** (`add n 0 = n`, `n + m = m + n`,
  `n+(m+p) = (n+m)+p`, `mul` lemmas): the kernel now reduces a fixpoint applied to
  a *symbolic* constructor-headed argument (`add (S n) m ↝ S (add n m)`) while
  leaving a stuck recursive call headed by the `add`/`mul` *constant* (not a bare
  fix node), so `simpl` makes progress and `rewrite` matches it.  `rewrite` on a
  *quantified* induction hypothesis (whose type is the eliminator's beta-redex
  `(motive) n`) works too.  The old `bugs/segfault_*` reproducers run to
  completion, and `examples/computational_induction_rewrite.me` is the regression.
- **Single-variable `destruct`/`case`**, on `bool` *and* on a recursive type
  (`nat`/`list`).  MEngine generates no non-recursive `<T>_rec`/case principle,
  only the recursive `<T>_ind`, so `destruct` is emulated through that same
  eliminator: the step case still binds an induction hypothesis (`P n -> P (S n)`),
  which the translator introduces and immediately **discards with a bare
  `intro.`** before the case body (`sub_0_r`, `n - 0 = n`, is `destruct n`).  The
  resulting term is exactly what `induction` would build with the IH left unused —
  a full, Coq-checkable proof.  Its `as` clause therefore names only the
  constructor arguments (`destruct l as [| x l]`), not the IH that `induction`'s
  would (`induction l as [| x l IHl]`).
- **Parametric `list` induction** (`app_nil_r`, `app_assoc`, `length_app`,
  `map_app`, `length_map`, `rev_app_distr`, `rev_involutive` over `list A`): the
  compat prelude declares `list`/`option` with `A` as a *parameter*, so MEngine
  generates the Rocq-shaped `list_ind : forall (A : Type) (P : list A -> Prop), …`,
  and `induction l` translates to `intro A. intro l. apply (list_ind A <motive>)`
  with one focused subgoal per constructor (the `cons` case introducing the head,
  tail, and the IH).  Unblocked by engine fixes 7–9: list computations reduce, the
  pattern-substitution map no longer hangs on a multi-argument constructor, and
  `map`/`rev` can be defined as direct `Fixpoint`s — whose base branch is `nil <T>`
  (fix 9) — so they stay folded under `simpl` and the rev proofs can `rewrite` a
  recursive call with the IH (`rev_app_distr`, `rev_involutive`), exactly as the
  arithmetic proofs do on `nat`.  The translator names the case binders from the
  proof's `induction l as [| x l IHl]` intro-pattern, so the ported case lines line
  up, and now peels a function-typed leading binder (`f : A -> B` in `map_app`)
  whose printed type contains a comma.

What stays **out of Tier A** (documented in `corpus/manifest.json` → `excluded`):

- **Multi-variable / nested case analysis** (`andb_comm`, `orb b1 b2 = orb b2 b1`,
  de Morgan): the translator emits one `apply (<T>_ind motive)` and segments a
  single level of cases; a second `destruct` *inside* a case is not yet generated.
- **Induction over an inductive relation** (`le_trans`, `le_n_S` via `le_ind`):
  the eliminator's motive is dependent on the derivation; the translator only
  builds non-dependent `fun (x:T) => <body>` motives.

## Why whole stdlib files don't translate

Running the translator over the installed stdlib, per file, reports the first
blocking construct.  Real files are saturated with
`Notation`/`Ltac`/`Variant`/`Register`/multi-scrutinee `match`/qualified names,
so essentially none translate as a whole file (≈1/16 even in `Coq.Init`).  This
feasibility verdict is the reason the corpus is
built from curated lemmas drawn from stdlib content rather than from verbatim
stdlib files.  Those curated lemmas are then grouped one file per stdlib module
(`Bool`, `Lists`, `Logic`, `Nat`, `Peano`), so each benchmark file matches the library's
own file structure even though it holds only the Tier-A-provable subset of that
module's lemmas.
