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
# Z3's resource count for one query is not the same on every run when the
# query is nonlinear arithmetic: for this one it was measured between 4.7
# and 33 million steps for identical input, higher on a loaded machine, and
# the limit is 20 million.  So this row may be `proved` or `UNKNOWN` in subst
# mode, and the comparison accepts either.  Every other row is exact.
UNSTABLE = {("11-factorial-variants.top", "subst"): ("factDown", "loop invariant is preserved")}

# Demo 10 exists to make a large VC; its contract (the result is in a range)
# says almost nothing about its sixteen branches, so nearly every mutant
# survives for that one reason.
NO_MUTATION = {"10-many-ifs.top"}
MUTANT_JOBS = 4    # mutants of one file run at once
MIN_RUNTIME = 15  # verified functions whose contract the run-time tests execute

VERDICT = re.compile(r"^  (proved|FAILED|UNKNOWN)\s+line (\d+): (.*)$")
HEADER = re.compile(r"^(\S.*):$")
# What follows the obligation on a verdict line and is not compared: a
# counterexample's values and Z3's own message vary between Z3 versions and
# VC modes.  A mark in capitals (such as VACUOUS) is part of the verdict.
VARYING = re.compile(r"\s{3}(when .*|\(z3: .*)$")
MARK = re.compile(r"\s{3}([A-Z]{4,})\b.*$")


def directives(src):
    """`// flags: ...` and `// modes: passive` lines at the top of a test."""
    d = {"flags": [], "modes": ["passive", "passive-copy", "subst"]}
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
            # Tests already run several at once, so each topc runs one Z3
            # at a time.
            p = subprocess.run([str(TOPC), "--vjobs=1", *flags, src.name], cwd=d, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=TIMEOUT)
            return re.sub(r"\x1b\[[0-9;]*m", "", p.stdout)  # no colour codes
        except subprocess.TimeoutExpired:
            return "TIMEOUT\n"


def verdicts(out):
    """The report as rows: (function, line, verdict, obligation).

    Lines that are neither a heading nor a verdict (a rejected spec function,
    a skipped function) are rows too, with verdict `note`.  The size line,
    traces, the note on the resource limit (it repeats the UNKNOWN rows, and
    is tested by limit_note_test), and everything from the trust summary on
    are not part of it.
    """
    rows, fn = [], ""
    for line in out.splitlines():
        if line.startswith(("VC (", "profiling:", "Z3 reached its resource limit")) or not line.strip():
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
        rows = verdicts(topc(src, [f"--vcmode={mode}", *d["flags"], "--verify"]))
        fn, what = UNSTABLE.get((src.name, mode), (None, None))
        rows = [(r[0], r[1], "proved", r[3]) if r[0] == fn and r[3] == what and r[2] == "UNKNOWN" else r
                for r in rows]
        tables[mode] = table(rows)
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
        out = re.sub(r"^  Z3 version .*$", "  Z3", out, flags=re.M)
        exp = GOLDEN / (name + ".txt")
        if update:
            exp.write_text(out)
        want = exp.read_text() if exp.exists() else ""
        return [Result(f"golden/{name}", out == want,
                       diff(want, out, "expected", "actual") or "no expected file")]
    return [lambda n=n, s=s, f=f: one(n, s, f) for n, s, f in jobs]


# ---- run-time agreement: a verified contract holds when the program runs ----

def agreement_test(src, verbose):
    import agree
    clang = os.environ.get("TOPCLANG", "")
    rtlib = Path(os.environ.get("RTLIB", str(ROOT / "build/rtlib"))) / "top_rtlib.bc"
    report = topc(src, [*directives(src)["flags"], "--ptrust"])
    results = []
    for fn, ok, detail, skipped in agree.run(src, TOPC, clang, rtlib, report):
        r = Result(f"runtime/{src.name}:{fn}", ok, detail)
        r.skipped = skipped
        results.append(r)
    return results


# ---- mutation: a changed program must change a verdict ----------------------

def mutation_tests(src, survivors, seen):
    import mutate
    d = directives(src)
    # A mutant Z3 cannot decide costs the whole resource limit, so mutants
    # get a smaller one; the original is run with the same limit.
    flags = ["--vcmode=passive", "--vrlimit=10000000", *d["flags"], "--verify"]
    shape = lambda out: [(r[0], r[2]) for r in verdicts(out)]
    text = src.read_text()
    base = shape(topc(src, flags))
    results = []
    with tempfile.TemporaryDirectory(prefix="verutop_mut_") as tmp:
        def one(k, mutated):
            copy = Path(tmp) / str(k) / src.name
            copy.parent.mkdir()
            copy.write_text(mutated)
            return shape(topc(copy, flags)) == base
        all_mutants = list(mutate.mutants(text))
        with concurrent.futures.ThreadPoolExecutor(max_workers=MUTANT_JOBS) as pool:
            outcomes = list(pool.map(lambda km: one(km[0], km[1][1]), enumerate(all_mutants)))
        for (name, _), survived in zip(all_mutants, outcomes):
            name = f"{src.name}:{name}"
            seen.add(name)
            if survived and name not in survivors:
                results.append(Result(f"mutation/{name}", False,
                                      "every verdict is unchanged, and the mutant is not in mutation/SURVIVORS"))
            elif not survived and name in survivors and not survivors[name].startswith("unstable:"):
                results.append(Result(f"mutation/{name}", False,
                                      "listed in mutation/SURVIVORS but a verdict now changes: remove it"))
            else:
                r = Result(f"mutation/{name}", True)
                r.survived = survived
                results.append(r)
    return results


def limit_note_test():
    """An UNKNOWN caused by the resource limit is named in a note that says
    how to raise the limit; a run that stays within the limit has no note."""
    src = DEMOS / "11-factorial-variants.top"
    out = topc(src, ["--verify", "--vrlimit=1000"])
    note = [l for l in out.splitlines() if l.startswith("Z3 reached its resource limit (1000)")]
    unknown = [(fn, line) for fn, line, verdict, _ in verdicts(out) if verdict == "UNKNOWN"]
    ok = len(note) == 1 and bool(unknown) and "--vrlimit=N" in note[0] and \
        all(f"{fn} line {line}" in note[0] for fn, line in unknown)
    quiet = "resource limit" not in topc(DEMOS / "01-abs.top", ["--verify"])
    return [Result("limit/note", ok and quiet,
                   f"note: {note}\nUNKNOWN rows: {unknown}\nno note for demo 01: {quiet}")]


def agreement_self_check():
    """The harness must be able to fail: half's contract in demo 06 is false
    at -1 (and its proof fails).  Told that half is verified, the run-time
    test must catch it."""
    import agree
    clang = os.environ.get("TOPCLANG", "")
    rtlib = Path(os.environ.get("RTLIB", str(ROOT / "build/rtlib"))) / "top_rtlib.bc"
    got = list(agree.run(DEMOS / "06-division.top", TOPC, clang, rtlib, "half:\n  => verified\n"))
    caught = len(got) == 1 and not got[0][1] and "false at run time" in got[0][2]
    return [Result("runtime/self-check", caught,
                   "a false contract, presented as verified, was not caught: " + repr(got))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--update", action="store_true",
                    help="rewrite expected verdicts and golden text from this run; review the diff")
    ap.add_argument("--mutation", action="store_true",
                    help="run only the mutation tests (several minutes)")
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
    jobs.append(("limit/note", limit_note_test))
    for g in golden_tests(args.update):
        jobs.append(("golden/", g))
    if os.environ.get("TOPCLANG"):
        for src in sorted(DEMOS.glob("*.top")) + sorted(CASES.glob("*.top")):
            jobs.append((f"runtime/{src.name}", lambda s=src: agreement_test(s, args.verbose)))
        jobs.append(("runtime/self-check", agreement_self_check))
    else:
        print("run-time agreement tests skipped: TOPCLANG is not set")
    if args.mutation:
        import mutate
        survivors, seen = mutate.listed(HERE / "mutation" / "SURVIVORS"), set()
        jobs = [(f"mutation/{src.name}", lambda s=src: mutation_tests(s, survivors, seen))
                for src in sorted(DEMOS.glob("*.top")) + sorted(CASES.glob("*.top"))
                if src.name not in NO_MUTATION]
    jobs = [(n, j) for n, j in jobs if args.only in n or n == "golden/"]

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for rs in pool.map(lambda nj: nj[1](), jobs):
            results.extend(r for r in rs if args.only in r.name)
    results.sort(key=lambda r: r.name)
    # A contract that cannot be executed is not a test; say so, and insist
    # that enough can be.
    skipped = [r for r in results if getattr(r, "skipped", None)]
    results = [r for r in results if not getattr(r, "skipped", None)]
    ran = [r for r in results if r.name.startswith("runtime/")]
    if os.environ.get("TOPCLANG") and not args.only and not args.mutation and len(ran) < MIN_RUNTIME:
        results.append(Result("runtime/coverage", False,
                              f"only {len(ran)} verified functions could be run; expected {MIN_RUNTIME}"))
    if args.verbose:
        for r in skipped:
            print(f"skip  {r.name}: {r.skipped}")

    if args.mutation:
        for name in sorted(set(survivors) - seen):
            if args.only in name:
                results.append(Result(f"mutation/{name}", False,
                                      "listed in mutation/SURVIVORS but no such mutant exists: remove it"))
        alive = sum(1 for r in results if getattr(r, "survived", False))
        print(f"mutants: {len(results)}, killed {len(results) - alive - sum(not r.ok for r in results)}, "
              f"listed survivors {alive}")
    failed = [r for r in results if not r.ok]
    xfail = [r for r in results if r.xfail]
    for r in results:
        if args.verbose:
            print(("XFAIL" if r.xfail else "ok   " if r.ok else "FAIL ") + " " + r.name
                  + (f"  ({r.detail})" if r.ok and r.detail and r.name.startswith("runtime/") else ""))
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
