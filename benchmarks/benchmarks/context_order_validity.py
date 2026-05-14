"""Context validity and order-structure behavior.

The generated program creates a deep context and repeatedly checks an early
variable in the final context. This isolates valid_in_context/context ancestry:
linked-list order pays for the depth of each query, while order_demain should
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
        return [Strategy("mengine", "native", "Mengine", color="blue", marker="x")]

    def generate(self, strategy, params, workdir):
        n = params["n"]
        declarations = ["Axiom nat : Type."]
        declarations.extend(f"Axiom x{i} : nat." for i in range(n))
        checks = ["Check x0." for _ in range(n)]

        content = "\n".join(declarations + [""] + checks) + "\n"
        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        return [engine_path, "-q", generated_file]
