import json
import subprocess
import time
import os
import glob

benchmark_results_fn = "mengine_benchmark_results.json"
context_fn = "base.v"
program = "coqc"

rewrite_strategies = [
    "repeat rewrite eq_fa_a.",
    "rewrite! eq_fa_a.",
    "rewrite_strat topdown eq_fa_a.",
    "rewrite_strat bottomup eq_fa_a."
]

SKIP_STRATEGIES = {
    "repeat rewrite eq_fa_a.",
    "rewrite! eq_fa_a."
}

def get_theorem_with_file_content(filename, input_expr, expected_equality, index, rewrite_strat_str):
    with open(filename, 'r') as file:
        content = file.readlines()

    theorem_str = f"\nTheorem test_{index} : eq nat ({input_expr}) ({expected_equality}). Proof. {rewrite_strat_str} reflexivity. Qed. End Test."
    combined_content = ''.join(content) + theorem_str + '\n'

    return combined_content


def read_benchmark_results(filename):
    with open(filename, 'r') as file:
        benchmark_data = json.load(file)

    return benchmark_data


def calculate_equality(test_case):
    if test_case['G'] != 0:
        return '(g a)'
    else:
        return 'a'


def run_tests():
    input_values = read_benchmark_results(benchmark_results_fn)
    results = {}

    if os.path.exists('coq_benchmark_results.json'):
        with open('coq_benchmark_results.json', 'r') as f:
            results = json.load(f)

    for key, test_case in input_values.items():
        test_input = test_case['input']
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


            coq_benchmark_content = get_theorem_with_file_content(
                context_fn, test_input, test_output, key, rewrite_strat
            )

            test_filename = f"test_{key}.v"
            with open(test_filename, 'w') as f:
                f.write(coq_benchmark_content)

            # Time how long it takes to run "coqc test_{key}.v"
            start_time = time.time()
            result = subprocess.run([program, test_filename],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            elapsed_time = time.time() - start_time

            vo_filename = f"test_{key}.vo"
            rewrite_result = {
                "rewrite_command": rewrite_strat.strip(),
                "status": "PASSED" if os.path.exists(vo_filename) else "FAILED",
                "time": elapsed_time if os.path.exists(vo_filename) else None
            }

            results[key][rewrite_strat.strip()] = rewrite_result
            print(f"Test {key} with '{rewrite_strat.strip()}': {rewrite_result['status']}")

            for file in glob.glob(f"test_{key}.*") + glob.glob(f".test_{key}.*"):
                os.remove(file)

        with open('coq_benchmark_results.json', 'w') as f:
            json.dump(results, f, indent=4)

    return results


if __name__ == "__main__":
    run_tests()
