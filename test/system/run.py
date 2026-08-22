#!/usr/bin/env python3
"""TOP system test runner.

Replaces test/system/run.sh with parallel execution and structured output.

Categories
----------
    selftests   compile + link + run (.top)
    iotests     compile + link + run with argv, diff stdout
    polytests   compile + run; diff --psource --ptype snapshot
    snapshots   diff --psource --ptype output for every selftest
    driver      flag/error-handling smoke tests

Usage
-----
  python3 run.py [--jobs N] [--junit FILE] [--verbose]

Environment
-----------
  TOPC      path to topc binary (default: <root>/build/src/topc)
  TOPCLANG  path to clang for linking (required)
  RTLIB     path to rtlib directory (default: <root>/rtlib)
"""

import argparse
import concurrent.futures
import difflib
import os
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Sequence

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).parent.resolve()
ROOT_DIR = Path(
    os.environ.get("GITHUB_WORKSPACE", str(SCRIPT_DIR / "../.."))
).resolve()

TOPC     = Path(os.environ.get("TOPC",    str(ROOT_DIR / "build/src/topc")))
TOPCLANG = os.environ.get("TOPCLANG", "")
RTLIB    = Path(os.environ.get("RTLIB",   str(ROOT_DIR / "rtlib")))

SELFTESTS_DIR = SCRIPT_DIR / "selftests"
IOTESTS_DIR   = SCRIPT_DIR / "iotests"
POLYTESTS_DIR = SCRIPT_DIR / "polytests"

TIMEOUT = 30  # seconds per compiled-program execution

EXPECTED_ERROR_SUBSTRINGS = {
    "higher-order-borrow-conflict-error.top":
        "Cannot unify Own with Borrow",
    "recursive-function-unsupported-error.top":
        "recursive types are not yet supported in ownership analysis",
    "owned-move-then-use-error.top":
        "used after move",
    "alloc-owned-error.top":
        "owned pointers cannot nest",
    "alloc-nested-error.top":
        "owned pointers cannot nest",
    "case-unknown-ctor-error.top":
        "unknown constructor",
    "case-unreachable-arm-error.top":
        "unreachable case arm",
    "case-unreachable-nested-error.top":
        "unreachable case arm",
    "case-mixed-type-error.top":
        "belongs to type",
    "borrow-escape-assign-error.top":
        "escapes into assignment",
    "borrow-escape-return-error.top":
        "escapes into return",
    "double-move-args-error.top":
        "moved more than once",
    "ctor-expr-unknown-error.top":
        "unknown constructor",
    "ctor-expr-arity-error.top":
        "expects 2 argument(s) but expression provides 3",
    "apply-nonfunction-error.top":
        "Cannot unify",
    "apply-formal-nonfunction-error.top":
        "Cannot unify",
}

# ---------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------

@dataclass
class TestResult:
    name: str
    passed: bool
    message: str = ""
    duration: float = 0.0


# ---------------------------------------------------------------------------
# Low-level helpers
# ---------------------------------------------------------------------------

def _run(cmd: Sequence, *,
     timeout: int = TIMEOUT,
     args: Sequence[str] = (),
     cwd: Optional[Path] = None,
     env: Optional[dict] = None,
     input_text: Optional[str] = None) -> subprocess.CompletedProcess:
    """Run *cmd* and return the CompletedProcess; never raises on non-zero exit."""
    full = list(cmd) + list(args)
    return subprocess.run(full, capture_output=True, text=True,
                          timeout=timeout, cwd=cwd, env=env, input=input_text)


# Environment for running *compiled TOP programs*: enable LeakSanitizer so that
# any owned value the program fails to free is reported as a test failure. The
# programs are linked with -fsanitize=address; on macOS LeakSanitizer is opt-in
# via ASAN_OPTIONS. This applies only to the compiled program runs, not to topc
# itself.
LSAN_ENV = {**os.environ, "ASAN_OPTIONS": "detect_leaks=1"}


def _compile(srcfile: Path, out_bc: Path,
             extra_flags: Sequence[str] = ()) -> subprocess.CompletedProcess:
    return _run([str(TOPC)] + list(extra_flags) + [str(srcfile), "-o", str(out_bc)])


def _macos_sysroot_flags() -> List[str]:
    """On macOS, Homebrew clang does not locate the system SDK on its own, so
    the linker fails to find libSystem ("library 'System' not found"). Pass an
    explicit -isysroot pointing at the active SDK. Returns no flags on other
    platforms."""
    if sys.platform != "darwin":
        return []
    sdk = os.environ.get("SDKROOT")
    if not sdk:
        try:
            sdk = subprocess.run(["xcrun", "--show-sdk-path"],
                                 capture_output=True, text=True).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            sdk = ""
    return ["-isysroot", sdk] if sdk else []


MACOS_SYSROOT_FLAGS = _macos_sysroot_flags()


def _link(bc_files: Sequence[Path], out_exe: Path) -> subprocess.CompletedProcess:
    return _run([TOPCLANG, "-w", "-fsanitize=address"]
                + MACOS_SYSROOT_FLAGS
                + [str(b) for b in bc_files]
                + ["-o", str(out_exe)])


def _diff_text(actual: str, expected: str, label: str) -> str:
    """Return a unified diff (capped at 40 lines) or empty string if equal."""
    lines = list(difflib.unified_diff(
        expected.splitlines(keepends=True),
        actual.splitlines(keepends=True),
        fromfile=f"{label}.expected",
        tofile=f"{label}.actual",
        n=3,
    ))
    return "".join(lines[:40])


# ---------------------------------------------------------------------------
# Selftest
# ---------------------------------------------------------------------------

def run_selftest(srcfile: Path, scratch: Path,
                 extra_flags: Sequence[str] = ()) -> TestResult:
    """Compile, link, and run a single self-checking program."""
    suffix = ".do" if "-do" in extra_flags else ""
    name = f"selftest.{srcfile.stem}{suffix}"
    t0 = time.monotonic()

    bc  = scratch / f"{srcfile.stem}.bc"
    exe = scratch / srcfile.stem

    r = _compile(srcfile, bc, extra_flags)
    if r.returncode != 0:
        return TestResult(name, False,
                          f"compile failed:\n{r.stderr.strip()}",
                          time.monotonic() - t0)

    r = _link([bc, RTLIB / "top_rtlib.bc"], exe)
    if r.returncode != 0:
        return TestResult(name, False,
                          f"link failed:\n{r.stderr.strip()}",
                          time.monotonic() - t0)

    try:
        r = _run([str(exe)], timeout=TIMEOUT, env=LSAN_ENV)
    except subprocess.TimeoutExpired:
        return TestResult(name, False, "timeout", time.monotonic() - t0)

    if r.returncode != 0:
        return TestResult(name, False,
                          f"exit {r.returncode}",
                          time.monotonic() - t0)

    return TestResult(name, True, duration=time.monotonic() - t0)


# ---------------------------------------------------------------------------
# IO test
# ---------------------------------------------------------------------------

def run_iotest(expected_file: Path, scratch: Path) -> TestResult:
    """Compile, link, run with argv from filename, diff stdout."""
    # expected_file stem: <program>-<arg>.expected  (arg may be empty)
    stem  = expected_file.stem           # e.g. "fib-7"
    parts = stem.split("-", 1)
    program_name = parts[0]              # e.g. "fib"
    prog_arg     = parts[1] if len(parts) > 1 else ""   # e.g. "7"

    name = f"iotest.{stem}"
    t0   = time.monotonic()

    src = IOTESTS_DIR / f"{program_name}.top"
    bc  = scratch / f"{program_name}.bc"
    exe = scratch / program_name

    r = _compile(src, bc)
    if r.returncode != 0:
        return TestResult(name, False,
                          f"compile failed:\n{r.stderr.strip()}",
                          time.monotonic() - t0)

    r = _link([bc, RTLIB / "top_rtlib.bc"], exe)
    if r.returncode != 0:
        return TestResult(name, False,
                          f"link failed:\n{r.stderr.strip()}",
                          time.monotonic() - t0)

    try:
        cmd = [str(exe)] + ([prog_arg] if prog_arg else [])
        stdin_fixture = expected_file.with_suffix(".stdin")
        stdin_text = stdin_fixture.read_text() if stdin_fixture.exists() else None
        r = _run(cmd, timeout=TIMEOUT, env=LSAN_ENV, input_text=stdin_text)
    except subprocess.TimeoutExpired:
        return TestResult(name, False, "timeout", time.monotonic() - t0)

    expected = expected_file.read_text()
    actual   = r.stdout
    if actual != expected:
        diff = _diff_text(actual, expected, name)
        return TestResult(name, False,
                          f"output mismatch:\n{diff}",
                          time.monotonic() - t0)

    return TestResult(name, True, duration=time.monotonic() - t0)


# ---------------------------------------------------------------------------
# Snapshot test (stdout-based inspection combinations)
# ---------------------------------------------------------------------------

def run_stdout_snapshot(srcfile: Path, flags: Sequence[str],
                        golden: Path, scratch: Path,
                        name: str) -> TestResult:
    """Run topc with *flags* on *srcfile*; compare stdout to *golden*."""
    t0 = time.monotonic()

    r = _run([str(TOPC)] + list(flags) + [str(srcfile)])
    if r.returncode != 0:
        return TestResult(name, False,
                          f"topc failed:\n{r.stderr.strip()}",
                          time.monotonic() - t0)

    actual   = r.stdout
    expected = golden.read_text()
    if actual != expected:
        diff = _diff_text(actual, expected, name)
        return TestResult(name, False,
                          f"snapshot mismatch:\n{diff}",
                          time.monotonic() - t0)

    return TestResult(name, True, duration=time.monotonic() - t0)


# ---------------------------------------------------------------------------
# Polytest
# ---------------------------------------------------------------------------

def run_polytest(srcfile: Path, scratch: Path) -> List[TestResult]:
    """Compile and run, then check the pppt snapshot."""
    base = f"polytest.{srcfile.stem}"
    results: List[TestResult] = []

    bc  = scratch / f"{srcfile.stem}.bc"
    exe = scratch / srcfile.stem
    t0  = time.monotonic()

    r = _compile(srcfile, bc, [])
    if r.returncode != 0:
        results.append(TestResult(f"{base}.run", False,
                                  f"compile failed:\n{r.stderr.strip()}",
                                  time.monotonic() - t0))
        results.append(TestResult(f"{base}.pppt", False, "skipped (compile failed)"))
        return results

    r = _link([bc, RTLIB / "top_rtlib.bc"], exe)
    if r.returncode != 0:
        results.append(TestResult(f"{base}.run", False,
                                  f"link failed:\n{r.stderr.strip()}",
                                  time.monotonic() - t0))
        results.append(TestResult(f"{base}.pppt", False, "skipped (link failed)"))
        return results

    try:
        r = _run([str(exe)], timeout=TIMEOUT, env=LSAN_ENV)
        if r.returncode != 0:
            results.append(TestResult(f"{base}.run", False,
                                      f"exit {r.returncode}",
                                      time.monotonic() - t0))
        else:
            results.append(TestResult(f"{base}.run", True,
                                      duration=time.monotonic() - t0))
    except subprocess.TimeoutExpired:
        results.append(TestResult(f"{base}.run", False,
                                  "timeout", time.monotonic() - t0))

    golden = srcfile.with_suffix(srcfile.suffix + ".pppt")
    results.append(run_stdout_snapshot(
        srcfile, ["--psource", "--ptype"], golden, scratch, f"{base}.pppt"))

    return results


# ---------------------------------------------------------------------------
# Driver / argument tests (run serially — some have shared side effects)
# ---------------------------------------------------------------------------

def run_driver_tests(scratch: Path) -> List[TestResult]:
    results: List[TestResult] = []

    def check(name: str, fn) -> None:
        t0 = time.monotonic()
        try:
            msg = fn()
            results.append(TestResult(name, msg is None,
                                      msg or "", time.monotonic() - t0))
        except Exception as exc:
            results.append(TestResult(name, False,
                                      str(exc), time.monotonic() - t0))

    # -- --psource --psym snapshot ---------------------------------------------
    results.append(run_stdout_snapshot(
        IOTESTS_DIR / "fib.top", ["--psource", "--psym"],
        IOTESTS_DIR / "fib.ppps", scratch, "driver.fib.ppps"))

    # -- --asm default output file (no -o; writes <src>.ll next to source) ------
    def asm_default_output():
        out = IOTESTS_DIR / "main.top.ll"
        out.unlink(missing_ok=True)
        r = _run([str(TOPC), "--asm", str(IOTESTS_DIR / "main.top")])
        if r.returncode != 0:
            return f"topc --asm failed:\n{r.stderr.strip()}"
        if not out.exists():
            return f"expected output file {out} was not created"
        out.unlink()
        return None
    check("driver.asm_default_output", asm_default_output)

    # -- --asm with -o; skip first 3 lines (LLVM version header) ---------------
    def asm_explicit_output():
        out = scratch / "fib.top.ll"
        r = _run([str(TOPC), "--asm", str(IOTESTS_DIR / "fib.top"), "-o", str(out)])
        if r.returncode != 0:
            return f"topc --asm failed:\n{r.stderr.strip()}"
        actual_lines   = out.read_text().splitlines(keepends=True)[3:]
        expected_lines = (IOTESTS_DIR / "fib.top.ll").read_text().splitlines(keepends=True)[3:]
        diff = _diff_text("".join(actual_lines), "".join(expected_lines), "fib.top.ll")
        if diff:
            return f"LLVM IR mismatch:\n{diff}"
        return None
    check("driver.fib.ll", asm_explicit_output)

    # -- --pcallgraph call graph ------------------------------------------------
    def pcallgraph_fib():
        out = scratch / "fib.top.callgraph.dot"
        r = _run([str(TOPC), "--pcallgraph", "--output-dir", str(scratch),
                  str(IOTESTS_DIR / "fib.top"), "-o", str(scratch / "fib.bc")])
        if r.returncode != 0:
            return f"topc --pcallgraph failed:\n{r.stderr.strip()}"
        diff = _diff_text(out.read_text(),
                          (IOTESTS_DIR / "fib.top.dot").read_text(), "fib.top.dot")
        return f"call graph mismatch:\n{diff}" if diff else None
    check("driver.pcallgraph.fib", pcallgraph_fib)

    # -- --ptype --constraint smoke -------------------------------------------
    def pc_type_smoke():
        r = _run([str(TOPC), "--ptype", "--constraint",
                  str(IOTESTS_DIR / "recursive-function-unsupported-error.top")])
        if r.returncode != 0:
            return f"topc --ptype --constraint failed:\n{r.stderr.strip()}"
        if "[type-constraints]" not in r.stdout:
            return "expected [type-constraints] section in stdout"
        if "[type-schemes]" not in r.stdout:
            return "expected [type-schemes] section in stdout"
        if "[type-instantiations]" not in r.stdout:
            return "expected [type-instantiations] section in stdout"
        if "[type-inferred]" not in r.stdout:
            return "expected [type-inferred] section in stdout"
        return None
    check("driver.ptype.constraint", pc_type_smoke)

    # -- --ptype --constraint snapshots ----------------------------------------
    def pc_type_snapshot(stem: str, expected_inferred: str = ""):
        src = SELFTESTS_DIR / f"{stem}.top"
        r = _run([str(TOPC), "--ptype", "--constraint", str(src)])
        if r.returncode != 0:
            return f"topc --ptype --constraint failed for {src.name}:\n{r.stderr.strip()}"
        required = [
            "[type-constraints]",
            "[type-schemes]",
            "[type-instantiations]",
            "[type-inferred]",
        ]
        for section in required:
            if section not in r.stdout:
                return f"missing section {section} in --ptype --constraint output"
        if expected_inferred and expected_inferred not in r.stdout:
            return f"missing inferred record: {expected_inferred}"
        return None

    check("driver.pc_type.exprs", lambda: pc_type_snapshot("exprs"))
    check("driver.pc_type.ptr4", lambda: pc_type_snapshot("ptr4"))
    check("driver.pc_type.sumtype_basic",
          lambda: pc_type_snapshot(
              "sumtype-basic",
              "type Direction : North | South | East | West"))
    check("driver.pc_type.polyfun", lambda: pc_type_snapshot("polyfun"))

    # -- --pcallgraph --constraint snapshot ------------------------------------
    def pc_cg_snapshot(stem: str):
        src = SELFTESTS_DIR / f"{stem}.top"
        golden = SELFTESTS_DIR / f"{stem}.top.pc.cg"
        r = _run([str(TOPC), "--pcallgraph", "--constraint", "--output-dir",
                  str(scratch), str(src)])
        if r.returncode != 0:
            return f"topc --pcallgraph --constraint failed for {src.name}:\n{r.stderr.strip()}"
        diff = _diff_text(r.stdout, golden.read_text(), golden.name)
        return f"--pcallgraph --constraint snapshot mismatch:\n{diff}" if diff else None

    check("driver.pc_cg.polyfun", lambda: pc_cg_snapshot("polyfun"))

    # -- --pownership --constraint snapshot ------------------------------------
    def pc_ownership_snapshot(stem: str):
        src = SELFTESTS_DIR / f"{stem}.top"
        golden = SELFTESTS_DIR / f"{stem}.top.pc.ownership"
        r = _run([str(TOPC), "--pownership", "--constraint", str(src)])
        if r.returncode != 0:
            return (
                f"topc --pownership --constraint failed for {src.name}:\n"
                f"{r.stderr.strip()}"
            )
        if "[ownership-constraints]" not in r.stdout:
            return "missing [ownership-constraints] section"
        if "[ownership-result]" not in r.stdout:
            return "missing [ownership-result] section"
        diff = _diff_text(r.stdout, golden.read_text(), golden.name)
        return f"--pownership --constraint snapshot mismatch:\n{diff}" if diff else None

    check("driver.pc_ownership.move", lambda: pc_ownership_snapshot("moveNoDoubleFree"))
    check("driver.pc_ownership.poly_identity_own",
          lambda: pc_ownership_snapshot("poly-identity-own"))

    # -- interprocedural ownership: owned results returned through higher-order
    #    calls must be destroyed (Oracle B: --pownership destroy count) ---------
    def iown_destroys(stem: str, expected: str):
        src = SELFTESTS_DIR / f"{stem}.top"
        r = _run([str(TOPC), "--pownership", str(src)])
        if r.returncode != 0:
            return f"topc --pownership failed for {src.name}:\n{r.stderr.strip()}"
        if expected not in r.stdout:
            return f"expected '{expected}' for {src.name}; got:\n{r.stdout}"
        return None

    check("driver.iown.return_factory",
          lambda: iown_destroys("iown-return-factory", "main : 2 destroys"))
    check("driver.iown.chain_factory",
          lambda: iown_destroys("iown-chain-factory", "main : 1 destroy"))
    check("driver.iown.local_factory",
          lambda: iown_destroys("iown-local-factory", "main : 1 destroy"))

    # -- --pborrow --constraint snapshot ---------------------------------------
    def pc_borrow_snapshot(stem: str):
        src = SELFTESTS_DIR / f"{stem}.top"
        golden = SELFTESTS_DIR / f"{stem}.top.pc.borrow"
        r = _run([str(TOPC), "--pborrow", "--constraint", str(src)])
        if r.returncode != 0:
            return f"topc --pborrow --constraint failed for {src.name}:\n{r.stderr.strip()}"
        if "[borrow-constraints]" not in r.stdout:
            return "missing [borrow-constraints] section"
        if "[borrow-result]" not in r.stdout:
            return "missing [borrow-result] section"
        diff = _diff_text(r.stdout, golden.read_text(), golden.name)
        return f"--pborrow --constraint snapshot mismatch:\n{diff}" if diff else None

    check("driver.pc_borrow.basic", lambda: pc_borrow_snapshot("borrow-basic"))
    check("driver.pc_borrow.interproc_pass_through",
          lambda: pc_borrow_snapshot("interproc-borrow-pass-through"))

    # -- --constraint with unsupported view should fail ------------------------
    def invalid_constraint_usage():
        r = _run([str(TOPC), "--past", "--constraint",
                  str(IOTESTS_DIR / "recursive-function-unsupported-error.top")])
        if r.returncode == 0:
            return "expected non-zero exit for unsupported --constraint usage"
        if "--constraint is unsupported" not in r.stderr:
            return f"expected unsupported --constraint message; got: {r.stderr!r}"
        return None
    check("driver.constraint.invalid_usage", invalid_constraint_usage)

    # -- invalid --past format should fail -------------------------------------
    def invalid_past_format():
        r = _run([str(TOPC), "--past=weird", str(SELFTESTS_DIR / "ptr4.top")])
        if r.returncode == 0:
            return "expected non-zero exit for invalid --past format"
        if "invalid --past format" not in r.stderr:
            return f"expected invalid --past format message; got: {r.stderr!r}"
        return None
    check("driver.past.invalid_format", invalid_past_format)

    # -- non-existent input should fail -----------------------------------------
    def nonexistent_input():
        r = _run([str(TOPC), "/tmp/__topc_no_such_file__.top"])
        if r.returncode == 0:
            return "expected non-zero exit for non-existent input"
        return None
    check("driver.nonexistent_input", nonexistent_input)

    # -- semantic logging levels and file behavior -----------------------------
    def logging_levels():
        source = SELFTESTS_DIR / "poly-identity-own.top"
        level1 = _run([str(TOPC), "--verbose=1", "--pownership", str(source)])
        level2 = _run([str(TOPC), "--verbose=2", "--pownership", str(source)])
        level3 = _run([str(TOPC), "--verbose=3", "--pownership", str(source)])
        for level, result in enumerate((level1, level2, level3), start=1):
            if result.returncode != 0:
                return f"--verbose={level} failed: {result.stderr!r}"
        if "[semantic][ownership-classification] start" not in level1.stderr:
            return "level 1 omitted semantic phase lifecycle"
        if "[semantic][move-analysis] line=" in level1.stderr:
            return "level 1 included a per-declaration move decision"
        if "[semantic][move-analysis] line=8 event=move" not in level2.stderr:
            return "level 2 omitted the polymorphic move decision"
        if "[semantic][unification] unify" in level2.stderr:
            return "level 2 included solver mechanics"
        if "[semantic][unification] unify" not in level3.stderr:
            return "level 3 omitted unification mechanics"
        return None
    check("driver.logging.levels", logging_levels)

    def logging_file():
        source = SELFTESTS_DIR / "poly-identity-own.top"
        log = scratch / "semantic.log"
        command = [str(TOPC), "--pownership", f"--log={log}", str(source)]
        first = _run(command)
        second = _run(command)
        if first.returncode != 0 or second.returncode != 0:
            return f"file logging failed: {first.stderr!r} {second.stderr!r}"
        content = log.read_text()
        marker = "[semantic][ownership-classification] start"
        if content.count(marker) != 2:
            return "--log did not capture level 1 records in append mode"
        if "[semantic][unification] unify" not in content:
            return "--log did not capture level 3 records"

        error_source = IOTESTS_DIR / "recursive-function-unsupported-error.top"
        failed = _run([str(TOPC), f"--log={log}", str(error_source)])
        expected = EXPECTED_ERROR_SUBSTRINGS[error_source.name]
        if failed.returncode == 0 or expected not in failed.stderr:
            return "--log suppressed a compiler diagnostic on stderr"
        return None
    check("driver.logging.file", logging_file)

    # -- AST visualizer: ptr4 --------------------------------------------------
    def past_ptr4():
        out = scratch / "ptr4.top.ast.dot"
        r = _run([str(TOPC), "--past", "--output-dir", str(scratch),
                  str(SELFTESTS_DIR / "ptr4.top")])
        if r.returncode != 0:
            return f"topc --past failed:\n{r.stderr.strip()}"
        diff = _diff_text(out.read_text(),
                          (SELFTESTS_DIR / "ptr4.top.dot").read_text(),
                          "ptr4.top.dot")
        return f"AST dot mismatch:\n{diff}" if diff else None
    check("driver.past.ptr4", past_ptr4)

    # -- AST visualizer: ptr4 ascii --------------------------------------------
    def past_ascii_ptr4():
        out = scratch / "ptr4.top.ast.txt"
        r = _run([str(TOPC), "--past=ascii", "--output-dir", str(scratch),
                  str(SELFTESTS_DIR / "ptr4.top")])
        if r.returncode != 0:
            return f"topc --past=ascii failed:\n{r.stderr.strip()}"
        diff = _diff_text(out.read_text(),
                          (SELFTESTS_DIR / "ptr4.top.ast.txt").read_text(),
                          "ptr4.top.ast.txt")
        return f"AST ascii mismatch:\n{diff}" if diff else None
    check("driver.past_ascii.ptr4", past_ascii_ptr4)

    # -- Error-input files must fail to compile --------------------------------
    for err_file in sorted(IOTESTS_DIR.glob("*error.top")):
        name = f"driver.error.{err_file.stem}"
        t0 = time.monotonic()
        r = _run([str(TOPC), str(err_file)])
        if r.returncode == 0:
            results.append(TestResult(name, False,
                                      f"expected compile failure for {err_file.name}",
                                      time.monotonic() - t0))
        elif (expected := EXPECTED_ERROR_SUBSTRINGS.get(err_file.name)) is not None and \
                expected not in r.stderr:
            results.append(TestResult(name, False,
                                      f"expected diagnostic containing {expected!r}; got: {r.stderr!r}",
                                      time.monotonic() - t0))
        else:
            results.append(TestResult(name, True, duration=time.monotonic() - t0))

    return results


# ---------------------------------------------------------------------------
# JUnit XML
# ---------------------------------------------------------------------------

def write_junit(results: List[TestResult], path: Path) -> None:
    failures = [r for r in results if not r.passed]
    suite = ET.Element("testsuite",
                       name="topc-system-tests",
                       tests=str(len(results)),
                       failures=str(len(failures)),
                       time=f"{sum(r.duration for r in results):.3f}")
    for r in results:
        tc = ET.SubElement(suite, "testcase",
                           name=r.name, time=f"{r.duration:.3f}")
        if not r.passed:
            ET.SubElement(tc, "failure",
                          message=r.message[:200]).text = r.message
    ET.ElementTree(suite).write(str(path), encoding="unicode",
                                xml_declaration=True)


# ---------------------------------------------------------------------------
# Progress printer
# ---------------------------------------------------------------------------

def _record(result: TestResult,
            all_results: List[TestResult],
            verbose: bool) -> None:
    all_results.append(result)
    print("." if result.passed else "F", end="", flush=True)
    if not result.passed and verbose:
        print(f"\n  FAIL {result.name}: {result.message.splitlines()[0]}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="TOP system test runner")
    parser.add_argument("--jobs", "-j", type=int, default=1,
                        help="parallel workers for selftests (default: 1)")
    parser.add_argument("--junit", metavar="FILE",
                        help="write JUnit XML results to FILE")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="print failure detail immediately")
    args = parser.parse_args()

    if not TOPCLANG:
        print("error: TOPCLANG env var must be set", file=sys.stderr)
        return 1
    if not TOPC.exists():
        print(f"error: topc not found at {TOPC}", file=sys.stderr)
        return 1

    scratch_root = Path(tempfile.mkdtemp(prefix="topc_tests_"))
    all_results: List[TestResult] = []

    try:
        # ── Selftests (parallelisable) ─────────────────────────────────────
        tasks = []
        for src in sorted(SELFTESTS_DIR.glob("*.top")):
            d_normal = scratch_root / f"{src.stem}_normal"; d_normal.mkdir()
            d_do     = scratch_root / f"{src.stem}_do";     d_do.mkdir()
            tasks.append((src, d_normal, []))
            tasks.append((src, d_do,     ["-do"]))

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futs = {pool.submit(run_selftest, s, d, f): None
                    for s, d, f in tasks}
            for fut in concurrent.futures.as_completed(futs):
                _record(fut.result(), all_results, args.verbose)

        # ── IO tests ──────────────────────────────────────────────────────
        for expected in sorted(IOTESTS_DIR.glob("*.expected")):
            d = scratch_root / f"io_{expected.stem}"; d.mkdir()
            _record(run_iotest(expected, d), all_results, args.verbose)

        # ── Polytests ─────────────────────────────────────────────────────
        for src in sorted(POLYTESTS_DIR.glob("*.top")):
            d = scratch_root / f"poly_{src.stem}"; d.mkdir()
            for r in run_polytest(src, d):
                _record(r, all_results, args.verbose)

        # ── Driver / argument tests (serial) ──────────────────────────────
        d_drv = scratch_root / "driver"; d_drv.mkdir()
        for r in run_driver_tests(d_drv):
            _record(r, all_results, args.verbose)

    finally:
        shutil.rmtree(scratch_root, ignore_errors=True)

    print()  # newline after progress dots

    # ── Summary ───────────────────────────────────────────────────────────
    failures = [r for r in all_results if not r.passed]
    total    = len(all_results)
    if failures:
        print(f"\n{total - len(failures)}/{total} tests passed  "
              f"({len(failures)} failed)")
        for r in failures:
            print(f"  FAIL: {r.name}")
            if r.message and not args.verbose:
                for line in r.message.splitlines()[:5]:
                    print(f"        {line}")
    else:
        print(f"\n{total}/{total} tests passed")

    if args.junit:
        write_junit(all_results, Path(args.junit))

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
