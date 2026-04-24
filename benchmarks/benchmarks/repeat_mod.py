"""
Repeated modulo rewriting: (mod (mod ... b p) p) = (mod b p)

Tests rewriting nested modulo chains.
Mengine: native
Coq: repeat rewrite, repeat setoid_rewrite, rewrite_strat bottomup/topdown
Lean: repeat rw, simp only
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


class RepeatMod(Benchmark):
    @property
    def name(self):
        return "repeat_mod"

    @property
    def description(self):
        return "Rewriting nested modulo chains (mod (mod ... b p) p) = (mod b p)"

    @property
    def params(self):
        return [ParamSpec("n", start=2, stop=2001, step=25)]

    @property
    def x_label(self):
        return "n (# rewriting locations)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "Mengine", color="blue", marker="x"),
            Strategy("coq", "repeat_rewrite", "Rocq: repeat rewrite", color="red", marker="o"),
            Strategy("coq", "repeat_setoid_rewrite", "Rocq: repeat setoid_rewrite", color="red", marker="*"),
            Strategy("coq", "rewrite_bottomup", "Rocq: rewrite_strat bottomup", color="red", marker="s"),
            Strategy("coq", "rewrite_topdown", "Rocq: rewrite_strat topdown", color="red", marker="D"),
            Strategy("lean", "repeat_rw", "Lean: repeat rw [mod_mod]", color="green", marker="v"),
            Strategy("lean", "simp_only", "Lean: simp only [mod_mod]", color="green", marker="d"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            lhs = "b"
            for _ in range(n):
                lhs = f"(mod {lhs} p)"
            rhs = "(mod b p)"
            content = f"""Axiom nat : Type.
Axiom b : nat.
Axiom p : nat.
Axiom mod : forall (_: nat), forall (_: nat), nat.
Lemma mod_mod : forall (a: nat), forall (n: nat), eq nat (mod (mod a n) n) (mod a n).
Admitted.

Theorem bench : eq nat {lhs} {rhs}.
rewrite mod_mod with eq.
apply eq_refl.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "coq":
            tactic_map = {
                "repeat_rewrite": "repeat rewrite",
                "repeat_setoid_rewrite": "repeat setoid_rewrite",
                "rewrite_bottomup": "rewrite_strat bottomup",
                "rewrite_topdown": "rewrite_strat topdown",
            }
            tactic = tactic_map[strategy.name]

            lhs = "b"
            for _ in range(n):
                lhs = f"(mod {lhs} p)"
            rhs = "mod b p"

            content = f"""Require Import Morphisms Setoid.
Section Test.

Variable nat : Set.
Variable b : nat.
Variable p : nat.
Variable mod : nat -> nat -> nat.
Variable mod_mod : forall (a : nat) (n : nat), (mod (mod a n) n) = (mod a n).

Theorem bench : {lhs} = {rhs}.
Proof.
  {tactic} mod_mod. reflexivity.
Qed.
End Test.
"""
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "lean":
            tactic_map = {
                "repeat_rw": "repeat rw [mod_mod]",
                "simp_only": "simp only [mod_mod]",
            }
            tactic = tactic_map[strategy.name]

            lhs = "b"
            for _ in range(n):
                lhs = f"mod ({lhs}) p"
            rhs = "mod b p"

            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 100000

axiom nat : Type
axiom b : nat
axiom p : nat
axiom mod : nat → nat → nat
axiom mod_mod : ∀ (a n : nat), mod (mod a n) n = mod a n

theorem bench : {lhs} = {rhs} := by
  {tactic}
"""
            path = os.path.join(workdir, "test.lean")
            with open(path, "w") as f:
                f.write(content)
            return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]

        return [engine_path, generated_file]
