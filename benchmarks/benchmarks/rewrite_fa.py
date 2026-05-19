"""
Rewrite f(f(...f(a))) = a

Tests repeated rewriting of nested function applications.
Mengine: rewrite eq_fa_a with eq / apply eq_refl
Coq: multiple strategies (repeat rewrite, setoid_rewrite, rewrite_strat, rewrite!)
Lean: repeat rw [eq_fa_a]
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


COQ_PREAMBLE = r"""Require Import Coq.Setoids.Setoid.
Require Import Coq.Classes.RelationClasses.
Require Import Coq.Classes.Morphisms.

Section Test.
    Variable eq : (forall (A: Type), (forall (_: A), (forall (_: A), Prop))).
    Variable eq_refl : (forall (B: Type), (forall (x: B), (((eq B) x) x))).
    Variable eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
    Variable eq_subst : (forall (P: Prop), (forall (Q: Prop), (forall (_: (((eq Prop) P) Q)), (forall (_: Q), P)))).
    Variable app_cong : (forall (A: Type), (forall (B: Type), (forall (f: (forall (_: A), B)), (forall (g: (forall (_: A), B)), (forall (x: A), (forall (y: A), (forall (_: (((eq (forall (_: A), B)) f) g)), (forall (_: (((eq A) x) y)), (((eq B) (f x)) (g y)))))))))).
    Variable eq_trans : (forall (A: Type), (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x) y)), (forall (_: (((eq A) y) z)), (((eq A) x) z))))))).
    
    Variable nat : Set.
    Variable f : (forall (_: nat), nat).
    Variable g : (forall (_: nat), nat).
    Variable a : nat.
    Variable eq_fa_a : forall (x : nat), (((eq nat) (f x)) x).

    Instance eq_Equivalence (A : Type) : Equivalence (@eq A) := {
      Equivalence_Reflexive := @eq_refl A;
      Equivalence_Symmetric := @eq_sym A;
      Equivalence_Transitive := @eq_trans A
    }.
    Instance fn_Proper (A : Type) (fn : A -> A) : Proper (eq A ==> eq A) fn.
    Proof.
      intro x. intro y. intro Hxy. 
      exact (app_cong A A fn fn x y (eq_refl (A -> A) fn) Hxy).
    Qed.

    Fixpoint repeat_f (n : Datatypes.nat) : nat :=
      match n with
      | 0 => a
      | S n' => f (repeat_f n')
      end.
    Definition gfa (n : Datatypes.nat) : nat := g (repeat_f n).

    Theorem bench : eq nat (repeat_f {N}) (a).
    Proof.
      cbn [repeat_f].
      Time ({tactic}).
      apply eq_refl.
    Qed.
End Test.
"""

COQ_TACTICS = {
    "repeat_rewrite": "repeat rewrite eq_fa_a",
    "repeat_setoid_rewrite": "repeat setoid_rewrite eq_fa_a",
    "rewrite_bottomup": "rewrite_strat bottomup eq_fa_a",
    "rewrite_topdown": "rewrite_strat topdown eq_fa_a",
    "rewrite_bang": "rewrite! eq_fa_a",
}


class RewriteFa(Benchmark):
    @property
    def name(self):
        return "rewrite_fa"

    @property
    def description(self):
        return "Rewriting f(f(...f(a))) = a with n nested applications"

    @property
    def params(self):
        return [ParamSpec("n", start=1, stop=1001, step=15)]

    @property
    def x_label(self):
        return "n (# rewriting locations)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "MEngine", color="blue", marker="x"),
            Strategy("coq", "repeat_rewrite", "Rocq: repeat rewrite", color="red", marker="o"),
            Strategy("coq", "repeat_setoid_rewrite", "Rocq: repeat setoid_rewrite", color="red", marker="*"),
            Strategy("coq", "rewrite_bottomup", "Rocq: rewrite_strat bottomup", color="red", marker="s"),
            Strategy("coq", "rewrite_topdown", "Rocq: rewrite_strat topdown", color="red", marker="D"),
            Strategy("coq", "rewrite_bang", "Rocq: rewrite!", color="red", marker="P"),
            Strategy("lean", "repeat_rw", "Lean: repeat rw", color="green", marker="v"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            # Build f(f(...f(a)...)) with n layers
            expr = "a"
            for _ in range(n):
                expr = f"(f {expr})"
            content = f"""Axiom nat : Type.
Axiom f : forall (_: nat), nat.
Axiom a : nat.
Lemma eq_fa_a : forall (x: nat), eq nat (f x) x.
Admitted.

Theorem bench : eq nat {expr} a.
rewrite eq_fa_a with eq.
apply eq_refl.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f_out:
                f_out.write(content)
            return path

        if strategy.engine == "coq":
            tactic = COQ_TACTICS[strategy.name]
            content = COQ_PREAMBLE.replace("{N}", str(n)).replace("{tactic}", tactic)
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f_out:
                f_out.write(content)
            return path

        if strategy.engine == "lean":
            expr = "a"
            for _ in range(n):
                expr = f"f ({expr})"
            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 4096

section Test

axiom nat : Type
axiom f : nat → nat
axiom a : nat
axiom eq_fa_a : ∀ x : nat, f x = x

theorem bench : ({expr}) = a := by
  repeat rw [eq_fa_a]

end Test
"""
            path = os.path.join(workdir, "test.lean")
            with open(path, "w") as f_out:
                f_out.write(content)
            return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]
        if strategy.engine == "lean":
            return [engine_path, "--tstack=1000000", generated_file]

        return [engine_path, generated_file]
