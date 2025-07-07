#!/usr/bin/env python3
"""
Simple script to generate Coq test files for measuring rewrite performance.
Generates files that test rewrite behavior with function f(x,...,x) = x.
"""

import os
import sys
from pathlib import Path
import subprocess
import time
import json

strategies = [
    "rewrite!",
    "repeat setoid_rewrite",
    "rewrite_strat topdown",
    "rewrite_strat bottomup"
]

def generate_coq_file(n, m, rewrite_strat, filename):
    """Generate a Coq file that tests n applications of function f with m arguments"""
    
    # Generate the function signature: f : nat -> ... -> nat (m+1 arguments total)
    nat_args = " -> ".join(["nat"] * (m + 1))
    
    # Generate the let bindings
    let_bindings = []
    for i in range(1, n + 1):
        if i == 1:
            # First binding uses x0
            args = " ".join(["x0"] * m)
            let_bindings.append(f"        let x{i} := f {args} in")
        else:
            # Subsequent bindings use the previous result
            args = " ".join([f"x{i-1}"] * m)
            let_bindings.append(f"        let x{i} := f {args} in")
    
    content = f"""Require Import Setoid Morphisms.
Section Test.
    Variable x0 : nat.
    Variable f : {nat_args}.

    Lemma f_n_x0 : f {" ".join(["x0"] * m)} = x0. Admitted.

    Goal 
{chr(10).join(let_bindings)}
        x{n} = x0.
    Proof.
        simpl.
        {rewrite_strat} f_n_x0.
        apply eq_refl.
    Qed.
End Test.
"""
    
    # Write to file
    with open(filename, 'w') as f:
        f.write(content)
    

def main():
    generate_coq_file(3, 3, "rewrite!", "test_n3_m3.v")
    exit()

    benchmark_results_fn = "generate_coq_tests_benchmark_results.json"    
    if os.path.exists(benchmark_results_fn):
        with open(benchmark_results_fn, "r") as f:
            results = json.load(f)
    else:
        results = {}
    
    # Generate test cases according to specified ranges
    test_cases = []
    
    # Test with varying n, fixed m=3
    for n in range(1, 300, 3):
        test_cases.append((n, 3))
    
    # Test with fixed n32, varying m
    for m in range(1, 20, 2):
        test_cases.append((3, m))
    
    # Test with varying n, fixed m=5
    for n in range(1, 35, 3):
        test_cases.append((n, 5))

    print(f"Generated {len(test_cases)} test cases")
    
    for i, n_m in enumerate(test_cases):
        n, m = n_m
        print(f"Running test {i}/{len(test_cases)}: n={n}, m={m}")
        for rewrite_strat in strategies:
            name = f"coq_{rewrite_strat}_n{n}_m{m}"
            if name in results:
                print(f"Skipping {name}, already exists in results")
                continue

            cleaned_rewrite_strat = rewrite_strat.replace(" ", "_").replace("!", "")

            filename = f"test_n{n}_m{m}_{cleaned_rewrite_strat}.v"
            generate_coq_file(m, n, rewrite_strat, filename)

            start_time = time.time()
            try:
                subprocess.run(
                    ["coqc", filename],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                success = True
            except subprocess.TimeoutExpired:
                success = False
            end_time = time.time()


            os.remove(filename)

            elapsed_time = end_time - start_time
            results[name] = {
                "n": n,
                "m": m,
                "time_taken": elapsed_time,
                "success": success
            }

            with open(benchmark_results_fn, "w") as f:
                json.dump(results, f, indent=4)

            print(f"{'✅' if success else '❌'} {name}: {'success' if success else 'timeout'} in {elapsed_time} secs")

if __name__ == "__main__":
    main()
