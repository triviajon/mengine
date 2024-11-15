import json
import matplotlib.pyplot as plt

with open('mengine_benchmark_results.json') as f:
    mengine_results = json.load(f)

with open('mengine_benchmark_2_results.json') as f:
    mengine_no_proof_results = json.load(f)

with open('coq_benchmark_results.json') as f:
    coq_results = json.load(f)

with open('verification_results.json') as f:
    verification_results = json.load(f)

rewrite_strategies_to_color = {
    "repeat rewrite eq_fa_a.": "red",
    "rewrite! eq_fa_a.": "orange",
    "rewrite_strat topdown eq_fa_a.": "aqua",
    "rewrite_strat bottomup eq_fa_a.": "purple"
}


mengine_key_times_G0 = [(key, entry['N'], entry['time'], verification_results.get(key, {}).get('time')) for key, entry in mengine_results.items() if entry['G'] == 0]
mengine_key_times_G1 = [(key, entry['N'], entry['time'], verification_results.get(key, {}).get('time')) for key, entry in mengine_results.items() if entry['G'] == 1]
mengine_np_key_times_G0 = [(key, entry['N'], entry['time'], verification_results.get(key, {}).get('time')) for key, entry in mengine_no_proof_results.items() if entry['G'] == 0]
mengine_np_key_times_G1 = [(key, entry['N'], entry['time'], verification_results.get(key, {}).get('time')) for key, entry in mengine_no_proof_results.items() if entry['G'] == 1]
coq_key_times_G0 = [(key, N, strat, result['time']) for key, N, _, _ in mengine_key_times_G0 
                    if key in coq_results for strat, result in coq_results[key].items()]
coq_key_times_G1 = [(key, N, strat, result['time']) for key, N, _, _ in mengine_key_times_G1 
                    if key in coq_results for strat, result in coq_results[key].items()]

mengine_N_G0 = [N for _, N, _, _ in mengine_key_times_G0]
mengine_np_N_G0 = [N for _, N, _, _ in mengine_np_key_times_G0]
coq_N_G0 = [N for _, N, _, _ in coq_key_times_G0]
mengine_times_G0 = [time for _, _, time, _ in mengine_key_times_G0]
mengine_np_times_G0 = [time for _, _, time, _ in mengine_np_key_times_G0]
verification_times_G0 = [v_time for _, _, _, v_time in mengine_key_times_G0]
combined_N_G0 = [N for N, v_time in zip(mengine_N_G0, verification_times_G0) if v_time is not None]
combined_times_G0 = [b_time + v_time for b_time, v_time in zip(mengine_times_G0, verification_times_G0) if v_time is not None]
coq_times_G0 = [time for _, _, _, time in coq_key_times_G0]

mengine_N_G1 = [N for _, N, _, _ in mengine_key_times_G1]
mengine_np_N_G1 = [N for _, N, _, _ in mengine_np_key_times_G1]
coq_N_G1 = [N for _, N, _, _ in coq_key_times_G1]
mengine_times_G1 = [time for _, _, time, _ in mengine_key_times_G1]
mengine_np_times_G1 = [time for _, _, time, _ in mengine_np_key_times_G1]
verification_times_G1 = [v_time for _, _, _, v_time in mengine_key_times_G1]
combined_N_G1 = [N for N, v_time in zip(mengine_N_G1, verification_times_G1) if v_time is not None]
combined_times_G1 = [b_time + v_time for b_time, v_time in zip(mengine_times_G1, verification_times_G1) if v_time is not None]
coq_times_G1 = [time for _, _, _, time in coq_key_times_G1]

fig, axes = plt.subplots(1, 2, figsize=(14, 6))

axes[0].plot(mengine_N_G0, mengine_times_G0, label='MEngine Time (s)', marker='o', linestyle='None', color='blue')
axes[0].plot(mengine_np_N_G0, mengine_np_times_G0, label='MEngine (n.p.) Time (s)', marker='o', linestyle='None', color='black')
for key, N, strat, time in coq_key_times_G0:
    axes[0].scatter(N, time, label=f'Coq Time ({strat})', marker='x', color=rewrite_strategies_to_color[strat])
axes[0].plot(combined_N_G0, combined_times_G0, label='MEngine Generation + Coq Verification (s)', marker='^', linestyle='None', color='green')
axes[0].set_title('Benchmark Times for f (f ... (f a))')
axes[0].set_xlabel('N')
axes[0].set_ylabel('Time (seconds)')
axes[0].legend()
axes[0].grid()

axes[1].plot(mengine_N_G1, mengine_times_G1, label='MEngine Time (s)', marker='o', linestyle='None', color='blue')
axes[1].plot(mengine_np_N_G1, mengine_np_times_G1, label='MEngine Time (n.p.) (s)', marker='o', linestyle='None', color='black')
for key, N, strat, time in coq_key_times_G1:
    axes[1].scatter(N, time, label=f'Coq Time ({strat})', marker='x', color=rewrite_strategies_to_color[strat])
axes[1].plot(combined_N_G1, combined_times_G1, label='MEngine Generation + Coq Verification (s)', marker='^', linestyle='None', color='green')
axes[1].set_title('Benchmark Times for g (f ... (f a))')
axes[1].set_xlabel('N')
axes[1].set_ylabel('Time (seconds)')
axes[1].legend()
axes[1].grid()

handles, labels = axes[0].get_legend_handles_labels()
unique_labels = dict(zip(labels, handles))
axes[0].legend(unique_labels.values(), unique_labels.keys())

handles, labels = axes[1].get_legend_handles_labels()
unique_labels = dict(zip(labels, handles))
axes[1].legend(unique_labels.values(), unique_labels.keys())

plt.figtext(0.5, 0.035, "Both programs were given roughly the same context, including a Lemma stating '(f a)) = a', and asked to, only using rewrites, prove 'g (f ... (f a)) = (g a)' or '(f ... (f a) = a' with N intermediate f's.", ha='center', va='top', fontsize=10)
plt.tight_layout(rect=[0, 0.05, 1, 1])
plt.savefig("rewrite_gfa_benchmark_comparison_with_verification.png")
plt.show()
