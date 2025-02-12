import subprocess
import time
import json
import os

N_values = [i for i in range(1, 50, 2)]
benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
gen_proof_bool = "--proof=0"
subprogram = "addr0"
subprograms_types = ["native", "letin"]

if __name__ == "__main__":
    results = {}

    if os.path.exists(benchmark_results_fn):
        with open(benchmark_results_fn, "r") as f:
            results = json.load(f)

    for subprogram_type in subprograms_types:
        if subprogram_type not in results:
            results[subprogram_type] = {}

        for N in N_values:
            if str(N) in results[subprogram_type]:
                print(f"Test {subprogram_type} N={N}: SKIPPED.")
                continue

            start_time = time.time()

            result = subprocess.run(
                [program, gen_proof_bool, subprogram, subprogram_type, str(N)],
                capture_output=True,
                text=True
            )
            end_time = time.time()

            elapsed_time = end_time - start_time
            results[subprogram_type][N] = {
                "output": result.stdout.strip(),
                "time": elapsed_time
            }

            with open(benchmark_results_fn, "w") as f:
                json.dump(results, f, indent=4)

            print(f"Test {subprogram_type} N={N}: FINISHED")

    with open(benchmark_results_fn, "w") as f:
        json.dump(results, f, indent=4)
