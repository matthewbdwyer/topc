#!/usr/bin/env python3
"""Test adequacy for the ownership rules: every rule must be load-bearing.

For each rule id topc knows (`topc --unsafe-list-rules <file>`), disable just
that rule (TOPC_UNSAFE_DISABLE) and run the test tiers in order until one
fails:

  1. rejection tests   every test/system/iotests/*error.top must still be
                       rejected with its expected diagnostic (compile only)
  2. unit tests        ctest (the analyses run in-process)
  3. system tests      run.py (selftests and iotests under ASan/LSan)
  4. soundness grid    run.py --soundness

A rule that no tier notices is untested (or unnecessary): the run fails.

    RTLIB=build/rtlib TOPCLANG=/path/to/clang \\
        python3 test/system/soundness/adequacy.py [-j N] [--rules id ...]
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SYSTEM = HERE.parent
ROOT = SYSTEM.parents[1]
TOPC = Path(os.environ.get("TOPC", ROOT / "build/src/topc"))
BUILD = ROOT / "build"

sys.path.insert(0, str(SYSTEM))
import run as system_run  # noqa: E402  (EXPECTED_ERROR_SUBSTRINGS)


def rules():
    out = subprocess.run([str(TOPC), "--unsafe-list-rules",
                          str(SYSTEM / "iotests" / "fib.top")],
                         capture_output=True, text=True)
    return [line.strip() for line in out.stdout.splitlines() if line.strip()]


def env_for(rule):
    return {**os.environ, "TOPC_UNSAFE_DISABLE": rule}


def tier_rejections(rule, jobs):
    import concurrent.futures
    import tempfile

    def check(err_file):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([str(TOPC), str(err_file), "-o",
                                str(Path(tmp) / "out.bc")],
                               capture_output=True, text=True, env=env_for(rule))
        expected = system_run.EXPECTED_ERROR_SUBSTRINGS.get(err_file.name)
        if r.returncode == 0:
            return f"{err_file.name} now compiles"
        if expected and expected not in r.stderr:
            return f"{err_file.name} rejected for another reason"
        return None

    files = sorted((SYSTEM / "iotests").glob("*error.top"))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for msg in pool.map(check, files):
            if msg:
                return msg
    return None


def tier_unit(rule, jobs):
    r = subprocess.run(["ctest", "--test-dir", str(BUILD), "-j", str(jobs)],
                       capture_output=True, text=True, env=env_for(rule))
    if r.returncode != 0:
        failed = [l.strip() for l in r.stdout.splitlines() if "(Failed)" in l]
        return f"unit: {len(failed)} failed, e.g. {failed[0] if failed else ''}"
    return None


def tier_system(rule, jobs, extra):
    r = subprocess.run([sys.executable, str(SYSTEM / "run.py"), "-j", str(jobs), *extra],
                       capture_output=True, text=True, env=env_for(rule))
    if r.returncode != 0:
        fails = [l.strip() for l in r.stdout.splitlines() if l.strip().startswith("FAIL")]
        return f"{'grid' if extra else 'system'}: {len(fails)} failed, e.g. {fails[0] if fails else ''}"
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", "--jobs", type=int, default=8)
    ap.add_argument("--rules", nargs="*")
    args = ap.parse_args()
    ids = args.rules or rules()
    if not ids:
        print("no rule ids (is topc built?)", file=sys.stderr)
        return 1
    undetected = []
    for rule in ids:
        t0 = time.monotonic()
        tiers = [("rejection tests", lambda: tier_rejections(rule, args.jobs)),
                 ("unit tests", lambda: tier_unit(rule, args.jobs)),
                 ("system tests", lambda: tier_system(rule, args.jobs, [])),
                 ("soundness grid", lambda: tier_system(rule, args.jobs, ["--soundness"]))]
        caught = None
        for name, tier in tiers:
            msg = tier()
            if msg:
                caught = (name, msg)
                break
        took = time.monotonic() - t0
        if caught:
            print(f"detected   {rule:26s} by {caught[0]:16s} ({took:4.0f}s)  {caught[1][:90]}")
        else:
            print(f"UNDETECTED {rule:26s} no test fails with this rule disabled ({took:4.0f}s)")
            undetected.append(rule)
        sys.stdout.flush()
    print(f"\n{len(ids) - len(undetected)}/{len(ids)} rules detected")
    return 1 if undetected else 0


if __name__ == "__main__":
    sys.exit(main())
