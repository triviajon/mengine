## MENgine

MEngine is a dependently typed kernel and proof engine with explicit context management, a tactic language, and a theorem prover, optimized for highly automated proof scripts. 

### Claims 

The paper makes the following claims:

| # | Location                  | Claim                                                             | Evidence                              |
|---|---------------------------|-------------------------------------------------------------------|---------------------------------------|
| 1 | Section 1                 | Proof terms export to Rocq and the Rocq 9.1.1 kernel accepts them | `Print Rocq <ident>.`                 |
| 2 | Subsection 3.2            | 77 stdlib lemmas typecheck in both Rocq and MEngine               | `benchmarks/stdlib`                   |
| 3 | Subsection 4.1, Figure 5  | `rewrite_fa`: MEngine ~linear in n                                | `benchmarks/benchmarks/rewrite_fa.py` |
| 4 | Subsection 4.1, Figure 6  | `repeat_mod`: MEngine ~linear in n                                | `repeat_mod.py`                       |
| 5 | Subsection 4.2, Figure 7  | `rewrite_nm`: MEngine ~linear in n                                | `rewrite_nm.py`                       |
| 6 | Subsection 4.2, Figure 8  | `addr0_let_in`: MEngine and Lean `simp only` ~linear              | `addr0_let_in.py`                     |
| 7 | Subsection 4.3, Figure 9  | `cancel`: MEngine, Lean, Rocq (Bedrock2/Coqutil) comparable       | `separation_logic.py`                 |
| 8 | Subsection 4.4, Figure 10 | `symbolic_execution`: MEngine and SymM completes n = 241 in < 1s  | `symbolic_execution.py`               |

### Installation Instructions

Assuming you are on the VM image provided by the authors, no installation is required for MEngine, Rocq 9.1.1 with coqutil, or Lean v4.32.0-rc1. 

Without the VM image, you will need to install a few dependencies. On Ubuntu-like systems, you can run the following commands to build MEngine:
```shell
apt update
apt install clang make python3-matplotlib
make release
```

You can sanity check that MEngine is built correctly by running:
```shell
./build/mengine --help
./build/mengine version
./build/mengine examples/rewrite_simple.me
```

Running `./build/mengine` alone will start the interactive REPL:
```shell
$ ./build/mengine                                        
MEngine REPL. Type 'quit.' to exit.
> Check eq.
eq
        : forall (A: Type), forall (x: A), forall (y: A), Prop
> ^C
$ 
```

### Evaluation Instructions

> Claim 1: Proof terms export to Rocq and the Rocq 9.1.1 kernel accepts them

To evaluate this claim, please add the following command to the end of the file you wish to export a term (and its associated context) to Rocq:
```
Print Rocq <ident>.
```
For example, the end of the file `examples/rewrite_add_n_0.me` contains the command `Print Rocq rewrite_zeros_let.`: 

```coq
(* Our 2nd example: rewriting the same expression built with let-bindings *)
Theorem rewrite_zeros_let : eq nat 
    (let v1: nat := (add v0 zero) in
     let v2: nat := (add v1 zero) in
     let v3: nat := (add v2 zero) in
     v3
    )
    (v0).
(* Note: This notation 'rewrite _ with eq.' is temporary, currently has no bearing on the proof, and will be removed in the future. *)
rewrite add_n_0 with eq.
apply eq_refl.

Print Rocq rewrite_zeros_let.
```

Running MEngine on this file (in quiet mode) will print to stdout a fully parseable Rocq file that contains a term which has the same type as `rewrite_zeros_let` and the same context as `rewrite_zeros_let`.

Note that all of the variables in the exported Rocq file are named by their memory address. This is because the MEngine kernel only uses the user-friendly name of a variable for printing, but it is not considered part of the identity of a variable.

> Claim 2: 77 stdlib lemmas typecheck in both Rocq and MEngine

To evaluate this claim, you can run the following commands in the `benchmarks` directory:
```shell
python3 stdlib/stdlib_bench.py regen     # rebuild every generated file, in dependency order
python3 stdlib/stdlib_bench.py clean     # remove every generated file (keep sources)
python3 stdlib/stdlib_bench.py test      # faithfulness gate (see below)
python3 stdlib/stdlib_bench.py fidelity  # check each statement vs the real stdlib
```

More details about the `stdlib_bench.py` script can be found in the `benchmarks/stdlib/README.md` file.

> Claims 3-8: various benchmarks showcasing the performance of MEngine compared to Rocq and Lean

We provide a suite of benchmarks in the `benchmarks` directory that showcase the performance of MEngine compared to Rocq and Lean. These benchmarks are designed to be runnable on the VM image provided by the authors. Otherwise, you may need to edit `benchmarks/config.json` to point to the correct paths. 

Once configured, you can smoke test everything is installed correctly by running:
```shell
python3 bench.py test
```
This runs each benchmark with small inputs once to check that everything is installed correctly.

To run a specific benchmark, list existing benchmarks with `python3 bench.py list` and then run `python3 bench.py run <benchmark_name>`. Each benchmark is implemented for MEngine, Rocq, and Lean, referred to as engines. Some benchmarks have multiple proof strategies for a particular engine (e.g., using `rewrite` vs `rewrite_strat` for Rocq). To use a particular engine, use the `--engine` flag and all strategies for that engine will be run.

```shell
python3 benchmarks/bench.py run rewrite_nm
python3 benchmarks/bench.py run rewrite_nm --engine mengine
```

To run the full benchmark suite as ran for the paper, run:
```shell
python3 benchmarks/bench.py run --force
```
Using the force flag will overwrite any existing results. Please note that this will take a while (in our experience, about 4-5 hours with a peak memory usage of 2GiB). 

By default, the results will be written to the `benchmarks/results` directory in JSON files. To plot the results, run:
```shell
python3 benchmarks/bench.py plot
python3 benchmarks/bench.py plot [benchmark_name] # just a single benchmark

```

More detailed instructions for running the benchmarks can be found in the `benchmarks/README.md` file. For those interested in adding a new benchmark, we recommend copying `benchmarks/benchmarks/_template.py` as a template and modifying it to fit your needs.

## Other

### File Structure

The file structure of the repository is as follows:
```
.
├── benchmarks # contains the suite of benchmarks
├── examples # examples of supported MEngine language syntax and 
├── prelude/tactics.me # file that is imported by default on every mengine invocation
├── scripts # development scripts
├── src # source code of MEngine
├── tests # test suite for MEngine
├── Makefile 
└── README.md
```

The `src/` directory is separated into the following subdirectories:
```
.
├── commandlanguage # parser and executor for top-level commands
├── common # shared containers, lexer, and parser base
├── engine # proof engine: goals, tactics, unification, rewriting
├── kernel # dependently typed kernel: expressions, contexts, conversion
├── runtime # driver: global context, modes, REPL
├── tacticlanguage # parser and interpreter for proof scripts
├── termlanguage # parser for the term language
└── main.c # CLI entry point
```

**`commandlanguage/`**: The parser and executor for the top-level command language, which is similar to the vernacular of Rocq. `commandlanguage/bnf.md` contains the grammar of the command language.

**`common/`**: Shared infrastructure reused by all layers.

**`engine/`**: The proof engine described in Section 3. A `ProofState` holds the pending theorem and the list of open goals, where each goal is a hole with an expected type. `tactics.c` implements the primitive tactics, for example `intro`, `apply`, `exact`, `rewrite`, and `reflexivity`. `unify.c` unifies the type of a lemma with the type of a goal, instantiates the lemma, and returns the new subgoals.

**`kernel/`**: The kernel implements the dependently typed core described in Section 2. `expression.h` contains the key data structures used for the kernel. Each expression constructed using the constructor is treated as trusted. `context.c` manages explicit contexts, with two interchangeable backends for order maintenance (`order_demain.c`, `order_linkedlist.c`). 

**`runtime/`**: `runtime.c` defines the `MEngineRuntime`, which holds the global context, the proof state, and the environment of user-defined tactics. The runtime switches between command mode (which is used to make declarations) and proof mode (which is used to apply tactics to solve a goal). The runtime is also responsible for handling the REPL and loading the prelude. 

**`tacticlanguage/`**: The parser and interpreter for the tactic language, which runs during proof mode. The language provides some primitive combinators `;`, `||`, `try`, `repeat`, `first`, and `match Goal`, and a `Tactic name := ...` command defines new tactics (grammar is fully described in `tacticlanguage/bnf.md`). The tactic interpreter only implements the necessary primitives to support the benchmarks in the paper, and is not currently intended to be a full-featured tactic language.

**`termlanguage/`**: The parser for the term/expression language. The grammar covers `fun`, `forall`, `let`, `match`, and `fix` expressions, and applications. `parser.c` parses a term into a generic AST, and `ast_to_expression.c` converts the AST into a kernel expression.

**`main.c`**: The CLI entry point.
