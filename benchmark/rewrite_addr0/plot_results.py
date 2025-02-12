import json
import matplotlib.pyplot as plt

benchmark_results_fn = "mengine_benchmark_results.json"
subprograms = ["native", "letin", "tree"]

if __name__ == "__main__":
    with open(benchmark_results_fn, "r") as f:
        results = json.load(f)

    plt.figure(figsize=(10, 6))

    for subprogram in subprograms:
        if subprogram not in results:
            continue

        N_values = sorted(int(N) for N in results[subprogram].keys())
        times = [results[subprogram][str(N)]["time"] for N in N_values]

        plt.plot(N_values, times, marker="o", label=subprogram)

    plt.title("N vs Execution Time")
    plt.xlabel("N")
    plt.ylabel("Time (seconds)")
    plt.legend()
    plt.grid(True)
    plt.savefig("benchmark_plot.png")
    plt.show()
