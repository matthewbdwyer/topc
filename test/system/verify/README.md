# VERUTOP verifier tests

Tests of `topc --verify`. They need `z3` on the `PATH` and are skipped with
a notice without it. They are not part of the default system-test run:

```bash
python3 test/system/verify/runner.py            # or: run.py --verifier
python3 test/system/verify/runner.py --only liars -v
```

A full run takes about a minute.

## What is tested

| Kind | Programs | Passes when |
| --- | --- | --- |
| verdicts | `verutop-demos/*.top`, `cases/*.top` | the report's rows (function, line, verdict, obligation) equal `expected/<name>.verdicts` |
| modes | the same | `--vcmode=wp` gives the same rows as passive: the two VC generators check each other |
| liars | `liars/*.top` | every function named `liar*`, whose contract is false, has an obligation that is not plainly `proved` |
| golden | `golden/MANIFEST` | a view's text equals `golden/<name>.txt` |

Verdict rows are compared, not bytes: a counterexample's values
(`when x = -1`) and Z3's messages differ between Z3 versions and VC modes
and are dropped. A mark in capitals after the obligation (`VACUOUS`) is
part of the verdict. Counterexample traces are not pinned for the same
reason.

A test may start with directives:

```
// flags: --overflow        extra topc flags
// modes: passive           VC modes to run (default: passive wp)
```

## Liars

A liar is a program that claims something false. If the verifier proves
it, the verifier is unsound, or it is hiding an assumption. There is one
for each known way to cheat: a spec function that does not terminate, a
cycle of spec functions or lemmas, recursion with no measure, overlapping
borrows, a write the caller forgets, `assume false`, overflow. **A change
to the verifier that could prove more should come with a liar.**

`liars/KNOWN-FAILURES` lists liars the verifier still proves, with the
reason. They are reported, not failed; one that starts passing fails the
run until its line is removed, so the list stays current.

## Changing expected output

```bash
python3 test/system/verify/runner.py --update
git diff test/system/verify
```

`--update` rewrites `expected/` and `golden/` from the current build.
Every changed row is a change in what the verifier claims: review the diff
and commit it with the change that caused it.
