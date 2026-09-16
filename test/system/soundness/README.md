# Ownership soundness grid

A bounded-exhaustive test of topc's ownership, borrow, and destruction rules.
It is **not** part of the default system-test run; run it explicitly:

```bash
TOPCLANG=/path/to/clang ./bin/runtests.sh -s -- --soundness -j 10
# or
RTLIB=build/rtlib TOPCLANG=/path/to/clang python3 test/system/run.py --soundness -j 10
```

About 1,100 programs; each is compiled with `--san`, and each accepted
program is linked with AddressSanitizer and run twice (`main(0)`, `main(1)`)
with LeakSanitizer on. A full run takes about a minute with `-j 10` on a
10-core machine (roughly 8 CPU-minutes). Run it before pushing any change to
the ownership passes (`src/semantic/ownership/`), the weeding borrow check,
or destruction-related code generation.

## What a case is

One owner `x` of a given **kind**, one or two **operations** on it, placed in
a **context**:

| Dimension | Values |
| --- | --- |
| kind | `int` (Copy), `own` (`own&int`), `sum` (sum with a Copy payload), `sumown` (sum with an owned payload) |
| operation | `assign` (`y = x`), `consume` (by-value call), `ident` (generic identity), `borrow` (`look(&x)`), `read` (`*x`), `write` (`*x = 3`), `casev` (by-value `case x of`), `payload` (`Wrap(x)`), `temp` (`*make()`, an unbound owned result) |
| context | `seq` (two statements), `expr` (one expression), `call` (two actuals of one call), `if1` / `if2` (one branch / both branches), `loop` (while body), `cond` (while condition), `arm1` (one case arm), `gen` (inside a generic body), `shadow` (inside both arms of a match whose binder has the same name) |

## The expected verdict comes from the rules, not from the compiler

`grid.py` derives each case's verdict and acceptable diagnostics (`expect`)
from the three design decisions behind TOP ownership:

- **Static destruction:** ownership state is the same on every path to a
  point; joins agree, a loop condition and body are invariant, and moves take
  effect in evaluation order.
- **Call-bounded, non-invalidating borrows:** a call may not move an owner
  one of its actuals borrows.
- **One compiled body per generic function:** an owned instance is passed on
  exactly once and not used after.

## Classes

| Class | Meaning | Test result |
| --- | --- | --- |
| `OK` | compiler agrees with the model; accepted programs run clean | pass |
| `ILLTYPED` | rejected by type inference: not a TOP program | pass |
| `UNSOUND` | accepted, but a sanitizer report or nonzero exit | **fail** |
| `SLACK` | the model accepts, the compiler rejects | **fail** (a restriction to review) |
| `PERMISSIVE` | the model rejects, the compiler accepts and runs clean | **fail** (model or compiler to review) |
| `DIAG` | both reject, with a diagnostic the model does not expect | **fail** |
| `ERROR` | link failure or timeout | **fail** |

## Files

- `grid.py` — kinds, operations, contexts, program construction, and the model.
- `generate.py` — writes `cases/<kind>/<context>/<name>.top` and `manifest.tsv`;
  `generate.py --check` fails if they differ from `grid.py`. Each case file
  begins with a comment stating its expected verdict.
- `cases/`, `manifest.tsv` — the checked-in test inputs. `run.py --soundness`
  refuses to run if they are stale.
- `runner.py` — compiles, runs, and classifies; also usable directly
  (`python3 runner.py -j 10 --kinds own --contexts call gen`).

## Changing the grid

Edit `grid.py`, run `generate.py`, and review the `manifest.tsv` diff: every
changed `expect` is a claim about the language. A new operation or context
should come with its rule in `expect`, stated before running the compiler.

## Test adequacy: every rule must be load-bearing

`adequacy.py` disables one ownership rule or safety mechanism at a time and
checks that some test notices:

```bash
RTLIB=build/rtlib TOPCLANG=/path/to/clang python3 test/system/soundness/adequacy.py -j 10
```

Rules are listed by `topc --unsafe-list-rules <any .top file>` and disabled with
the hidden option `--unsafe-disable=<id>` or the `TOPC_UNSAFE_DISABLE`
environment variable (read by every process that runs the analyses, so the unit
tests see it too). Both exist only for this harness; a compiler with a rule
disabled is deliberately unsound.

For each rule the harness runs, in order, until one fails: the rejection
iotests (compile only), the unit tests, the system tests, and this grid. It
reports the tier that caught the rule, and fails if any rule goes undetected.
Most rules are caught in seconds by the rejection tests; the code-generation
mechanisms need the system tests. A full run takes a few minutes.

When adding a rule, give its check an id in `src/error/RuleToggles.cpp`, guard
the check with `RuleToggles::enabled("<id>")`, and add a test that fails
without it. A check that no program can trigger is an internal invariant: throw
`InternalError` instead of adding an id.
