#!/usr/bin/env python3
"""Proof-script fidelity record: corpus proof vs the real Rocq stdlib proof.

`fidelity.py` checks that each curated *statement* matches its stdlib counterpart
(`.v` <- stdlib, by convertibility in Rocq's kernel).  This module documents the
*remaining* gap: the corpus **proof scripts** are not — and for structural reasons
**cannot** be — the stdlib's verbatim proofs.  It regenerates
`corpus/PROOF_FIDELITY.md`, re-extracting the real proof of every mapped lemma
straight from the **installed** stdlib (it asks `coqc` `About <ref>` for the exact
source location, then reads back the verbatim `Proof … Qed` block), so the record
stays current and is never guessed — the same "flag, never guess" discipline the
translator and `fidelity.py` use.

It classifies *why* each corpus proof must diverge from the library's:

  constructor     the lemma maps to an inductive **constructor** (`conj`, `le_n`,
                  `or_introl`, `ex_intro`, `eq_refl`): the library has no proof
                  script at all; the corpus builds the same term with
                  `constructor`/`split`/`left`/`right`/`exists`/`reflexivity`.
  functor         the library proof is produced by module-functor `Include` over
                  an abstract setoid structure (`==`, custom Ltac `nzinduct`/
                  `nzsimpl`); there is no concrete-`nat` proof script to copy.
                  The corpus reproves it with explicit `nat_ind` + `rewrite`.
  untranslatable  a concrete script exists, but uses tactics MEngine has no
                  equivalent of (the `destr_bool` Ltac macro, `;` chaining,
                  `auto`, `f_equal`, `destruct N`, `trivial`, `discriminate`, …).
  near-match      the library proof is `reflexivity`/`trivial`-only and the
                  corpus proof has the same shape (modulo MEngine surface syntax).
  original        no named stdlib counterpart (ground computation / bespoke
                  combination, per `stdlib_map.json`); nothing to be faithful to.

The categories are not value judgements: only `near-match` could ever be verbatim;
every other category is a structural reason the proof *cannot* be the stdlib's.
"""

import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local module: comment/sentence split)

# Tactics / constructs that MEngine genuinely has *no* equivalent of (so a script
# containing them cannot run verbatim), each with a human-readable gloss.  NOTE:
# this is *not* about the tactic *combinators* — MEngine has `;`, `||`, `try`,
# `repeat`, `first […]`, `match Goal`, and emulates `auto`/`trivial`/`now`/`simpl`/
# `symmetry` in the prelude + compat prelude (the corpus proofs use `;` and
# `split; assumption` themselves).  What is missing is specific *leaf* tactics,
# Ltac macros, and automation.
BLOCKING = [
    (r"\bdestr_bool\b", "the `destr_bool` Ltac macro "
                        "(`destruct_all bool; simpl in *; trivial; try discriminate`)"),
    (r"\bdestruct_all\b", "`destruct_all`"),
    (r"\bnzinduct\b", "the abstract-functor Ltac `nzinduct`"),
    (r"\bnzsimpl'?\b", "the abstract-functor Ltac `nzsimpl` (`autorewrite with nz`)"),
    (r"\bautorewrite\b", "`autorewrite` (hint-database rewriting)"),
    (r"\bf_equal\b", "`f_equal`"),
    (r"\bdestruct\s+\d", "`destruct N` (case-analyse the N-th premise — MEngine "
                         "only eliminates a named/leading variable)"),
    (r"\bdestruct\s+[A-Za-z_]", "`destruct` of a hypothesis"),
    (r"\bapply\b[^.]*\bin\b", "`apply … in H` (forward reasoning into a hypothesis)"),
    (r"\bdiscriminate\b", "`discriminate`"),
    (r"\binduction\b[^.]*\busing\b", "`induction … using`"),
    (r"\blia\b", "`lia`"),
    (r"\bring\b", "`ring`"),
    (r"\binversion\b", "`inversion`"),
    (r"\bfalse_hyp\b", "the `false_hyp` Ltac"),
]

# Tactics MEngine *emulates* but more weakly than Rocq — present, yet not strong
# enough to replay the stdlib proof as written.  Surfaced in the note when no
# strictly-absent construct is found, so the divergence is still explained.
WEAKER = [
    (r"\bauto\b", "`auto` (MEngine's emulated `auto` is `repeat intro; try "
                  "assumption; try reflexivity` — far weaker than Rocq's hint "
                  "search)"),
]


# ───────────────────────────── stdlib location ───────────────────────────────

def _coqlib_roots(coq):
    """Filesystem roots Rocq library DirPaths resolve under."""
    try:
        out = subprocess.run([coq, "-config"], capture_output=True, text=True).stdout
    except OSError as e:
        raise RuntimeError(f"could not run '{coq} -config': {e}")
    m = re.search(r"^COQLIB=(.*)$", out, re.M)
    if not m:
        raise RuntimeError("coqc -config did not report COQLIB")
    lib = m.group(1).strip()
    return [os.path.join(lib, "theories"), os.path.join(lib, "user-contrib")]


def _resolve_source(dirpath, roots):
    """A Rocq library DirPath (`Stdlib.Bool.Bool`, `Corelib.Init.Logic`) -> the
    `.v` file on disk, or None.  `Stdlib.*` lives under `user-contrib/Stdlib/…`;
    `Corelib.*` under `theories/…` with the `Corelib` segment dropped."""
    parts = dirpath.split(".")
    for root in roots:
        for rel in (parts, parts[1:]):  # full path, or drop the top segment
            cand = os.path.join(root, *rel) + ".v"
            if os.path.exists(cand):
                return cand
    return None


def _about(coq, ref, preamble, workdir):
    """Run `About <ref>.` and parse it.  Returns (kind, dirpath, line) where kind
    is 'Constant' | 'Constructor' | other, or raises if the ref doesn't resolve."""
    fd, path = tempfile.mkstemp(suffix=".v", prefix="about_", dir=workdir)
    bare = ref.lstrip("@")
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(preamble) + f"\nAbout {bare}.\n")
    try:
        proc = subprocess.run([coq, "-q", os.path.basename(path)],
                              capture_output=True, text=True, cwd=workdir)
        # coqc wraps query output at ~78 cols, so "Declared in\nlibrary …" and
        # "Expands to:\nConstant …" can straddle a newline — flatten first.
        flat = re.sub(r"\s+", " ", proc.stdout + proc.stderr)
        if proc.returncode != 0 and "Expands to" not in flat:
            raise RuntimeError(f"About {bare} failed: {flat.strip()[:200]}")
        kind = None
        m = re.search(r"Expands to:\s*(\w+)\b", flat)
        if m:
            kind = m.group(1)
        m = re.search(r"Declared in library\s+([\w.]+),\s*line\s+(\d+)", flat)
        if not m:
            raise RuntimeError(f"About {bare}: no 'Declared in library' line")
        return kind, m.group(1), int(m.group(2))
    finally:
        base = os.path.splitext(path)[0]
        for ext in (".v", ".vo", ".vok", ".vos", ".glob"):
            try:
                os.remove(base + ext)
            except OSError:
                pass
        try:
            os.remove(os.path.join(workdir, "." + os.path.basename(base) + ".aux"))
        except OSError:
            pass


def _extract_block(vfile, line, kind):
    """Verbatim source for a lemma/constructor at 1-based `line` in `vfile`.

    For a Constant: the `Lemma/Theorem … Proof … Qed/Defined` block.  For a
    Constructor: the single constructor arm (the `| name : …` line, or the
    `Inductive …` header line if the constructor sits there)."""
    with open(vfile) as f:
        lines = f.readlines()
    i = line - 1
    if i < 0 or i >= len(lines):
        return f"(could not read {vfile}:{line})"
    if kind == "Constructor":
        return lines[i].rstrip()
    out = []
    for j in range(i, len(lines)):
        out.append(lines[j].rstrip())
        if re.search(r"\b(Qed|Defined|Admitted)\.", lines[j]):
            break
    return "\n".join(out)


def _proof_body(block):
    """Just the tactic text of a `Proof. … Qed.` block (drop statement + framing),
    collapsed to one line for the blocking-tactic scan and the note."""
    m = re.search(r"\bProof\b\.?\s*(.*?)\s*\b(Qed|Defined|Admitted)\b\.",
                  block, re.S)
    body = m.group(1) if m else block
    return re.sub(r"\s+", " ", body).strip()


def _scan(proof_body, table):
    """Glosses from `table` whose pattern appears in `proof_body`, de-duped in
    declaration order (so the note lists each reason once)."""
    found, seen = [], set()
    for pat, gloss in table:
        if re.search(pat, proof_body) and gloss not in seen:
            found.append(gloss)
            seen.add(gloss)
    return found


# ───────────────────────────── corpus parsing ────────────────────────────────

CORPUS_STMT = re.compile(
    r"\b(?:Lemma|Theorem|Example|Corollary|Fact|Remark|Proposition)\s+"
    r"([A-Za-z_][A-Za-z0-9_']*)\b")
CLOSER = re.compile(r"\b(?:Qed|Defined|Admitted)\.")


def _corpus_proofs(vpath):
    """{name: verbatim proof body} for every curated lemma in a corpus rocq.v.

    The body is the text between `Proof.` and `Qed./Defined./Admitted.`, with the
    statement and framing dropped and whitespace collapsed — so it keeps the
    proof's own `.` / bullet structure (`induction b. - reflexivity. ...`)."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    body = {}
    for m in CORPUS_STMT.finditer(text):
        name = m.group(1)
        close = CLOSER.search(text, m.end())
        block = text[m.end():close.end()] if close else text[m.end():]
        pm = re.search(r"\bProof\b\.?\s*(.*?)\s*\b(?:Qed|Defined|Admitted)\.",
                       block, re.S)
        proof = pm.group(1) if pm else block
        body[name] = re.sub(r"\s+", " ", proof).strip()
    return body


# ───────────────────────────── classification ────────────────────────────────

def classify(coq, ref, preamble, roots, workdir):
    """(category, detail-dict) for one mapped lemma.  detail carries the verbatim
    stdlib proof, its source location, and any blocking constructs."""
    kind, dirpath, line = _about(coq, ref, preamble, workdir)
    vfile = _resolve_source(dirpath, roots)
    loc = (f"{os.path.relpath(vfile, os.path.dirname(roots[0]))}:{line}"
           if vfile else f"{dirpath}:{line}")
    block = _extract_block(vfile, line, kind) if vfile else "(source not found)"
    detail = {"loc": loc, "dirpath": dirpath, "block": block, "kind": kind}
    if kind == "Constructor":
        detail["blocking"] = []
        return "constructor", detail
    pbody = _proof_body(block)
    detail["proof"] = pbody
    detail["blocking"] = _scan(pbody, BLOCKING)
    detail["weaker"] = _scan(pbody, WEAKER)
    abstract = vfile and ("/Numbers/" in vfile or "/Structures/" in vfile)
    if abstract:
        return "functor", detail
    if (not detail["blocking"] and not detail["weaker"] and
            re.fullmatch(r"(reflexivity|trivial|easy)\.?", pbody)):
        return "near-match", detail
    return "untranslatable", detail


CATEGORY_ORDER = ["near-match", "untranslatable", "functor", "constructor",
                  "original"]

CATEGORY_BLURB = {
    "near-match": "library proof is `reflexivity`/`trivial`; corpus proof matches",
    "untranslatable": "a concrete script exists but uses tactics MEngine lacks",
    "functor": "library proof is functor-generated over an abstract setoid; no "
               "concrete script exists",
    "constructor": "maps to an inductive constructor; the library has no proof "
                   "script",
    "original": "no named stdlib counterpart",
}


def _note(category, ref, detail, corpus_proof, original_reason):
    if category == "original":
        return (f"No named stdlib lemma ({original_reason}); there is no library "
                f"proof to be faithful to.")
    if category == "constructor":
        return (f"`{ref}` is an inductive **constructor** — the library has no "
                f"proof script for it.  The corpus introduces the hypotheses and "
                f"builds the same proof term "
                f"(`{corpus_proof or 'constructor/split/left/right/exists'}`).")
    if category == "functor":
        bl = ", ".join(detail["blocking"] + detail.get("weaker", [])) \
            or "abstract-functor Ltac"
        setoid = (" over setoid equality `==` (not Leibniz `=`)"
                  if "==" in detail["block"] else "")
        return (f"`{ref}` is produced by module-functor `Include`; its only proof "
                f"script lives in the abstract functor (`{detail['loc']}`){setoid}, "
                f"written with {bl}.  No concrete-`nat` script exists to copy, and "
                f"MEngine has no module system — the corpus reproves it in "
                f"MEngine's tactic subset (`{corpus_proof}`).")
    if category == "untranslatable":
        absent, weaker = detail["blocking"], detail.get("weaker", [])
        if absent:
            why = (f"uses {', '.join(absent)}, which MEngine has no equivalent of")
        elif weaker:
            why = (f"leans on {', '.join(weaker)} to close goals MEngine's "
                   f"emulation cannot")
        else:
            why = ("relies on automation/structure MEngine's translator does not "
                   "reproduce mechanically")
        return (f"A concrete script exists but {why}.  (MEngine *does* have `;`, "
                f"`try`, `repeat`, `first`, `match Goal` and emulated "
                f"`auto`/`trivial`/`simpl` — the gap is the leaf tactics above, "
                f"not sequencing.)  The corpus proves the same statement with "
                f"MEngine's primitive tactics (`{corpus_proof}`).")
    if category == "near-match":
        return (f"The library proof is `{detail['proof']}`; the corpus proof is "
                f"the same modulo MEngine's surface syntax (`{corpus_proof}`).")
    return ""


# ─────────────────────────────── rendering ───────────────────────────────────

def _load_map(corpus_dir):
    with open(os.path.join(corpus_dir, "stdlib_map.json")) as f:
        return json.load(f)


def build(cfg, modules=None):
    """Regenerate corpus/PROOF_FIDELITY.md.  Returns (exit_code, path)."""
    corpus_dir = cfg["corpus_dir"]
    coq = cfg["coq_path"]
    spec = _load_map(corpus_dir)
    preamble = spec["preamble"]
    mod_map = spec["modules"]
    roots = _coqlib_roots(coq)

    all_mods = sorted(n for n in os.listdir(corpus_dir)
                      if os.path.isdir(os.path.join(corpus_dir, n))
                      and os.path.exists(os.path.join(corpus_dir, n, "rocq.v")))
    sel = modules or all_mods

    tally = {c: 0 for c in CATEGORY_ORDER}
    errors = []
    sections = []
    workdir = tempfile.mkdtemp(prefix="prooffid_")
    try:
        for mod in sel:
            vpath = os.path.join(corpus_dir, mod, "rocq.v")
            corpus = _corpus_proofs(vpath)
            entries = mod_map.get(mod, {})
            lines = [f"## {mod}\n"]
            for name, corpus_proof in corpus.items():
                entry = entries.get(name)
                if entry is None:
                    errors.append(f"{mod}.{name}: not in stdlib_map.json")
                    lines.append(f"### `{name}` — **UNMAPPED** "
                                 f"(add to stdlib_map.json)\n")
                    continue
                if "original" in entry:
                    cat, detail, ref = "original", {"blocking": []}, None
                    note = _note(cat, None, detail, corpus_proof, entry["original"])
                else:
                    ref = entry["stdlib"]
                    try:
                        cat, detail = classify(coq, ref, preamble, roots, workdir)
                    except Exception as e:  # noqa: BLE001 — surface, never guess
                        errors.append(f"{mod}.{name} ({ref}): {e}")
                        lines.append(f"### `{name}` → `{ref}` — **ERROR**: {e}\n")
                        continue
                    note = _note(cat, ref, detail, corpus_proof, None)
                tally[cat] += 1
                head = f"### `{name}`"
                if ref:
                    head += f" → `{ref}`"
                head += f"  *(proof: {cat})*\n"
                lines.append(head)
                if cat != "original":
                    lines.append(f"- **stdlib** — `{detail['loc']}` "
                                 f"(`{detail['dirpath']}`):")
                    lines.append("  ```coq")
                    for bl in detail["block"].splitlines():
                        lines.append("  " + bl)
                    lines.append("  ```")
                lines.append(f"- **corpus** (`{mod}/rocq.v`): "
                             f"`{corpus_proof or '(none)'}`")
                lines.append(f"- **why it diverges:** {note}\n")
            sections.append("\n".join(lines))
    finally:
        try:
            os.rmdir(workdir)
        except OSError:
            pass

    total = sum(tally.values())
    summary = [
        "# Proof-script fidelity: corpus vs the Rocq standard library",
        "",
        "> **Generated** by `proof_fidelity.py` (`stdlib_bench.py proof-fidelity`)."
        "  Do not edit by hand — it is re-extracted from the **installed** stdlib"
        " on each run.",
        "",
        "`fidelity.py` already proves every corpus *statement* is convertible to"
        " its stdlib counterpart (`fidelity` subcommand).  This file documents the"
        " one fidelity gap that remains: the corpus **proof scripts** are not the"
        " stdlib's verbatim proofs.  For all but the `near-match` lemmas this is"
        " *structural*, not a curation choice — see each lemma's \"why it"
        " diverges\" and the category summary below.",
        "",
        "| category | meaning | count |",
        "|----------|---------|------:|",
    ]
    for c in CATEGORY_ORDER:
        summary.append(f"| `{c}` | {CATEGORY_BLURB[c]} | {tally[c]} |")
    summary.append(f"| **total** | | **{total}** |")
    summary.append("")
    summary.append(
        "Only `near-match` proofs could ever be verbatim.  `constructor` lemmas"
        " have no library script at all (they are inductive constructors);"
        " `functor` lemmas (the bulk of the arithmetic) are generated by module"
        " `Include` over an abstract setoid and have no concrete `nat` script;"
        " `untranslatable` lemmas have a script that uses leaf tactics MEngine"
        " lacks — the `destr_bool` Ltac macro, `f_equal`, `destruct` on a premise,"
        " `discriminate`, `apply … in H` — or automation (`auto`) stronger than"
        " MEngine's emulation; `original` lemmas have no named stdlib counterpart."
        "  **This is *not* about tactic sequencing:** MEngine has `;`, `||`,"
        " `try`, `repeat`, `first […]`, and `match Goal`, and the corpus proofs"
        " use `;` themselves (`split; assumption`).  This is why the corpus is"
        " re-proved in MEngine's tactic subset rather than generated from stdlib"
        " proof scripts (statements *are* generated/checked — see `fidelity.py`).")
    summary.append("")

    md = "\n".join(summary) + "\n" + "\n".join(sections) + "\n"
    out_path = os.path.join(corpus_dir, "PROOF_FIDELITY.md")
    with open(out_path, "w") as f:
        f.write(md)

    print(f"Wrote {os.path.relpath(out_path)}")
    print("  " + ", ".join(f"{tally[c]} {c}" for c in CATEGORY_ORDER) +
          f"  ({total} lemmas)")
    if errors:
        print("\nPROOF-FIDELITY ERRORS (flag, never guess):")
        for e in errors:
            print(f"  - {e}")
        return 1, out_path
    return 0, out_path


def run(cfg, modules=None):
    code, _ = build(cfg, modules)
    return code
