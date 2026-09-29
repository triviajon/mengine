"""
Separation logic predicate reversal.

Proves: eq M (sep P1 (sep P2 ...Pn)) (sep Pn (...P2 P1))
i.e., full reversal of a right-associated separation logic predicate chain.

Mengine: the cancel tactic moves each element of the right-hand side to the front
  of the left-hand side with sep_swap (three-element rotate) and sep_cong_r, then
  strips it, chaining the steps with eq_trans: O(n^2) kernel constructions.
Coq: cancel tactic (requires coqutil library)
Lean: the same cancellation procedure as a custom tactic in the Elab monad
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


SCRIPTED_CANCEL = """\
Tactic __bring_to_front target lhs :=
  first [
    (expr_eq lhs target;
     let T := type_of lhs in
     let refl := constr ((eq_refl T) lhs) in
     pair lhs refl)
  |
    (match lhs with
     | ((?s ?lhs_head) ?lhs_rest) =>
     first [
       (expr_eq lhs_head target;
        let T := type_of lhs in
        let refl := constr ((eq_refl T) lhs) in
        pair lhs refl)
     |
       (let sub := __bring_to_front target lhs_rest in
        let rest' := fst sub in
        let rest_proof := snd sub in
        let T := type_of lhs in
        first [
          (expr_eq rest' target;
           let mid     := constr ((sep lhs_head) target) in
           let cong    := constr ((((sep_cong_r lhs_head) lhs_rest) target) rest_proof) in
           let swapped := constr ((sep target) lhs_head) in
           let swap    := constr ((sep_comm lhs_head) target) in
           let trans   := constr ((((((eq_trans T) lhs) mid) swapped) cong) swap) in
           pair swapped trans)
        |
          (match rest' with
           | ((?s2 ?x2) ?rest2) =>
           let mid     := constr ((sep lhs_head) rest') in
           let cong    := constr ((((sep_cong_r lhs_head) lhs_rest) rest') rest_proof) in
           let swapped := constr ((sep target) ((sep lhs_head) rest2)) in
           let swap    := constr (((sep_swap lhs_head) target) rest2) in
           let trans   := constr ((((((eq_trans T) lhs) mid) swapped) cong) swap) in
           pair swapped trans
           end)
        ])
     ]
     end)
  ].
Tactic __cancel_one :=
  match Goal with
  | [ |- (((eq ?A) ?LHS) ((sep ?B) ?REST_R)) ] =>
    let rot       := __bring_to_front B LHS in
    let lhs'      := fst rot in
    let rot_proof := snd rot in
    match lhs' with
    | ((?s3 ?b3) ?lhs'_rest) =>
    let g         := current_goal in
    let new_goal_ty := constr (((eq A) lhs'_rest) REST_R) in
    let h         := mk_hole new_goal_ty in
    let strip     := constr ((((sep_cong_r B) lhs'_rest) REST_R) h) in
    let full      := constr ((((((eq_trans A) LHS) lhs') ((sep B) REST_R)) rot_proof) strip) in
    fill g full
    end
  end.
Tactic cancel := repeat __cancel_one; try reflexivity."""


class SeparationLogic(Benchmark):
    @property
    def name(self):
        return "separation_logic"

    @property
    def description(self):
        return "Separation logic predicate cancellation (reordering sep predicates)"

    @property
    def params(self):
        return [ParamSpec("n", start=2, stop=351, step=5)]

    @property
    def x_label(self):
        return "n (# predicates)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "cancel", "Mengine: cancel", color="blue", marker="s"),
            Strategy("coq", "cancel", "Rocq: cancel", color="red", marker="o"),
            Strategy("lean", "cancel", "Lean: cancel", color="green", marker="v"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            return self._generate_mengine(n, workdir)

        if strategy.engine == "coq":
            return self._generate_coq(n, workdir)

        if strategy.engine == "lean":
            return self._generate_lean(n, workdir)

    # ── Mengine generator ──────────────────────────────────────────────

    def _generate_mengine(self, n, workdir):
        preds = [f"P{i}" for i in range(1, n + 1)]
        lhs = self._sep_chain(preds)
        rhs = self._sep_chain(preds[::-1])

        lines = []
        lines.append(f"(* Separation logic reversal benchmark - n={n} *)")
        lines.append(f"(* Goal: eq M (sep P1 (sep P2 ...P{n})) (sep P{n} (...P1)) *)")
        lines.append("")
        lines.append("Axiom M : Type.")
        lines.append("Axiom sep : forall (_ : M), forall (_ : M), M.")
        lines.append("")
        lines.append("(* Proof lemmas for sep equality *)")
        lines.append("Axiom sep_comm : forall (P : M), forall (Q : M),")
        lines.append("    eq M ((sep P) Q) ((sep Q) P).")
        lines.append("Axiom sep_swap : forall (P : M), forall (Q : M), forall (R : M),")
        lines.append("    eq M ((sep P) ((sep Q) R)) ((sep Q) ((sep P) R)).")
        lines.append("Axiom sep_cong_r : forall (P : M), forall (Q : M), forall (R : M),")
        lines.append("    forall (_ : eq M Q R), eq M ((sep P) Q) ((sep P) R).")
        lines.append("Axiom sep_cong_l : forall (A : M), forall (B : M), forall (C : M),")
        lines.append("    forall (_ : eq M A B), eq M ((sep A) C) ((sep B) C).")
        lines.append("")
        lines.append("(* Cancel tactic: rotate the matching element to the front, strip it with sep_cong_r *)")
        lines.append(SCRIPTED_CANCEL)

        lines.append("(* Predicates *)")
        for p in preds:
            lines.append(f"Axiom {p} : M.")
        lines.append("")

        lines.append(f"Theorem sep_rev_{n} : eq M {lhs} {rhs}.")
        lines.append("  cancel.")
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

    # ── Lean 4 generator ───────────────────────────────────────────────
    #
    # Embeds a static Lean 4 cancel tactic implemented via Lean.Elab.Tactic,
    # mirroring the mengine approach exactly.  Python only generates the axioms,
    # predicates, and theorem statement - the proof is always just `cancel`.

    _LEAN_CANCEL_TACTIC = """\
import Lean
open Lean Meta Elab Tactic

-- bring_to_front target lhs
--   Returns (lhs', proof : lhs = lhs')  where lhs' = sep target rest.
private partial def bringToFront (target lhs : Expr) : MetaM (Expr × Expr) := do
  if ← isDefEq lhs target then
    return (lhs, ← mkEqRefl lhs)
  let args := lhs.getAppArgs
  if args.size != 2 then
    throwError "bringToFront: expected 'sep head rest', got {lhs}"
  let head := args[0]!
  let rest  := args[1]!
  if ← isDefEq head target then
    return (lhs, ← mkEqRefl lhs)
  let (rest', restProof) ← bringToFront target rest
  if ← isDefEq rest' target then
    -- 2-element tail: lhs = sep head target  →  sep target head
    let cong    ← mkAppM `sep_cong_r #[head, rest, target, restProof]
    let swapped ← mkAppM `sep        #[target, head]
    let swap    ← mkAppM `sep_comm   #[head, target]
    return (swapped, ← mkEqTrans cong swap)
  else
    -- n-element tail: lhs = sep head (sep target rest2)  →  sep target (sep head rest2)
    let rest2   := rest'.getAppArgs[1]!
    let cong    ← mkAppM `sep_cong_r #[head, rest, rest', restProof]
    let swapped ← mkAppM `sep        #[target, ← mkAppM `sep #[head, rest2]]
    let swap    ← mkAppM `sep_swap   #[head, target, rest2]
    return (swapped, ← mkEqTrans cong swap)

-- cancel_one: match goal  LHS = sep B REST_R,
--   rotate B to front of LHS, strip via sep_cong_r, leave LHS_rest = REST_R.
private def cancelOne : TacticM Unit := do
  let goal   ← getMainGoal
  let goalTy ← instantiateMVars (← goal.getType)
  -- goalTy = @Eq M lhs (sep B restR)
  let eqArgs := goalTy.getAppArgs
  unless eqArgs.size == 3 do throwError "cancelOne: goal is not an equality"
  let lhs    := eqArgs[1]!
  let rhs    := eqArgs[2]!
  let rhsArgs := rhs.getAppArgs
  unless rhsArgs.size == 2 do throwError "cancelOne: rhs is not a sep application"
  let b      := rhsArgs[0]!
  let restR  := rhsArgs[1]!
  let (lhs', rotProof) ← bringToFront b lhs
  let lhsRest := lhs'.getAppArgs[1]!
  let newGoalTy ← mkEq lhsRest restR
  let newGoal   ← mkFreshExprSyntheticOpaqueMVar newGoalTy
  let strip  ← mkAppM `sep_cong_r #[b, lhsRest, restR, newGoal]
  let full   ← mkEqTrans rotProof strip
  goal.assign full
  replaceMainGoal [newGoal.mvarId!]

elab "cancel_one" : tactic => cancelOne

macro "cancel" : tactic =>
  `(tactic| (repeat cancel_one; try rfl))

"""

    def _generate_lean(self, n, workdir):
        preds = [f"P{i}" for i in range(1, n + 1)]

        def sep_chain(ps):
            if len(ps) == 1:
                return ps[0]
            return f"sep {ps[0]} ({sep_chain(ps[1:])})"

        lhs = sep_chain(preds)
        rhs = sep_chain(preds[::-1])

        lines = [self._LEAN_CANCEL_TACTIC]
        lines.append(f"-- Separation logic reversal benchmark (Lean 4) - n={n}")
        lines.append("set_option maxHeartbeats 0")
        lines.append("set_option maxRecDepth 1000000")
        lines.append("")
        lines.append("axiom M : Type")
        lines.append("axiom sep : M → M → M")
        lines.append("axiom sep_comm  : ∀ (P Q : M),   sep P Q = sep Q P")
        lines.append("axiom sep_swap  : ∀ (P Q R : M), sep P (sep Q R) = sep Q (sep P R)")
        lines.append("axiom sep_cong_r : ∀ (P Q R : M), Q = R → sep P Q = sep P R")
        lines.append("")
        for p in preds:
            lines.append(f"axiom {p} : M")
        lines.append("")
        lines.append(f"theorem bench : {lhs} = {rhs} := by")
        lines.append("  cancel")
        lines.append("")

        content = "\n".join(lines)
        path = os.path.join(workdir, "test.lean")
        with open(path, "w") as f:
            f.write(content)
        return path

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

        if strategy.engine == "lean":
            return [engine_path, "--tstack=1000000", generated_file]
