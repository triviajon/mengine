"""Substitution over a shared let-bound subtree.

This benchmark builds a dependent function type whose body repeats the same
let-bound expression many times. Applying the lemma substitutes through that
body, so the benchmark isolates whether substitution preserves/reuses the
shared subtree instead of rebuilding the same result for every occurrence.
"""

import os

from framework.benchmark import Benchmark, ParamSpec, Strategy


class SubstitutionSharing(Benchmark):
    @property
    def name(self):
        return "substitution_sharing"

    @property
    def description(self):
        return "Substitution through a dependent type with repeated references to one shared let-bound subtree"

    @property
    def params(self):
        return [ParamSpec("n", start=10, stop=10000, step=100)]

    @property
    def x_label(self):
        return "n (shared occurrences)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "Mengine", color="blue", marker="x"),
            Strategy("coq", "native", "Rocq", color="red", marker="o"),
            Strategy("lean", "native", "Lean", color="green", marker="v"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]
        if strategy.engine == "mengine":
            return self._generate_mengine(n, workdir)
        if strategy.engine == "coq":
            return self._generate_coq(n, workdir)
        if strategy.engine == "lean":
            return self._generate_lean(n, workdir)

    def _generate_mengine(self, n, workdir):
        r_type = "Prop"
        for _ in range(n):
            r_type = f"forall (_: nat), {r_type}"
        y_args = " ".join(["y"] * n)

        content = f"""Axiom nat : Type.
Axiom zero : nat.
Axiom arg : nat.
Axiom add : forall (_: nat), forall (_: nat), nat.
Axiom R : {r_type}.
Axiom lemma : forall (x: nat), let y: nat := (add x zero) in R {y_args}.

Check (lemma arg).
"""
        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_coq(self, n, workdir):
        r_type = "Prop"
        for _ in range(n):
            r_type = f"Nat_ -> {r_type}"
        y_args = " ".join(["y"] * n)

        content = f"""Axiom Nat_ : Set.
Axiom zero : Nat_.
Axiom arg : Nat_.
Axiom add : Nat_ -> Nat_ -> Nat_.
Axiom R : {r_type}.
Axiom lemma : forall (x : Nat_), let y := add x zero in R {y_args}.

Check (lemma arg).
"""
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean(self, n, workdir):
        r_type = "Prop"
        for _ in range(n):
            r_type = f"Nat_ → {r_type}"
        y_args = " ".join(["y"] * n)

        content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 1000000

axiom Nat_ : Type
axiom zero : Nat_
axiom arg : Nat_
axiom add : Nat_ → Nat_ → Nat_
axiom R : {r_type}
axiom mylemma : ∀ x : Nat_, let y := add x zero; R {y_args}

#check (mylemma arg)
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
