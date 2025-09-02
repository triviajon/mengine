import json
import subprocess
import time
import os
import glob
import re

benchmark_results_fn = "mengine_benchmark_2_results.json"
context_fn = "base.lean"
program = "lean"

rewrite_strategies = ["sorry", "repeat' rw [eq_fa_a]", "simp only [eq_fa_a]"]

SKIP_STRATEGIES = {}


def get_theorem_with_file_content(
    filename, input_expr, expected_equality, index, rewrite_strat_str
):
    with open(filename, "r") as file:
        content = file.readlines()

    indented_rewrite_strat_str = rewrite_strat_str.replace("\n", "\n  ")
    theorem_str = f"\nset_option trace.profiler true in\nset_option trace.profiler.threshold 0 in\nexample : ({input_expr}) = ({expected_equality}) := by\n  {indented_rewrite_strat_str}\nend Test"
    combined_content = "".join(content) + theorem_str + "\n"

    return combined_content


def read_benchmark_results(filename):
    with open(filename, "r") as file:
        benchmark_data = json.load(file)

    return benchmark_data


def calculate_equality(test_case):
    if test_case["G"] != 0:
        return "(g a)"
    else:
        return "a"


def run_tests():
    input_values = read_benchmark_results(benchmark_results_fn)
    results = {}

    if os.path.exists("lean_benchmark_results.json"):
        with open("lean_benchmark_results.json", "r") as f:
            results = json.load(f)

    for key, test_case in input_values.items():
        test_input = test_case["input"]
        test_output = calculate_equality(test_case)

        if key not in results:
            results[key] = {}

        for rewrite_strat in rewrite_strategies:
            rewrite_command = rewrite_strat.strip()

            if rewrite_command in results[key]:
                print(f"Test {key} with '{rewrite_command}': EXISTS")
                continue

            if rewrite_command in SKIP_STRATEGIES:
                print(f"Test {key} with '{rewrite_command}': SKIPPED")
                continue

            lean_benchmark_content = get_theorem_with_file_content(
                context_fn, test_input, test_output, key, rewrite_strat
            )

            test_filename = f"test_{key}.lean"
            with open(test_filename, "w") as f:
                f.write(lean_benchmark_content)

            # Time how long it takes to run "lean test_{key}.lean"
            start_time = time.time()
            result = subprocess.run(
                [program, test_filename],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            elapsed_time = time.time() - start_time

            times = re.findall(r"\[Elab.command\] \[(\d+\.\d+)\] example", result.stdout.decode("utf-8"))

            rewrite_result = {
                "rewrite_command": rewrite_strat.strip(),
                "status": "PASSED" if result.returncode == 0 else "FAILED",
                "file_time": elapsed_time if result.returncode == 0 else None,
                "time": max(map(float, times)) if len(times) != 0 else None,
            } | (
                {}
                if result.returncode == 0 or len(times) != 1
                else {
                    "stdout": result.stdout.decode("utf-8"),
                    "stderr": result.stderr.decode("utf-8"),
                    "returncode": result.returncode,
                    "times": times,
                }
            )

            results[key][rewrite_strat.strip()] = rewrite_result
            print(
                f"Test {key} with '{rewrite_strat.strip()}': {rewrite_result['status']}"
            )

            for file in glob.glob(f"test_{key}.*") + glob.glob(f".test_{key}.*"):
                os.remove(file)

        with open("lean_benchmark_results.json", "w") as f:
            json.dump(results, f, indent=4)

    return results


if __name__ == "__main__":
    run_tests()
