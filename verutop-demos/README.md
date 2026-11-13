# VERUTOP demonstrations

Run each with `topc --verify prog.top`. The VC is the versioned (passive)
one by default; `--vcmode=wp` gives the substitution form. The queries are written to `prog.top.vc.smt2`.

Each stage can be printed; any of these flags implies `--verify`, and
`--vfn=f` limits the output to one function:

| Flag | Shows |
|---|---|
| `--pgcl` | guarded commands: `assume`, `assert`, `:=`, `havoc`, `[]` |
| `--ppassive` | the passive program: versions and merges |
| `--pwp` | a proof outline: the weakest precondition before each command |
| `--pvc` | the function's VC, in infix |
| `--psmt[=line]` | the SMT-LIB queries sent to Z3 |
| `--pspec` | spec-function definitions, axioms, and termination obligations |
| `--pcex` | a trace for each failure |

For example, `topc --pcex --vfn=factAny 13-factorial-total.top`:
```
factAny:
  FAILED  line 50: recursive call decreases the measure
    entry     n = -1
    line 46   r := 0                            r = 0
    line 46   m := 0                            m = 0
    line 47   else: n == 0 fails
    line 50   FAILS: recursive call decreases the measure
              0 <= -1 - 1 && -1 - 1 < -1
```

Without `--verify`, each program compiles and runs as an ordinary TOP
program: contracts, invariants, `assert`, and `assume` are type-checked and
then erased, so they generate no code; a spec function generates no code at
all, and code may not call one. The last column is the run.

| Program | Run |
|---|---|
| `01-abs` | 7 |
| `02-multiply` | 42 |
| `03-weak-invariant` | 42: the program is right; only the proof fails |
| `04-isqrt` | 7 |
| `05-factorial` | 120 |
| `06-division` | `2` prints 5; `0` stops with `division by zero` (exit 255) |
| `07-error` | `1` returns 20; `5` reaches `error 1` (exit 255) |
| `08-assume` | 5, although the "proved" postcondition says 42 |
| `09-overflow` | `5` prints 1 and 6; `9223372036854775807` prints 0 and -9223372036854775808 |
| `10-many-ifs` | 17 |
| `11-factorial-variants` | 120, five times |
| `12-termination` | 0 |
| `13-factorial-total` | 3628800 |
| `14-tree-height` | 0 (the selftest's checks pass) |
| `15-expr-borrow` | 0 (the selftest's checks pass) |
| `16-swap` | 3 |
| `17-bst-insert` | the inputs in order, then their count (`5 3 8 1 0` prints 1 3 5 8 and 4) |
| `18-tree-least` | 2 |

| Program | What it shows | Outcome |
|---|---|---|
| `01-abs.top` | wp of an `if`: both branches must establish the postcondition | all proved |
| `02-multiply.top` | a loop needs an invariant; `r == i * y` connects the loop to `result == x * y`; `decreases x - i` proves it terminates; `assert mult(6, 7) == 42` follows from the contract | all proved |
| `03-weak-invariant.top` | the same loop without `i <= x`: the invariant is preserved, but on exit `i` may be past `x` | postcondition fails, `x = 0, y = 1` |
| `04-isqrt.top` | nonlinear arithmetic; a postcondition that pins the answer exactly, so `assert isqrt(50) == 7` is proved | all proved |
| `05-factorial.top` | calls use contracts, so recursion works; `decreases n` makes it total correctness; the contract (`result >= n`) is weak | all proved |
| `06-division.top` | run-time checks become obligations; TOP's division truncates toward zero, so `half(-1)` is 0 | `half` fails at `x = -1`; `main` fails the precondition at `k = 0` |
| `07-error.top` | `error` is an obligation that it cannot run; a precondition makes it unreachable | `lookup` proved; `main` fails at `i = -1` |
| `08-assume.top` | `assume` is trusted: `assume x > x` proves `result == 42` | "proved", vacuously |
| `09-overflow.top` | mathematical integers are not 64-bit integers: `result > x` is proved, yet `./09-overflow 9223372036854775807` prints 0 for `y > x` | proved, and wrong at run time |
| `10-many-ifs.top` | 16 consecutive `if`s: the wp query is 9.3 MB, the passive one 7.5 KB | proved in both modes |
| `11-factorial-variants.top` | a spec function `factorial` states exactly what is computed; five implementations (recursion, a loop up, a loop down, tail recursion with an accumulator, and a loop that multiplies by repeated addition) are each proved equal to it, with termination | all 34 proved |
| `12-termination.top` | a recursive call with no measure, a measure that does not decrease, a loop measure that increases; and `bad(n) = bad(n) + 1`, a spec function whose definition is false | each fails with a counterexample; `bad`'s definition is not used, so `liar` fails; with `--trust-specs` the false definition is used and `liar` "proves" `result == 42` |
| `13-factorial-total.top` | total correctness in one function: `requires n >= 0`, `ensures result == factorial(n)`, `decreases n`; the same body without the precondition | `fact` proved; `factAny` fails termination at `n = -1` |
| `14-tree-height.top` | the `tree-borrow` selftest with a contract: a borrowed tree is a value, `case` is constructor tests, `decreases rank(*p)` measures the tree; `main`'s `error` checks are proved unreachable | all proved |
| `15-expr-borrow.top` | the `borrow-tree-reborrow` selftest with contracts: `copy` ensures `result == *e`; `main` lends `f` three times and proves it unchanged with no frame condition | all 47 proved |
| `16-swap.top` | in-out borrows: `swap` ensures `*p == old(*q)`; `bump` reads `q` without writing it; `swap(&a, &a)` | proved, except "borrowed arguments do not overlap" at `swap(&a, &a)` |
| `17-bst-insert.top` | the consuming BST `insert` from `iotests/recursive_tree.top`: one more node, and it has `v`; a loop invariant over a tree | all proved |
| `18-tree-least.top` | the least element; a fact about every subtree needs induction, so a `lemma` supplies it with `use` | `least` proved; `least0`, without the lemma, unknown |
