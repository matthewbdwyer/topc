# VERUTOP verifier tests

Tests of `topc --verify`. They need `z3` on the `PATH` and are skipped with
a notice without it. They are not part of the default system-test run:

```bash
python3 test/system/verify/runner.py            # or: run.py --verifier
python3 test/system/verify/runner.py --only liars -v
python3 test/system/verify/runner.py --mutation # about two minutes; run before a push
```

A run takes about 20 seconds. With `TOPCLANG` set (the clang used to link,
as for the system tests) it also compiles and runs the verified functions.

## What is tested

| Kind | Programs | Passes when |
| --- | --- | --- |
| verdicts | `verutop-demos/*.top`, `cases/*.top` | the report's rows (function, line, verdict, obligation) equal `expected/<name>.verdicts` |
| modes | the same | `--vcmode=wp` gives the same rows as passive: the two VC generators check each other |
| liars | `liars/*.top` | every function named `liar*`, whose contract is false, has an obligation that is not plainly `proved` |
| golden | `golden/MANIFEST` | a view's text equals `golden/<name>.txt` |
| runtime | the same programs as verdicts | each function reported `=> verified` checks its own `ensures` when run on sample arguments, and none is false (`agree.py`) |
| mutation (`--mutation`) | the same, except demo 10 | each small change to the code of a function with a contract changes some verdict, or is listed with a reason in `mutation/SURVIVORS` (`mutate.py`) |

Verdict rows are compared, not bytes: a counterexample's values
(`when x = -1`) and Z3's messages differ between Z3 versions and VC modes
and are dropped. A mark in capitals after the obligation (`VACUOUS`) is
part of the verdict. Counterexample traces are not pinned for the same
reason.

One row is not exact. Z3's resource count for a query is supposed to be
the same on every run, and for nonlinear arithmetic it is not: demo 11's
`factDown` invariant, in wp mode, was measured between 4.7 and 33 million
steps for the same input, against a limit of 20 million. That row may be
`proved` or `UNKNOWN` (`UNSTABLE` in `runner.py`).

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

## Run-time agreement

The verifier and the code generator read the same AST but share no code,
so "proved" says nothing about the binary unless something compares them.
For each function `--ptrust` reports as verified with nothing assumed,
`agree.py` builds a copy of the program in which the function checks its
`ensures` before returning (`error 99` if false), the spec functions its
contract mentions are made ordinary functions, and `main` calls it on
sample arguments under its `requires`. Recursive functions check every
call. A contract that cannot be executed (it uses `old`, `rank`, `==>`, or
a borrow, or the function may not terminate) is skipped; `-v` lists them
and the run fails if fewer than 15 functions could be run. A self-check
presents demo 06's false contract as verified and requires the harness to
catch it.

## Mutation

A mutant replaces a comparison or arithmetic operator, adds one to a
constant, or drops a loop invariant, in the code of a function that has
an `ensures`. The contract is untouched, so the verifier should object. A
mutant whose verdicts are all unchanged survives; that is legitimate when
the mutant is equivalent or the contract is weak, and each such mutant is
recorded with its reason in `mutation/SURVIVORS`. An unlisted survivor
fails the run: look at it, and either list it or find out why the verifier
accepted it. Mutants run with `--vrlimit=10000000` so that one Z3 cannot
decide does not cost the full limit. One listed mutant is marked
`unstable:`: it is equivalent, but its proof is nonlinear and Z3 finds it
in some runs only, so it may survive or not.

## Changing expected output

```bash
python3 test/system/verify/runner.py --update
git diff test/system/verify
```

`--update` rewrites `expected/` and `golden/` from the current build.
Every changed row is a change in what the verifier claims: review the diff
and commit it with the change that caused it.
