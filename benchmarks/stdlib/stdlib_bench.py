#!/usr/bin/env python3
"""Corpus runner for the Rocq-stdlib benchmark (MEngine vs Rocq).

A sibling to ``benchmarks/bench.py``: instead of sweeping a parameter range it
iterates a fixed manifest of curated, auto-translated stdlib units.  It reuses
the timing discipline of ``framework/runner.run_single`` (process-group kill on
timeout, N trials keeping the minimum) without subclassing ``Benchmark``.

Subcommands:
    list                 show the corpus and tiers
    test [unit ...]      faithfulness gate: coqc compiles rocq.v, MEngine runs
                         mengine.me clean, and the statement digests correspond
    run  [unit ...]      time both engines per unit; write results/stdlib.json
    report               regenerate the markdown table + scatter plot
    manifest             regenerate corpus/manifest.json from the corpus dir
    triage [--dir D]     run translate.py --report over a stdlib checkout
"""

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BENCH_ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local module)


# ─────────────────────────────── config ──────────────────────────────────────

def load_config():
    with open(os.path.join(BENCH_ROOT, "config.json")) as f:
        cfg = json.load(f)
    s = cfg.get("stdlib", {})
    def exp(p):
        return os.path.expanduser(p) if p else p
    return {
        "mengine_path": exp(cfg["mengine_path"]),
        "mengine_root": exp(cfg.get("mengine_root", "")),
        "coq_path": exp(cfg.get("coq_path", "coqc")),
        "corpus_dir": os.path.join(BENCH_ROOT, s.get("corpus_dir", "stdlib/corpus")),
        "compat": os.path.join(BENCH_ROOT, s.get("compat", "stdlib/compat/stdlib_compat.me")),
        "results": os.path.join(BENCH_ROOT, s.get("results", "stdlib/results/stdlib.json")),
        "plots_dir": os.path.join(BENCH_ROOT, s.get("plots_dir", "stdlib/plots")),
        "timeout": s.get("timeout", 20),
        "trials": s.get("trials", 5),
        "coq_timeout_multiplier": cfg.get("coq_timeout_multiplier", 1.5),
        "rocq_stdlib_src": exp(s.get("rocq_stdlib_src", "")),
    }


# ─────────────────────────── corpus discovery ────────────────────────────────

# Tier inferred from the unit-name prefix; kept in sync with the manifest.
def unit_tier(name):
    if name.startswith("bool_"):
        return "bool"
    if name.startswith("nat_"):
        return "nat_ground"
    if name.startswith("le_"):
        return "le"
    if name.startswith("logic_"):
        return "logic"
    return "other"


def discover_units(cfg):
    units = []
    for name in sorted(os.listdir(cfg["corpus_dir"])):
        d = os.path.join(cfg["corpus_dir"], name)
        if os.path.isdir(d) and os.path.exists(os.path.join(d, "rocq.v")):
            units.append(name)
    return units


def unit_paths(cfg, name):
    d = os.path.join(cfg["corpus_dir"], name)
    return os.path.join(d, "rocq.v"), os.path.join(d, "mengine.me"), d


# ───────────────────────────── timing core ───────────────────────────────────
# Mirrors framework.runner.run_single: N trials, keep the min successful time,
# kill the whole process group on timeout.

def time_command(cmd, cwd, timeout, trials):
    best = None
    success = False
    all_trials = []
    last_err = ""
    for _ in range(trials):
        start = time.perf_counter()
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    text=True, cwd=cwd, start_new_session=True)
            out, err = proc.communicate(timeout=timeout)
            elapsed = time.perf_counter() - start
            ok = proc.returncode == 0
            all_trials.append({"time_taken": elapsed, "success": ok})
            if not ok:
                last_err = (err or out)[:300]
            if ok:
                success = True
                if best is None or elapsed < best:
                    best = elapsed
            else:
                break  # don't repeat a failing point
        except subprocess.TimeoutExpired:
            elapsed = time.perf_counter() - start
            try:
                os.killpg(proc.pid, signal.SIGTERM)
                proc.communicate(timeout=1)
            except Exception:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except Exception:
                    pass
            all_trials.append({"time_taken": elapsed, "success": False, "timeout": True})
            last_err = f"timeout after {timeout}s"
            break
    return {"time_taken": best if best is not None else 0.0,
            "success": success, "trials": all_trials, "error": last_err}


# ─────────────────────────── per-engine commands ─────────────────────────────

def mengine_combined_file(cfg, mepath):
    """Write compat-prelude + unit into a temp .me; return its path."""
    import tempfile
    fd, path = tempfile.mkstemp(suffix=".me", prefix="stdlib_unit_")
    with os.fdopen(fd, "w") as f:
        with open(cfg["compat"]) as c:
            f.write(c.read())
        f.write("\n")
        with open(mepath) as m:
            f.write(m.read())
    return path


def run_mengine(cfg, name, timeout, trials):
    _v, mepath, _d = unit_paths(cfg, name)
    combined = mengine_combined_file(cfg, mepath)
    try:
        cmd = [cfg["mengine_path"], "-q", combined]
        res = time_command(cmd, cfg["mengine_root"] or None, timeout, trials)
    finally:
        os.remove(combined)
    return res


def run_rocq(cfg, name, timeout, trials):
    vpath, _m, d = unit_paths(cfg, name)
    cmd = [cfg["coq_path"], "-q", vpath]
    to = timeout * cfg["coq_timeout_multiplier"]
    res = time_command(cmd, d, to, trials)
    # clean coqc artifacts
    for ext in (".vo", ".vok", ".vos", ".glob"):
        p = vpath[:-2] + ext
        if os.path.exists(p):
            os.remove(p)
    for fn in os.listdir(d):
        if fn.endswith(".aux") or fn.startswith(".rocq") or fn.startswith(".coq"):
            try:
                os.remove(os.path.join(d, fn))
            except OSError:
                pass
    return res


# ─────────────────────────── faithfulness gate ───────────────────────────────

def mengine_statement_digests(cfg, mepath):
    with open(mepath) as f:
        return dict(translate.statement_digests(f.read()))


def rocq_statement_names(vpath):
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    names = []
    for m in re.finditer(r"\b(?:Lemma|Theorem|Example|Corollary|Fact|Remark|Proposition)\s+"
                         r"([A-Za-z_][A-Za-z0-9_']*)", text):
        names.append(m.group(1))
    return names


def test_unit(cfg, name):
    vpath, mepath, d = unit_paths(cfg, name)
    problems = []

    # 1. Rocq compiles.
    rr = run_rocq(cfg, name, cfg["timeout"], 1)
    if not rr["success"]:
        problems.append(f"rocq.v fails coqc: {rr['error'][:120]}")

    # 2. MEngine runs clean.
    mr = run_mengine(cfg, name, cfg["timeout"], 1)
    if not mr["success"]:
        problems.append(f"mengine.me fails: {mr['error'][:120]}")

    # 3. mengine.me re-derivable from rocq.v by the translator (no drift), and
    #    statement names correspond.
    tr = subprocess.run(["python3", os.path.join(HERE, "translate.py"), vpath],
                        capture_output=True, text=True)
    if tr.returncode != 0:
        problems.append(f"translator flags rocq.v: {tr.stderr.strip()[:120]}")
    else:
        with open(mepath) as f:
            on_disk = f.read()
        if _norm(on_disk) != _norm(tr.stdout):
            problems.append("mengine.me is stale vs translate.py output (re-run manifest)")
        me_names = set(dict(translate.statement_digests(tr.stdout)).keys())
        rq_names = set(rocq_statement_names(vpath))
        if me_names != rq_names:
            problems.append(f"statement names differ: rocq={rq_names} mengine={me_names}")

    return problems


def _norm(s):
    return "\n".join(line.rstrip() for line in s.strip().splitlines())


# ───────────────────────────── manifest ──────────────────────────────────────

def sha256_file(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()[:16]


def build_manifest(cfg):
    units = []
    for name in discover_units(cfg):
        vpath, mepath, _d = unit_paths(cfg, name)
        digests = list(translate.statement_digests(open(mepath).read()))
        units.append({
            "name": name,
            "tier": "A",
            "category": unit_tier(name),
            "rocq_sha256": sha256_file(vpath),
            "statements": [{"name": n, "digest": dg} for n, dg in digests],
            "deps": "Coq.Init (auto-loaded)",
        })
    manifest = {
        "description": "Tier-A: Rocq stdlib units auto-translated with zero manual "
                       "edits and proved by MEngine + the compat prelude.",
        "engine_note": "Requires the cbv applied-fix and GC-dedup kernel fixes "
                       "(see benchmarks/stdlib/README.md).",
        "units": units,
        "excluded": EXCLUDED_DOC,
    }
    out = os.path.join(cfg["corpus_dir"], "manifest.json")
    with open(out, "w") as f:
        json.dump(manifest, f, indent=2)
    return out, len(units)


# Documented Tier-2 boundary (kept out of Tier A); see README.
EXCLUDED_DOC = [
    {"pattern": "destruct/case-analysis with a constant RHS "
                "(e.g. andb b false = false)",
     "boundary": "tier2-unify",
     "reason": "apply cannot solve the eliminator's scrutinee evar when the "
               "scrutinee variable has no bare occurrence in the goal; the "
               "unifier whnf-folds the function application into a stuck match."},
    {"pattern": "computational induction over a Fixpoint "
                "(e.g. add n 0 = n, n + 0 = n)",
     "boundary": "tier2-symbolic",
     "reason": "needs symbolic-argument fixpoint conversion; hits the documented "
               "stack-overflow (bugs/segfault_apply_fixpoint_motive.me, "
               "bugs/segfault_exact_eliminator_fixpoint.me)."},
]


# ─────────────────────────────── subcommands ─────────────────────────────────

def cmd_list(cfg, args):
    units = discover_units(cfg)
    by_cat = {}
    for u in units:
        by_cat.setdefault(unit_tier(u), []).append(u)
    print(f"Corpus: {len(units)} Tier-A units under {os.path.relpath(cfg['corpus_dir'])}\n")
    for cat in sorted(by_cat):
        print(f"  {cat} ({len(by_cat[cat])}):")
        for u in by_cat[cat]:
            print(f"    - {u}")


def cmd_test(cfg, args):
    units = args.units or discover_units(cfg)
    npass = 0
    for u in units:
        problems = test_unit(cfg, u)
        if problems:
            print(f"  [FAIL] {u}")
            for p in problems:
                print(f"          {p}")
        else:
            print(f"  [ OK ] {u}")
            npass += 1
    print(f"\n{npass}/{len(units)} units pass the faithfulness gate.")
    return 0 if npass == len(units) else 1


def cmd_run(cfg, args):
    units = args.units or discover_units(cfg)
    results = load_results(cfg["results"])
    for u in units:
        mr = run_mengine(cfg, u, cfg["timeout"], cfg["trials"])
        rr = run_rocq(cfg, u, cfg["timeout"], cfg["trials"])
        results[f"mengine_{u}"] = mr
        results[f"coq_{u}"] = rr
        save_results(cfg["results"], results)
        ms = f"{mr['time_taken']*1000:.1f}ms" if mr["success"] else "FAIL"
        rs = f"{rr['time_taken']*1000:.1f}ms" if rr["success"] else "FAIL"
        ratio = (f"{rr['time_taken']/mr['time_taken']:.2f}x"
                 if mr["success"] and rr["success"] and mr["time_taken"] > 0 else "-")
        print(f"  {u:24s} MEngine={ms:>10s}  Rocq={rs:>10s}  speedup={ratio}")
    print(f"\nResults written to {os.path.relpath(cfg['results'])}")


def cmd_report(cfg, args):
    import report
    report.generate(cfg)


def cmd_manifest(cfg, args):
    out, n = build_manifest(cfg)
    print(f"Wrote {os.path.relpath(out)} ({n} units).")


def cmd_triage(cfg, args):
    d = args.dir or cfg["rocq_stdlib_src"]
    if not d or not os.path.isdir(d):
        print(f"stdlib source dir not found: {d}", file=sys.stderr)
        return 1
    subprocess.run(["python3", os.path.join(HERE, "translate.py"), "--report", "--dir", d])


# ───────────────────────────── results io ────────────────────────────────────

def load_results(path):
    if os.path.exists(path):
        with open(path) as f:
            return json.load(f)
    return {}


def save_results(path, results):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(results, f, indent=2)


# ──────────────────────────────── main ───────────────────────────────────────

def main():
    cfg = load_config()
    ap = argparse.ArgumentParser(description="Rocq-stdlib benchmark corpus runner")
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("list")
    p_test = sub.add_parser("test"); p_test.add_argument("units", nargs="*")
    p_run = sub.add_parser("run"); p_run.add_argument("units", nargs="*")
    sub.add_parser("report")
    sub.add_parser("manifest")
    p_tri = sub.add_parser("triage"); p_tri.add_argument("--dir")
    args = ap.parse_args()

    dispatch = {
        "list": cmd_list, "test": cmd_test, "run": cmd_run,
        "report": cmd_report, "manifest": cmd_manifest, "triage": cmd_triage,
    }
    rc = dispatch[args.command](cfg, args)
    sys.exit(rc or 0)


if __name__ == "__main__":
    main()
