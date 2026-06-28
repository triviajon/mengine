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
    # Per-unit keys are `mengine_<u>` / `coq_<u>` (single underscore); every
    # baseline key uses a double underscore, so exclude those.
    unit_keys = {k.split("_", 1)[1] for k in results if "_" in k and "__" not in k}
    rows = []
    for u in sorted(unit_keys):
        m = results.get(f"mengine_{u}")
        r = results.get(f"coq_{u}")
        if not m or not r:
            continue
        mt = m["time_taken"] if m["success"] else None
        rt = r["time_taken"] if r["success"] else None
        # Rocq's startup floor is this module's own Require/Import preamble (so a
        # module that loads a library has that load subtracted, not charged to the
        # proof); fall back to the global empty-.v floor if not recorded.
        ru_floor, ru_noise = _baseline(results, f"coq__baseline__{u}")
        if ru_floor is None:
            ru_floor, ru_noise = r_floor, r_noise
        rows.append({
            "unit": u, "category": _category(u), "nlemmas": counts.get(u),
            "mengine": mt, "rocq": rt,
            "mengine_proof": _proof_time(mt, m_floor),
            "rocq_proof": _proof_time(rt, ru_floor),
            "rocq_floor": ru_floor, "rocq_noise": ru_noise,
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
            r_below = rp is not None and rp <= row.get("rocq_noise", r_noise)
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
    colors = {"Bool": "tab:blue", "Lists": "tab:purple", "Logic": "tab:red",
              "Nat": "tab:green", "Peano": "tab:orange"}
    m_floor = meta.get("mengine_floor")
    m_noise = meta.get("mengine_noise") or 0.0

    # Plot *startup-subtracted proof times*, not whole-file times.  This is the
    # one honest axis choice when modules have different startup floors: `Lists`
    # `Require`s `Coq.Lists.List`, so its Rocq floor (~128 ms) is double every
    # other module's (~62 ms).  A whole-file scatter charges that library load to
    # Lists' x-coordinate, and a single parity line — anchored at the *global*
    # Rocq floor — credits Rocq only ~62 ms of it, so Lists drops below parity
    # and reads as an MEngine win when MEngine's proof is in fact ~2× slower.
    # Each point here instead carries its *own* module's floor (subtracted in
    # `_load` via `coq__baseline__<unit>`), so the axes are directly comparable
    # and the parity line is the honest y = x: above it MEngine's proof is
    # slower, below it faster.  (Whole-file numbers remain in REPORT.md's table.)
    pts = []
    dropped = []
    for r in rows:
        rp, mp = r.get("rocq_proof"), r.get("mengine_proof")
        if rp is None or mp is None or rp <= 0 or mp <= 0:
            dropped.append(r["unit"])
            continue
        pts.append(r)
    if not pts:
        print("(skipping scatter plot: no module has a measurable proof residual; "
              f"all at/below startup floor: {', '.join(dropped) or 'none'})")
        return
    if dropped:
        print(f"(scatter: {', '.join(dropped)} omitted — proof residual clamped to "
              "0, i.e. indistinguishable from startup)")

    fig, ax = plt.subplots(figsize=(6, 6))
    for cat in sorted({r["category"] for r in pts}):
        sub = [r for r in pts if r["category"] == cat]
        xs = [r["rocq_proof"] * 1000 for r in sub]
        ys = [r["mengine_proof"] * 1000 for r in sub]
        ax.scatter(xs, ys, label=cat, color=colors.get(cat, "gray"),
                   s=45, alpha=0.85, edgecolors="k", linewidths=0.3, zorder=3)
    for r in pts:  # label each point with its module name
        ax.annotate(r["unit"], (r["rocq_proof"] * 1000, r["mengine_proof"] * 1000),
                    textcoords="offset points", xytext=(5, 3), fontsize=7)

    xs_all = [r["rocq_proof"] * 1000 for r in pts]
    ys_all = [r["mengine_proof"] * 1000 for r in pts]
    lo = min(xs_all + ys_all) * 0.6
    hi = max(xs_all + ys_all) * 1.6

    # Honest parity: equal proof time on both engines is y = x (startup already
    # removed per module).  Shade the MEngine-faster half so the read is obvious.
    ax.plot([lo, hi], [lo, hi], "k--", lw=1, label="parity (equal proof time)")
    ax.fill_between([lo, hi], [lo, lo], [lo, hi], color="tab:green", alpha=0.05)
    ax.text(hi * 0.85, lo * 2.4, "MEngine faster", fontsize=8, ha="right",
            va="bottom", color="tab:green", style="italic")
    ax.text(hi * 0.10, hi * 0.62, "MEngine slower", fontsize=8, ha="left",
            va="top", color="tab:red", style="italic")

    # Reliability floor: a residual at/below the baseline's own run-to-run jitter
    # is not distinguishable from startup.  Shade that band so borderline points
    # (e.g. Peano) are read with appropriate caution rather than as hard numbers.
    if m_floor is not None and m_noise > 0:
        ax.axhspan(lo, m_noise * 1000, color="tab:gray", alpha=0.12, zorder=0)
        ax.text(hi * 0.96, m_noise * 1000, "MEngine noise floor", fontsize=6,
                ha="right", va="bottom", color="dimgray")

    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlim(lo, hi); ax.set_ylim(lo, hi)
    ax.set_aspect("equal")
    ax.set_xlabel("Rocq proof time (ms, log) — startup-subtracted")
    ax.set_ylabel("MEngine proof time (ms, log) — startup-subtracted")
    ax.set_title("Rocq stdlib modules: proof cost (per-module startup removed)")
    ax.legend(fontsize=8, loc="upper left")
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
