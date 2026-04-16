"""
Separation logic predicate cancellation.

Proves: iff1 (sep P1 (sep P2 ...)) (sep Pn (... P1))
i.e., reordering of separation logic predicates.

Mengine: NOT YET FUNCTIONAL — requires cross-goal evar propagation (see below).
Coq: cancel tactic (requires coqutil library)
Lean: not yet supported

NOTE on the mengine formulation:
  mengine's rewrite tactic supports eq (not iff1), so the benchmark is
  formulated as eq-based: eq M (sep P1 ..) (sep Pn ..).

  The proof of reversal requires using `eapply eq_trans` to compose steps
  (each step is a sep_swap or sep_comm). However, `eapply eq_trans` creates
  an evar `?Y : M` for the intermediate term. The first subgoal determines ?Y
  (by proving LHS = ?Y), and the second subgoal must use the now-fixed ?Y
  (proving ?Y = RHS). Mengine does not propagate the determination of ?Y
  from goal 1 into goal 2 — this is the same "cross-goal evar propagation"
  missing feature as in symbolic_execution.

  What mengine needs (any one of):
    A. Cross-goal evar propagation.
    B. A `have`/`assert` tactic to state the intermediate eq M LHS mid explicitly.
    C. A dedicated `sep_cancel` tactic implemented as a C-level primitive.
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


class SeparationLogic(Benchmark):
    @property
    def name(self):
        return "separation_logic"

    @property
    def description(self):
        return "Separation logic predicate cancellation (reordering sep predicates)"

    @property
    def params(self):
        return [ParamSpec("n", start=2, stop=201, step=5)]

    @property
    def x_label(self):
        return "n (# predicates)"

    @property
    def strategies(self):
        # NOTE: mengine strategy is not yet functional.
        # The proof requires cross-goal evar propagation (see module docstring).
        return [
            Strategy("coq", "cancel", "Rocq: cancel", color="red", marker="o"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            return self._generate_mengine(n, workdir)

        if strategy.engine == "coq":
            return self._generate_coq(n, workdir)

    # ── Mengine generator ──────────────────────────────────────────────

    def _generate_mengine(self, n, workdir):
        preds = [f"P{i}" for i in range(1, n + 1)]
        lhs = self._sep_chain(preds)
        rhs = self._sep_chain(preds[::-1])

        lines = []
        lines.append(f"(* Separation logic cancellation benchmark - n={n} *)")
        lines.append(f"(* Goal: eq M (sep P1 (sep P2 ...P{n})) (sep P{n} (...P1)) *)")
        lines.append("")
        lines.append("Axiom M : Type.")
        lines.append("Axiom sep : forall (_ : M), forall (_ : M), M.")
        lines.append("")
        lines.append("(* Proof lemmas for sep equality *)")
        lines.append("(* NOTE: eq M (not iff1) because mengine's rewrite tactic supports only eq. *)")
        lines.append("Axiom sep_comm : forall (P : M), forall (Q : M),")
        lines.append("    eq M ((sep P) Q) ((sep Q) P).")
        lines.append("Axiom sep_swap : forall (P : M), forall (Q : M), forall (R : M),")
        lines.append("    eq M ((sep P) ((sep Q) R)) ((sep Q) ((sep P) R)).")
        lines.append("Axiom sep_cong_r : forall (P : M), forall (Q : M), forall (R : M),")
        lines.append("    forall (_ : eq M Q R), eq M ((sep P) Q) ((sep P) R).")
        lines.append("")

        lines.append("(* Predicates *)")
        for p in preds:
            lines.append(f"Axiom {p} : M.")
        lines.append("")

        lines.append("Register Relation eq eq_refl eq_trans Bad_App_Congruence.")
        lines.append("")
        lines.append("(* TODO: The tactic-based proof of sep chain reversal is blocked by a")
        lines.append("   missing mengine feature: cross-goal evar propagation.")
        lines.append("")
        lines.append("   The reversal proof needs O(n^2) steps composed via eq_trans:")
        lines.append("     eq M LHS mid_1")
        lines.append("     eq M mid_1 mid_2")
        lines.append("     ...")
        lines.append("     eq M mid_k RHS")
        lines.append("")
        lines.append("   Each `eapply eq_trans` creates an evar ?Y for the intermediate term.")
        lines.append("   The first subgoal determines ?Y; the second must USE the determined ?Y.")
        lines.append("   Mengine does not propagate ?Y from goal 1 to goal 2 after goal 1 is proved.")
        lines.append("")
        lines.append("   In Coq: `cancel.` (from coqutil) handles this via an AC-unification")
        lines.append("   algorithm implemented as an Ltac tactic that does NOT use eq_trans evars,")
        lines.append("   but instead builds a list-of-predicates and checks subset containment.")
        lines.append("")
        lines.append("   What mengine needs:")
        lines.append("     Option A: Cross-goal evar propagation (filling ?Y in goal 1 propagates")
        lines.append("               into sibling goals that reference ?Y).")
        lines.append("     Option B: A `have TYPE by TACTIC` / `assert TYPE` tactic so that")
        lines.append("               intermediate equalities can be proved and named explicitly,")
        lines.append("               avoiding floating evars entirely.")
        lines.append("     Option C: A sep_cancel primitive tactic implemented at the C level.")
        lines.append("*)")
        lines.append("")
        lines.append(f"Theorem sep_cancel_test : eq M {lhs} {rhs}.")
        lines.append("Admitted.")
        lines.append("")
        content = "\n".join(lines)

        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    # ── Helpers ────────────────────────────────────────────────────────

    @staticmethod
    def _sep_chain(preds):
        """Build right-associated sep chain: sep P1 (sep P2 (... Pn))."""
        if len(preds) == 1:
            return preds[0]
        return f"((sep {preds[0]}) {SeparationLogic._sep_chain(preds[1:])})"

    # ── Coq generator ─────────────────────────────────────────────────

    def _generate_coq(self, n, workdir):
        preds = [f"P{i}" for i in range(1, n + 1)]

        def make_sep(lst):
            if len(lst) == 1:
                return lst[0]
            return f"sep {lst[0]} ({make_sep(lst[1:])})"

        lhs = make_sep(preds)
        rhs = make_sep(preds[::-1])

        content = f"""Require Import coqutil.Lift1Prop.
Require Import coqutil.Map.Interface coqutil.Map.Properties coqutil.Map.Separation coqutil.Map.SeparationLogic.
Require Import Coq.Classes.Morphisms.
Require Import Coq.Lists.List.
Require Import coqutil.sanity coqutil.Decidable coqutil.Tactics.destr coqutil.Tactics.ltac_list_ops.

Import Map.Interface.map Map.Properties.map.

Section Test.
  Context {{key value}} {{map : map key value}} {{ok : ok map}}.
  Context {{key_eqb: key -> key -> bool}} {{key_eq_dec: EqDecider key_eqb}}.
  Local Open Scope sep_scope.
  Import List.ListNotations.

  Goal forall ({' '.join(preds)} : map -> Prop),
    iff1 ({lhs})
         ({rhs}).

  Proof.
    intros.
    cancel.
  Qed.
End Test.
"""
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]

        if strategy.engine == "coq":
            if config and config.coqutil_root:
                coqutil_binding = os.path.join(config.coqutil_root, "src", "coqutil")
                return [engine_path, "-Q", coqutil_binding, "coqutil", generated_file]
            return [engine_path, generated_file]
