"""Filling a hole with a large evar-free term.

The benchmark fills the current theorem goal with a large proof term that does
not contain holes. With the evar-ref shortcut enabled, the occurs check should
not walk the whole term just to discover that the filled hole is absent.
"""

import os

from framework.benchmark import Benchmark, ParamSpec, Strategy


class EvarFreeFilling(Benchmark):
    @property
    def name(self):
        return "evar_free_filling"

    @property
    def description(self):
        return "Hole filling with a large evar-free proof term"

    @property
    def params(self):
        return [ParamSpec("n", start=10, stop=200010, step=10000)]

    @property
    def x_label(self):
        return "n (proof-term size)"

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
        term = "p"
        for _ in range(n):
            term = f"(idP {term})"

        content = f"""Axiom P : Prop.
Axiom p : P.
Axiom idP : forall (_: P), P.

Theorem bench : P.
let g := current_goal in fill g {term}.
"""
        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_coq(self, n, workdir):
        term = "p"
        for _ in range(n):
            term = f"(idP {term})"

        content = f"""Axiom P : Prop.
Axiom p : P.
Axiom idP : P -> P.

Theorem bench : P.
Proof. exact {term}. Qed.
"""
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean(self, n, workdir):
        term = "p"
        for _ in range(n):
            term = f"(idP {term})"

        content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 1000000

axiom P : Prop
axiom p : P
axiom idP : P → P

theorem bench : P := {term}
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
