## MEngine

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

*By ~linear we mean the trend of the curves over the range of n in which Rocq's strategies do not time out.*

### Installation Instructions

#### Using the VM image

To use the [provided](https://doi.org/10.5281/zenodo.23197497) VM image, import `mengine-ae-amd64.ova` (or `mengine-ae-arm64.ova` on Apple Silicon) into VirtualBox 7.1 or later with File > Import Appliance. You should have about 9 GiB of free disk. The VM boots into a desktop logged in as user `artifact` (password `artifact`). It has 4 CPUs and 8 GB of RAM by default, and attempts to port-forward `localhost:2222` to port `22` on the VM so that files and plots can be copied out by you or your favorite AI assistant. No further installation is required for MEngine, Rocq 9.1.1 with coqutil, or Lean v4.32.0-rc1.

Once the desktop envirnoment is running, you can start by opening the terminal and `cd ~/mengine`. 

#### Without the VM image

`scripts/ae/provision-ubuntu.sh` installs everything the VM has on Ubuntu 24.04 (run it from the repository root). If you only need MEngine, the following commands are enough:
```shell
apt update
apt install clang make python3-matplotlib
make release
```

#### Sanity check

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

Running MEngine on this file (in quiet mode) will print to stdout a fully parseable Rocq file that contains a term which has the same type as `rewrite_zeros_let` and the same context as `rewrite_zeros_let`. To check it with the Rocq kernel:
```shell
./build/mengine -q examples/rewrite_add_n_0.me > export.v
coqc export.v; echo $?
```

Note that all of the variables in the exported Rocq file are named by their memory address. This is because the MEngine kernel only uses the user-friendly name of a variable for printing, but it is not considered part of the identity of a variable.

> Claim 2: 77 stdlib lemmas typecheck in both Rocq and MEngine

To evaluate this claim, you can run the following commands in the `benchmarks` directory:
```shell
python3 stdlib/stdlib_bench.py test      # faithfulness gate (see below)
python3 stdlib/stdlib_bench.py fidelity  # check each statement vs the real stdlib
python3 stdlib/stdlib_bench.py regen     # rebuild every generated file, in dependency order
python3 stdlib/stdlib_bench.py clean     # remove every generated file (keep sources)
```

More details about the `stdlib_bench.py` script can be found in the `benchmarks/stdlib/README.md` file.

> Claims 3-8: various benchmarks showcasing the performance of MEngine compared to Rocq and Lean

We provide a suite of benchmarks in the `benchmarks` directory that showcase the performance of MEngine compared to Rocq and Lean. These benchmarks are designed to be runnable on the VM image provided by the authors. Otherwise, you may need to edit `benchmarks/config.json` to point to the correct paths. 

Once configured, you can smoke test everything is installed correctly by running, from the `benchmarks` directory:
```shell
python3 bench.py test
```
This runs each benchmark with small inputs once to check that everything is installed correctly. In a VM, the first Lean run after boot can take a minute while Lean's library is read from disk.

To run a specific benchmark, list existing benchmarks with `python3 bench.py list` and then run `python3 bench.py run <benchmark_name>`. Each benchmark is implemented for MEngine, Rocq, and Lean, referred to as engines. Some benchmarks have multiple proof strategies for a particular engine (e.g., using `rewrite` vs `rewrite_strat` for Rocq). To use a particular engine, use the `--engine` flag and all strategies for that engine will be run.

```shell
python3 bench.py --results-dir results/mine run rewrite_nm
python3 bench.py --results-dir results/mine run rewrite_nm --engine mengine
```

The paper's results and plots are in  `benchmarks/results` and `benchmarks/plots` (i7-12700K, 16 GB RAM, WSL2). To rerun the six benchmarks of the paper into a separate directory and plot them, run:
```shell
for b in rewrite_fa repeat_mod addr0_let_in separation_logic symbolic_execution rewrite_nm; do
    python3 bench.py --results-dir results/mine run $b
done

# Plot the results of `results/mine` into `plots/mine`
python3 bench.py --results-dir results/mine --plots-dir plots/mine plot
```
Please note that this will take a while (in our experience, about 4 hours, with MEngine peaking at about 2 GiB of memory). The absolute times will likely differ from the paper when in the VM.

Without `--results-dir`, `run` results attempts to write JSON files to `benchmarks/results`, but it will skip points already measured unless you use `run --force`. 

More detailed instructions for running the benchmarks can be found in the `benchmarks/README.md` file. For those interested in adding a new benchmark, we recommend copying `benchmarks/benchmarks/_template.py` as a template and modifying it to fit your needs.

## Other

### File Structure

The file structure of the repository is as follows:
```
.
├── benchmarks # contains the suite of benchmarks
├── examples # examples of supported MEngine language syntax
├── prelude/tactics.me # file that is imported by default on every mengine invocation
├── scripts # development scripts and artifact evaluation scripts
├── src # source code of MEngine
├── tests # test suite for MEngine
├── Makefile 
└── README.md
```

The `src/` directory is further separated into the following subdirectories:
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

#### Kernel and Engine

**`engine/`**: The proof engine described in Section 3. A `ProofState` holds the pending theorem and the list of open goals, where each goal is a hole with an expected type. `tactics.c` implements the primitive tactics, for example `intro`, `apply`, `exact`, `rewrite`, and `reflexivity`. `unify.c` unifies the type of a lemma with the type of a goal, instantiates the lemma, and returns the new subgoals.

**`kernel/`**: The kernel implements the dependently typed core described in Section 2. `expression.h` contains the key data structures used for the kernel. Each expression constructed using the constructor is treated as trusted. `context.c` manages explicit contexts, with two interchangeable backends for order maintenance (`order_tagrange.c`, the default, and `order_linkedlist.c`). 

#### Lanaguage

**`commandlanguage/`**: The parser and executor for the top-level command language, which is similar to the vernacular of Rocq. `commandlanguage/bnf.md` contains the grammar of the command language.

**`tacticlanguage/`**: The parser and interpreter for the tactic language, which runs during proof mode. The language provides some primitive combinators `;`, `||`, `try`, `repeat`, `first`, and `match Goal`, and a `Tactic name := ...` command defines new tactics (grammar is fully described in `tacticlanguage/bnf.md`). The tactic interpreter only implements the necessary primitives to support the benchmarks in the paper, and is not currently intended to be a full-featured tactic language.

**`termlanguage/`**: The parser for the term/expression language. The grammar covers `fun`, `forall`, `let`, `match`, and `fix` expressions, and applications. `parser.c` parses a term into a generic AST, and `ast_to_expression.c` converts the AST into a kernel expression.

#### Other

**`common/`**: Shared infrastructure reused by all layers.

**`runtime/`**: `runtime.c` defines the `MEngineRuntime`, which holds the global context, the proof state, and the environment of user-defined tactics. The runtime switches between command mode (which is used to make declarations) and proof mode (which is used to apply tactics to solve a goal). The runtime is also responsible for handling the REPL and loading the prelude. 

**`main.c`**: The CLI entry point.
