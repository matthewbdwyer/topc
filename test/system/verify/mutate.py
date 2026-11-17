"""Mutation tests: a verifier that proves too much proves mutants too.

Each mutant is a demo with one small change to the *code* of a function
that has a contract: a comparison or an arithmetic operator replaced, a
constant changed by one, or a loop invariant dropped.  The contract is left
alone, so the mutant computes something else against the same claim, and
the verifier should notice: some verdict should change.

A mutant whose verdicts are all unchanged *survives*.  That is not always
wrong: the mutant may compute the same thing, or the contract may not pin
down what was changed.  Every survivor must be listed in
mutation/SURVIVORS with the reason, so each has been looked at; a
survivor that is not listed fails the run, and so does a listed mutant
that is now killed.

A mutant is named file:line:column:old->new.
"""
import re
from pathlib import Path

import agree

OPERATORS = [
    (r" < ", " <= "), (r" <= ", " < "), (r" > ", " >= "), (r" >= ", " > "),
    (r" == ", " != "), (r" != ", " == "),
    (r" \+ ", " - "), (r" - ", " + "), (r" \* ", " + "),
]
PROOF_LINE = re.compile(r"^\s*(assert|assume|use|invariant|decreases)\b")


def mutants(text):
    """Yield (name, mutated text) for every mutant of `text`."""
    for fn, m in agree.functions(text).items():
        if m.group("kind") or fn == "main" or not agree.clauses(m.group("specs"), "ensures"):
            continue
        start = m.start("body")
        offset = start
        for line in m.group("body").splitlines(True):
            lineno = text.count("\n", 0, offset) + 1
            code = line.split("//")[0]
            if re.match(r"^\s*invariant\b", line):
                yield f"{lineno}:1:drop invariant", text[:offset] + text[offset + len(line):]
            elif not PROOF_LINE.match(line):
                for pat, new in OPERATORS:
                    for o in re.finditer(pat, code):
                        a = offset + o.start()
                        yield (f"{lineno}:{o.start() + 1}:{o.group(0).strip()}->{new.strip()}",
                               text[:a] + new + text[a + len(o.group(0)):])
                for o in re.finditer(r"(?<![\w.])\d+\b", code):
                    a = offset + o.start()
                    n = str(int(o.group(0)) + 1)
                    yield (f"{lineno}:{o.start() + 1}:{o.group(0)}->{n}",
                           text[:a] + n + text[a + len(o.group(0)):])
            offset += len(line)


def listed(path):
    out = {}
    if path.exists():
        for line in path.read_text().splitlines():
            if line.strip() and not line.startswith("#"):
                name, _, why = line.partition("\t")
                out[name.strip()] = why.strip()
    return out
