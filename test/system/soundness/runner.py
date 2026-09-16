"""Run the checked-in soundness cases (see README.md).

Used by run.py --soundness, or directly:

    python3 runner.py [-j N] [--kinds own sum] [--contexts call gen]
                      [--show CLASS ...]

Environment: TOPC, RTLIB (directory holding top_rtlib.bc), TOPCLANG.
"""

import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import List

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MANIFEST = HERE / "manifest.tsv"
PROBLEM_CLASSES = ("UNSOUND", "SLACK", "PERMISSIVE", "DIAG", "ERROR")
ANSI = re.compile(r"\x1b\[[0-9;]*m")


@dataclass
class Entry:
    name: str
    kind: str
    context: str
    op1: str
    op2: str
    expect: str
    reasons: List[str]

    @property
    def path(self) -> Path:
        return HERE / "cases" / self.kind / self.context / f"{self.name}.top"


def load(kinds=None, contexts=None) -> List[Entry]:
    entries = []
    lines = MANIFEST.read_text().splitlines()[1:]
    for line in lines:
        name, kind, context, op1, op2, expect, reasons = line.split("\t")
        if kinds and kind not in kinds:
            continue
        if contexts and context not in contexts:
            continue
        entries.append(Entry(name, kind, context, op1, op2, expect,
                             [] if reasons == "-" else reasons.split(" | ")))
    return entries


def _sysroot():
    if sys.platform != "darwin":
        return []
    sdk = os.environ.get("SDKROOT") or subprocess.run(
        ["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
    return ["-isysroot", sdk] if sdk else []


class Runner:
    def __init__(self, topc: Path, rtlib: Path, clang: str):
        self.topc, self.rtlib, self.clang = topc, rtlib, clang
        self.sysroot = _sysroot()
        self.env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=1",
                    "MallocNanoZone": "0"}

    def classify(self, entry: Entry, scratch: Path):
        """Return (class, detail) for one case."""
        bc = scratch / f"{entry.name}.bc"
        r = subprocess.run([str(self.topc), "--san", str(entry.path), "-o", str(bc)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            first = ANSI.sub("", (r.stderr.strip().splitlines() or [""])[0])
            if "Cannot unify" in r.stderr or "parse error" in r.stderr:
                return "ILLTYPED", first
            if entry.expect == "accept":
                return "SLACK", first
            if entry.reasons and not any(s in r.stderr for s in entry.reasons):
                return "DIAG", first
            return "OK", ""
        exe = scratch / entry.name
        link = subprocess.run([self.clang, "-w", "-fsanitize=address", *self.sysroot,
                               str(bc), str(self.rtlib / "top_rtlib.bc"),
                               "-o", str(exe)], capture_output=True, text=True)
        if link.returncode != 0:
            return "ERROR", (link.stderr.strip().splitlines() or [""])[0]
        for c in ("0", "1"):
            try:
                run = subprocess.run([str(exe), c], capture_output=True, text=True,
                                     env=self.env, timeout=30)
            except subprocess.TimeoutExpired:
                return "ERROR", f"c={c} timeout"
            if run.returncode != 0:
                report = [l.strip() for l in run.stderr.splitlines() if "Sanitizer:" in l]
                return "UNSOUND", f"c={c} exit={run.returncode} {report[0] if report else ''}"
        if entry.expect == "reject":
            return "PERMISSIVE", "accepted and ran clean; expected " + (entry.reasons[0] if entry.reasons else "reject")
        return "OK", ""

    def run(self, entries: List[Entry], jobs: int):
        with tempfile.TemporaryDirectory(prefix="topc_soundness_") as tmp:
            with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
                futs = {pool.submit(self.classify, e, Path(tmp)): e for e in entries}
                return [(futs[f], *f.result())
                        for f in concurrent.futures.as_completed(futs)]


def summary(results) -> str:
    counts = Counter(cls for _, cls, _ in results)
    return f"{len(results)} cases: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=8)
    ap.add_argument("--kinds", nargs="*")
    ap.add_argument("--contexts", nargs="*")
    ap.add_argument("--show", nargs="*", default=list(PROBLEM_CLASSES))
    args = ap.parse_args()
    runner = Runner(Path(os.environ.get("TOPC", ROOT / "build/src/topc")),
                    Path(os.environ.get("RTLIB", ROOT / "build/rtlib")),
                    os.environ.get("TOPCLANG", "clang"))
    results = runner.run(load(args.kinds, args.contexts), args.jobs)
    for entry, cls, detail in sorted(results, key=lambda t: (t[1], t[0].name)):
        if cls in args.show:
            print(f"{cls:10s} {entry.name:40s} {detail}")
    print(summary(results))
    return 1 if any(cls in PROBLEM_CLASSES for _, cls, _ in results) else 0


if __name__ == "__main__":
    sys.exit(main())
