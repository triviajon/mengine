import subprocess
import time
import json
import os

n_values = [i for i in range(1, 500, 10)]
num_trials = 1
benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
subprogram = "sep"

if __name__ == "__main__":
    if os.path.exists(benchmark_results_fn):
        with open(benchmark_results_fn, "r") as f:
            results = json.load(f)
    else:
        results = {}


    for n in n_values:
        for _ in range(num_trials):
            start_time = time.time()

            result = subprocess.run([program, subprogram, str(n)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            end_time = time.time()

            elapsed_time = end_time - start_time
            results[n] = {
                "time_taken": elapsed_time
            }

            with open(benchmark_results_fn, "w") as f:
                json.dump(results, f, indent=4)

            print(f"✅ tests/Test{n}.v: success in {elapsed_time} secs")
