"""
List fold benchmark: sum_list [1; 1; ...; 1] = n

Tests fixpoint evaluation over a recursive list structure.
Mengine: native (fix/iota reduction)
Coq: reflexivity, vm_compute (kernel vs. VM)
Lean: rfl (kernel reduction)
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


class ListFold(Benchmark):
    @property
    def name(self):
        return "list_fold"

    @property
    def description(self):
        return "Reducing sum_list of n ones to n by fixpoint evaluation"

    @property
    def params(self):
        return [ParamSpec("n", start=10, stop=801, step=20)]

    @property
    def x_label(self):
        return "n (list length)"

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
            cons_chain = "nil"
            for _ in range(n):
                cons_chain = f"(cons (S O) {cons_chain})"

            nat_chain = "O"
            for _ in range(n):
                nat_chain = f"(S {nat_chain})"

            content = f"""Inductive nat : Type := | O : nat | S : forall (_: nat), nat.
Inductive list : Type := | nil : list | cons : forall (_: nat), forall (_: list), list.

Fixpoint add (m : nat) (n : nat) {{struct m}} : nat :=
  match m with
  | O => n
  | S m' => S (add m' n)
  end.

Fixpoint sum_list (l : list) {{struct l}} : nat :=
  match l with
  | nil => O
  | cons x xs => add x (sum_list xs)
  end.

Theorem bench : eq nat (sum_list {cons_chain}) {nat_chain}.
apply eq_refl.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "coq":
            ones = "; ".join(["1"] * n)
            content = f"""Require Import List Arith.
Import ListNotations.

Fixpoint sum_list (l : list nat) : nat :=
  match l with
  | [] => 0
  | x :: xs => x + sum_list xs
  end.

Theorem bench : sum_list [{ones}] = {n}.
Proof. reflexivity. Qed.
"""
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "lean":
            ones = ", ".join(["1"] * n)
            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 100000

def sumList : List Nat → Nat
  | [] => 0
  | x :: xs => x + sumList xs

theorem bench : sumList [{ones}] = {n} := by rfl
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
