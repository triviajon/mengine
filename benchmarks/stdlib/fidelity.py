#!/usr/bin/env python3
"""Statement-level fidelity check: curated corpus lemma vs real Rocq stdlib.

The corpus `rocq.v` files are *hand-curated* re-statements of standard-library
lemmas (the real stdlib files don't translate to MEngine — see README §"Why
whole stdlib files don't translate").  That hand step is the one place a curated
statement could silently drift from the library lemma it claims to be.  This
module closes that gap by letting **Rocq's own kernel** compare the two:

  - For a lemma mapped to a stdlib counterpart, it emits
    ``Check (<stdlib_ref> : <curated statement>).`` — Rocq accepts it iff the
    stdlib lemma's type is convertible to the curated statement's type.
  - For a counterpart whose statement is the *mirror* of the curated one
    (``relation: symmetry`` — e.g. stdlib ``add_assoc`` is the other
    orientation), it emits ``Goal <curated>. Proof. intros; symmetry; apply
    <stdlib_ref>. Qed.``, verifying the two are equivalent up to ``eq_sym``.
  - A curated lemma with no library counterpart (ground computation, bespoke
    combination) is declared ``original`` in the map and reported, not checked.

The map (`corpus/stdlib_map.json`) is the single source of truth and must cover
every curated lemma; an unmapped lemma is a failure, so the check stays honest
as the corpus grows.  Nothing here is guessed — every ref/relation was confirmed
against the installed Rocq before being recorded (the translator's "flag, never
guess" principle, applied to the curation step).
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local module; reuse its comment/sentence split)

STMT_KW = ("Lemma", "Theorem", "Example", "Corollary", "Fact", "Remark",
           "Proposition")
STMT_RE = re.compile(r"^(?:" + "|".join(STMT_KW) +
                     r")\s+([A-Za-z_][A-Za-z0-9_']*)\s*(.*)$", re.S)


def _split_top_colon(s):
    """Split `s` at its first top-level ':' (paren depth 0). Returns
    (before, after) or (None, None) if there is none."""
    depth = 0
    for i, c in enumerate(s):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == ":" and depth == 0:
            return s[:i], s[i + 1:]
    return None, None


def extract_statements(vpath):
    """Ordered [(name, rocq_type_text)] for every theorem-like statement in a
    curated `rocq.v`.  The type text is verbatim Rocq surface syntax (notation,
    implicits intact) — exactly what we ascribe the stdlib lemma to."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    out = []
    for s in translate.split_sentences(text):
        m = STMT_RE.match(s.strip())
        if not m:
            continue
        name, rest = m.group(1), m.group(2)
        binders, typ = _split_top_colon(rest)
        if typ is None:
            raise ValueError(f"{name}: statement has no top-level ':'")
        typ = re.sub(r"\s+", " ", typ).strip()
        binders = re.sub(r"\s+", " ", binders).strip()
        rocq_type = f"forall {binders}, {typ}" if binders else typ
        out.append((name, rocq_type))
    return out


def _run_coqc(coq, preamble, body, workdir):
    """Write preamble+body to a temp .v, run coqc, return (ok, message)."""
    fd, path = tempfile.mkstemp(suffix=".v", prefix="fidelity_", dir=workdir)
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(preamble) + "\n" + body + "\n")
    try:
        try:
            proc = subprocess.run([coq, "-q", os.path.basename(path)],
                                  capture_output=True, text=True, cwd=workdir)
        except OSError as e:
            return False, f"could not run '{coq}': {e}"
        if proc.returncode == 0:
            return True, ""
        msg = (proc.stderr or proc.stdout).strip()
        msg = re.sub(r"\s+", " ", msg)
        return False, msg
    finally:
        base = os.path.splitext(path)[0]
        leftovers = [base + ext for ext in (".v", ".vo", ".vok", ".vos", ".glob")]
        # coqc also drops a hidden `.<name>.aux` next to the source.
        leftovers.append(os.path.join(workdir, "." + os.path.basename(base) + ".aux"))
        for p in leftovers:
            if os.path.exists(p):
                try:
                    os.remove(p)
                except OSError:
                    pass


def check_lemma(coq, preamble, entry, rocq_type, workdir):
    """Run the Rocq-side check for one curated lemma.

    Returns (status, ref, detail) where status is one of:
      'match'    convertible to the stdlib lemma (Check ascription succeeded),
      'symmetry' verified the mirror equivalence (symmetry; apply succeeded),
      'original' no stdlib counterpart (declared in the map; not checked),
      'MISMATCH' the claimed correspondence does NOT hold (a real finding),
      'ERROR'    the check could not run (e.g. stdlib ref not found)."""
    if "original" in entry:
        return "original", None, entry["original"]
    ref = entry.get("stdlib")
    if not ref:
        return "ERROR", None, "map entry has neither 'stdlib' nor 'original'"
    relation = entry.get("relation", "convertible")
    if relation == "convertible":
        ok, msg = _run_coqc(coq, preamble, f"Check ({ref} : {rocq_type}).", workdir)
        return ("match" if ok else "MISMATCH"), ref, msg
    if relation == "symmetry":
        body = (f"Goal {rocq_type}.\n"
                f"Proof. intros; symmetry; apply {ref}. Qed.")
        ok, msg = _run_coqc(coq, preamble, body, workdir)
        return ("symmetry" if ok else "MISMATCH"), ref, msg
    return "ERROR", ref, f"unknown relation '{relation}'"


def _load_map(corpus_dir):
    import json
    with open(os.path.join(corpus_dir, "stdlib_map.json")) as f:
        return json.load(f)


_LABEL = {"match": "match", "symmetry": "symmetry", "original": "original",
          "MISMATCH": "MISMATCH", "ERROR": "ERROR", "UNMAPPED": "UNMAPPED"}


def run(cfg, modules=None):
    """Check every curated lemma's statement against its stdlib counterpart.

    Returns 0 if every mapped correspondence holds and every curated lemma is
    mapped; 1 if any mismatch / unmapped lemma / stale map entry / coqc error."""
    corpus_dir = cfg["corpus_dir"]
    coq = cfg["coq_path"]
    spec = _load_map(corpus_dir)
    preamble = spec["preamble"]
    mod_map = spec["modules"]

    all_mods = sorted(n for n in os.listdir(corpus_dir)
                      if os.path.isdir(os.path.join(corpus_dir, n))
                      and os.path.exists(os.path.join(corpus_dir, n, "rocq.v")))
    sel = modules or all_mods

    tally = {k: 0 for k in ("match", "symmetry", "original", "MISMATCH",
                            "ERROR", "UNMAPPED", "STALE")}
    failures = []
    workdir = tempfile.mkdtemp(prefix="fidelity_")
    try:
        for mod in sel:
            vpath = os.path.join(corpus_dir, mod, "rocq.v")
            if not os.path.exists(vpath):
                print(f"  ! no such module: {mod}", file=sys.stderr)
                continue
            stmts = extract_statements(vpath)
            entries = mod_map.get(mod, {})
            print(f"\n{mod} ({len(stmts)} lemmas):")
            seen = set()
            for name, rocq_type in stmts:
                seen.add(name)
                if name not in entries:
                    tally["UNMAPPED"] += 1
                    failures.append(f"{mod}.{name}: not in stdlib_map.json")
                    print(f"  [UNMAPPED] {name}  (add it to stdlib_map.json)")
                    continue
                status, ref, detail = check_lemma(coq, preamble, entries[name],
                                                  rocq_type, workdir)
                tally[status] += 1
                if status == "match":
                    print(f"  [match   ] {name:18s} <- {ref}")
                elif status == "symmetry":
                    print(f"  [symmetry] {name:18s} <- {ref}  "
                          f"(mirror; verified equivalent up to eq_sym)")
                elif status == "original":
                    print(f"  [original] {name:18s}    ({detail})")
                else:  # MISMATCH / ERROR
                    print(f"  [{_LABEL[status]:8s}] {name:18s} <- {ref}")
                    print(f"             {detail[:240]}")
                    failures.append(f"{mod}.{name} ({status}): {detail[:160]}")
            # Stale map entries: a name in the map with no curated lemma.
            for name in entries:
                if name not in seen:
                    tally["STALE"] += 1
                    failures.append(f"{mod}.{name}: in stdlib_map.json but not in rocq.v")
                    print(f"  [STALE   ] {name}  (in map, absent from rocq.v)")
    finally:
        try:
            os.rmdir(workdir)
        except OSError:
            pass

    total = sum(tally[k] for k in ("match", "symmetry", "original", "MISMATCH",
                                   "ERROR", "UNMAPPED"))
    print(f"\n{total} curated lemmas: {tally['match']} convertible, "
          f"{tally['symmetry']} symmetry-variant, "
          f"{tally['original']} original (no stdlib counterpart), "
          f"{tally['MISMATCH']} mismatch, {tally['ERROR']} error, "
          f"{tally['UNMAPPED']} unmapped, {tally['STALE']} stale.")
    if failures:
        print("\nFIDELITY FAILURES:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\nAll curated statements correspond to their stdlib counterparts.")
    return 0
