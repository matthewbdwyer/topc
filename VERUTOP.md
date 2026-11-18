# VERUTOP: verifying TOP programs (throwaway prototype, branch `verutop`)

`topc --verify prog.top` checks a TOP program against its contracts. It
lowers each function to guarded commands, computes a verification condition
from them, and asks Z3, one proof obligation at a time, whether each
obligation can fail.

```
sum(n)
  requires n >= 0
  ensures result == n * (n + 1) / 2
{
  var i, s;
  i = 0;
  s = 0;
  while (i < n)
    invariant 0 <= i && i <= n
    invariant s == i * (i + 1) / 2
  {
    i = i + 1;
    s = s + i;
  }
  return s;
}

main() { var t; t = sum(10); assert t == 55; return t; }
```
```
sum:
  proved  line 8: loop invariant holds on entry
  proved  line 8: loop invariant is preserved
  proved  line 15: postcondition
main:
  proved  line 20: precondition of sum(10)
  proved  line 21: assertion
  proved  line 22: postcondition
```
A failure reports a counterexample for the formals:
`FAILED line 2: divisor is not zero when ((|a| 0) (|b| 0))`. The queries are
written to `prog.top.vc.smt2`, which is readable SMT-LIB.

## The language extension

| Construct | Syntax |
|---|---|
| Contract, between `)` and `{` | `requires E`, `ensures E` (repeatable); `result` is the returned value; in `ensures`, a formal means its value on entry |
| Loop invariant | `while (E) invariant E ... S` |
| Proof statements | `assert E;` `assume E;` |
| Specification operators | `!` `&&` `\|\|` `==>` `<` `<=` `>=` |
| Spec function | `spec f(x) decreases E { ... return E; }` |
| Termination | `decreases E` on a function or a loop; `E` is an int. Every loop needs one. `decreases *` says instead that the loop or function may not terminate (code only) |
| Size of a sum value | `rank(E)`, an int, in specifications only: `decreases rank(t)` |
| Entry value | `old(E)` in `ensures`: `*p == old(*q)` |
| Lemma | `lemma f(x) requires ... ensures ... decreases ... { ... }`, applied with `use f(args);` |

The extension reserves eleven words that were identifiers in TOP:
`requires`, `ensures`, `invariant`, `decreases`, `assert`, `assume`,
`spec`, `lemma`, `use`, `old`, `rank`. A program that names a function or
variable with one of them no longer parses.

`!E` is built as `E == 0`. The other operators are new binary operators.
`<`, `<=`, `>=`, `&&`, and `\|\|` also work in code, where the connectives do
not short-circuit.

## How specifications fit the compiler: checked, then erased

- Contracts and invariants are children of `ASTFunction` and `ASTWhileStmt`.
  `assert` and `assume` are an `ASTProofStmt`, a subclass of `ASTBlockStmt`
  with no statements.
- `ASTVisitor::visitSpecs()` (default false) decides whether a traversal
  enters specifications. Only the local-name collector and the type-constraint
  visitor say yes. So a contract is symbol-checked and type-checked
  (`ensures result == y` with no `y` is a symbol error; `ensures result ==
  alloc 1` on an int function is a type error), and `result` gets the return
  type through one new constraint.
- Every other phase never sees a specification. To ownership, borrows,
  release, the CFG, and code generation, an `assert` is an empty block. This
  is erasure, as in Verus, and it is also why a mention of an owned variable
  in a contract is not a move.
- `old(E)` and `rank(E)` are `ASTOldExpr` and `ASTRankExpr`, which appear
  only in specifications (`rank` in code is a semantic error); `use`
  is a third kind of `ASTProofStmt`. A lemma, like a spec function, is
  skipped by code generation, and code that mentions one is a semantic error.
- **Specifications are pure, so the ownership rules do not apply to them.**
  The ownership passes skip spec functions and lemmas (they never run), and
  never see contracts or proof statements. A specification may use a value
  twice (`size(t) + size(t)`), or mention a variable the code has moved: it
  still denotes the value it held. This is Verus's rule: spec code is not
  borrow-checked. A lemma may not write through a borrow, because its `use`
  is erased and the program would never make the change.
- The code the specifications describe must pass every ownership rule: the
  verifier's model (a reference is its referent, a borrow an in-out
  parameter) is sound only for code that does.
- `--verify` runs the full semantic analysis first, then the VC generator, and
  exits. Without `--verify`, the program compiles and runs as before. The
  verifier reads the inferred types to give each variable its SMT sort.

Size: about 350 changed lines in existing files, plus `src/verify/VCGen.cpp`
(about 2,200 lines: terms and their two printers, sorts, lowering, both VC
modes, spec functions, termination, the proof outline, and counterexample
traces), `ASTProofStmt.h`, and `ASTOldExpr.h`.

## The pipeline (`src/verify/`), and a view of each stage

```
AST --lower--> guarded commands --passivize--> passive commands --> VC --> SMT queries --> Z3
               --pgcl                          --ppassive           --pvc   --psmt         --pcex
                                                                    --pwp (proof outline)
```

| Flag | Prints |
|---|---|
| `--pgcl` | each function as guarded commands |
| `--ppassive` | the passive form: every variable versioned, merges at joins |
| `--pwp` | a proof outline: the weakest precondition before every command, substituted and simplified (a formula over 4000 nodes is shown as a placeholder) |
| `--pvc` | the function's VC in infix, every obligation checked, in the selected `--vcmode` |
| `--psmt[=line]` | the SMT-LIB queries sent to Z3, or only those for one line |
| `--pspec` | each spec function's definition, its axiom, and its termination obligations |
| `--pcex` | a trace for each failure: entry values, each assignment with its value, each branch taken, and the failing assertion with the values substituted |
| `--ptrust` | what each proof uses, why a vacuous proof is vacuous, a verdict for each function, and what is always trusted; with `--psmt`, also the queries that ask |
| `--vfn=f` | only function `f` (or spec function `f`) |

Any of these implies `--verify`. A TOP condition is true when it is not
zero. Integers are mathematical (`Int`) unless `--overflow` is given, which
adds the obligation that each `+ - * /` in code stays in 64 bits.
Queries are independent of each other, so they are sent to several Z3
processes at once: `--vjobs=N` (default: one per core, at most 4;
`--vjobs=1` runs them one at a time). The report is the same either way.
`--vrlimit=N` sets Z3's resource limit for a query (default 20000000): a
count of Z3's own steps, not seconds, so a verdict depends much less on
machine load than a timeout does. It is not perfectly repeatable: for
nonlinear arithmetic Z3's count varies from run to run (`factDown`'s
invariant in demo 11, in wp mode, between 4.7 and 33 million), so that
obligation is occasionally `UNKNOWN` on a busy machine. When an `UNKNOWN`
is caused by the limit, and not by a question Z3 cannot decide, a line
after the verdicts names the obligation and the flag:

```
Z3 reached its resource limit (20000000) on: least0 line 90. A larger limit may decide it: --vrlimit=N.
```

"May": a larger limit settles `factDown`, but not `least0` in demo 18,
which needs a lemma. (To tell the two causes apart, each query sent to Z3
also has `(get-info :reason-unknown)` after its `(check-sat)`; `--psmt` and
the `.vc.smt2` file leave that line out.)

### Guarded commands

Five commands, plus sequencing: `assume P`, `assert P`, `x := E`,
`havoc x`, and a choice `S1 [] S2`. Every supported TOP construct lowers to
them, and both VC generators work on them, not on the AST.

| TOP | Guarded commands |
|---|---|
| function | `assume requires; locals := 0; body; result := E; assert ensures` |
| `x = E;` | `x := E` |
| `x = E / D` | `assert D != 0` before the assignment |
| `input` | `havoc input#k`, a fresh value |
| `if (C) S1 else S2` | `{ assume C; S1 } [] { assume !C; S2 }` |
| `while (C) invariant J decreases D S` | `assert J; havoc x...; assume J; { assume C; m := D; S; assert J; assert 0 <= m && D < m; assume false } [] { assume !C }`, where `x...` are the variables `S` assigns |
| `x = f(A);` | `assert pre_f[A]; havoc x; assume post_f[A, x]` (and, if recursive, `assert` the measure decreases); a call inside an expression gets a fresh result variable `f#k` |
| `f(&x)`, where `f` writes its parameter | also `havoc x'; ...; x := x'`, with `post_f` relating `x'` (the final `*p`) to `x` (`old(*p)`) |
| `alloc E`, `*E`, `&E` | `E`: a reference is represented by its referent |
| `*x = E;` | `x := E` |
| `case E of { C(a, b) -> S; ... }` | `{ assume E is C; a := E.f1; b := E.f2; S } [] { assume !(E is C); ... }`; nested patterns add tests |
| `assert P;` `assume P;` | the same |
| `use L(A);` | the call's contract alone: `assert pre_L[A]; assume post_L[A]` |
| `error E;` | `assert false; assume false` |

In `ensures`, a formal that the body assigns means its entry value, `old(x)`.

`abs` from `--pgcl`:
```
  r := 0;                                     // locals start at 0
  {
    assume x > 0;                             // then
    r := x;
  } [] {
    assume !(x > 0);                          // else
    r := 0 - x;
  }
  result := r;                                // return
  assert result >= 0 && (result == x || result == 0 - x);  // #0 line 13: postcondition
```

### Weakest preconditions over guarded commands

| Command | wp(S, Q) |
|---|---|
| `assume P` | `P ==> Q` |
| `assert P` | `P && Q` (when it is the obligation checked; otherwise `P ==> Q`) |
| `x := E` | `Q[E/x]`, sent to Z3 as `(let ((x E)) Q)` |
| `havoc x` | `forall x :: Q` |
| `S1; S2` | `wp(S1, wp(S2, Q))` |
| `S1 [] S2` | `wp(S1, Q) && wp(S2, Q)` |

The function's VC is `wp(body, true)`, which must be valid. `--pwp` shows
each intermediate `wp`:
```
  { i + 1 <= x &&
    r == (i + 1) * y &&
    0 <= measure#0 &&
    x - (i + 1) < measure#0 }
  i := i + 1;
  { i <= x && r == i * y && 0 <= measure#0 && x - i < measure#0 }
  assert i <= x && r == i * y;                // #1 line 11: loop invariant is preserved
```

- **TOP's single return makes this easy.** The body is straight-line
  structured code that ends in its only `return`. There are no early exits, so
  there is no need for the continuation machinery that `break` and early
  `return` would require.
- **Calls are modular.** A call is checked against the callee's contract, not
  its body, so recursion works; `decreases` makes it total correctness.
- **One obligation per query.** Each query checks one obligation and assumes
  all the others, so every failure points to a line. The checked obligation
  becomes `P ∧ Q`; each assumed one becomes `P ⇒ Q`.
- **Division matches C.** SMT-LIB's `div` is Euclidean; the prelude defines
  `tdiv`, which truncates toward zero as the generated code does. This is a
  small, concrete case of "the verifier must agree with the code generator".

## Spec functions, termination, and triggers

```
spec factorial(n)
  decreases n
{
  var r;
  if (n <= 0) { r = 1; } else { r = n * factorial(n - 1); }
  return r;
}

factUp(n)
  requires n >= 0
  ensures result == factorial(n)
{
  ...
  while (i < n)
    invariant 0 <= i && i <= n
    invariant r == factorial(i)
    decreases n - i
  { i = i + 1; r = r * i; }
  return r;
}
```

- **Spec functions** (`spec f(...) {...}`) are pure TOP functions used only in
  specifications: no loops, input, output, error, heap, or calls to ordinary
  functions. They are type-checked like any function and erased: code
  generation skips them, and code that mentions one is a semantic error.
- **Translation.** With each `if` merged into an `ite`, a spec function's body
  is one term. It becomes an uninterpreted function and a defining axiom
  `∀n. factorial(n) = (ite (<= n 0) 1 (* n (factorial (- n 1))))` with the
  trigger `factorial(n)`: Z3 unfolds the definition once for each
  `factorial(t)` term in the proof. Z3's own `define-funs-rec` left three of
  the factorial obligations unknown; the triggered axiom (and Dafny's one-step
  "fuel" encoding) prove all of them.
- **`decreases`** on a function: each direct recursive call must make the
  measure smaller and keep it non-negative. On a loop: the measure at the end
  of the body must be smaller than at its start, and not negative. A
  recursive call with no measure is a failed obligation, and so is a loop
  with none, so every proof is of total correctness. `decreases *` on a
  loop or a function opts out: it may not terminate, the report lists it,
  and partial correctness is still checked. A spec function or a lemma may
  not opt out. Termination has to hold across calls, so a function without
  `decreases *` may neither contain a `decreases *` loop nor call a
  `decreases *` function (Dafny's rule); otherwise a lemma could call a
  function that never returns and take `ensures false` from it.
- **Recursion through several functions.** A call from `f` to `g` on a
  cycle of calls must make `g`'s measure, at the call, smaller than `f`'s
  was on entry, and not negative. This holds for spec functions, lemmas,
  and code alike (`19-mutual-recursion`). Without it, `f(n) = g(n) + 1`
  and `g(n) = f(n)` would be used as axioms and prove anything.
- **Termination comes first for spec functions.** A definition that does not
  terminate can be false: `bad(n) = bad(n) + 1` has no solution, and from a
  false axiom Z3 proves anything. So each spec function's recursive calls are
  checked with no definitions in scope, and only the definitions of those
  that terminate are given to Z3. `--trust-specs` uses them anyway, to show
  what goes wrong.
- **One Z3 process per query.** Run in one process with push/pop, state from
  earlier queries made Z3 return unknown on later ones.

## Two VC generators: substitution and versioning

The VC is always a weakest precondition. `--vcmode` chooses how it is
computed: `subst`, by substitution over the guarded commands as above
(`wp` is an older name for this mode and still works), or `passive`, the
default, which first makes the program passive and then needs no
substitution. The second follows Flanagan and Saxe ("Avoiding exponential
explosion", POPL 2001), as Boogie and Verus do. Both start from the same
guarded commands, and the two formulas are equivalent: for a passive
command `S`,

```
wp(S, Q)  =  ok(S) ∧ (post(S) ⇒ Q)
```

where `ok` and `post` are defined in step 4 below, and the passive VC is
`ok(body)`, that is, `wp(body, true)`.

The difference is where `Q`, the rest of the function, is written. By
substitution it is copied under both branches of every choice, and each
copy is rewritten by that branch's assignments: `wp(S1, Q) ∧ wp(S2, Q)`.
In a passive program nothing is assigned, so `Q` is the same formula
under both branches and can be written once, after the join, by

```
(A ⇒ Q) ∧ (B ⇒ Q)  =  (A ∨ B) ⇒ Q
```

where `A` and `B` are what the two branches establish (their `post`).
That is why versioning comes first: with `r := x` in one branch and
`r := 0 - x` in the other, the two copies of `Q` are different formulas
and there is nothing common to write once.

1. **Version every variable.** `x := e` becomes `assume x@k == e` for a fresh
   constant `x@k`; the program now assigns nothing (it is passive).
2. **Havoc is a new version** with nothing assumed about it. This is how the
   loop cut forgets the assigned variables; unassigned variables keep their
   versions, so facts about them survive.
3. **Merge at joins.** At the end of a choice, each branch assumes the merged
   version equals its own: `r@3 == r@1` in one branch, `r@3 == r@2` in the
   other. A branch that ends in `assume false` has nothing to merge.
4. **VC of a passive program.** For each command compute `ok` (its assertions
   hold) and `post` (what it establishes). A sequence's `ok` is
   `ok1 ∧ (post1 ⇒ ok2)`; a choice's `post` is `post1 ∨ post2`. Each `post` is
   named with a `define-fun`, so it is written once and referenced, never
   copied.

The `abs` query in passive form:
```
(define-fun |N!0| () Bool (and (> |x@0| 0) (= |r@0| |x@0|)))
(define-fun |N!1| () Bool (and |N!0| (= |r@2| |r@0|)))
(define-fun |N!2| () Bool (and (not (> |x@0| 0)) (= |r@1| (- 0 |x@0|))))
(define-fun |N!3| () Bool (and |N!2| (= |r@2| |r@1|)))
(define-fun |N!4| () Bool (or |N!1| |N!3|))
(assert (not (=> |N!4| (and (>= |r@2| 0) (or (= |r@2| |x@0|) (= |r@2| (- 0 |x@0|)))))))
```

`--ppassive` shows steps 1 to 3, and `--pvc` shows step 4. Both modes give
the same verdicts on every example. They differ in size. A
function with n consecutive `if (y > i) { y = y - 1; } else { y = y + 1; }`
statements, `ensures result >= 0`:

| n | wp bytes | wp seconds | passive bytes | passive seconds |
|---:|---:|---:|---:|---:|
| 4 | 2,567 | 0.05 | 2,224 | 0.05 |
| 8 | 36,167 | 0.05 | 3,992 | 0.06 |
| 12 | 580,935 | 0.07 | 5,766 | 0.06 |
| 16 | 9,305,415 | 0.46 | 7,542 | 0.06 |
| 20 | 148,897,095 | 8.33 | 9,362 | 0.07 |

Substitution copies the rest of the function into both branches of every
`if`, so the VC grows 16 times for every 4 `if`s; versioning grows by about
440 bytes per `if`. (The first passive measurements, before removing
`(and true ...)` noise, were about twice these sizes.)

## Heap data: what ownership buys

Dafny and Frama-C model memory as a map from addresses to values. Every
write updates the map, every pair of pointers might be equal, and every call
needs a frame condition (`modifies`) to say what it leaves alone. TOP's
ownership rules remove all three, so VERUTOP has no heap model:

- **A reference is its referent.** An owned box has one owner, so a variable
  of type `own&T` is represented by the `T` value it holds: `alloc E`, `*E`,
  and `&E` translate to `E`, and `*p = E` assigns `p`. No two variables share
  a box, so there is no aliasing to consider.
- **A sum value is an SMT datatype value.** Each `type` becomes a
  `declare-datatypes`; `case` becomes constructor tests (`t is Node`) and
  selectors (`t.left`). `copy` can promise `result == *e`: equality of
  values, not of addresses.
- **A borrow is an in-out parameter.** A borrow is only ever a call argument,
  and TOP forbids writing an owned value through one, so only an int can be
  written. The callee's `ensures` gives `*p` (the final value) in terms of
  `old(*p)`; the caller's variable takes that final value.
- **No frame conditions.** A function that never writes through a borrow
  leaves the caller's variable alone, and the verifier knows this from the
  program, not from an annotation: `15-expr-borrow` lends `f` to three
  functions and proves it unchanged.
- **One gap: TOP lets two borrows of one variable reach a call.**
  `swap(&a, &a)` is accepted, and the in-out model is wrong for it. The
  verifier reports a failed obligation, "borrowed arguments do not overlap",
  which is Verus's `&mut` exclusivity rule.
- **Structural termination.** A measure is an int, so recursion on a sum
  value is measured by `rank`: `decreases rank(t)`. For every `type`, the
  verifier defines `rank` by an axiom: a value's rank is one more than the
  sum of the ranks of its fields that are sum values (an int field adds
  nothing; a boxed field counts as what it holds). So any part is smaller
  than the whole, for lists, trees, or a pair of lists alike. That a rank is
  at least 1 needs induction, so the axiom states it, and one query per
  type checks the induction step: if every sum-valued field has rank at
  least 1, so does the value.
- **Induction is the programmer's job.** Z3 does not do induction, so a fact
  about every subtree needs a lemma: a recursive function, verified like any
  other, whose recursion is the induction. `18-tree-least` needs one; without
  it Z3 answers unknown.

## What a proof rests on

A `proved` is a claim relative to assumptions. The verifier names them and
reports them.

**Every run** ends with a summary:

```
Assumed without proof:
  assume (line 36) in positive
  contract of seven (not verified), used by eight
  4 arithmetic operations in code: no overflow (--overflow checks them)
Checked in this run:
  definition of double: not recursive
```

and marks a proof `VACUOUS` when nothing reaches the obligation. After
`assume x > x`, or after a contract or definition that is false, every
obligation is proved; so each proved obligation is asked once more, as
`false`. If that is proved too, no execution gets there. A branch the
precondition rules out is marked the same way, with the precondition as
the reason. This check gets a hundredth of the resource limit: Z3 finds a
contradiction quickly or not at all.

**`--ptrust`** adds, for each proof, the assumptions it used, taken from
Z3's unsat core: each `assume`, each callee's or lemma's postcondition, and
each definition is asserted under a name. Then a verdict for each function:

```
eight:
  line 29: postcondition
      uses contract of seven (line 28)
  => verified, assuming: contract of seven (not verified)
client:
  line 46: postcondition
      uses contract of positive (line 45)
  => verified, assuming: assume (line 36) (through positive)
guarded:
  line 58: assertion
      VACUOUS: nothing reaches line 58, given the precondition
  => verified; 1 proof is vacuous
```

The core is the one Z3's proof used, not a smallest one: minimizing a core
is a search that the resource limit does not bound.

**What is checked instead of assumed.** A spec function's definition is
used only if its termination is proved. `rank(x) >= 1` is checked by an
induction step. With `--overflow`, arithmetic in code is checked to stay in
64 bits.

**What is always trusted.**

- topc's parser, type checker, and ownership checks. The encoding (a
  reference is its referent, a borrow an in-out parameter) is sound only
  for programs that pass them.
- The translation of the program to verification conditions.
- Z3.
- Erasure, code generation, LLVM, and the runtime: the source is verified,
  not the binary.
- The model of the world: `input` is any integer; output is not modelled.
- Structural induction over sum types, and that a terminating recursive
  definition defines a function.

## Architecture

The verifier is a library, `src/verify/`, in namespace `verutop`, built
like the rest of the compiler: each IR has one job and a printer, each
pass is a function from one IR to the next, and the back end targets a
solver instead of a machine.

```
AST, types, ownership                       the front end
  │  Lowering  (with Encoding and Specs)
guarded commands        Gcl                 --pgcl
  │  Passivize
passive commands        Gcl                 --ppassive
  │  VcStrategy: SubstVc | PassiveVc
a formula               Term                --pvc, --pwp
  │  SmtEncode
queries                 Query               --psmt
  │  Solver: Z3Process
answers                 Answer
  │  Feedback, Trust
results                 VerifyResult        the report, --pcex, --ptrust
```

| Module | What it is |
|---|---|
| `Term`, `TermPrint` | The logic: one representation of formulas, printed as SMT-LIB for Z3 and in infix for people. Substitution, free variables, and a simplifier used only for display. |
| `Encoding` | How a TOP value becomes a logical one: sorts, a reference as its referent, a written borrow as an in-out parameter, `case` as constructor tests, `rank`, the range of a machine integer. Its header is the statement of the heap-free encoding. |
| `Gcl` | Guarded commands: a variant of `Assume`, `Assert`, `Assign`, `Havoc`, `Seq`, `Choice`. Every traversal is a `match` with one function per kind. |
| `Obligations` | `Obligation` (a kind, a position, a subject; its message is derived) and `Assumption` (what a proof may use that the function does not establish). |
| `Termination` | The call graph's cycles, and the obligation that a call on a cycle decreases the measure. |
| `Lowering` | AST to guarded commands: `Lowering.cpp` (a function), `LowerExpr.cpp`, `LowerStmt.cpp`, `LowerCall.cpp`. |
| `Specs` | Spec functions as definitions (`Axiom`) with termination checks; the rule that code may not use them. |
| `Passivize` | Guarded commands to passive form (Flanagan and Saxe). |
| `VcStrategy` | Two interchangeable ways to compute a VC: `SubstVc` and `PassiveVc`. |
| `Outline` | The `--pwp` proof outline. |
| `Query`, `SmtEncode` | A query as data (`Query`, `Axiom`, `Background`) and its one renderer; the queries asked about an obligation: its verdict, a trace, what the proof uses, whether anything reaches it. |
| `Solver` | `Solver::check(Background, Query) -> Answer`. `Z3Process` pipes SMT-LIB to `z3 -in`; `CannedSolver` answers by rule, for tests; `checkAll` runs a batch on several processes. |
| `Feedback` | From an answer back to the program: the counterexample's arguments, and the trace. |
| `Results` | What a run found, as data: `VerifyResult`. |
| `Trust` | From the results alone: what is assumed and checked, and each function's verdict with what it rests on. |
| `Report` | Presentation only: every view, the verdicts, the trust report, the `.vc.smt2` file. |
| `Driver` | Wires the stages: `verify(program, types, solver, options) -> VerifyResult`. Prints nothing. |

Three rules keep the boundaries honest:

- **A query is never text until it is rendered.** The definitions a query
  needs are found from the function symbols in its terms. What `--psmt`
  shows is `render(background, query)`; `Z3Process` sends that, plus one
  line after `check-sat` asking why an answer is `unknown`.
- **The driver decides; it does not print or parse.** Everything it
  learns is in the `VerifyResult`, so a test can run it on a program with
  a `CannedSolver` and inspect what was asked and what was concluded.
- **Messages are derived, not matched.** An obligation's kind determines
  its wording, whether it is a missing clause, and whether it is exempt
  from the vacuity check.

To add a kind of obligation: a value of `ObKind`, its message in
`Obligations.cpp`, and the `assertion(...)` call in lowering that creates
it. To use another solver: implement `Solver`.

## Tests

`test/unit/verify/` (built as `verify_unit_tests`; ctest runs them, named
`verify.…`): unit
tests for each module, none of which runs Z3. Programs are built by hand
as guarded commands, or parsed from source; VCs are checked by evaluating
them over a small domain; the driver is run with a canned solver.

`test/system/verify/` (run `python3 test/system/verify/runner.py`; needs
`z3`): the verdicts of every demo, pinned; agreement between the two VC
generators; the text of some views; *liars*, programs with a false
contract that must not be plainly proved, one for each known way to cheat;
verified functions compiled and run with their `ensures` checked; and
mutants of the demos, which must change a verdict.
A change that lets the verifier prove more should come with a liar. See
the README there.

## What is not there yet

- **Quantifiers in specifications.** `insert` can promise that the new tree
  has `v`, but not that it keeps every old value: that needs
  `forall x :: has(t, x) ==> has(result, x)`.
- **Polymorphic sum types.** A sum type gets one SMT datatype, so one used
  at two payload types is not supported.
- **Overflow by default.** `--overflow` checks it; without the flag,
  integers are mathematical, which is unsound for 64-bit TOP, and the
  report says how many operations were not checked. Verus checks by
  default.
- **Calls through function values** need contracts on function types; only
  direct calls by name are supported.

## A teaching sequence this supports

1. Lowering to guarded commands (`--pgcl`): five commands are enough.
2. wp of assignment as substitution; the `abs` proof outline (`--pwp`), then
   its VC (`--pvc --vcmode=wp`) and query (`--psmt`).
3. Loops need an invariant; the loop cut in `--pgcl`; a too-weak invariant
   and its trace (`--pcex`).
4. Calls: modularity; `assert t == 55` proved from `sum`'s contract alone.
5. Runtime checks become obligations: `D ≠ 0`, and `error` as unreachable.
6. Size: `10-many-ifs` with `--pwp` and `--pvc` in both modes; passive form
   (`--ppassive`) keeps it linear.
7. Why ownership matters: `14-tree-height` and `15-expr-borrow` (a borrowed
   tree is a value; no frame conditions), `16-swap` (in-out borrows, `old`,
   and the overlap the model cannot handle).
8. Induction: `18-tree-least`, with and without its lemma.
