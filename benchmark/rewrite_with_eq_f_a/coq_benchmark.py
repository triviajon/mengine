import json
import subprocess
import time
import os
import glob

benchmark_results_fn = "mengine_benchmark_results.json"
context_fn = "base.v"
program = "coqc"


def get_theorem_with_file_content(filename, input_expr, expected_equality, index):
    with open(filename, 'r') as file:
        content = file.readlines()

    theorem_str = f"\nTheorem test_{index} : eq ({input_expr}) ({expected_equality}). Proof. repeat rewrite eq_fa_a. reflexivity. Qed."
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
        coq_benchmark_content = get_theorem_with_file_content(
            context_fn, test_input, test_output, key)

        test_filename = f"test_{key}.v"
        with open(test_filename, 'w') as f:
            f.write(coq_benchmark_content)

        # Time how long it takes to run "coqc test_{key}.v"
        start_time = time.time()
        result = subprocess.run([program, test_filename],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        elapsed_time = time.time() - start_time

        vo_filename = f"test_{key}.vo"
        if os.path.exists(vo_filename):
            results[key] = {"status": "PASSED", "time": elapsed_time}
            print(f"Test {key}: PASSED")
        else:
            results[key] = {"status": "FAILED"}
            print(f"Test {key}: FAILED")

        with open('coq_benchmark_results.json', 'w') as f:
            json.dump(results, f, indent=4)

        for file in glob.glob(f"test_{key}.*") + glob.glob(f".test_{key}.*"):
            os.remove(file)

    return results


if __name__ == "__main__":
    run_tests()
