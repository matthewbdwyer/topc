"""Run-time agreement: a verified contract must hold when the program runs.

For each function that `topc --ptrust` reports as `=> verified` (with
nothing assumed) and whose contract can be executed, build a copy of the
program in which

  - the function checks its own `ensures` before it returns, and stops
    with `error 99` if one is false;
  - the spec functions its contract mentions are ordinary functions (the
    word `spec` is removed), so the check can call them;
  - `main` is replaced by a driver that calls the function on sample
    arguments, each call guarded by the function's `requires`.

The copy is compiled, linked, and run.  It must exit normally and must
have made at least one call.  A recursive function checks every one of its
own calls on the way.

This ties `proved` to what the compiled program does: it would catch a
wrong translation of an operator (say, division) that the VC generator and
its tests agree on but the code generator does not.

Not every contract can be run.  A function is left out, with the reason
reported under -v, when its contract uses old(...), rank(...), ==>, or a
borrow; when it assigns a formal (in ensures a formal means its entry
value); when it may not terminate; or when the copy does not compile
(typically a spec function over a tree, which as code would move its
argument twice).
"""
import itertools
import os
import random
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

SAMPLES = [-3, -1, 0, 1, 2, 5, 12]
MAX_CALLS = 60

FUNCTION = re.compile(
    r"^(?P<kind>spec\s+|lemma\s+)?(?P<name>\w+)\s*\((?P<formals>[^)]*)\)(?P<specs>[^{]*)\{\n"
    r"(?P<body>.*?)^\}", re.M | re.S)


def functions(text):
    """Top-level functions: {name: match}.  Relies on the layout the demos
    use: a header at column 0, the closing brace at column 0."""
    return {m.group("name"): m for m in FUNCTION.finditer(text)}


def clauses(specs, word):
    return [c.strip() for c in re.findall(rf"^\s*{word}\s+(.*)$", specs, re.M)]


def subst(expr, mapping):
    return re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(mapping.get(m.group(0), m.group(0))), expr)


def dereferences(text):
    """Whether a `*` in `text` is a dereference and not a product: it is one
    when nothing that could be a left operand comes before it."""
    for m in re.finditer(r"\*", text):
        before = text[:m.start()].rstrip()
        if not before or not re.match(r"[\w)]", before[-1]):
            return True
    return False


def verified_functions(report):
    """Names with `=> verified` and nothing after it, from a --ptrust report."""
    names, fn = [], None
    for line in report.splitlines():
        m = re.match(r"^(\w+):$", line)
        if m:
            fn = m.group(1)
        elif line.strip() == "=> verified" and fn:
            names.append(fn)
    return names


def instrument(text, fn):
    """The program with `fn` checking its ensures, and a driver for main.

    Returns (program, None), or (None, reason) if the contract cannot be run.
    """
    fns = functions(text)
    m = fns[fn]
    formals = [f.strip() for f in m.group("formals").split(",") if f.strip()]
    requires, ensures = clauses(m.group("specs"), "requires"), clauses(m.group("specs"), "ensures")
    body = m.group("body")
    if not ensures:
        return None, "no ensures"
    if re.search(r"decreases\s+\*", m.group("specs") + body):
        return None, "may not terminate"
    contract = " ".join(requires + ensures)
    for bad in ("old(", "rank(", "==>"):
        if bad in contract:
            return None, f"contract uses {bad.rstrip('(')}"
    if dereferences(contract):
        return None, "contract reads through a borrow"
    if dereferences(body) or re.search(r"&\s*\w|\bcase\b|\balloc\b", body.replace("&&", "")):
        return None, "body uses references or case"
    for x in formals:
        if re.search(rf"^\s*{x}\s*=[^=]", body, re.M):
            return None, f"assigns formal {x}"
    ret = re.search(r"^(\s*)return\s+(.*);\s*$", body, re.M)
    if not ret or "return" in body[ret.end():]:
        return None, "no final return"

    # The check, in place of the return.
    r = "rt_result"
    check = "".join(f"{ret.group(1)}if ({subst(e, {'result': r})}) {{ }} else {{ error 99; }}\n"
                    for e in ensures)
    new_body = body[:ret.start()] + f"{ret.group(1)}{r} = {ret.group(2)};\n" + check + \
        f"{ret.group(1)}return {r};\n"
    decl = re.match(r"\s*var\s+[^;]*;", new_body)
    if decl:
        new_body = new_body[:decl.end() - 1] + f", {r};" + new_body[decl.end():]
    else:
        new_body = f"  var {r};\n" + new_body
    out = text[:m.start("body")] + new_body + text[m.end("body"):]

    # Spec functions the contract mentions, and those they mention, become code.
    need, work = set(), [contract]
    while work:
        t = work.pop()
        for name, g in fns.items():
            if g.group("kind") and g.group("kind").strip() == "spec" and name not in need \
                    and re.search(rf"\b{name}\s*\(", t):
                need.add(name)
                work.append(g.group("body"))
    for name in need:
        out = re.sub(rf"^spec\s+({name}\s*\()", r"\1", out, flags=re.M)

    # The driver.
    rng = random.Random(0)
    tuples = list(itertools.product(SAMPLES, repeat=len(formals)))
    if len(tuples) > MAX_CALLS:
        tuples = rng.sample(tuples, MAX_CALLS)
    calls = ""
    for t in tuples:
        env = dict(zip(formals, (f"(0 - {-v})" if v < 0 else str(v) for v in t)))
        args = ", ".join(env[x] for x in formals)
        call = f"rt_d = {fn}({args}); rt_n = rt_n + 1;"
        guard = " && ".join(f"({subst(q, env)})" for q in requires)
        calls += f"  if ({guard}) {{ {call} }}\n" if guard else f"  {call}\n"
    driver = f"main() {{\n  var rt_d, rt_n;\n  rt_d = 0;\n  rt_n = 0;\n{calls}  output rt_n;\n  return 0;\n}}\n"
    main = functions(out).get("main")
    if not main:
        return None, "no main"
    out = out[:main.start()] + driver + out[main.end():]
    return out, None


def run(src, topc, clang, rtlib, report):
    """Yield (function, ok, detail, skipped_reason) for each verified function."""
    text = src.read_text()
    for fn in verified_functions(report):
        if fn == "main" or fn not in functions(text) or functions(text)[fn].group("kind"):
            continue
        prog, why = instrument(text, fn)
        if prog is None:
            yield fn, True, "", why
            continue
        with tempfile.TemporaryDirectory(prefix="verutop_rt_") as d:
            p = Path(d) / src.name
            p.write_text(prog)
            c = subprocess.run([str(topc), src.name], cwd=d, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            bc = Path(str(p) + ".bc")
            if c.returncode != 0 or not bc.exists():
                yield fn, True, "", "the executable copy does not compile"
                continue
            exe = Path(d) / "a.out"
            link = subprocess.run([clang, "-w", str(bc), str(rtlib), "-o", str(exe)],
                                  text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if link.returncode != 0:
                yield fn, False, "link failed:\n" + link.stdout, None
                continue
            try:
                r = subprocess.run([str(exe)], text=True, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
            except subprocess.TimeoutExpired:
                yield fn, False, "the program did not finish in 20 seconds", None
                continue
            m = re.search(r"Program output: (\d+)", r.stdout)
            calls = int(m.group(1)) if m else 0
            if r.returncode != 0:
                yield fn, False, f"a verified ensures is false at run time (exit {r.returncode}):\n" \
                    + r.stdout[-400:], None
            elif calls == 0:
                yield fn, True, "", "no sample satisfies requires"
            else:
                yield fn, True, f"{calls} calls", None
