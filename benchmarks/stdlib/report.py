#!/usr/bin/env python3
"""Reporting for the Rocq-stdlib benchmark: markdown table + scatter plot.

Reads results/stdlib.json (written by stdlib_bench.py run) and emits:
  - stdlib/results/REPORT.md  : per-unit table + geometric-mean summary
  - stdlib/plots/stdlib_scatter.png : Rocq time (x) vs MEngine time (y), log-log
"""

import json
import math
import os


def _category(name):
    for pre, cat in (("bool_", "bool"), ("nat_", "nat"), ("le_", "le"),
                     ("logic_", "logic"), ("eq_", "eq"), ("ex_", "ex")):
        if name.startswith(pre):
            return cat
    return "other"


def _load(cfg):
    with open(cfg["results"]) as f:
        results = json.load(f)
    units = sorted({k.split("_", 1)[1] for k in results if "_" in k})
    rows = []
    for u in units:
        m = results.get(f"mengine_{u}")
        r = results.get(f"coq_{u}")
        if not m or not r:
            continue
        rows.append({
            "unit": u, "category": _category(u),
            "mengine": m["time_taken"] if m["success"] else None,
            "rocq": r["time_taken"] if r["success"] else None,
        })
    return rows


def _fmt_ms(t):
    return f"{t*1000:.1f}" if t is not None else "FAIL"


def generate(cfg):
    rows = _load(cfg)
    if not rows:
        print("No results yet — run `stdlib_bench.py run` first.")
        return

    lines = []
    lines.append("# Rocq stdlib benchmark — MEngine vs Rocq\n")
    lines.append("Per-unit wall-clock (best of N trials), whole-process: each engine "
                 "pays its own startup (MEngine loads `prelude/tactics.me` + the compat "
                 "prelude; Rocq starts its process and loads `Coq.Init`).\n")
    lines.append("| Unit | Category | Rocq (ms) | MEngine (ms) | Speedup (Rocq/MEngine) |")
    lines.append("|------|----------|-----------|--------------|------------------------|")

    speedups = []
    for row in sorted(rows, key=lambda r: (r["category"], r["unit"])):
        m, r = row["mengine"], row["rocq"]
        if m and r and m > 0:
            sp = r / m
            speedups.append(sp)
            sp_s = f"{sp:.2f}×"
        else:
            sp_s = "-"
        lines.append(f"| `{row['unit']}` | {row['category']} | {_fmt_ms(r)} | "
                     f"{_fmt_ms(m)} | {sp_s} |")

    if speedups:
        geo = math.exp(sum(math.log(s) for s in speedups) / len(speedups))
        med = sorted(speedups)[len(speedups) // 2]
        lines.append("")
        lines.append(f"**Units:** {len(rows)} (all Tier A).  "
                     f"**Both-succeed:** {len(speedups)}.")
        lines.append(f"**Geometric-mean speedup (Rocq/MEngine):** {geo:.2f}×  "
                     f"(median {med:.2f}×).")
        lines.append("")
        lines.append("> MEngine times are dominated by prelude+compat startup at this "
                     "problem size; the comparison reflects fixed per-invocation cost, "
                     "not asymptotics. See README for scope and caveats.")

    out_md = os.path.join(os.path.dirname(cfg["results"]), "REPORT.md")
    with open(out_md, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Wrote {os.path.relpath(out_md)}")

    _scatter(cfg, rows)


def _scatter(cfg, rows):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:  # noqa: BLE001
        print(f"(skipping scatter plot: matplotlib unavailable: {e})")
        return

    colors = {"bool": "tab:blue", "nat": "tab:green", "le": "tab:orange",
              "logic": "tab:red", "eq": "tab:purple", "ex": "tab:brown",
              "other": "gray"}
    pts = [r for r in rows if r["mengine"] and r["rocq"]]
    if not pts:
        return
    fig, ax = plt.subplots(figsize=(6, 6))
    for cat in sorted({r["category"] for r in pts}):
        xs = [r["rocq"] * 1000 for r in pts if r["category"] == cat]
        ys = [r["mengine"] * 1000 for r in pts if r["category"] == cat]
        ax.scatter(xs, ys, label=cat, color=colors.get(cat, "gray"),
                   s=40, alpha=0.8, edgecolors="k", linewidths=0.3)

    allv = [r["rocq"] * 1000 for r in pts] + [r["mengine"] * 1000 for r in pts]
    lo, hi = min(allv) * 0.7, max(allv) * 1.4
    ax.plot([lo, hi], [lo, hi], "k--", lw=1, label="parity")
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlim(lo, hi); ax.set_ylim(lo, hi)
    ax.set_xlabel("Rocq time (ms, log)")
    ax.set_ylabel("MEngine time (ms, log)")
    ax.set_title("Rocq stdlib units: MEngine vs Rocq")
    ax.legend(fontsize=8, loc="best")
    ax.grid(True, which="both", ls=":", alpha=0.4)
    os.makedirs(cfg["plots_dir"], exist_ok=True)
    out = os.path.join(cfg["plots_dir"], "stdlib_scatter.png")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"Wrote {os.path.relpath(out)}")


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from stdlib_bench import load_config
    generate(load_config())
