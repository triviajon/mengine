import json
import matplotlib.pyplot as plt

with open('mengine_benchmark_results.json') as f:
    data = json.load(f)

with open('/home/jonros/coqutil/results.json') as f:
    coq_data = json.load(f)

n_vals = sorted([int(n) for n in data.keys()])
times = [data[str(n)]['time_taken'] for n in n_vals]

coq_n_vals = sorted([int(n) for n in coq_data.keys()])
coq_times = [coq_data[str(n)]['time_taken'] for n in coq_n_vals]

plt.figure(figsize=(8, 5))
plt.plot(n_vals, times, marker='o', label='MEngine')
plt.plot(coq_n_vals, coq_times, marker='s', label='CoqUtil')
plt.xlabel('Problem Size (n)')
plt.ylabel('Time (seconds)')
plt.title('MEngine vs Coqutil Performance')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig("fig_compare.png")