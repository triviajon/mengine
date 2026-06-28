#!/usr/bin/env python3
"""Mechanical Rocq (.v) -> MEngine (.me) translator for the stdlib benchmark.

This is a *lightweight* sentence-aware rewriter with a small Pratt parser for
the term sublanguage, NOT a full Coq elaborator.  Its guiding principle is
**flag, never guess**: any construct it cannot translate soundly is reported as
an `unhandled` item and (in unit mode) raises `Untranslatable`, so the unit is
excluded from Tier A rather than mistranslated.  A wrong translation that happened
to compile would silently corrupt the benchmark, which is the worst outcome.

Scope (Tier A): the computational/structural corners reachable by today's MEngine
+ compat prelude — Bool, ground Nat, the `le` order, and propositional logic.
Polymorphic list reasoning needs element-type inference and is deferred (Tier B).

Usage:
    translate.py unit.v              # emit MEngine source on stdout
    translate.py --report unit.v     # print handled/unhandled construct summary
    translate.py --report --dir D    # report over every .v under D (triage)
"""

import argparse
import os
import re
import sys


# ───────────────────────── symbol table (type synthesis) ─────────────────────
#
# Result type (as a MEngine type expression string) of each known head symbol,
# used to recover the implicit type argument of `=`/eq.  Heads not listed here
# make eq-type inference fail -> the unit is flagged (never guessed).

NAT_FUNCS = {"O", "S", "add", "mul", "sub"}
BOOL_FUNCS = {"true", "false", "negb", "andb", "orb", "implb", "xorb",
              "eqb", "leb", "ltb"}
# Heads whose application yields a Prop (used to spot non-eq relational goals).
PROP_HEADS = {"le", "lt", "and", "or", "ex", "not", "iff", "True", "False"}

# Infix/notation operators mapped to prefix heads.  Each entry:
#   symbol -> (mengine_head_or_None, binding_power, right_assoc, needs_type_arg)
# needs_type_arg: 'eq' for equality (type inferred from LHS), None otherwise.
BINOPS = {
    "->":  ("__arrow__", 5,  True,  None),
    "/\\": ("and",       20, True,  None),
    "\\/": ("or",        15, True,  None),
    "=":   ("eq",        30, False, "eq"),
    "<=":  ("le",        30, False, None),
    "+":   ("add",       50, False, None),
    "-":   ("sub",       50, False, None),
    "*":   ("mul",       60, False, None),
}


class Untranslatable(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


# ───────────────────────────── comment / sentence split ──────────────────────

def strip_comments(text):
    """Remove (* ... *) comments (nested), keeping newlines for line numbers."""
    out = []
    depth = 0
    i = 0
    n = len(text)
    while i < n:
        if text[i:i + 2] == "(*":
            depth += 1
            i += 2
        elif text[i:i + 2] == "*)" and depth > 0:
            depth -= 1
            i += 2
        elif depth > 0:
            out.append("\n" if text[i] == "\n" else " ")
            i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def split_sentences(text):
    """Split on '.' followed by whitespace/EOF, ignoring '..'/'...' and decimals."""
    sentences = []
    buf = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        buf.append(c)
        if c == ".":
            nxt = text[i + 1] if i + 1 < n else " "
            prev = text[i - 1] if i > 0 else " "
            is_ellipsis = nxt == "." or prev == "."
            is_decimal = prev.isdigit() and nxt.isdigit()
            if not is_ellipsis and not is_decimal and (nxt.isspace() or i + 1 == n):
                sentences.append("".join(buf).strip().rstrip("."))
                buf = []
        i += 1
    tail = "".join(buf).strip().rstrip(".")
    if tail:
        sentences.append(tail)
    return [s for s in sentences if s]


# ───────────────────────────────── term lexer ────────────────────────────────

TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<arrow>->)
  | (?P<andop>/\\)
  | (?P<orop>\\/)
  | (?P<dle><=)
  | (?P<fatarrow>=>)
  | (?P<sym>[-=+*(),:])
  | (?P<num>\d+)
  | (?P<ident>[A-Za-z_][A-Za-z0-9_']*)
""", re.VERBOSE)

KEYWORDS = {"forall", "fun", "match", "with", "end", "let", "in", "Prop",
            "Type", "Set"}


class Tok:
    def __init__(self, kind, val):
        self.kind = kind
        self.val = val

    def __repr__(self):
        return f"Tok({self.kind},{self.val!r})"


def lex_term(s):
    toks = []
    i = 0
    n = len(s)
    while i < n:
        m = TOKEN_RE.match(s, i)
        if not m:
            raise Untranslatable(f"unexpected character {s[i]!r} in term")
        i = m.end()
        kind = m.lastgroup
        if kind == "ws":
            continue
        val = m.group()
        if kind == "ident" and val in KEYWORDS:
            toks.append(Tok(val, val))
        elif kind == "ident":
            toks.append(Tok("ident", val))
        elif kind == "num":
            toks.append(Tok("num", val))
        elif kind == "arrow":
            toks.append(Tok("op", "->"))
        elif kind == "andop":
            toks.append(Tok("op", "/\\"))
        elif kind == "orop":
            toks.append(Tok("op", "\\/"))
        elif kind == "dle":
            toks.append(Tok("op", "<="))
        elif kind == "fatarrow":
            toks.append(Tok("=>", "=>"))
        elif kind == "sym":
            if val in ("=", "+", "*", "-"):
                toks.append(Tok("op", val))
            else:
                toks.append(Tok(val, val))
    return toks


# ───────────────────────────────── AST ───────────────────────────────────────
# Node forms (tuples):
#   ("var", name) | ("app", f, a) | ("forall", var, ty, body)
#   ("fun", var, ty, body) | ("op", sym, lhs, rhs) | ("arrow", a, b)
#   ("num", int)


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    def peek(self):
        return self.toks[self.pos] if self.pos < len(self.toks) else Tok("eof", None)

    def next(self):
        t = self.peek()
        self.pos += 1
        return t

    def expect(self, kind):
        t = self.next()
        if t.kind != kind:
            raise Untranslatable(f"expected {kind}, got {t.kind} {t.val!r}")
        return t

    # binders: parse `(x y : T)` groups or bare `x y` (then ',' or ':') for
    # forall/fun.  Returns list of (name, type_or_None).
    def parse_binders(self):
        binders = []
        while True:
            t = self.peek()
            if t.kind == "(":
                self.next()
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                if not names:
                    raise Untranslatable("empty binder group")
                ty = None
                if self.peek().kind == ":":
                    self.next()
                    ty = self.parse_expr(0)
                self.expect(")")
                for nm in names:
                    binders.append((nm, ty))
            elif t.kind == "ident":
                # bare names until ':' or ',' (untyped binders need a later ':')
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                ty = None
                if self.peek().kind == ":":
                    self.next()
                    ty = self.parse_expr(0)
                for nm in names:
                    binders.append((nm, ty))
                break
            else:
                break
        return binders

    def parse_atom(self):
        t = self.peek()
        if t.kind == "(":
            self.next()
            e = self.parse_expr(0)
            self.expect(")")
            return e
        if t.kind == "forall":
            self.next()
            binders = self.parse_binders()
            self.expect(",")
            body = self.parse_expr(0)
            node = body
            for nm, ty in reversed(binders):
                if ty is None:
                    raise Untranslatable("forall binder without a type annotation")
                node = ("forall", nm, ty, node)
            return node
        if t.kind == "fun":
            self.next()
            binders = self.parse_binders()
            self.expect("=>")
            body = self.parse_expr(0)
            node = body
            for nm, ty in reversed(binders):
                if ty is None:
                    raise Untranslatable("fun binder without a type annotation")
                node = ("fun", nm, ty, node)
            return node
        if t.kind in ("match", "let"):
            raise Untranslatable(f"'{t.kind}' in statement term (Tier B)")
        if t.kind == "ident":
            self.next()
            return ("var", t.val)
        if t.kind in ("Prop", "Type", "Set"):
            self.next()
            return ("var", "Prop" if t.kind == "Prop" else "Type")
        if t.kind == "num":
            self.next()
            return ("num", int(t.val))
        raise Untranslatable(f"unexpected token {t.kind} {t.val!r} in term")

    def parse_app(self):
        node = self.parse_atom()
        while self.peek().kind in ("ident", "(", "num", "Prop", "Type", "Set"):
            arg = self.parse_atom()
            node = ("app", node, arg)
        return node

    def parse_expr(self, min_bp):
        left = self.parse_app()
        while True:
            t = self.peek()
            if t.kind != "op":
                break
            sym = t.val
            head, bp, right_assoc, _ = BINOPS[sym]
            if bp < min_bp:
                break
            self.next()
            next_min = bp if right_assoc else bp + 1
            right = self.parse_expr(next_min)
            if sym == "->":
                left = ("arrow", left, right)
            else:
                left = ("op", sym, left, right)
        return left


def parse_term(s):
    toks = lex_term(s)
    p = Parser(toks)
    e = p.parse_expr(0)
    if p.peek().kind != "eof":
        raise Untranslatable(f"trailing tokens after term: {p.peek().val!r}")
    return e


# ───────────────────────────── type synthesis ────────────────────────────────
# Minimal bottom-up synthesizer over {nat, bool, Prop, Type} used only to
# recover the implicit type argument of eq.  Returns a type string or raises.

def head_symbol(node):
    while node[0] == "app":
        node = node[1]
    return node


def synth_type(node, env):
    """Return 'nat' | 'bool' | 'Prop' for node, or raise Untranslatable."""
    if node[0] == "num":
        return "nat"
    if node[0] == "var":
        nm = node[1]
        if nm in env:
            return env[nm]
        if nm in NAT_FUNCS:
            return "nat"
        if nm in BOOL_FUNCS:
            return "bool"
        if nm in PROP_HEADS:
            return "Prop"
        raise Untranslatable(f"cannot synthesize type of '{nm}' for eq")
    if node[0] in ("op", "arrow"):
        if node[0] == "arrow" or node[1] in ("/\\", "\\/", "="):
            return "Prop"
        if node[1] == "<=":
            return "Prop"
        if node[1] in ("+", "-", "*"):
            return "nat"
    if node[0] == "app":
        h = head_symbol(node)
        if h[0] == "var":
            nm = h[1]
            if nm in env:
                return env[nm]
            if nm in NAT_FUNCS:
                return "nat"
            if nm in BOOL_FUNCS:
                return "bool"
            if nm in PROP_HEADS:
                return "Prop"
        raise Untranslatable("cannot synthesize type of application for eq")
    raise Untranslatable("cannot synthesize type for eq argument")


# ───────────────────────────── pretty printer ────────────────────────────────

def emit(node, env):
    """Render an AST node as fully-parenthesized MEngine prefix syntax."""
    kind = node[0]
    if kind == "var":
        return node[1]
    if kind == "num":
        out = "O"
        for _ in range(node[1]):
            out = f"(S {out})"
        return out
    if kind == "app":
        f = emit(node[1], env)
        a = emit(node[2], env)
        return f"({f} {a})"
    if kind == "arrow":
        a = emit(node[1], env)
        b = emit(node[2], env)
        return f"forall (_ : {a}), {b}"
    if kind == "forall":
        nm, ty, body = node[1], node[2], node[3]
        ty_s = emit(ty, env)
        env2 = dict(env)
        env2[nm] = type_to_synth(ty)
        return f"forall ({nm} : {ty_s}), {emit(body, env2)}"
    if kind == "fun":
        nm, ty, body = node[1], node[2], node[3]
        ty_s = emit(ty, env)
        env2 = dict(env)
        env2[nm] = type_to_synth(ty)
        return f"fun ({nm} : {ty_s}) => {emit(body, env2)}"
    if kind == "op":
        sym, lhs, rhs = node[1], node[2], node[3]
        head = BINOPS[sym][0]
        if sym == "=":
            ty = synth_type(lhs, env)
            return f"(((eq {ty}) {emit(lhs, env)}) {emit(rhs, env)})"
        return f"(({head} {emit(lhs, env)}) {emit(rhs, env)})"
    raise Untranslatable(f"cannot emit node {kind}")


def type_to_synth(ty_node):
    """Map a (already-parsed) type expression to a synth tag for env tracking."""
    if ty_node[0] == "var":
        if ty_node[1] in ("nat", "bool"):
            return ty_node[1]
        if ty_node[1] in ("Prop", "Type", "Set"):
            return "Prop"
    return "?"  # unknown; eq-synth will fail loudly if it matters


def translate_term(src):
    return emit(parse_term(src.strip()), {})


# ──────────────────────────── command translation ────────────────────────────

DROP_PREFIXES = ("Require", "Import", "Export", "Open", "Close", "Set", "Unset",
                 "Hint", "Arguments", "Local", "Global", "#[", "Scope",
                 "Declare", "Generalizable", "Print", "Check", "Search",
                 "Comments", "Section", "End", "Module", "Include")

PROOF_FRAMING = ("Proof", "Qed", "Defined", "Admitted", "Abort")


def translate_definition(sentence, report):
    """Definition/Lemma/Theorem/Example name [binders] : type [:= body]."""
    m = re.match(r"^(Definition|Lemma|Theorem|Example|Corollary|Fact|Remark|Proposition)\s+"
                 r"([A-Za-z_][A-Za-z0-9_']*)\s*(.*)$", sentence, re.S)
    if not m:
        raise Untranslatable("unrecognized definition form")
    kw, name, rest = m.group(1), m.group(2), m.group(3)
    is_thm = kw in ("Lemma", "Theorem", "Example", "Corollary", "Fact",
                    "Remark", "Proposition")

    # Separate optional binders + ': type' + optional ':= body'.
    body = None
    if ":=" in rest:
        head, body = rest.split(":=", 1)
    else:
        head = rest
    if ":" not in head:
        raise Untranslatable("definition without ': type'")
    binders_src, type_src = head.split(":", 1)

    binders = _parse_binder_src(binders_src)
    type_node = parse_term(type_src.strip())
    # Wrap explicit binders as leading foralls (for type) / funs (for body).
    env = {}
    type_full = type_node
    for nm, ty in reversed(binders):
        type_full = ("forall", nm, ty, type_full)
    type_out = emit(type_full, env)

    if is_thm:
        report.add_handled(kw)
        return f"Theorem {name} : {type_out}."
    # Definition with a body.
    if body is None:
        report.add_handled("Definition(no body)")
        return f"Axiom {name} : {type_out}."
    body_node = parse_term(body.strip())
    body_full = body_node
    for nm, ty in reversed(binders):
        body_full = ("fun", nm, ty, body_full)
    report.add_handled("Definition")
    return f"Definition {name} : {type_out} := {emit(body_full, env)}."


def _parse_binder_src(src):
    src = src.strip()
    if not src:
        return []
    p = Parser(lex_term(src))
    binders = p.parse_binders()
    if p.peek().kind != "eof":
        raise Untranslatable("could not parse definition binders")
    for nm, ty in binders:
        if ty is None:
            raise Untranslatable(f"binder '{nm}' lacks a type annotation")
    return binders


def translate_axiom(sentence, report):
    m = re.match(r"^(Axiom|Parameter|Conjecture|Variable|Hypothesis)\s+"
                 r"([A-Za-z_][A-Za-z0-9_']*)\s*:\s*(.*)$", sentence, re.S)
    if not m:
        raise Untranslatable("unrecognized axiom form")
    name, type_src = m.group(2), m.group(3)
    report.add_handled("Axiom")
    return f"Axiom {name} : {translate_term(type_src)}."


def is_dropped(sentence):
    for p in DROP_PREFIXES:
        if sentence.startswith(p):
            return True
    return False


def is_framing(sentence):
    word = re.match(r"^([A-Za-z]+)", sentence)
    return bool(word and word.group(1) in PROOF_FRAMING)


# ──────────────────────────── tactic translation ─────────────────────────────
#
# Token-level mapping of a single Rocq tactic atom (no ';') to MEngine.  Unknown
# tactics hard-stop (Untranslatable) so the unit leaves Tier A.

# Inductive data for the induction/destruct scaffold (compat-prelude types).
# Each case: (constructor, [recursive-arg flags]); a True flag means the arg is
# recursive and carries an induction hypothesis in <T>_ind.
INDUCTIVES = {
    "bool": {"ind": "bool_ind", "cases": [("true", []), ("false", [])]},
    "nat":  {"ind": "nat_ind",  "cases": [("O", []), ("S", [True])]},
}

DIRECT_TACTICS = {
    "reflexivity", "assumption", "split", "left", "right", "auto",
    "constructor", "simpl", "symmetry", "trivial", "easy", "idtac",
}

UNSUPPORTED = {
    "lia", "ring", "omega", "nia", "field", "auto with", "eauto", "inversion",
    "congruence", "f_equal", "unfold", "fold", "replace", "generalize",
    "revert", "case", "pose", "set", "specialize", "discriminate", "injection",
    "contradiction", "exfalso", "cbn", "red", "change", "rename", "clear",
    "remember", "induction'", "dependent", "functional", "decide", "destruct'",
}


def translate_tactic_atom(atom, report):
    atom = atom.strip()
    if not atom:
        return None
    head = re.match(r"^([A-Za-z_][A-Za-z0-9_']*)", atom)
    name = head.group(1) if head else None

    if name in UNSUPPORTED:
        raise Untranslatable(f"unsupported tactic '{name}'")

    if name in DIRECT_TACTICS:
        rest = atom[len(name):].strip()
        if rest:
            raise Untranslatable(f"tactic '{name}' with unexpected arguments")
        report.add_handled(f"tac:{name}")
        return name

    if name in ("intro", "intros"):
        args = atom[len(name):].strip()
        report.add_handled("tac:intro")
        if not args:
            return "intros" if name == "intros" else "intro"
        names = args.split()
        return " ".join(f"intro {nm}." for nm in names).rstrip(".")

    if name in ("apply", "eapply"):
        arg = atom[len(name):].strip()
        if " in " in f" {arg} " or arg.endswith(" in"):
            raise Untranslatable("apply ... in H (Tier B)")
        report.add_handled(f"tac:{name}")
        return f"{name} ({translate_term(arg)})"

    if name == "exact":
        arg = atom[len("exact"):].strip()
        report.add_handled("tac:exact")
        return f"exact ({translate_term(arg)})"

    if name in ("exists",):
        arg = atom[len("exists"):].strip()
        report.add_handled("tac:exists")
        return f"exists ({translate_term(arg)})"

    if name == "rewrite":
        rest = atom[len("rewrite"):].strip()
        if rest.startswith("<-"):
            raise Untranslatable("rewrite <- (Tier B: rewrite_s is forward-only)")
        if " in " in f" {rest} ":
            raise Untranslatable("rewrite ... in H (Tier B)")
        report.add_handled("tac:rewrite")
        return f"rewrite_s ({translate_term(rest)})"

    if name in ("now",):
        # now tac  ≈  tac; easy.  Bare 'now' is unusual; treat 'now' alone as easy.
        rest = atom[len("now"):].strip()
        report.add_handled("tac:now")
        if not rest:
            return "easy"
        inner = translate_tactic_atom(rest, report)
        return f"{inner}; easy"

    raise Untranslatable(f"unknown tactic '{name or atom}'")


def translate_tactic_sentence(sentence, report):
    """Translate one Rocq tactic sentence (may contain ';') -> MEngine, '.'-ended."""
    parts = _split_semicolons(sentence)
    out = []
    for part in parts:
        t = translate_tactic_atom(part, report)
        if t is not None:
            out.append(t)
    if not out:
        return ""
    return "; ".join(out) + "."


def _split_semicolons(s):
    """Split on top-level ';' (not inside brackets/parens)."""
    parts = []
    depth = 0
    buf = []
    for c in s:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == ";" and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(c)
    parts.append("".join(buf))
    return [p.strip() for p in parts if p.strip()]


# ──────────────────────────── proof translation ──────────────────────────────

def parse_statement_leading_binder(type_out):
    """From an emitted 'forall (x : T), Body' string, return (x, T, Body) or None."""
    m = re.match(r"^forall \(([A-Za-z_][A-Za-z0-9_']*) : ([^,]+)\), (.*)$",
                 type_out, re.S)
    if not m:
        return None
    return m.group(1), m.group(2).strip(), m.group(3).strip()


def translate_proof(proof_sentences, stmt_type_out, report):
    """Translate a list of Rocq proof tactic-sentences into MEngine proof lines.

    Handles two shapes:
      (A) straight-line tactic sequence (no induction/destruct);
      (B) a leading `induction x` / `destruct x` on the goal's leading binder,
          with bullet- or position-segmented cases (nat/bool only).
    Anything else is flagged (Untranslatable)."""
    # Drop proof framing sentences.
    body = [s for s in proof_sentences if not is_framing(s)]
    if not body:
        raise Untranslatable("empty proof")

    first = body[0].strip()
    m = re.match(r"^(induction|destruct)\s+([A-Za-z_][A-Za-z0-9_']*)\s*$", first)
    if m:
        return _translate_induction(m.group(1), m.group(2), body[1:],
                                    stmt_type_out, report)

    # Shape (A): straight-line.  Forbid any later induction/destruct.
    for s in body:
        if re.match(r"^(induction|destruct)\b", s.strip()):
            raise Untranslatable("induction/destruct not as first tactic (Tier B)")
    lines = []
    for s in body:
        t = translate_tactic_sentence(s, report)
        if t:
            lines.append(t)
    return lines


def _segment_cases(case_sentences, ncases):
    """Segment proof remainder into per-case sentence lists, by '-' bullets or
    (fallback) one sentence per case."""
    bulleted = any(s.lstrip().startswith(("-", "+", "*")) for s in case_sentences)
    if bulleted:
        cases = []
        cur = None
        for s in case_sentences:
            st = s.lstrip()
            if st.startswith(("-", "+", "*")):
                if cur is not None:
                    cases.append(cur)
                cur = [st.lstrip("-+* ").strip()]
            else:
                if cur is None:
                    raise Untranslatable("proof text before first bullet")
                cur.append(s)
        if cur is not None:
            cases.append(cur)
        return cases
    # Fallback: require exactly one sentence per case.
    if len(case_sentences) != ncases:
        raise Untranslatable(
            f"cannot segment {len(case_sentences)} sentences into {ncases} cases "
            "(use '-' bullets)")
    return [[s] for s in case_sentences]


def _translate_induction(kind, var, remainder, stmt_type_out, report):
    lb = parse_statement_leading_binder(stmt_type_out)
    if lb is None:
        raise Untranslatable("induction/destruct goal has no leading forall binder")
    x, ty, body = lb
    if x != var:
        raise Untranslatable(
            f"{kind} on '{var}' but leading binder is '{x}' (Tier B)")
    if ty not in INDUCTIVES:
        raise Untranslatable(f"{kind} on type '{ty}' not supported (nat/bool only)")
    info = INDUCTIVES[ty]
    motive = f"fun ({x} : {ty}) => {body}"

    lines = [f"intro {x}.", f"apply ({info['ind']} ({motive}))."]
    cases = _segment_cases(remainder, len(info["cases"]))
    if len(cases) != len(info["cases"]):
        raise Untranslatable(
            f"{kind}: {len(cases)} cases provided, expected {len(info['cases'])}")

    report.add_handled(f"tac:{kind}")
    for (ctor, rec_flags), case_body in zip(info["cases"], cases):
        case_lines = ["simpl."]
        if kind == "induction":
            # Introduce recursive args and their IHs (named like Rocq: IH<var>).
            for _ in rec_flags:
                case_lines.append(f"intro {x}.")
                case_lines.append(f"intro IH{x}.")
        for s in case_body:
            t = translate_tactic_sentence(s, report)
            if t:
                case_lines.append(t)
        lines.extend(case_lines)
    return lines


# ─────────────────────────────── report ──────────────────────────────────────

class Report:
    def __init__(self):
        self.handled = {}
        self.unhandled = []

    def add_handled(self, key):
        self.handled[key] = self.handled.get(key, 0) + 1

    def add_unhandled(self, reason):
        self.unhandled.append(reason)

    def ok(self):
        return not self.unhandled


# ─────────────────────────── unit-level translation ──────────────────────────

def translate_unit(text, report):
    """Translate a whole .v unit to a MEngine source string, or raise."""
    text = strip_comments(text)
    sentences = split_sentences(text)

    out_lines = []
    i = 0
    n = len(sentences)
    while i < n:
        s = sentences[i].strip()
        if not s:
            i += 1
            continue
        word = re.match(r"^([A-Za-z_#\[]+)", s)
        kw = word.group(1) if word else ""

        if is_dropped(s):
            i += 1
            continue
        if kw in ("Inductive", "Fixpoint", "CoFixpoint"):
            raise Untranslatable(f"'{kw}' in unit (define it in the compat prelude)")
        if kw in ("Axiom", "Parameter", "Conjecture", "Variable", "Hypothesis"):
            out_lines.append(translate_axiom(s, report))
            i += 1
            continue
        if kw in ("Definition",):
            out_lines.append(translate_definition(s, report))
            i += 1
            continue
        if kw in ("Lemma", "Theorem", "Example", "Corollary", "Fact", "Remark",
                  "Proposition"):
            stmt = translate_definition(s, report)
            out_lines.append(stmt)
            # Collect the proof: subsequent sentences until Qed/Defined/Admitted/Abort.
            type_out = stmt[len(f"Theorem "):].split(" : ", 1)[1].rstrip(".")
            proof = []
            i += 1
            while i < n:
                ps = sentences[i].strip()
                proof.append(ps)
                w = re.match(r"^([A-Za-z]+)", ps)
                if w and w.group(1) in ("Qed", "Defined", "Admitted", "Abort"):
                    break
                i += 1
            if any(re.match(r"^Admitted", p) or re.match(r"^Abort", p) for p in proof):
                raise Untranslatable("proof is Admitted/Abort")
            proof_lines = translate_proof(proof, type_out, report)
            out_lines.extend(proof_lines)
            i += 1
            continue
        # Unknown command.
        raise Untranslatable(f"unrecognized command '{kw}'")

    return "\n".join(out_lines) + "\n"


# ─────────────────────────────── statement digest ────────────────────────────

def statement_digests(mengine_src):
    """Normalized digest of each Theorem statement, for correspondence checking."""
    digs = []
    for line in mengine_src.splitlines():
        m = re.match(r"^Theorem\s+(\S+)\s*:\s*(.*)\.$", line)
        if m:
            norm = re.sub(r"\s+", " ", m.group(2)).strip()
            digs.append((m.group(1), norm))
    return digs


# ─────────────────────────────────── main ────────────────────────────────────

def report_over_file(path):
    with open(path) as f:
        text = f.read()
    report = Report()
    try:
        translate_unit(text, report)
        status = "OK"
    except Untranslatable as e:
        status = f"FLAG: {e.reason}"
    return status, report


def main():
    ap = argparse.ArgumentParser(description="Rocq .v -> MEngine .me translator")
    ap.add_argument("input", nargs="?", help="input .v file")
    ap.add_argument("--report", action="store_true", help="report handled/unhandled")
    ap.add_argument("--dir", help="report over every .v under this directory")
    args = ap.parse_args()

    if args.dir:
        rows = []
        for root, _dirs, files in os.walk(args.dir):
            for fn in sorted(files):
                if fn.endswith(".v"):
                    p = os.path.join(root, fn)
                    status, _rep = report_over_file(p)
                    rows.append((os.path.relpath(p, args.dir), status))
        ok = sum(1 for _, s in rows if s == "OK")
        for rel, status in rows:
            print(f"  [{'OK ' if status == 'OK' else 'FLAG'}] {rel}: {status}")
        print(f"\n{ok}/{len(rows)} files fully translatable (Tier A).")
        return

    if not args.input:
        ap.error("input file required (or use --dir)")

    with open(args.input) as f:
        text = f.read()
    report = Report()
    try:
        src = translate_unit(text, report)
    except Untranslatable as e:
        if args.report:
            print(f"FLAG: {e.reason}")
            sys.exit(2)
        sys.stderr.write(f"Untranslatable: {e.reason}\n")
        sys.exit(2)

    if args.report:
        print(f"OK ({args.input})")
        for k in sorted(report.handled):
            print(f"  handled {k}: {report.handled[k]}")
    else:
        sys.stdout.write(src)


if __name__ == "__main__":
    main()
