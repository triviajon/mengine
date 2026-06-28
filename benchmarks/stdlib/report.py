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
    # Each unit is a stdlib module file (Bool/Logic/Nat/Peano); category == module.
    return name


MENGINE_BASELINE_KEY = "mengine__startup_baseline"
ROCQ_BASELINE_KEY = "coq__startup_baseline"


def _baseline(results, key):
    """(floor_seconds, noise_seconds) for a stored startup baseline, or (None, 0)."""
    b = results.get(key)
    if not b or not b.get("success"):
        return None, 0.0
    floor = b["time_taken"]
    times = [t["time_taken"] for t in b.get("trials", []) if t.get("success")]
    if len(times) >= 2:
        mean = sum(times) / len(times)
        noise = (sum((t - mean) ** 2 for t in times) / (len(times) - 1)) ** 0.5
    else:
        noise = 0.0
    return floor, noise


def _proof_time(total, floor):
    """Marginal statement+proof cost: whole-file minus startup floor, clamped."""
    if total is None:
        return None
    if floor is None:
        return total
    return max(0.0, total - floor)


def _lemma_counts(cfg):
    """Map module name -> number of statements, read from the corpus manifest."""
    path = os.path.join(cfg.get("corpus_dir", ""), "manifest.json")
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        manifest = json.load(f)
    return {u["name"]: len(u.get("statements", [])) for u in manifest.get("units", [])}


def _load(cfg):
    with open(cfg["results"]) as f:
        results = json.load(f)
    m_floor, m_noise = _baseline(results, MENGINE_BASELINE_KEY)
    r_floor, r_noise = _baseline(results, ROCQ_BASELINE_KEY)
    counts = _lemma_counts(cfg)
    unit_keys = {k.split("_", 1)[1] for k in results
                 if "_" in k and k not in (MENGINE_BASELINE_KEY, ROCQ_BASELINE_KEY)}
    rows = []
    for u in sorted(unit_keys):
        m = results.get(f"mengine_{u}")
        r = results.get(f"coq_{u}")
        if not m or not r:
            continue
        mt = m["time_taken"] if m["success"] else None
        rt = r["time_taken"] if r["success"] else None
        rows.append({
            "unit": u, "category": _category(u), "nlemmas": counts.get(u),
            "mengine": mt, "rocq": rt,
            "mengine_proof": _proof_time(mt, m_floor),
            "rocq_proof": _proof_time(rt, r_floor),
        })
    meta = {
        "mengine_floor": m_floor, "mengine_noise": m_noise,
        "rocq_floor": r_floor, "rocq_noise": r_noise,
        "nlemmas": sum(c for c in counts.values()) if counts else None,
    }
    return rows, meta


def _fmt_ms(t):
    return f"{t*1000:.1f}" if t is not None else "FAIL"


def generate(cfg):
    rows, meta = _load(cfg)
    if not rows:
        print("No results yet — run `stdlib_bench.py run` first.")
        return

    m_floor, m_noise = meta["mengine_floor"], meta["mengine_noise"]
    r_floor, r_noise = meta["rocq_floor"], meta["rocq_noise"]
    have_baselines = m_floor is not None and r_floor is not None

    lines = []
    lines.append("# Rocq stdlib benchmark — MEngine vs Rocq\n")
    if have_baselines:
        lines.append(
            "Whole-file wall-clock is dominated by fixed per-invocation cost — a "
            "`coqc` process plus its auto-loaded `Prelude`, and an `mengine` "
            "process plus `prelude/tactics.me` + the compat prelude — none of "
            "which is the unit's proof. To compare *proof* cost fairly we time "
            "each engine's preamble alone (an empty `.v`; the compat prelude with "
            "no unit) and subtract it. **Proof** columns are this "
            "startup-subtracted residual (best of N trials, clamped at 0).\n")
        lines.append(
            f"**Startup floor:** Rocq {_fmt_ms(r_floor)} ms "
            f"(±{r_noise*1000:.1f}), MEngine {_fmt_ms(m_floor)} ms "
            f"(±{m_noise*1000:.1f}). A proof residual at or below its engine's "
            "jitter (±) is reported as `~0` — indistinguishable from startup.\n")
        lines.append("| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | "
                     "MEngine total (ms) | MEngine proof (ms) | Proof speedup |")
        lines.append("|--------|--------|-----------------|-----------------|"
                     "--------------------|--------------------|---------------|")
    else:
        lines.append("Per-module wall-clock (best of N trials), whole-process: each "
                     "engine pays its own startup. **No startup baseline recorded** — "
                     "re-run `stdlib_bench.py run` to get startup-subtracted proof "
                     "times.\n")
        lines.append("| Module | Lemmas | Rocq (ms) | MEngine (ms) | "
                     "Speedup (Rocq/MEngine) |")
        lines.append("|--------|--------|-----------|--------------|"
                     "------------------------|")

    speedups = []
    below_noise = 0
    for row in sorted(rows, key=lambda r: r["unit"]):
        nl = row["nlemmas"] if row["nlemmas"] is not None else "?"
        if have_baselines:
            mp, rp = row["mengine_proof"], row["rocq_proof"]
            m_below = mp is not None and mp <= m_noise
            r_below = rp is not None and rp <= r_noise
            mp_s = "~0" if m_below else _fmt_ms(mp)
            rp_s = "~0" if r_below else _fmt_ms(rp)
            if m_below or r_below:
                below_noise += 1
                sp_s = "—"
            elif mp and rp and mp > 0:
                sp = rp / mp
                speedups.append(sp)
                sp_s = f"{sp:.2f}×"
            else:
                sp_s = "-"
            lines.append(f"| `{row['unit']}` | {nl} | "
                         f"{_fmt_ms(row['rocq'])} | {rp_s} | "
                         f"{_fmt_ms(row['mengine'])} | {mp_s} | {sp_s} |")
        else:
            m, r = row["mengine"], row["rocq"]
            if m and r and m > 0:
                sp = r / m
                speedups.append(sp)
                sp_s = f"{sp:.2f}×"
            else:
                sp_s = "-"
            lines.append(f"| `{row['unit']}` | {nl} | {_fmt_ms(r)} | "
                         f"{_fmt_ms(m)} | {sp_s} |")

    nlem = meta.get("nlemmas")
    lem_s = f", {nlem} lemmas" if nlem else ""
    lines.append("")
    lines.append(f"**Modules:** {len(rows)}{lem_s} (all Tier A) — one benchmark file "
                 "per stdlib module, matching the library's own file structure.")
    if have_baselines:
        lines.append(f"**Below startup-noise floor (proof time ~0 on either engine):** "
                     f"{below_noise} of {len(rows)}.")
        if speedups:
            geo = math.exp(sum(math.log(s) for s in speedups) / len(speedups))
            med = sorted(speedups)[len(speedups) // 2]
            lines.append(f"**Proof-only speedup (Rocq/MEngine), over the "
                         f"{len(speedups)} module(s) above the noise floor:** "
                         f"{geo:.2f}× geomean (median {med:.2f}×).")
        lines.append("")
        if below_noise == len(rows):
            lines.append("> Even grouped per module, these Tier-A proofs are cheap "
                         "enough that their startup-subtracted cost stays at or below "
                         "measurement jitter on both engines — so the whole-file ratio "
                         "is still mostly a *process-startup* ratio. Heavier modules "
                         "(computational induction, list reasoning) are needed to "
                         "measure proof speed clearly above the noise floor.")
        else:
            lines.append("> Proof columns are startup-subtracted; modules above the "
                         "noise floor give a real proof-speed comparison, while the "
                         "whole-file columns still include each engine's fixed "
                         "per-invocation cost (which dominates the raw ratio).")
    elif speedups:
        geo = math.exp(sum(math.log(s) for s in speedups) / len(speedups))
        med = sorted(speedups)[len(speedups) // 2]
        lines.append(f"**Both-succeed:** {len(speedups)}.")
        lines.append(f"**Geometric-mean whole-file ratio (Rocq/MEngine):** {geo:.2f}×  "
                     f"(median {med:.2f}×) — dominated by startup, not proof cost.")

    out_md = os.path.join(os.path.dirname(cfg["results"]), "REPORT.md")
    with open(out_md, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Wrote {os.path.relpath(out_md)}")

    _scatter(cfg, rows, meta)


def _scatter(cfg, rows, meta=None):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:  # noqa: BLE001
        print(f"(skipping scatter plot: matplotlib unavailable: {e})")
        return

    meta = meta or {}
    colors = {"Bool": "tab:blue", "Nat": "tab:green", "Peano": "tab:orange",
              "Logic": "tab:red"}
    pts = [r for r in rows if r["mengine"] and r["rocq"]]
    if not pts:
        return
    fig, ax = plt.subplots(figsize=(6, 6))
    for cat in sorted({r["category"] for r in pts}):
        xs = [r["rocq"] * 1000 for r in pts if r["category"] == cat]
        ys = [r["mengine"] * 1000 for r in pts if r["category"] == cat]
        ax.scatter(xs, ys, label=cat, color=colors.get(cat, "gray"),
                   s=40, alpha=0.8, edgecolors="k", linewidths=0.3)

    # Independent x/y limits, each padded around its own data and startup floor.
    # The data now sits in one corner (Rocq is startup-bound, MEngine proofs are
    # ~ms), so a shared square range would waste most of the canvas; the parity
    # line is an explicit curve, not a 45° diagonal, so equal aspect isn't needed.
    LO_PAD, HI_PAD = 0.85, 1.25
    r_floor, m_floor = meta.get("rocq_floor"), meta.get("mengine_floor")
    xvals = [r["rocq"] * 1000 for r in pts] + ([r_floor * 1000] if r_floor else [])
    yvals = [r["mengine"] * 1000 for r in pts] + ([m_floor * 1000] if m_floor else [])
    xlo, xhi = min(xvals) * LO_PAD, max(xvals) * HI_PAD
    ylo, yhi = min(yvals) * LO_PAD, max(yvals) * HI_PAD

    # Startup floors: the scatter is dominated by per-invocation cost, so drawing
    # the floors makes explicit that the whole-file comparison is mostly startup.
    if r_floor:
        ax.axvline(r_floor * 1000, color="tab:gray", ls=":", lw=1,
                   label="Rocq startup floor")
    if m_floor:
        ax.axhline(m_floor * 1000, color="tab:cyan", ls=":", lw=1,
                   label="MEngine startup floor")

    # Parity line.  A whole-file time is startup + proof, so *equal proof cost*
    # is not y = x (that assumes zero startup and makes every point look far
    # below parity — the dishonest, startup-dominated ratio).  It is
    # y - m0 = x - r0: a slope-1 line in linear space anchored where the two
    # startup floors meet, (r0, m0).  A point on it has equal proof time on both
    # engines; below it MEngine's proof is faster, above it slower.  On log-log
    # axes this is a curve that bends at the floor cross and asymptotes to y = x.
    # Without baselines we cannot subtract startup, so fall back to naive y = x.
    if r_floor and m_floor:
        r0, m0 = r_floor * 1000, m_floor * 1000
        npts = 200
        xs_line = [r0 + (xhi - r0) * k / (npts - 1) for k in range(npts)]
        ys_line = [m0 + (x - r0) for x in xs_line]
        ax.plot(xs_line, ys_line, "k--", lw=1, label="parity (equal proof time)")
        ax.plot([r0], [m0], "k.", ms=7, zorder=5)  # the floor cross = origin
    else:
        ax.plot([xlo, xhi], [ylo, yhi], "k--", lw=1, label="parity")

    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlim(xlo, xhi); ax.set_ylim(ylo, yhi)
    ax.set_xlabel("Rocq time (ms, log)")
    ax.set_ylabel("MEngine time (ms, log)")
    ax.set_title("Rocq stdlib modules: MEngine vs Rocq")
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
