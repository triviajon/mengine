"""
Let-binding rewriting with add_r_O: nested let x := add (add v v) O in ...

Mengine: --proof=0 addr0 native <n>
Coq: repeat rewrite (with native let), setoid_rewrite (with LetIn const)
Lean: repeat rw, simp only
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


COQ_LETIN_TEMPLATE = r"""Require Import Coq.Setoids.Setoid.
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
  Variable v0 : nat.
  Variable O : nat.
  Variable add : nat -> nat -> nat.
  Variable add_r_O : (forall (n: nat), (((eq nat) ((add n) O)) n)).

  Instance eq_Equivalence (A : Type) : Equivalence (@eq A) := {{
    Equivalence_Reflexive := @eq_refl A;
    Equivalence_Symmetric := @eq_sym A;
    Equivalence_Transitive := @eq_trans A
  }}.
  Instance fn_Proper (A : Type) (fn : A -> A) : Proper (eq A ==> eq A) fn.
  Proof.
    intro x. intro y. intro Hxy. 
    exact (app_cong A A fn fn x y (eq_refl (A -> A) fn) Hxy).
  Qed.
  Instance fn2_Proper (A : Type) (fn : A -> A -> A) : Proper (eq A ==> eq A ==> eq A) fn.
  Proof.
    intro x1. intro x2. intro Hx.
    intro y1. intro y2. intro Hy.
    apply app_cong; auto.
  Qed.

  Goal exists v', eq nat (
{lets}
    ) v'.
  Proof.
    eexists.
    Time repeat rewrite add_r_O.
    apply eq_refl.
  Qed.
  
End Test.
"""

COQ_LETIN_CONST_TEMPLATE = r"""Require Import Coq.Setoids.Setoid.
Require Import Coq.Classes.RelationClasses.
Require Import Coq.Classes.Morphisms.

Local Parameter eq : (forall (A: Type), (forall (_: A), (forall (_: A), Prop))).
Local Parameter eq_refl : (forall (B: Type), (forall (x: B), (((eq B) x) x))).
Local Parameter eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
Local Parameter eq_subst : (forall (P: Prop), (forall (Q: Prop), (forall (_: (((eq Prop) P) Q)), (forall (_: Q), P)))).
Local Parameter app_cong : (forall (A: Type), (forall (B: Type), (forall (f: (forall (_: A), B)), (forall (g: (forall (_: A), B)), (forall (x: A), (forall (y: A), (forall (_: (((eq (forall (_: A), B)) f) g)), (forall (_: (((eq A) x) y)), (((eq B) (f x)) (g y)))))))))).
Local Parameter eq_trans : (forall (A: Type), (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x) y)), (forall (_: (((eq A) y) z)), (((eq A) x) z))))))).
Local Parameter lambda_extensionality : (forall (A: Type), (forall (B: Type), (forall (f: (forall (_: A), B)), (forall (g: (forall (_: A), B)), (forall (_: (forall (x: A), (((eq B) (f x)) (g x)))), (((eq (forall (_: A), B)) f) g)))))).

Module Type LetInT.
  Parameter Let_In : forall {{A P}} (x : A) (f : forall a : A, P a), P x.
  Axiom Let_In_def : @Let_In = fun A P x f => let y := x in f y.
  Reserved Notation "'dlet_nd' x .. y := v 'in' f"
          (at level 200, x binder, y binder, f at level 200, format "'dlet_nd'  x .. y  :=  v  'in' '//' f").
  Reserved Notation "'dlet' x .. y := v 'in' f"
          (at level 200, x binder, y binder, f at level 200, format "'dlet'  x .. y  :=  v  'in' '//' f").
  Notation "'dlet_nd' x .. y := v 'in' f" := (Let_In (P:=fun _ => _) v (fun x => .. (fun y => f) .. )) (only parsing).
  Notation "'dlet' x .. y := v 'in' f" := (Let_In v (fun x => .. (fun y => f) .. )).
  Axiom Let_In_nd_Proper : forall {{A P}},
      Proper ((@eq A) ==> pointwise_relation _ (@eq P) ==> (@eq P)) (@Let_In A (fun _ => P)).
  Hint Extern 0 (Proper _ (@Let_In _ _)) => simple apply @Let_In_nd_Proper : typeclass_instances.
End LetInT.

Module Export LetIn : LetInT.
  Definition Let_In {{A P}} (x : A) (f : forall a : A, P a) : P x
    := let y := x in f y.
  Lemma Let_In_def : @Let_In = fun A P x f => let y := x in f y.
  Proof. reflexivity. Qed.
  Global Strategy 100 [Let_In].
  Hint Opaque Let_In : rewrite.
  Global Instance Let_In_nd_Proper {{A P}}
    : Proper ((@eq A) ==> pointwise_relation _ (@eq P) ==> (@eq P)) (@Let_In A (fun _ => P)).
  Proof. cbv; intros; subst. apply app_cong. apply lambda_extensionality. exact H0. exact H. Qed.
End LetIn.

Section Test.
  Local Parameter nat : Set.
  Local Parameter v0 : nat.
  Local Parameter O : nat.
  Local Parameter add : nat -> nat -> nat.
  Local Parameter add_r_O : (forall (n: nat), (((eq nat) ((add n) O)) n)).

  Instance eq_Equivalence (A : Type) : Equivalence (@eq A) := {{
    Equivalence_Reflexive := @eq_refl A;
    Equivalence_Symmetric := @eq_sym A;
    Equivalence_Transitive := @eq_trans A
  }}.

  Goal exists v', eq nat (
{lets}
    ) v'.
  Proof.
    eexists.
    Time setoid_rewrite add_r_O.
    apply eq_refl.
  Qed.
  
End Test.
"""


def _generate_mengine_let_bindings(n):
    lines = []
    for i in range(1, n + 1):
        vprev = "v0" if i == 1 else f"v{i-1}"
        lines.append(f"   let v{i}: nat := add (add {vprev} {vprev}) O in")
    last = f"v{n}" if n > 0 else "v0"
    lines.append(f"   add (add {last} {last}) O")
    return "\n".join(lines)


def _generate_let_bindings(n):
    lines = []
    for i in range(1, n + 1):
        vprev = "v0" if i == 1 else f"v{i-1}"
        lines.append(f"    let v{i} := add (add {vprev} {vprev}) O in")
    lines.append(f"    add (add v{n} v{n}) O")
    return "\n".join(lines)


def _generate_lean_let_bindings(n):
    lines = []
    for i in range(1, n + 1):
        vprev = "v0" if i == 1 else f"v{i-1}"
        lines.append(f"  let v{i} := add (add {vprev} {vprev}) O")
    last = f"v{n}" if n > 0 else "v0"
    lines.append(f"  add (add {last} {last}) O")
    return "\n".join(lines)


class Addr0LetIn(Benchmark):
    @property
    def name(self):
        return "addr0_let_in"

    @property
    def description(self):
        return "Rewriting add_r_O inside nested let-bindings"

    @property
    def params(self):
        return [ParamSpec("n", start=1, stop=1001, step=10)]

    @property
    def x_label(self):
        return "n (# let-bindings)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "Mengine", color="blue", marker="x"),
            Strategy("coq", "letin", "Rocq: let ... in (repeat rewrite)", color="red", marker="o"),
            Strategy("coq", "letin_const", "Rocq: LetIn const (setoid_rewrite)", color="red", marker="s"),
            Strategy("lean", "repeat_rw", "Lean: repeat rw [add_r_O]", color="green", marker="v"),
            Strategy("lean", "simp_only", "Lean: simp only [add_r_O]", color="green", marker="d"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            lets = _generate_mengine_let_bindings(n)
            last = f"v{n}" if n > 0 else "v0"
            content = f"""Axiom nat : Type.
Axiom add : forall (_: nat), forall (_: nat), nat.
Axiom O : nat.
Axiom v0 : nat.
Lemma add_r_O : forall (x: nat), eq nat (add x O) x.
Admitted.

Theorem bench : eq nat
    (
{lets}
    )
    v0.
rewrite add_r_O with eq.
Admitted.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "coq":
            lets = _generate_let_bindings(n)
            if strategy.name == "letin":
                content = COQ_LETIN_TEMPLATE.format(lets=lets)
            else:
                content = COQ_LETIN_CONST_TEMPLATE.format(lets=lets)
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "lean":
            tactic_map = {
                "repeat_rw": "repeat rw [add_r_O]",
                "simp_only": "simp only [add_r_O]",
            }
            lets = _generate_lean_let_bindings(n)
            last = f"v{n}" if n > 0 else "v0"
            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 50000
section Test

axiom nat : Type
axiom add : nat → nat → nat
axiom O : nat
axiom v0 : nat
axiom add_r_O : ∀ (n : nat), add n O = n

theorem bench : ∃ v', (
{lets}
) = v' := by
  {tactic_map[strategy.name]}
  exact ⟨_, rfl⟩
end Test
"""
            path = os.path.join(workdir, "test.lean")
            with open(path, "w") as f:
                f.write(content)
            return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]

        return [engine_path, generated_file]
