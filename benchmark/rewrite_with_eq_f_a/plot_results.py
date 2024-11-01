import json
import matplotlib.pyplot as plt

with open('mengine_benchmark_results.json') as f:
    mengine_results = json.load(f)

with open('coq_benchmark_results.json') as f:
    coq_results = json.load(f)

mengine_key_times_G0 = [(key, entry['N'], entry['time']) for key, entry in mengine_results.items() if entry['G'] == 0]
coq_key_times_G0 = [(key, N, coq_results[key]['time']) for key, N, _ in mengine_key_times_G0 if key in coq_results]

mengine_key_times_G1 = [(key, entry['N'], entry['time']) for key, entry in mengine_results.items() if entry['G'] == 1]
coq_key_times_G1 = [(key, N, coq_results[key]['time']) for key, N, _ in mengine_key_times_G1 if key in coq_results]

mengine_N_G0 = [N for _, N, _ in mengine_key_times_G0]
coq_N_G0 = [N for _, N, _ in coq_key_times_G0]
mengine_times_G0 = [time for _, _, time in mengine_key_times_G0]
coq_times_G0 = [time for _, _, time in coq_key_times_G0]

mengine_N_G1 = [N for _, N, _ in mengine_key_times_G1]
coq_N_G1 = [N for _, N, _ in coq_key_times_G1]
mengine_times_G1 = [time for _, _, time in mengine_key_times_G1]
coq_times_G1 = [time for _, _, time in coq_key_times_G1]

fig, axes = plt.subplots(1, 2, figsize=(14, 6))

axes[0].plot(mengine_N_G0, mengine_times_G0, label='MEngine Time (s)', marker='o', linestyle='None', color='blue')
axes[0].plot(coq_N_G0, coq_times_G0, label='Coq Time (s)', marker='x', linestyle='None', color='orange')
axes[0].set_title('Benchmark Times for f (f ... (f a))')
axes[0].set_xlabel('N')
axes[0].set_ylabel('Time (seconds)')
axes[0].legend()
axes[0].grid()

axes[1].plot(mengine_N_G1, mengine_times_G1, label='MEngine Time (s)', marker='o', linestyle='None', color='blue')
axes[1].plot(coq_N_G1, coq_times_G1, label='Coq Time (s)', marker='x', linestyle='None', color='orange')
axes[1].set_title('Benchmark Times for g (f ... (f a))')
axes[1].set_xlabel('N')
axes[1].set_ylabel('Time (seconds)')
axes[1].legend()
axes[1].grid()

plt.figtext(0.5, 0.035, "Both programs were given roughly the same context, including a Lemma stating '(f a)) = a', and asked to, only using rewrites, prove 'g (f ... (f a)) = (g a)' or '(f ... (f a) = a' with N intermediate f's.", ha='center', va='top', fontsize=10)
plt.savefig("rewrite_gfa_benchmark_comparison.png")
plt.show()
