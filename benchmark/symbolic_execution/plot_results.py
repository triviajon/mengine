import json
import matplotlib.pyplot as plt

with open('mengine_benchmark_results.json') as f:
    data = json.load(f)

n_vals = [entry['n'] for entry in data.values()]
times = [entry['time'] for entry in data.values()]

plt.plot(n_vals, times, marker='o')
plt.xlabel('n')
plt.ylabel('Time (s)')
plt.title('MEngine Benchmark Results')
plt.grid(True)
plt.tight_layout()
plt.savefig("fig.png")