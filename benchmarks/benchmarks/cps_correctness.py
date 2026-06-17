"""
CPS correctness: cps e id = eval e for right-nested addition expression of depth n

Tests beta reduction through continuation lambdas via the CPS correctness theorem.
Mengine: native (apply eq_refl)
Coq: reflexivity, vm_compute (kernel vs. VM)
Lean: rfl (kernel reduction)
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


def nat_n(n):
    result = "O"
    for _ in range(n):
        result = f"(S {result})"
    return result


class CpsCorrectness(Benchmark):
    @property
    def name(self):
        return "cps_correctness"

    @property
    def description(self):
        return "CPS correctness: cps e id = eval e for right-nested addition expression of depth n"

    @property
    def params(self):
        return [ParamSpec("n", start=1, stop=151, step=5)]

    @property
    def x_label(self):
        return "n (expression depth)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "MEngine", color="blue", marker="x"),
            Strategy("coq", "reflexivity", "Rocq: reflexivity", color="red", marker="o"),
            Strategy("lean", "rfl", "Lean: rfl", color="green", marker="v"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            SO = "(S O)"
            expr = f"(Const {SO})"
            for _ in range(n - 1):
                expr = f"(Add (Const {SO}) {expr})"

            expected = nat_n(n)

            content = f"""Inductive nat : Type := | O : nat | S : forall (_: nat), nat.

Fixpoint add (n : nat) (m : nat) {{struct n}} : nat :=
  match n with | O => m | S n' => S (add n' m) end.

Inductive expr : Type :=
  | Const : forall (_: nat), expr
  | Add : forall (_: expr), forall (_: expr), expr.

Fixpoint eval (e : expr) {{struct e}} : nat :=
  match e with
  | Const n => n
  | Add e1 e2 => add (eval e1) (eval e2)
  end.

Fixpoint cps (e : expr) (k : forall (_: nat), nat) {{struct e}} : nat :=
  match e with
  | Const n => k n
  | Add e1 e2 => cps e1 (fun (v1 : nat) => cps e2 (fun (v2 : nat) => k (add v1 v2)))
  end.

Theorem bench : eq nat (cps {expr} (fun (x : nat) => x)) {expected}.
apply eq_refl.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "coq":
            expr_coq = "Const 1"
            for _ in range(n - 1):
                expr_coq = f"Add (Const 1) ({expr_coq})"
            content = f"""Section CPS.

Inductive expr : Type :=
  | Const : nat -> expr
  | Add : expr -> expr -> expr.

Fixpoint eval (e : expr) : nat :=
  match e with
  | Const n => n
  | Add e1 e2 => eval e1 + eval e2
  end.

Fixpoint cps (e : expr) (k : nat -> nat) : nat :=
  match e with
  | Const n => k n
  | Add e1 e2 => cps e1 (fun v1 => cps e2 (fun v2 => k (v1 + v2)))
  end.

Theorem bench : cps ({expr_coq}) (fun x => x) = {n}.
Proof. reflexivity. Qed.

End CPS.
"""
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "lean":
            expr_lean = ".const 1"
            for _ in range(n - 1):
                expr_lean = f".add (.const 1) ({expr_lean})"

            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 100000

inductive Expr where
  | const : Nat → Expr
  | add : Expr → Expr → Expr

def eval : Expr → Nat
  | .const n => n
  | .add e1 e2 => eval e1 + eval e2

def cps (e : Expr) (k : Nat → Nat) : Nat :=
  match e with
  | .const n => k n
  | .add e1 e2 => cps e1 (fun v1 => cps e2 (fun v2 => k (v1 + v2)))

theorem bench : cps ({expr_lean}) (fun x => x) = {n} := by rfl
"""
            path = os.path.join(workdir, "test.lean")
            with open(path, "w") as f:
                f.write(content)
            return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]
        if strategy.engine == "lean":
            return [engine_path, "--tstack=1000000", generated_file]
        return [engine_path, generated_file]
