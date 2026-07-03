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
    fidelity [module ...] verify each curated statement matches its real stdlib
                         counterpart (corpus/stdlib_map.json), via Rocq's kernel
    proof-fidelity [module ...]  regenerate corpus/PROOF_FIDELITY.md: each
                         corpus proof vs the real stdlib proof, with the
                         structural reason the two diverge (re-extracted from
                         the installed stdlib; statements are still checked by
                         `fidelity`)
"""

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
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
    }


# ─────────────────────────── corpus discovery ────────────────────────────────

# Each unit is now a whole stdlib *module* file (Bool, Logic, Nat, Peano),
# mirroring how the standard library is organized — not a one-lemma fragment.


def discover_units(cfg):
    return translate.corpus_modules(cfg["corpus_dir"])


def unit_paths(cfg, name):
    d = os.path.join(cfg["corpus_dir"], name)
    return os.path.join(d, "rocq.v"), os.path.join(d, "mengine.me"), d


def unit_statement_digests(mepath):
    """(name, digest) pairs for each Theorem in a unit's mengine.me."""
    with open(mepath) as f:
        return translate.statement_digests(f.read())


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
        except OSError as e:
            # Missing/unexecutable binary: record a failed trial and stop, like the
            # sibling scripts (translate/fidelity) — don't crash the whole run.
            last_err = f"could not run {cmd[0]!r}: {e}"
            all_trials.append({"time_taken": None, "success": False})
            break
        try:
            out, err = proc.communicate(timeout=timeout)
            elapsed = time.perf_counter() - start
            ok = proc.returncode == 0
            all_trials.append({"time_taken": elapsed, "success": ok})
            if ok:
                success = True
                if best is None or elapsed < best:
                    best = elapsed
            else:
                last_err = (err or out)[:300]
                break  # don't repeat a failing point
        except subprocess.TimeoutExpired:
            elapsed = time.perf_counter() - start
            # Mirror what subprocess.run(timeout=…) does on expiry — kill, then
            # communicate() to reap — but across the whole process group, since
            # start_new_session=True made the child a group leader.  SIGKILL can't
            # be caught, so a single killpg + communicate() reliably tears down the
            # child and any grandchildren with no lingering pipe or zombie.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass  # already exited between timeout and kill
            proc.communicate()
            all_trials.append({"time_taken": elapsed, "success": False, "timeout": True})
            last_err = f"timeout after {timeout}s"
            break
    return {"time_taken": best,
            "success": success, "trials": all_trials, "error": last_err}


# ─────────────────────────── per-engine commands ─────────────────────────────

def mengine_combined_file(cfg, mepath):
    """Write compat-prelude + unit into a temp .me; return its path."""
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
    translate.clean_coqc_byproducts(vpath)  # keep the corpus rocq.v, drop .vo/.aux/…
    return res


# ─────────────────────────── startup baselines ───────────────────────────────
# Whole-file wall-clock is dominated by fixed per-invocation cost: a coqc process
# plus its auto-loaded Prelude (~65 ms here), and an mengine process plus
# prelude/tactics.me + the compat prelude (~3-6 ms).  None of that is the unit's
# proof.  We time each engine's preamble *alone* — an empty .v for Rocq (same
# Prelude every unit auto-loads; the corpus has no `Require`), and the compat
# prelude with no unit appended for MEngine — and the report subtracts it to
# isolate the marginal statement+proof cost.  Measured once and reused, so we
# spend extra trials to pin the floor down.

def run_mengine_baseline(cfg, timeout, trials):
    """Time the compat prelude with no unit: MEngine's per-invocation floor."""
    cmd = [cfg["mengine_path"], "-q", cfg["compat"]]
    return time_command(cmd, cfg["mengine_root"] or None, timeout, trials)


def run_rocq_baseline(cfg, timeout, trials):
    """Time an empty .v: coqc startup + auto-loaded Prelude, no statement."""
    fd, path = tempfile.mkstemp(suffix=".v", prefix="stdlib_baseline_")
    os.close(fd)  # empty file
    try:
        cmd = [cfg["coq_path"], "-q", path]
        to = timeout * cfg["coq_timeout_multiplier"]
        res = time_command(cmd, os.path.dirname(path), to, trials)
    finally:
        translate.clean_coqc_temp(path)  # temp .v + its coqc byproducts
    return res


def module_rocq_preamble(vpath):
    """The leading directive sentences of a corpus rocq.v (everything the
    translator drops before its first statement): its Require/Import/Open/Set
    lines.  Returns a .v source string, possibly empty (a Require-free module).

    Reuses ``translate.is_dropped`` — the translator's own drop-list — as the
    single source of truth, so the *timed* preamble here and the preamble the
    translator *discards* can never diverge (e.g. a leading ``#[...]`` attribute
    or ``Declare Scope`` is recognized by both, or by neither)."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    out = []
    for s in translate.split_sentences(text):
        s = s.strip()
        if translate.is_dropped(s):
            out.append(s + ".")
        else:
            break
    return "\n".join(out)


def run_rocq_module_baseline(cfg, name, timeout, trials):
    """Time a .v holding only this module's Require/Import preamble — the module's
    own Rocq startup floor.  A `Require`-free module yields an empty file, i.e.
    the same Prelude-only floor as the global baseline; a module that loads a
    library (Lists -> `Require Import List`) pays that load here, so the report
    subtracts it instead of charging it to the proof.  Run from the unit dir so
    load paths resolve exactly as the unit's own compile does."""
    vpath, _m, d = unit_paths(cfg, name)
    pre = module_rocq_preamble(vpath)
    fd, path = tempfile.mkstemp(suffix=".v", prefix="stdlib_pre_", dir=d)
    with os.fdopen(fd, "w") as f:
        f.write(pre + "\n")
    try:
        cmd = [cfg["coq_path"], "-q", os.path.basename(path)]
        to = timeout * cfg["coq_timeout_multiplier"]
        res = time_command(cmd, d, to, trials)
    finally:
        translate.clean_coqc_temp(path)  # temp .v + its coqc byproducts
    return res


# Keys under which baselines are stored in results (alongside per-unit keys).
# Baseline keys use a double underscore so report's per-unit key discovery (which
# splits on the first '_') can exclude them by testing for "__".
MENGINE_BASELINE_KEY = "mengine__startup_baseline"
ROCQ_BASELINE_KEY = "coq__startup_baseline"


def rocq_module_baseline_key(name):
    return f"coq__baseline__{name}"


# ─────────────────────────── faithfulness gate ───────────────────────────────

def rocq_statement_names(vpath):
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    pat = r"\b(?:" + translate.THEOREM_ALT + \
          r")\s+([A-Za-z_][A-Za-z0-9_']*)"
    return [m.group(1) for m in re.finditer(pat, text)]


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
    #    statement names correspond.  Statement types are elaborated through Rocq
    #    (`Set Printing All`), so the gate runs the translator's own pipeline
    #    (rocq_elaborate -> translate_unit) directly, exactly as translate.py's
    #    CLI does.
    try:
        with open(vpath) as f:
            vtext = f.read()
        elab = translate.rocq_elaborate(vtext, vpath, cfg["coq_path"],
                                        translate.statement_names(vtext))
        me_src = translate.translate_unit(vtext, translate.Report(), elab)
    except translate.Untranslatable as e:
        problems.append(f"translator flags rocq.v: {e.reason[:120]}")
    else:
        with open(mepath) as f:
            on_disk = f.read()
        if _norm(on_disk) != _norm(me_src):
            problems.append("mengine.me is stale vs translate.py output (re-run manifest)")
        me_names = set(dict(translate.statement_digests(me_src)).keys())
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
        digests = unit_statement_digests(mepath)
        units.append({
            "name": name,
            "tier": "A",
            "category": name,  # the category is simply the module name
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
    {"pattern": "multi-variable case analysis / nested induction "
                "(e.g. andb_comm, orb b1 b2 = orb b2 b1, de Morgan)",
     "boundary": "tier2-nested",
     "reason": "the translator emits one `apply (<T>_ind motive)` per proof and "
               "segments a single level of cases; a second destruct inside a case "
               "(needed to decide a goal in two booleans) is not yet generated."},
    {"pattern": "induction over an inductive relation "
                "(e.g. le_trans, le_n_S via le_ind)",
     "boundary": "tier2-relational",
     "reason": "the eliminator has a dependent motive over the derivation; the "
               "translator only builds non-dependent `fun (x:T) => <body>` motives."},
]


# ─────────────────────────────── subcommands ─────────────────────────────────

def cmd_list(cfg, args):
    units = discover_units(cfg)
    total = 0
    print(f"Corpus: {len(units)} Tier-A module files under "
          f"{os.path.relpath(cfg['corpus_dir'])} "
          f"(one file per stdlib module)\n")
    for u in units:
        _v, mepath, _d = unit_paths(cfg, u)
        names = [n for n, _dg in unit_statement_digests(mepath)]
        total += len(names)
        print(f"  {u} ({len(names)} lemmas):")
        for n in names:
            print(f"    - {n}")
    print(f"\n{total} lemmas across {len(units)} modules.")


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
    print("(statement-vs-stdlib correspondence is a separate check: "
          "stdlib_bench.py fidelity)")
    return 0 if npass == len(units) else 1


def cmd_run(cfg, args):
    units = args.units or discover_units(cfg)
    results = load_results(cfg["results"])

    # Measure each engine's fixed startup/preamble floor once (extra trials to
    # pin its min and stddev down — the report subtracts the min from every
    # unit's whole-file time and uses the stddev as the noise floor).  Measured
    # once and reused, so the extra trials are cheap.  See run_*_baseline above.
    base_trials = max(cfg["trials"], 25)
    mb = run_mengine_baseline(cfg, cfg["timeout"], base_trials)
    rb = run_rocq_baseline(cfg, cfg["timeout"], base_trials)
    results[MENGINE_BASELINE_KEY] = mb
    results[ROCQ_BASELINE_KEY] = rb
    save_results(cfg["results"], results)
    mb_t, rb_t = mb["time_taken"], rb["time_taken"]
    print(f"  {'startup baseline':24s} "
          f"MEngine={mb_t*1000:8.1f}ms  Rocq={rb_t*1000:8.1f}ms\n")

    for u in units:
        # Each module's own Rocq startup floor: its Require/Import preamble timed
        # alone.  Require-free modules match the global empty-.v floor; Lists
        # (`Require Import List`) pays the library load here so the report does
        # not charge it to the proof.
        rmb = run_rocq_module_baseline(cfg, u, cfg["timeout"], base_trials)
        results[rocq_module_baseline_key(u)] = rmb
        rb_u = rmb["time_taken"] if rmb["success"] else rb_t

        mr = run_mengine(cfg, u, cfg["timeout"], cfg["trials"])
        rr = run_rocq(cfg, u, cfg["timeout"], cfg["trials"])
        results[f"mengine_{u}"] = mr
        results[f"coq_{u}"] = rr
        save_results(cfg["results"], results)
        # Proof-only = whole-file minus the engine's startup floor (clamped at 0;
        # a tiny proof can dip below baseline jitter).  Rocq uses the module's own
        # preamble floor (rb_u); MEngine's preamble (the compat prelude) is the
        # same for every module.
        mp = max(0.0, mr["time_taken"] - mb_t) if mr["success"] else None
        rp = max(0.0, rr["time_taken"] - rb_u) if rr["success"] else None
        ms = f"{mr['time_taken']*1000:.1f}ms" if mr["success"] else "FAIL"
        rs = f"{rr['time_taken']*1000:.1f}ms" if rr["success"] else "FAIL"
        mps = f"{mp*1000:.1f}ms" if mp is not None else "FAIL"
        rps = f"{rp*1000:.1f}ms" if rp is not None else "FAIL"
        ratio = f"{rp/mp:.2f}x" if mp and rp else "-"
        print(f"  {u:24s} MEngine={ms:>9s}(proof {mps:>8s})  "
              f"Rocq={rs:>9s}(proof {rps:>8s})  proof-speedup={ratio}")
    print(f"\nResults written to {os.path.relpath(cfg['results'])}")


def cmd_report(cfg, args):
    import report
    report.generate(cfg)


def cmd_manifest(cfg, args):
    out, n = build_manifest(cfg)
    print(f"Wrote {os.path.relpath(out)} ({n} units).")


def cmd_fidelity(cfg, args):
    import fidelity
    return fidelity.run(cfg, args.modules or None)


def cmd_proof_fidelity(cfg, args):
    import proof_fidelity
    return proof_fidelity.run(cfg, args.modules or None)


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
    p_fid = sub.add_parser("fidelity"); p_fid.add_argument("modules", nargs="*")
    p_pf = sub.add_parser("proof-fidelity"); p_pf.add_argument("modules", nargs="*")
    args = ap.parse_args()

    dispatch = {
        "list": cmd_list, "test": cmd_test, "run": cmd_run,
        "report": cmd_report, "manifest": cmd_manifest,
        "fidelity": cmd_fidelity, "proof-fidelity": cmd_proof_fidelity,
    }
    rc = dispatch[args.command](cfg, args)
    sys.exit(rc or 0)


if __name__ == "__main__":
    main()
