#!/usr/bin/env python3
"""
Simple script to plot benchmark results comparing Coq vs mengine performance.
Creates three plots as described in the requirements.
"""

import json
import matplotlib.pyplot as plt
import numpy as np
import sys
from pathlib import Path

def load_results(coq_file, mengine_file):
    """Load results from both Coq and mengine benchmark files"""
    
    coq_results = {}
    if Path(coq_file).exists():
        with open(coq_file, 'r') as f:
            coq_results = json.load(f)
    
    mengine_results = {}
    if Path(mengine_file).exists():
        with open(mengine_file, 'r') as f:
            mengine_results = json.load(f)
    
    return coq_results, mengine_results

def create_plots(coq_results, mengine_results):
    """Create three comparison plots"""
    
    # Prepare data - organize by strategy
    coq_strategies = {}
    mengine_data = {}
    
    # Process Coq results - group by strategy
    for name, result in coq_results.items():
        if result['success'] and 'n' in result and 'm' in result:
            # Extract strategy from name (format: coq_{strategy}_n{n}_m{m})
            if name.startswith('coq_'):
                parts = name.split('_')
                if len(parts) >= 3:
                    strategy = '_'.join(parts[1:-2])  # Everything between 'coq_' and '_n{n}_m{m}'
                    if strategy not in coq_strategies:
                        coq_strategies[strategy] = {}
                    key = (result['n'], result['m'])
                    coq_strategies[strategy][key] = result['time_taken']
    
    # Process mengine results  
    for name, result in mengine_results.items():
        if 'n' in result and 'm' in result:
            key = (result['n'], result['m'])
            mengine_data[key] = result['time_taken']
    
    # Define markers for different strategies
    strategy_markers = {
        'rewrite!': 'o',
        'repeat_setoid_rewrite': 's', 
        'rewrite_strat_topdown': '^',
        'rewrite_strat_bottomup': 'v'
    }
    
    # Find all test cases across all strategies
    all_keys = set()
    for strategy_data in coq_strategies.values():
        all_keys.update(strategy_data.keys())
    all_keys.update(mengine_data.keys())
    
    if not all_keys:
        print("No successful test cases found")
        return
    
    print(f"Found {len(all_keys)} total test cases")
    print(f"Coq strategies: {list(coq_strategies.keys())}")
    
    # Create figure with 3 subplots
    fig, axs = plt.subplots(1, 3, figsize=(18, 6))
    (ax1, ax2, ax3) = axs

    # Plot 1: Performance vs n (fixing m=3)
    m_fixed = 3
    n_values = sorted(set(n for n, m in all_keys if m == m_fixed))
    
    if n_values:
        # Plot each Coq strategy
        for strategy, strategy_data in coq_strategies.items():
            valid_n = [n for n in n_values if (n, m_fixed) in strategy_data]
            if valid_n:
                times = [strategy_data[(n, m_fixed)] for n in valid_n]
                marker = strategy_markers.get(strategy, 'o')
                ax1.plot(valid_n, times, marker=marker, linestyle='-', 
                        label=f'Coq {strategy}', color='red', alpha=0.7)
        
        # Plot mengine
        valid_n_mengine = [n for n in n_values if (n, m_fixed) in mengine_data]
        if valid_n_mengine:
            times_mengine = [mengine_data[(n, m_fixed)] for n in valid_n_mengine]
            ax1.plot(valid_n_mengine, times_mengine, 'o-', label='mengine', 
                    color='blue', linewidth=2)
        
        ax1.set_xlabel('n (function arity)')
        ax1.set_ylabel('Time (seconds)')
        # ax1.set_title(f'Performance vs n (m={m_fixed})')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
    
    # Plot 2: Performance vs m (fixing n=3)
    n_fixed = 3
    m_values = sorted(set(m for n, m in all_keys if n == n_fixed))
    
    if m_values:
        # Plot each Coq strategy
        for strategy, strategy_data in coq_strategies.items():
            valid_m = [m for m in m_values if (n_fixed, m) in strategy_data]
            if valid_m:
                times = [strategy_data[(n_fixed, m)] for m in valid_m]
                marker = strategy_markers.get(strategy, 'o')
                ax2.plot(valid_m, times, marker=marker, linestyle='-',
                        label=f'Coq {strategy}', color='red', alpha=0.7)
        
        # Plot mengine
        valid_m_mengine = [m for m in m_values if (n_fixed, m) in mengine_data]
        if valid_m_mengine:
            times_mengine = [mengine_data[(n_fixed, m)] for m in valid_m_mengine]
            ax2.plot(valid_m_mengine, times_mengine, 'o-', label='mengine',
                    color='blue', linewidth=2)
        
        ax2.set_xlabel('m (# of let binders)')
        ax2.set_ylabel('Time (seconds)')
        # ax2.set_title(f'Performance vs m (n={n_fixed})')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
    
    # Plot 3: Performance vs n (fixing m=5)
    m_fixed_3 = 5
    n_values_3 = sorted(set(n for n, m in all_keys if m == m_fixed_3))
    
    if n_values_3:
        # Plot each Coq strategy
        for strategy, strategy_data in coq_strategies.items():
            valid_n = [5*n for n in n_values_3 if (n, m_fixed_3) in strategy_data]
            if valid_n:
                times = [strategy_data[(n // 5, m_fixed_3)] for n in valid_n]
                marker = strategy_markers.get(strategy, 'o')
                ax3.plot(valid_n, times, marker=marker, linestyle='-',
                        label=f'Coq {strategy}', color='red', alpha=0.7)
        
        # Plot mengine
        valid_n_mengine = [5*n for n in n_values_3 if (n, m_fixed_3) in mengine_data]
        if valid_n_mengine:
            times_mengine = [mengine_data[(n // 5, m_fixed_3)] for n in valid_n_mengine]
            ax3.plot(valid_n_mengine, times_mengine, 'o-', label='mengine',
                    color='blue', linewidth=2)
        
        ax3.set_xlabel('n * m')
        ax3.set_ylabel('Time (seconds)')
        # ax3.set_title(f'Performance vs n (m={m_fixed_3})')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Save the plot
    output_file = "benchmark_comparison.png"
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Plot saved to {output_file}")

    for i, ax in enumerate(axs):
        extent = ax.get_tightbbox(fig.canvas.get_renderer()).transformed(fig.dpi_scale_trans.inverted())
        fig.savefig(f"subplot_{i}.png", bbox_inches=extent)
        print(f"Subplot {i} saved to subplot_{i}.png")
    
    # Show some statistics
    print(f"\nBenchmark Statistics:")
    print(f"  Total test cases: {len(all_keys)}")
    print(f"  Coq strategies tested: {len(coq_strategies)}")
    for strategy, data in coq_strategies.items():
        print(f"    {strategy}: {len(data)} successful tests")
    print(f"  mengine successful tests: {len(mengine_data)}")

def main():
    coq_file = "generate_coq_tests_benchmark_results.json"
    mengine_file = "mengine_benchmark_results.json"
    
    if len(sys.argv) > 1:
        coq_file = sys.argv[1]
    if len(sys.argv) > 2:
        mengine_file = sys.argv[2]
    
    print(f"Loading Coq results from: {coq_file}")
    print(f"Loading mengine results from: {mengine_file}")
    
    coq_results, mengine_results = load_results(coq_file, mengine_file)
    
    if not coq_results and not mengine_results:
        print("No benchmark results found. Run the benchmark scripts first.")
        sys.exit(1)
    
    create_plots(coq_results, mengine_results)

if __name__ == "__main__":
    main()
