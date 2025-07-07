import subprocess
import time
import json
import os

benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
subprogram = "nm"

test_cases = []

# Test with varyingn, fixed m=3
for n in range(1, 15000, 25):
    test_cases.append((n, 3))

# Test with fixed n=3, varying m
for m in range(1, 3000, 50):
    test_cases.append((3, m))

# Test with varying n, fixed m=5
for n in range(1, 4000, 25):
    test_cases.append((n, 5))



if __name__ == "__main__":
    if os.path.exists(benchmark_results_fn):
        with open(benchmark_results_fn, "r") as f:
            results = json.load(f)
    else:
        results = {}

    for n, m in test_cases:
        start_time = time.perf_counter()

        result = subprocess.run([program, "--proof=0", subprogram, str(n), str(m)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        end_time = time.perf_counter()

        elapsed_time = end_time - start_time

        name = f"mengine_n{n}_m{m}"
        results[name] = {
            "n": n,
            "m": m,
            "time_taken": elapsed_time
        }

        with open(benchmark_results_fn, "w") as f:
            json.dump(results, f, indent=4)

        print(f"✅ {name}: success in {elapsed_time} secs")
