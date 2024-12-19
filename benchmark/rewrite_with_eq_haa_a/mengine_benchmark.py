import subprocess
import time
import json

h_values = [i for i in range(1, 15, 1)]
num_trials = 3
benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
gen_proof_bool = "--proof=1"
subprogram = "haa"

def encode_expression(h_depth):
    """Encodes the expression based on h_depth."""
    haa = f"(h a) a"
    current_expr = haa

    for _ in range(h_depth):
        intermediate = f"h ({current_expr})"
        current_expr = f"(({intermediate}) ({current_expr}))"
    return current_expr


if __name__ == "__main__":
    results = {}

    i = 0
    for h_depth in h_values:
        for _ in range(num_trials):
            start_time = time.time()

            result = subprocess.run(
                [program, gen_proof_bool, subprogram, str(h_depth)], capture_output=True, text=True)
            end_time = time.time()

            elapsed_time = end_time - start_time
            encoded_expression = encode_expression(h_depth)
            results[i] = {
                "h_depth": h_depth,
                "input": encoded_expression,
                "output": result.stdout.strip(),
                "time": elapsed_time
            }

            with open(benchmark_results_fn, "w") as f:
                json.dump(results, f, indent=4)

            print(f"Test {i}: FINISHED")
            i += 1

    with open(benchmark_results_fn, "w") as f:
        json.dump(results, f, indent=4)
