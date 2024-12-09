import subprocess
import time
import json

num_trials = 3
benchmark_results_fn = "mengine_benchmark_results.json"
program = "../../main"
subprogram = "gfa"
N_values = [i for i in range(1, 50, 10)]
G_flags = [0, 1]

def init_app_expression(func, arg):
    """Constructs the application expression `func(arg)`."""
    return f"{func} ({arg})"

def encode_expression(f_length, g_wrap):
    """Encodes the expression based on f_length and g_wrap."""
    current_expr = 'a'
    for _ in range(f_length):
        current_expr = init_app_expression('f', current_expr)

    if g_wrap:
        current_expr = init_app_expression('g', current_expr)

    return current_expr


if __name__ == "__main__":
    results = {}

    i = 0
    for N in N_values:
        for G in G_flags:
            for _ in range(num_trials):
                start_time = time.time()

                result = subprocess.run(
                    [program, subprogram, str(N), str(G)], capture_output=True, text=True)
                end_time = time.time()

                elapsed_time = end_time - start_time
                encoded_expression = encode_expression(N, G)
                results[i] = {
                    "N": N,
                    "G": G,
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
