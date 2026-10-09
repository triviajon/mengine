"""
Template for a new benchmark.

  1. cp _template.py my_bench.py
  2. Rename the class and set `name`, `description`, `params`, and `strategies`.
  3. Implement `generate()` for each engine: 
        Each engine should write the file to be type checked into a temporary file in
        `workdir` and must return its path
  4. Check it, then run it:
     bench.py test my_bench --engine coq
     bench.py run my_bench
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


class TemplateBenchmark(Benchmark):

    @property
    def name(self):
        return "template"  # TODO: unique CLI identifier, e.g. "my_bench"

    @property
    def description(self):
        return "TODO: one line on what this benchmark measures"

    @property
    def params(self):
        # For an example of using more than one parameter, see rewrite_nm.py
        return [ParamSpec("n", start=1, stop=101, step=10)]

    @property
    def strategies(self):
        return [
            Strategy("mengine", "native", "MEngine", color="blue", marker="x"),
            Strategy("coq", "tactic", "Rocq: tactic", color="red", marker="o"),
            Strategy("lean", "tactic", "Lean: tactic",
                     color="green", marker="v"),
        ]

    def generate(self, strategy, params, workdir):
        if strategy.engine == "mengine":
            # TODO: write the input file (test.me) into workdir, return its path
            raise NotImplementedError
        if strategy.engine == "coq":
            # TODO: write the proof script (test.v) into workdir, return its path
            raise NotImplementedError
        if strategy.engine == "lean":
            # TODO: write the proof (test.lean) into workdir, return its path
            raise NotImplementedError
        raise ValueError(f"unknown engine: {strategy.engine}")

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]
        if strategy.engine == "lean":
            return [engine_path, "--tstack=1000000", generated_file]
        return [engine_path, generated_file]
