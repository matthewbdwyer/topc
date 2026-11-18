# VERUTOP unit tests

One file per module of `src/verify/`. None of them runs Z3.

```bash
cmake --build build --target verify_unit_tests
./build/test/unit/verify/verify_unit_tests            # or, in build/: ctest -R "^verify\."
./build/test/unit/verify/verify_unit_tests "[Trust]"  # one module
```

| File | Tests | How |
| --- | --- | --- |
| `TermTest`, `TermPrintTest` | substitution and free variables under binders; each simplification; both printers | terms built by hand |
| `EncodingTest` | sorts, datatype declarations, `rank`, which borrows are written, `case` patterns | programs parsed from source |
| `GclTest` | the variant, flattening, the printer, `match` | commands built by hand |
| `ObligationsTest` | every kind's message, pinned to the wording the demos use | |
| `LoweringTest` | the guarded commands of small functions: a loop cut, a call, recursion, a written borrow, `--overflow` | parsed programs, compared as `--pgcl` text |
| `PassivizeTest` | versions, merges after a choice, dead branches, `old` | commands built by hand |
| `VcStrategyTest` | the wp rules; that `SubstVc` and `PassiveVc` agree; linear growth of the passive VC | VCs evaluated over a small domain (`GclHelpers.h`) |
| `QueryTest` | the dependency closure; rendering; the flag encoding; the trace query | |
| `SolverTest` | reading Z3's output; the text sent; batches answered in order whatever the number of jobs | text, and `CannedSolver` |
| `TrustTest` | verdicts and what they rest on: unverified callees, inheritance, recursion, vacuity | results built by hand |
| `DriverTest` | what the driver asks and concludes: one query per obligation, vacuity, missing clauses, a withheld spec definition, `--vfn`, independence from `--vjobs` | parsed programs and `CannedSolver` |

`GclHelpers.h` has the builders for hand-made programs and `valid`, which
decides a formula by trying every value in a small domain: enough to check
that two verification conditions agree without a solver. `ProgramHelper.h`
parses and type-checks source text.
