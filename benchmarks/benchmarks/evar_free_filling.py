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
        return [ParamSpec("n", start=10, stop=2001, step=50)]

    @property
    def x_label(self):
        return "n (proof-term size)"

    @property
    def strategies(self):
        return [Strategy("mengine", "native", "Mengine", color="blue", marker="x")]

    def generate(self, strategy, params, workdir):
        n = params["n"]
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

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        return [engine_path, "-q", generated_file]
