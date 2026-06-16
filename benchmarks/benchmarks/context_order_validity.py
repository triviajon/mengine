"""Context validity and order-structure behavior.

The generated program creates a deep context and repeatedly checks an early
variable in the final context. This isolates valid_in_context/context ancestry:
linked-list order pays for the depth of each query, while order_tagrange should
answer the same ancestry query in constant time after insertion maintenance.
"""

import os

from framework.benchmark import Benchmark, ParamSpec, Strategy


class ContextOrderValidity(Benchmark):
    @property
    def name(self):
        return "context_order_validity"

    @property
    def description(self):
        return "Deep-context valid_in_context queries for order-backend comparison"

    @property
    def params(self):
        return [ParamSpec("n", start=100, stop=5001, step=100)]

    @property
    def x_label(self):
        return "n (context depth and query count)"

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
        declarations = ["Axiom nat : Type."]
        declarations.extend(f"Axiom x{i} : nat." for i in range(n))
        checks = ["Check x0." for _ in range(n)]

        content = "\n".join(declarations + [""] + checks) + "\n"
        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_coq(self, n, workdir):
        declarations = ["Axiom Nat_ : Set."]
        declarations.extend(f"Axiom x{i} : Nat_." for i in range(n))
        checks = ["Check x0." for _ in range(n)]

        content = "\n".join(declarations + [""] + checks) + "\n"
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean(self, n, workdir):
        declarations = ["axiom Nat_ : Type"]
        declarations.extend(f"axiom x{i} : Nat_" for i in range(n))
        checks = ["#check x0" for _ in range(n)]

        content = "\n".join(["set_option maxHeartbeats 0", "set_option maxRecDepth 1000000", ""] + declarations + [""] + checks) + "\n"
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
