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
  proved  line 20: precondition of sum
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
| Termination | `decreases E` on a function or a loop; `E` is an int |
| Size of a sum value | `rank(E)`, an int, in specifications only: `decreases rank(t)` |
| Entry value | `old(E)` in `ensures`: `*p == old(*q)` |
| Lemma | `lemma f(x) requires ... ensures ... decreases ... { ... }`, applied with `use f(args);` |

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
- `--verify` runs the full semantic analysis first, then the VC generator, and
  exits. Without `--verify`, the program compiles and runs as before. The
  verifier reads the inferred types to give each variable its SMT sort.

Size: about 350 changed lines in existing files, plus `src/verify/VCGen.cpp`
(about 2,200 lines: terms and their two printers, sorts, lowering, both VC
modes, spec functions, termination, the proof outline, and counterexample
traces), `ASTProofStmt.h`, and `ASTOldExpr.h`.

## The pipeline (`src/verify/VCGen.cpp`), and a view of each stage

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
| `--psmt[=line]` | the SMT-LIB queries exactly as sent to Z3, or only those for one line |
| `--pspec` | each spec function's definition, its axiom, and its termination obligations |
| `--pcex` | a trace for each failure: entry values, each assignment with its value, each branch taken, and the failing assertion with the values substituted |
| `--vfn=f` | only function `f` (or spec function `f`) |

Any of these implies `--verify`. Integers are mathematical (`Int`); a TOP
condition is true when it is not zero.

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
  recursive call with no measure is a failed obligation, so every proof is of
  total correctness. Mutual recursion is not checked.
- **Termination comes first for spec functions.** A definition that does not
  terminate can be false: `bad(n) = bad(n) + 1` has no solution, and from a
  false axiom Z3 proves anything. So each spec function's recursive calls are
  checked with no definitions in scope, and only the definitions of those
  that terminate are given to Z3. `--trust-specs` uses them anyway, to show
  what goes wrong.
- **One Z3 process per query.** Run in one process with push/pop, state from
  earlier queries made Z3 return unknown on later ones.

## Two VC generators: substitution and versioning

`--vcmode=wp` is the substitution form above. `--vcmode=passive`, the
default, follows Flanagan and Saxe ("Avoiding exponential explosion", POPL 2001), as
Boogie does. Both start from the same guarded commands.

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
  at least 1 needs induction, so the axiom states it; it is true, and
  trusted.
- **Induction is the programmer's job.** Z3 does not do induction, so a fact
  about every subtree needs a lemma: a recursive function, verified like any
  other, whose recursion is the induction. `18-tree-least` needs one; without
  it Z3 answers unknown.

## What is not there yet

- **Quantifiers in specifications.** `insert` can promise that the new tree
  has `v`, but not that it keeps every old value: that needs
  `forall x :: has(t, x) ==> has(result, x)`.
- **Polymorphic sum types.** A sum type gets one SMT datatype, so one used
  at two payload types is not supported.
- **Overflow.** Mathematical integers are unsound for 64-bit TOP. Verus adds
  an obligation that each `+ - *` stays in range; that is a few lines here.
- **Calls through function values** need contracts on function types; only
  direct calls by name are supported.
- **Mutual recursion** is not checked for termination.

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
