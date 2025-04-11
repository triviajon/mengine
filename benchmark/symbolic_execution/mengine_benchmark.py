import subprocess
import time
import json
import os

n_values = [i for i in range(1, 100, 1)]
num_trials = 1
benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
subprogram = "sym"

if __name__ == "__main__":
    if os.path.exists(benchmark_results_fn):
        with open(benchmark_results_fn, "r") as f:
            results = json.load(f)
    else:
        results = {}

    i = max(map(int, results.keys()), default=-1) + 1

    for n in n_values:
        for _ in range(num_trials):
            start_time = time.time()

            result = subprocess.run([program, subprogram, str(n)])
            end_time = time.time()

            elapsed_time = end_time - start_time
            results[i] = {
                "n": n,
                "time": elapsed_time
            }

            with open(benchmark_results_fn, "w") as f:
                json.dump(results, f, indent=4)

            print(f"Test {i}: FINISHED")
            i += 1
