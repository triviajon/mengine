"""
Rewriting with cascading let-bindings and n-ary function applications.

Three variants controlled by which parameter is fixed:
  - rewrite_nm_fixedm3: fix m=3, vary n
  - rewrite_nm_fixedm5: fix m=5, vary n
  - rewrite_nm_fixedn3: fix n=3, vary m

This benchmark uses a single two-parameter definition.
The old suite split these into 3 separate directories with duplicated code.
Here we define one benchmark class with both n and m as parameters,
and let the plotter handle showing slices with one param fixed.

Mengine: native engine
Coq: multiple strategies
Lean: simp only
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


COQ_STRATEGIES = {
    "rewrite_bang": "rewrite!",
    "repeat_rewrite": "repeat rewrite",
    "repeat_setoid_rewrite": "repeat setoid_rewrite",
    "rewrite_bottomup": "rewrite_strat bottomup",
    "rewrite_topdown": "rewrite_strat topdown",
}


class RewriteNM(Benchmark):
    """
    Two-parameter benchmark: n (arity of f) and m (depth of let-nesting).
    
    To reproduce the old fixed-m=3, fixed-m=5, fixed-n=3 plots, use:
      bench.py plot rewrite_nm --fixed m=3
      bench.py plot rewrite_nm --fixed m=5
      bench.py plot rewrite_nm --fixed n=3
    
    Or run specific slices:
      bench.py run rewrite_nm --override n=1:4000:25 --override m=3:4:1
    """

    @property
    def name(self):
        return "rewrite_nm"

    @property
    def description(self):
        return "Rewriting f(x,...,x)=x in let x1:=f x0..x0 in ... let xm:=f x(m-1)..x(m-1) in xm=x0"

    @property
    def params(self):
        return [
            ParamSpec("n", start=1, stop=4001, step=25),
            ParamSpec("m", start=1, stop=11, step=1),
        ]

    @property
    def x_label(self):
        return "n (arity of f)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "Mengine", color="blue", marker="x"),
            Strategy("coq", "rewrite_bang", "Rocq: rewrite!", color="red", marker="P"),
            Strategy("coq", "repeat_rewrite", "Rocq: repeat rewrite", color="red", marker="o"),
            Strategy("coq", "repeat_setoid_rewrite", "Rocq: repeat setoid_rewrite", color="red", marker="*"),
            Strategy("coq", "rewrite_bottomup", "Rocq: rewrite_strat bottomup", color="red", marker="s"),
            Strategy("coq", "rewrite_topdown", "Rocq: rewrite_strat topdown", color="red", marker="D"),
            Strategy("lean", "simp_only", "Lean: simp only [f_n_x0]", color="green", marker="v"),
            Strategy("lean", "repeat_rw", "Lean: repeat rw [f_n_x0]", color="green", marker="d"),
        ]

    def generate(self, strategy, params, workdir):
        n = params["n"]
        m = params["m"]

        if strategy.engine == "mengine":
            # Axiomize nat, x0, and f with n arguments
            f_type_parts = ["forall (_: nat)"] * n + ["nat"]
            f_type = ", ".join(f_type_parts[:n]) + ", " + f_type_parts[-1] if n > 0 else "nat"
            # Build f type as nested foralls
            f_type = "nat"
            for _ in range(n):
                f_type = f"forall (_: nat), {f_type}"

            x0_args = " ".join(["x0"] * n)

            let_bindings = []
            for i in range(1, m + 1):
                prev = "x0" if i == 1 else f"x{i-1}"
                args = " ".join([prev] * n)
                let_bindings.append(f"    let x{i}: nat := (f {args}) in")
            let_str = "\n".join(let_bindings)

            content = f"""Axiom nat : Type.
Axiom x0 : nat.
Axiom f : {f_type}.
Lemma f_n_x0 : eq nat (f {x0_args}) x0.
Admitted.

Theorem bench : eq nat
    (\n{let_str}\n    x{m}\n    )
    x0.
rewrite f_n_x0 with eq.
apply eq_refl.
"""
            path = os.path.join(workdir, "test.me")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "coq":
            nat_args = " -> ".join(["nat"] * (n + 1))
            x0_args = " ".join(["x0"] * n)

            let_bindings = []
            for i in range(1, m + 1):
                prev = "x0" if i == 1 else f"x{i-1}"
                args = " ".join([prev] * n)
                let_bindings.append(f"        let x{i} := f {args} in")
            let_str = "\n".join(let_bindings)

            tactic = COQ_STRATEGIES[strategy.name]

            content = f"""Require Import Setoid Morphisms.
Section Test.
    Variable nat : Set.
    Variable x0 : nat.
    Variable f : {nat_args}.

    Lemma f_n_x0 : f {x0_args} = x0. Admitted.

    Theorem bench :
{let_str}
        x{m} = x0.
    Proof.
        simpl.
        {tactic} f_n_x0.
        apply eq_refl.
    Qed.
End Test.
"""
            path = os.path.join(workdir, "test.v")
            with open(path, "w") as f:
                f.write(content)
            return path

        if strategy.engine == "lean":
            nat_args = " → ".join(["nat"] * (n + 1))
            x0_args = " ".join(["x0"] * n)

            let_bindings = []
            for i in range(1, m + 1):
                prev = "x0" if i == 1 else f"x{i-1}"
                args = " ".join([prev] * n)
                let_bindings.append(f"        let x{i} := f {args}")
            let_str = "\n".join(let_bindings)

            tactic_map = {
                "simp_only": "simp only [f_n_x0]",
                "repeat_rw": "repeat rw [f_n_x0]",
            }

            content = f"""set_option maxHeartbeats 0
set_option maxRecDepth 1000000
variable (nat : Type)
variable (x0 : nat)
variable (f : {nat_args})

theorem f_n_x0 : f {x0_args} = x0 := sorry

theorem bench :
  {let_str}
  x{m} = x0 := by
  {tactic_map[strategy.name]}
"""
            path = os.path.join(workdir, "test.lean")
            with open(path, "w") as f:
                f.write(content)
            return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]

        return [engine_path, generated_file]
