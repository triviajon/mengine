import subprocess
import time
import json
import os

def generate_nested_expr(n):
    """Generates the deeply nested (h (h ...) (h ...)) term."""
    expr = "a"
    for _ in range(n):
        expr = f"(h {expr} {expr})"
    return expr

def generate_let_expr(n):
    """Generates the let-bound version of the nested term."""
    # let x1 := h a a in let x2 := h x1 x1 ... in xn
    if n == 0: return "a"
    lines = ["let x0: A := (h a a) in"]
    for i in range(1, n):
        lines.append(f"let x{i}: A := (h x{i-1} x{i-1}) in")
    lines.append(f"x{n-1}")
    return "\n".join(lines)

def run_benchmark(template_path, n, generator_func):
    with open(template_path, 'r') as f:
        template = f.read()
    
    expr = generator_func(n)
    full_code = template.replace("__EXPR__", expr)
    
    temp_filename = "temp_bench.me"
    with open(temp_filename, "w") as f:
        f.write(full_code)    

    try:
        start_time = time.perf_counter()
        subprocess.run(["mengine", "-q", temp_filename], check=True, capture_output=True)
        end_time = time.perf_counter()
        elapsed = end_time - start_time
    except subprocess.CalledProcessError as e:
        print(f"Error compiling n={n}: {e.stderr.decode()}")
        elapsed = None
    finally:
        pass
        # if os.path.exists(temp_filename): os.remove(temp_filename)

    return elapsed

def main():
    results = {"template_1": [], "template_2": []}

    print("Benchmarking Template 1 (Explicit Nesting)...")
    for n in range(1, 11):
        t = run_benchmark("rewrite_expo_1.template", n, generate_nested_expr)
        if t:
            results["template_1"].append({"n": n, "time": t})
            print(f"n={n}: {t:.4f}s")

    print("\nBenchmarking Template 2 (Let-bindings)...")
    for n in range(1, 1000, 50):
        t = run_benchmark("rewrite_expo_2.template", n, generate_let_expr)
        if t:
            results["template_2"].append({"n": n, "time": t})
            print(f"n={n}: {t:.4f}s")

    with open("benchmark_results.json", "w") as f:
        json.dump(results, f, indent=4)
    print("\nResults saved to benchmark_results.json")

if __name__ == "__main__":
    main()