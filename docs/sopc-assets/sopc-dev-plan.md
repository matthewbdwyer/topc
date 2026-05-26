# sopc Development Plan

This document describes the incremental development plan for the **sopc** compiler — the
instructor's internal validation build of the **SOP** language extension on top of
**topc**.

**This repo is private and will never be public.**  It serves two purposes:

1. **Validation** — confirm that the SOP v1 student project is achievable by building
   it ourselves before releasing the assignment.
2. **Calibration** — prototype the final project extension tracks (slices, `&mut`,
   closures) to assess their difficulty and document pitfalls before students attempt them.

Companion documents (to be moved here from the topc repo once topc is complete):
- `docs/design/TOP_SOP_design_consolidation.md` — full design Q&A rationale
- `docs/topc-dev-plan.md` — Phases 0–11 record

---

## Table of Contents

1. [topc Baseline (Phases 0–11)](#1-topc-baseline-phases-011)
2. [Locked Design Decisions for SOP](#2-locked-design-decisions-for-o-sip)
3. [Phase 12 — SOP v1: Sequences and Iteration](#3-phase-12--o-sip-v1-sequences-and-iteration)
4. [Phase 13 — SOP v2: Read-Only Slices (Final Project Track 1)](#4-phase-13--o-sip-v2-read-only-slices-final-project-track-1)
5. [Phase 14 — SOP v3: Exclusive Mutable Borrows (Final Project Track 2)](#5-phase-14--o-sip-v3-exclusive-mutable-borrows-final-project-track-2)
6. [Phase 15 — Open-Ended Extension (Final Project Track 3)](#6-phase-15--open-ended-extension-final-project-track-3)
7. [Example Programs](#7-example-programs)
8. [Open Questions for SOP](#8-open-questions-for-o-sip)
9. [Test Matrix Summary](#9-test-matrix-summary)
10. [File Change Summary](#10-file-change-summary)

---

## 1. topc Baseline (Phases 0–11)

Students receive the completed **topc** compiler as their starting point for the SOP
project.  The baseline provides:

| Component | Status at Phase 11 |
|---|---|
| Grammar | Full TOP: sum types, `case`, borrow `&`, `for`/range stubs, `s[i:j]` subscript stubs |
| AST nodes | `ASTSumTypeDecl`, `ASTSumVariant`, `ASTCaseStmt`, `ASTCaseArm`, `ASTBorrowExpr`, `ASTForStmt` (stub), `ASTRangeExpr` (stub), `ASTDestroyStmt` |
| Pretty printer / visualizer | All new nodes covered; round-trip property tested |
| Weeding | `CheckBorrowPositions`, `CheckCaseCompleteness`, `CheckSumTypeNames` |
| Symbol table | Constructor resolution, for-loop variable scoping, `TypeNameCollector` |
| Type terms | `TipOwningRef`, `TipBorrowRef`, `TipSumType`, `TipSeq` (stub) |
| Schema generalization | Auto-generalize singleton non-recursive SCCs; `KPOLY` removed |
| Ownership classifier | `OwnershipClassifier`: Copy / Own for every resolved type |
| CFG subsystem | `IntraproceduralCFGs`: per-function basic-block CFG as a separate named result object in `SemanticAnalysis` |
| Move analysis | `MoveAnalysis`: forward dataflow, `use-after-move` and `double-move` errors |
| Borrow checker | `BorrowChecker`: immutable borrows restricted to immediately-passed function args in TOP v1 |
| Destruction insertion | `ASTDestroyStmt` pass; free on all owned scope-exit paths |
| Test coverage | All unit + system tests green; zero leaks under ASan |

---

## 2. Locked Design Decisions for SOP

The following decisions were confirmed during design Q&A in
`docs/design/TOP_SOP_design_consolidation.md` and are binding for Phases 12–15.

| Decision | Choice |
|---|---|
| Sequence literals | `[e1, e2, e3]` (array-style) |
| Length operator | `#s` — keep consistent with TOP; do not overload `.` |
| Element indexing | `s[i]` — returns `T` for Copy elements, `BorrowRef(T)` for Own elements |
| Subseq syntax | `s[i:j]`, `s[i:]`, `s[:j]` — uniform syntax; copying `Seq(T)` in v1, zero-copy `Slice(T)` in v2 |
| `subseq` function | TOP library global (not a compiler primitive) |
| For-loop borrow | `for (e : &s)` required; `for (e : s)` on `Seq(T)` is a type error |
| `fold` / `map` | Auto-generalized TOP globals with `&s` (borrowed) sequence parameter |
| Borrow param declarations | `&IDENTIFIER` in function definition parameter list (Phase 1 grammar, mirrors call-site syntax) |
| Single-source borrow rule | Locked for SOP v2 slice phase: a returned `Slice(T)` must derive from exactly one borrowed input |
| `&mut` | Orthogonal to slices; separate final project track (Track 2); not required for Track 1 |
| Three final project tracks | (1) read-only slices, (2) `&mut`, (3) open-ended |
| `IntraproceduralCFGs` | Separate named result object in `SemanticAnalysis`, analogous to `CallGraph` |
| `TipSeq(T)` | Always `Own`; heap-allocated `{ int length; T* data }` struct |
| Sequence destruction | Recursive: free `data` array, then the outer struct |

---

## 3. Phase 12 — SOP v1: Sequences and Iteration

**Goal:** Add owned sequences, `for` loops, range expressions, subscript/slice syntax,
and the `fold`, `map`, and `subseq` library functions as the first SOP increment.

Students build this phase on top of the topc baseline.  This is the primary student
project deliverable.

Each sub-phase is independently testable and gated on prior sub-phases being green.

---

### 12a. Activate `ASTForStmt` and `ASTRangeExpr`

The grammar rules were stubbed in topc Phase 1 and the AST nodes in Phase 2.

**Tasks:**
- Complete `ASTBuilder` visitor methods for `ASTForStmt` and `ASTRangeExpr`.
- Update `PrettyPrinter` and `ASTVisualizer` for both nodes.
- Add selftest golden file pairs for for-loops and range expressions.

**Confirmed syntax:**

```
for (e : &s) statement           // borrowed iteration over Seq(T)
for (i : lo .. hi) statement     // range loop, exclusive upper bound
for (i : lo .. hi by step) statement
```

`for (e : s)` where `s : Seq(T)` (without `&`) is a **type error** with diagnostic:
`"cannot iterate over owned Seq(T): did you mean 'for (e : &s)'?"`.

**Selftest golden files to add:**

| Source file | Construct |
|---|---|
| `for-range.top` | Range loop `for (i : 1 .. 5)` |
| `for-range-by.top` | Step range `for (i : 0 .. 10 by 2)` |

---

### 12b. Sequence type and owned sequence allocation

**New type term:** `TipSeq(T)` fully wired (stubbed in topc Phase 6).

**Grammar additions** (if not already in Phase 1 stub):

```
seqLiteral : '[' (expr (',' expr)*)? ']' ;
subscriptExpr : expr '[' expr ']' ;
sliceExpr : expr '[' expr? ':' expr? ']' ;
```

**Syntax and semantics in SOP v1:**

| Expression | Type in v1 | Notes |
|---|---|---|
| `[e1, e2, e3]` | `Seq(T)` | sequence literal; always Own |
| `#s` | `int` | length |
| `s[i]` | `T` (Copy) or `BorrowRef(T)` (Own) | element access |
| `s[i:j]` | `Seq(T)` | copy subseq; same as `subseq(&s, i, j)` |
| `s[i:]` | `Seq(T)` | copy from i to end |
| `s[:j]` | `Seq(T)` | copy from start to j |

**Code generation:**
- `Seq(T)` → heap-allocated `{ i64 length; T* data }` struct.
- Sequence literal `[e1, ..., en]` → `malloc(sizeof(SeqHeader) + n * sizeof(T))`, fill
  in `length = n`, copy each element into `data[i]`.
- `s[i]` → bounds-check then `data[i]`.
- `s[i:j]` in v1 → allocate new Seq of length `j-i`, `memcpy` slice of `data`.
- `#s` → load `s->length`.

**Ownership:**
- `TipSeq(T)` is always `Own`.
- Assignment of a sequence variable is a move.
- Sequence destruction (Phase 11 mechanism): `free(s->data); free(s)`.

**New constraints in `TypeConstraintCollectVisitor`:**
- `visitASTSeqLiteral` → elements unify to `TipSeq(α)`.
- `visitSubscriptExpr` → `[[s[i]]] = α` where `[[s]] = TipSeq(α)`.
- `visitSliceExpr` → `[[s[i:j]]] = TipSeq(α)` where `[[s]] = TipSeq(α)`.

**[OQ1]** Should `append(s, e)` (add a single element, reallocating) be a compiler builtin
or an TOP library function?  A builtin can implement amortized doubling; a library function
must always copy.  Needed by `map` implementation.

**[OQ2]** How are sequence literals lowered: (a) stack-allocated temporary, then
`memcpy` to heap; (b) directly heap-allocated with the known count?  Option (b) is simpler
and avoids a redundant copy.

---

### 12c. For-loop lowering for sequences

`for (e : &s)` where `s : Seq(T)` lowers to:

```c
// Conceptual C lowering
for (int _i = 0; _i < s->length; _i++) {
    T* e = &s->data[_i];   // borrow: e is &T
    /* body */
}
// s remains Owned after the loop
```

For Copy element types `T`, the borrow `&T` is transparently dereferenced and `e` behaves
as a plain `T` value — no visible difference to the programmer.

`for (i : lo .. hi)` lowers to:

```c
for (i64 i = lo; i < hi; i++) { /* body */ }
```

`for (i : lo .. hi by step)` lowers to:

```c
for (i64 i = lo; i < hi; i += step) { /* body */ }
```

**Type checking for `for (e : &s)`:**
- `s` must have type `Seq(T)` and ownership state `Owned`.
- `e` is bound as `T` (Copy) or `BorrowRef(T)` (Own) inside the loop body.
- `s` must remain `Owned` after the loop (the loop does not move `s`).
- If the loop body attempts to move `s`, move analysis reports an error.

---

### 12d. `fold`, `map`, and `subseq` as TOP library functions

These are TOP global functions, written in TOP itself, auto-generalized by Phase 7's
schema generalization pass (they are eligible singleton non-recursive globals).

The `&s` borrowed parameter means the caller retains ownership of `s` after the call.

**Proposed SOP library source (to be placed in a standard library `.top` file):**

```
// subseq: allocate and copy elements [i, j) into a new Seq(T)
subseq(&s, i, j) {
  return s[i:j];
}

// fold: left fold with initial value and binary function
fold(&s, init, f) {
  var acc, e;
  acc = init;
  for (e : &s) { acc = f(acc, e); }
  return acc;
}

// map: apply f to each element, collect results into a new Seq(β)
map(&s, f) {
  var result, e;
  result = [];
  for (e : &s) { result = append(result, f(e)); }
  return result;
}
```

**Type schemas (after auto-generalization):**

```
subseq : ∀α.  &Seq(α) × int × int → Seq(α)
fold   : ∀α β.  &Seq(α) × β × (β × α → β) → β
map    : ∀α β.  &Seq(α) × (α → β) → Seq(β)
```

**[OQ3]** Should the standard library be a single `.top` file compiled and linked with every
SOP program, or should each program include only the functions it uses (as in a header)?

---

### 12e. Testing

#### Unit tests

| Test file | What is tested |
|---|---|
| `ASTForStmtTest.cpp` | For-loop and range AST construction and visitor traversal |
| `TipSeqTest.cpp` | `TipSeq(T)` type term, constraint generation, unification |
| `OwnershipClassifierTest.cpp` (extend) | `TipSeq(T)` is always Own |
| `BorrowCheckerTest.cpp` (extend) | `for (e : &s)` accepted; `for (e : s)` rejected |
| `MoveAnalysisTest.cpp` (extend) | sequence move, use-after-move, loop does not move `s` |

#### System tests (add to `test/system/`)

| Test file | Expected output | Construct |
|---|---|---|
| `sum.top` | 15 | `fold` over `[1,2,3,4,5]` |
| `double.top` | `[2,4,6]` | `map` with doubling function |
| `count-pos.top` | 3 | `fold` counting positives in `[-1,2,-3,4,5]` |
| `range-sum.top` | 5050 | range loop `1 .. 101` |
| `compose.top` | 7 | higher-order `apply` and `compose` |

#### Memory tests (ASan)

All new system tests run under AddressSanitizer:
- Sequence allocated by `map` is freed at scope exit — no leaks.
- `fold` loop does not double-free or use-after-free `s`.
- Range loop allocates no heap memory.

---

## 4. Phase 13 — SOP v2: Read-Only Slices (Final Project Track 1)

**Goal:** Upgrade `s[i:j]` from copying (`Seq(T)`) to zero-copy (`Slice(T)`); introduce
the `TipSlice(T, r)` type term and the slice-origin tracking analysis.

This phase is **Final Project Track 1**.  Students who choose this track implement it on
top of the topc baseline (or their SOP v1 build).

### Design

**New type term:** `TipSlice(T, r)` — a read-only borrowed view of a `Seq(T)` with region
parameter `r` (an internal region variable; not visible in source).

**Subtyping:** `Seq(T)` coerces to `Slice(T, r)` at any point where a borrowed view is
taken.  The region `r` is a fresh internal variable tied to the lifetime of the owner.

**Syntax:** no surface change.  `s[i:j]` now has type `Slice(T, r)` (zero-copy) instead of
`Seq(T)` (copying).  The upgrade is transparent to SOP v1 programs that did not store or
return subseq results.

**Single-source borrow rule** (locked):
- A function may return a `Slice(T, r)` only when the slice is derived from exactly one
  borrowed input parameter.
- Enforced by a new slice-origin tracking analysis (see below).
- `head`, `tail`, `take`, `drop`, any windowing `s[i:i+k]` — all work zero-copy.
- `pick(b, &s1, &s2)` — returning one of two slices conditionally — requires copying:
  `b ? s1[0:#s1] : s2[0:#s2]`.

### New components

| Component | Class / File | Description |
|---|---|---|
| Slice type term | `TipSlice.{h,cpp}` | `TipSlice(T, r)` with region parameter |
| Region variables | extend `TipVar` or add `TipRegion` | internal region variable type |
| Subtyping coercion | extend `TypeConstraintCollectVisitor` | `Seq(T)` coerces to `Slice(T, r)` at borrow points |
| Slice-origin tracking | `SliceOriginAnalysis.{h,cpp}` | maps each `Slice` value to the `Seq` owner it views |
| Region solver | `RegionSolver.{h,cpp}` | topological ordering / outlives check for region constraints |
| `BorrowChecker` extension | extend Phase 10 class | enforce slice-origin single-source rule |

### Grammar change

`s[i:j]` changes meaning but not syntax.  The type checker distinguishes v1 from v2 behavior
based on whether `TipSlice` is available.  No grammar change required.

### Testing

| Test file | Expected outcome |
|---|---|
| `first-half.top` | `s[0:#s/2]` is a zero-copy `Slice(int)`, sum correct |
| `sliding-max.top` | sliding windows of width 3, max of each |
| `prefix-sum.top` | prefix sums using indexed slice access |
| `rejectPickSlice.top` | `pick(b, s1[0:3], s2[0:3])` — single-source rule violation |
| `rejectSliceEscapes.top` | returning a slice of a local Seq — borrow escapes owner |

Memory tests: slices hold no ownership; the owner `Seq` is freed once at scope exit; no
double-free, no use-after-free.

---

## 5. Phase 14 — SOP v3: Exclusive Mutable Borrows (Final Project Track 2)

**Goal:** Add `&mut` for in-place mutation of sequences and owned values without
transferring ownership.

This phase is **Final Project Track 2**.  It is **orthogonal to Track 1** — a student can
implement Track 2 without having done Track 1, and vice versa.

### Design

**New type term:** `TipMutBorrowRef(T)` — an exclusive mutable borrow.

**Syntax:**
- Call site: `sort(&mut a)` — passes an exclusive mutable borrow of `a`.
- Definition site: `sort(&mut s) { ... }` — declares `s : MutBorrowRef(T)`.
- New keyword: `KAMUT : 'mut' ;`.

**Exclusivity invariant:**
- At most one `&mut` borrow of a variable may be live at a time.
- No `&` (immutable) borrow may coexist with a live `&mut` borrow of the same variable.

**Borrow checker extension:**
The Phase 10 `BorrowChecker` tracks two borrow modes (`Immutable`, `Mutable`) and enforces:
- `&` and `&mut` of the same variable cannot be simultaneously live.
- Two `&mut` borrows of the same variable cannot be simultaneously live.
- Owner cannot be moved while any borrow (immutable or mutable) is live.

### New components

| Component | Class / File | Description |
|---|---|---|
| Mutable borrow type | `TipMutBorrowRef.{h,cpp}` | exclusive mutable borrow |
| Grammar extension | `TOP.g4` | `KAMUT : 'mut' ;`; `&mut` in borrow expressions and parameter declarations |
| `BorrowChecker` extension | extend Phase 10 class | two-mode tracking; exclusivity enforcement |

### Testing

| Test file | Expected outcome |
|---|---|
| `sort.top` | in-place sort via `&mut`; caller retains ownership after call |
| `increment-all.top` | `for (e : &mut s)` mutates each element in place (`*e = *e + 1`) |
| `rejectDoubleAliasMut.top` | two simultaneous `&mut` borrows → error |
| `rejectMutAndImmut.top` | concurrent `&` and `&mut` → error |
| `rejectMoveWhileMutBorrowed.top` | move owner while `&mut` live → error |

**[OQ4]** Should `for (e : &mut s)` (mutable iteration — `e` is `&mut T`) be part of Track 2
or deferred to Track 3?  It requires the loop head to introduce a mutable borrow and the
borrow checker to verify no aliasing occurs across iterations.

---

## 6. Phase 15 — Open-Ended Extension (Final Project Track 3)

This track is open-ended.  Students propose and implement an extension of their choice,
agreed upon with the instructor.  Suggested directions:

### 15a. Closures (most complex)

Anonymous function values that capture enclosing variables.

**Complexity:**
1. New grammar: anonymous function expression (e.g., `\x -> expr` or `fun(x) { ... }`).
2. Heap-allocated environment struct; closure value is a fat pointer (fn ptr + env ptr).
3. Three-tier capture semantics: Copy capture (cheap copy), borrow capture (closure borrows
   owner), owned capture (closure moves the variable — callable once only).
4. Each closure has a unique structural type, complicating auto-generalization.
5. Local closures cannot be auto-generalized (Phase 7 restriction).

Estimated complexity: high.  Suitable for students with strong type-theory background.

### 15b. Mutable slices (combines Tracks 1 and 2)

`&mut s[i:j]` — an exclusive mutable view into a `Seq(T)`.

Requires both `TipSlice(T, r)` (Track 1) and `TipMutBorrowRef(T)` (Track 2).

Estimated complexity: medium-high (builds on both prior tracks).

### 15c. Additional combinators and lazy evaluation

`filter`, `zip`, `enumerate`, `scan`, `flatten` — all as auto-generalized TOP globals.

Optional: lazy/demand-driven evaluation via a `LazySeq(T)` type that defers computation.

Estimated complexity: medium.

### 15d. Student proposal

Any well-scoped extension agreed with the instructor.  Must include:
- A written design document (two pages minimum).
- A test plan with at least five non-trivial test programs.
- A complexity estimate relative to Track 1 or 2.

---

## 7. Example Programs

The `docs/examples/` directory contains representative SOP programs that serve as:
1. Design validation for the SOP v1 feature set.
2. System test corpus for Phase 12.
3. Upgrade path demonstration for the slice final project.

### SOP v1 (no slices)

**`sum.top`** — sum a sequence of integers via `fold`:
```
add(x, y) { return x + y; }

main() {
  var s;
  s = [1, 2, 3, 4, 5];
  return fold(&s, 0, add);
}
// Expected output: 15
```

**`double.top`** — `map` a sequence to double each element:
```
times2(x) { return x * 2; }

main() {
  var s, r;
  s = [1, 2, 3];
  r = map(&s, times2);
  output #r;
}
// Expected: 3 (length), elements [2, 4, 6]
```

**`count-pos.top`** — count positive elements via `fold`:
```
addIfPos(acc, x) {
  if (x > 0) return acc + 1;
  else return acc;
}

main() {
  var s;
  s = [-1, 2, -3, 4, 5];
  return fold(&s, 0, addIfPos);
}
// Expected output: 3
```

**`range-sum.top`** — sum integers 1..100 via range loop:
```
main() {
  var s, i;
  s = 0;
  for (i : 1 .. 101) { s = s + i; }
  return s;
}
// Expected output: 5050
```

**`compose.top`** — higher-order `apply` and `compose` using named globals:
```
apply(f, x)    { return f(x); }
compose(f, g, x) { return f(g(x)); }
inc(x)    { return x + 1; }
double(x) { return x * 2; }

main() {
  return compose(inc, double, 3);
}
// Expected output: 7  (double(3) = 6, inc(6) = 7)
```

### SOP v2 (with slices — final project Track 1 validation)

**`first-half.top`** — zero-copy first half of a sequence:
```
sumSeq(&s) { return fold(&s, 0, add); }

main() {
  var s, h;
  s = [1, 2, 3, 4, 5, 6];
  h = s[0:#s/2];           // Slice(int) in v2, zero-copy
  return sumSeq(&h);
}
// Expected output: 6  (1+2+3)
```

**`sliding-max.top`** — maximum over each sliding window of width 3:
```
max(a, b) { if (a > b) return a; else return b; }
windowMax(&s, i) { return fold(&s[i:i+3], s[i], max); }

main() {
  var s, i, m;
  s = [1, 5, 2, 8, 3, 7];
  for (i : 0 .. #s - 2) {
    m = windowMax(&s, i);
    output m;
  }
  return 0;
}
// Expected output: 5 8 8 8
```

**`prefix-sum.top`** — prefix sums using indexed access into a slice:
```
prefixSums(&s) {
  var result, i;
  result = [0, 0, 0, 0, 0];   // same length as s; ideally allocSeq(#s)
  result[0] = s[0];
  for (i : 1 .. #s) {
    result[i] = result[i-1] + s[i];
  }
  return result;
}

main() {
  var s, ps;
  s = [1, 2, 3, 4, 5];
  ps = prefixSums(&s);
  return ps[#ps - 1];
}
// Expected output: 15
```

---

## 8. Open Questions for SOP

| # | Phase | Question |
|---|---|---|
| OQ1 | 12 | Should `append(s, e)` be a compiler builtin (amortized realloc) or an TOP library function (always copies)? |
| OQ2 | 12 | How are sequence literals `[e1, e2, e3]` lowered: stack-then-copy-to-heap, or directly heap-allocated? |
| OQ3 | 12 | Should the SOP standard library (`fold`, `map`, `subseq`) be a single `.top` file linked with every program, or included per-use? |
| OQ4 | 14 | Should `for (e : &mut s)` (mutable element iteration) be in Track 2 or deferred to Track 3? |
| OQ5 | 13 | Should the region parameter `r` in `TipSlice(T, r)` be named in error messages (e.g., `r1 does not outlive r2`) or translated to source variable names only? |
| OQ6 | 12 | What is the semantics of `result[i] = e` for a sequence variable — does assigning to `result[i]` require a mutation borrow, or is element-write permitted on an owned `Seq(T)` without `&mut`? |
| OQ7 | 12 | How should the sequence length `#s` interact with `Slice(T)` in v2 — does `#slice` return the slice length or the owner length? |

---

## 9. Test Matrix Summary

| Phase | New unit test files | New system test programs | Property enforced |
|---|---|---|---|
| 12a | `ASTForStmtTest.cpp` (complete) | `for-range.top`, `for-range-by.top` | for/range AST; `for (e : s)` is type error |
| 12b | `TipSeqTest.cpp` | — | `Seq(T)` type term; literal codegen; `#s`, `s[i]`, `s[i:j]` |
| 12c | `BorrowCheckerTest.cpp` (extend) | — | `for (e : &s)` accepted; sequence live after loop |
| 12d | — | `sum.top`, `double.top`, `count-pos.top`, `range-sum.top`, `compose.top` | fold/map/subseq correct; no leaks |
| 13 | `SliceOriginAnalysisTest.cpp`, `RegionSolverTest.cpp` | `first-half.top`, `sliding-max.top`, `prefix-sum.top`, `rejectPickSlice.top` | zero-copy slices; single-source rule; owner freed once |
| 14 | `BorrowCheckerMutTest.cpp` | `sort.top`, `increment-all.top`, `rejectDoubleAliasMut.top`, `rejectMutAndImmut.top` | exclusivity invariant; `&mut` codegen correct |
| 15 | per-track | per-track | per-track |

---

## 10. File Change Summary

### Phase 12

| Sub-phase | New source files | Modified source files |
|---|---|---|
| 12a | — | `ASTBuilder.{h,cpp}`, `PrettyPrinter.{h,cpp}`, `ASTVisualizer.{h,cpp}` |
| 12b | `TipSeq.{h,cpp}` (complete) | `TypeConstraintCollectVisitor.{h,cpp}`, `OwnershipClassifier.{h,cpp}`, `CodeGenerator.cpp`, `CodeGenFunctions.cpp` |
| 12c | — | `CodeGenerator.cpp`, `BorrowChecker.{h,cpp}`, `MoveAnalysis.{h,cpp}` |
| 12d | `osip_stdlib.top` (library source) | — |

### Phase 13

| Component | New source files | Modified source files |
|---|---|---|
| Slice type | `TipSlice.{h,cpp}` | `TypeConstraintCollectVisitor.{h,cpp}` |
| Region variables | `TipRegion.{h,cpp}` (or extend `TipVar`) | `TipTypeVisitor.h` |
| Slice-origin analysis | `SliceOriginAnalysis.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| Region solver | `RegionSolver.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| Borrow checker extension | — | `BorrowChecker.{h,cpp}` |
| Codegen | — | `CodeGenerator.cpp` |

### Phase 14

| Component | New source files | Modified source files |
|---|---|---|
| Mutable borrow type | `TipMutBorrowRef.{h,cpp}` | `TypeConstraintCollectVisitor.{h,cpp}` |
| Grammar | — | `TOP.g4` (add `KAMUT`, `&mut` forms) |
| Borrow checker extension | — | `BorrowChecker.{h,cpp}` |
| Ownership classifier extension | — | `OwnershipClassifier.{h,cpp}` |
| Codegen | — | `CodeGenerator.cpp` |
