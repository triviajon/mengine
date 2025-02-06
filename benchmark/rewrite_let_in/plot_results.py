import json
import matplotlib.pyplot as plt

benchmark_results_fn = "mengine_benchmark_results.json"

if __name__ == "__main__":
    with open(benchmark_results_fn, "r") as f:
        results = json.load(f)

    N_values = sorted(int(N) for N in results.keys())
    times = [results[str(N)]["time"] for N in N_values]

    plt.figure(figsize=(10, 6))
    plt.plot(N_values, times, marker="o")
    plt.title("N vs Execution Time")
    plt.xlabel("N")
    plt.ylabel("Time (seconds)")
    plt.grid(True)
    plt.savefig("benchmark_plot.png")
    plt.show()
