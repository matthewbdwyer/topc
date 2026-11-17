#!/usr/bin/env python3
"""VERUTOP verifier tests.  See README.md in this directory.

  python3 runner.py [-j N] [--update] [--only SUBSTR] [-v]

Environment: TOPC (default <root>/build/src/topc).  Needs z3 on the PATH;
without it the suite is skipped (exit 0, with a notice).
"""
import argparse
import concurrent.futures
import difflib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).parent.resolve()
ROOT = (HERE / "../../..").resolve()
TOPC = Path(os.environ.get("TOPC", str(ROOT / "build/src/topc")))
DEMOS = ROOT / "verutop-demos"
CASES = HERE / "cases"
LIARS = HERE / "liars"
EXPECTED = HERE / "expected"
GOLDEN = HERE / "golden"
TIMEOUT = 300

VERDICT = re.compile(r"^  (proved|FAILED|UNKNOWN)\s+line (\d+): (.*)$")
HEADER = re.compile(r"^(\S.*):$")
# What follows the obligation on a verdict line and is not compared: a
# counterexample's values and Z3's own message vary between Z3 versions and
# VC modes.  A mark in capitals (such as VACUOUS) is part of the verdict.
VARYING = re.compile(r"\s{3}(when .*|\(z3: .*)$")
MARK = re.compile(r"\s{3}([A-Z]{4,})\b.*$")


def directives(src):
    """`// flags: ...` and `// modes: passive` lines at the top of a test."""
    d = {"flags": [], "modes": ["passive", "wp"]}
    for line in src.read_text().splitlines():
        m = re.match(r"//\s*(flags|modes):\s*(.*)$", line)
        if m:
            d[m.group(1)] = m.group(2).split()
    return d


def topc(src, flags):
    """Run topc on a copy of `src` in a scratch directory; return its output."""
    with tempfile.TemporaryDirectory(prefix="verutop_") as d:
        copy = Path(d) / src.name
        shutil.copy(src, copy)
        try:
            p = subprocess.run([str(TOPC), *flags, src.name], cwd=d, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=TIMEOUT)
            return p.stdout
        except subprocess.TimeoutExpired:
            return "TIMEOUT\n"


def verdicts(out):
    """The report as rows: (function, line, verdict, obligation).

    Lines that are neither a heading nor a verdict (a rejected spec function,
    a skipped function) are rows too, with verdict `note`.  The size line,
    traces, and everything from the trust summary on are not part of it.
    """
    rows, fn = [], ""
    for line in out.splitlines():
        if line.startswith(("VC (", "profiling:")) or not line.strip():
            continue
        if line.startswith(("Assumed without proof", "Checked", "Trust report")):
            break
        m = VERDICT.match(line)
        if m:
            what = VARYING.sub("", m.group(3))
            verdict = m.group(1)
            mark = MARK.search(what)
            if mark:
                verdict += " " + mark.group(1)
                what = what[:mark.start()]
            rows.append((fn, m.group(2), verdict, what.rstrip()))
            continue
        m = HEADER.match(line)
        if m:
            fn = m.group(1)
            continue
        if line.startswith("    "):  # a trace line
            continue
        rows.append(("", "", "note", line.strip()))
    return rows


def table(rows):
    return "".join("\t".join(r) + "\n" for r in rows)


class Result:
    def __init__(self, name, ok, detail="", xfail=False):
        self.name, self.ok, self.detail, self.xfail = name, ok, detail, xfail


def diff(want, got, a, b):
    return "".join(difflib.unified_diff(want.splitlines(True), got.splitlines(True), a, b))


# ---- verdict tests: the demos and cases/ -----------------------------------

def verdict_test(src, update):
    d = directives(src)
    exp = EXPECTED / (src.stem + ".verdicts")
    results, tables = [], {}
    for mode in d["modes"]:
        tables[mode] = table(verdicts(topc(src, [f"--vcmode={mode}", *d["flags"], "--verify"])))
    first = tables[d["modes"][0]]
    if update:
        exp.write_text(first)
    want = exp.read_text() if exp.exists() else ""
    results.append(Result(f"verdicts/{src.name}", first == want,
                          diff(want, first, "expected", "actual") or "no expected file"))
    # The two VC generators check each other.
    for mode in d["modes"][1:]:
        results.append(Result(f"modes/{src.name}", tables[mode] == first,
                              diff(first, tables[mode], d["modes"][0], mode)))
    return results


# ---- liars: a false claim must not be proved -------------------------------

def liar_test(src, known):
    """Every function whose name starts with `liar` claims something false.

    The test passes if, in each VC mode, each such function has an obligation
    that is not plainly proved (FAILED, UNKNOWN, or marked, e.g. VACUOUS), or
    is not verified at all (skipped).
    """
    d = directives(src)
    liars = re.findall(r"^(?:lemma\s+)?(liar\w*)\s*\(", src.read_text(), re.M)
    bad = []
    for mode in d["modes"]:
        rows = verdicts(topc(src, [f"--vcmode={mode}", *d["flags"], "--verify"]))
        for fn in liars:
            mine = [r for r in rows if r[0] == fn]
            skipped = any(r[2] == "note" and r[3].startswith(fn + ": skipped") for r in rows)
            if skipped:
                continue
            if not mine:
                bad.append(f"{mode}: {fn} has no obligations in the report")
            elif all(r[2] == "proved" for r in mine):
                bad.append(f"{mode}: {fn} is proved, and its claim is false")
    if not liars:
        bad.append("no function named liar*")
    name = f"liars/{src.name}"
    if src.name in known:
        if bad:
            return [Result(name, True, xfail=True)]
        return [Result(name, False, "listed in KNOWN-FAILURES but now passes: remove it")]
    return [Result(name, not bad, "\n".join(bad))]


# ---- golden text for some views --------------------------------------------

def golden_tests(update):
    """golden/MANIFEST: `name: file flags...`; the output is golden/name.txt."""
    jobs = []
    manifest = GOLDEN / "MANIFEST"
    if not manifest.exists():
        return jobs
    for line in manifest.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        name, rest = line.split(":", 1)
        file, *flags = rest.split()
        jobs.append((name.strip(), ROOT / file, flags))

    def one(name, src, flags):
        out = topc(src, flags)
        # The size line counts bytes of SMT-LIB, which any change to the
        # prelude moves.
        out = "\n".join(l for l in out.splitlines()
                        if not l.startswith(("profiling:", "VC ("))) + "\n"
        # Counterexample values vary between Z3 versions.
        out = re.sub(r"\s{3}when .*$", "", out, flags=re.M)
        exp = GOLDEN / (name + ".txt")
        if update:
            exp.write_text(out)
        want = exp.read_text() if exp.exists() else ""
        return [Result(f"golden/{name}", out == want,
                       diff(want, out, "expected", "actual") or "no expected file")]
    return [lambda n=n, s=s, f=f: one(n, s, f) for n, s, f in jobs]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--update", action="store_true",
                    help="rewrite expected verdicts and golden text from this run; review the diff")
    ap.add_argument("--only", default="", help="run tests whose name contains this")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not shutil.which("z3"):
        print("verify tests skipped: z3 is not on the PATH")
        return 0
    if not TOPC.exists():
        print(f"error: topc not found at {TOPC}", file=sys.stderr)
        return 1

    known_file = LIARS / "KNOWN-FAILURES"
    known = set()
    if known_file.exists():
        known = {l.split()[0] for l in known_file.read_text().splitlines()
                 if l.strip() and not l.startswith("#")}

    jobs = []
    for src in sorted(DEMOS.glob("*.top")) + sorted(CASES.glob("*.top")):
        jobs.append((f"verdicts/{src.name}", lambda s=src: verdict_test(s, args.update)))
    for src in sorted(LIARS.glob("*.top")):
        jobs.append((f"liars/{src.name}", lambda s=src: liar_test(s, known)))
    for g in golden_tests(args.update):
        jobs.append(("golden/", g))
    jobs = [(n, j) for n, j in jobs if args.only in n or n == "golden/"]

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for rs in pool.map(lambda nj: nj[1](), jobs):
            results.extend(r for r in rs if args.only in r.name)
    results.sort(key=lambda r: r.name)

    failed = [r for r in results if not r.ok]
    xfail = [r for r in results if r.xfail]
    for r in results:
        if args.verbose:
            print(("XFAIL" if r.xfail else "ok   " if r.ok else "FAIL ") + " " + r.name)
    for r in failed:
        print(f"FAIL: {r.name}")
        for line in r.detail.splitlines()[:40]:
            print("    " + line)
    print(f"verify tests: {len(results) - len(failed)}/{len(results)} passed"
          + (f", {len(xfail)} known failures ({', '.join(r.name for r in xfail)})" if xfail else "")
          + (f", {len(failed)} FAILED" if failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
