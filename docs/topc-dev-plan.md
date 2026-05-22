# topc Development Plan

This document describes the incremental development plan for the **topc** compiler — an
extension of the **tipc** TIP compiler to support the **TOP** and **SOP** languages as
described in `docs/design/TOP_SOP_design_consolidation.md`.

Each phase is a self-contained, testable increment.  The hard gate between phases is that all
tests from all prior phases must remain green before work on the next phase begins.

Open questions that require a decision before or during a phase are marked **[Q#]** and
collected in [§13 Open Questions](#13-open-questions).

---

## Execution Mode

The AI agent works autonomously through all phases by default, committing after each phase
once all tests are green.  There are two explicit **human checkpoints** where the agent
pauses and waits for review before continuing:

- **Checkpoint A (after Phase 1):** human reviews the extended grammar (`TIP.g4`) before
  AST nodes are built on top of it.  The agent presents a diff of the grammar changes and
  waits for approval or correction.

- **Checkpoint B (after Phase 3):** human runs the pretty-printer on representative `.top`
  programs and confirms the output matches intent.  The agent provides the demo commands
  and waits for sign-off before proceeding to weeding passes.

All other phases (0, 2, 4–11) run autonomously.  The agent raises a human question only
when it encounters a genuinely unresolvable blocker (e.g., a new design conflict not covered
by the locked decisions in §13).  Minor judgment calls within the scope of a locked decision
are resolved autonomously and noted in the commit message.

---

## Table of Contents

1. [Phase 0 — Baseline audit](#phase-0--baseline-audit)
2. [Phase 1 — TOP Grammar](#phase-1--o-tip-grammar)
3. [Phase 2 — AST Nodes](#phase-2--ast-nodes)
4. [Phase 3 — Pretty Printer and Visualizer](#phase-3--pretty-printer-and-visualizer)
5. [Phase 4 — Weeding Passes](#phase-4--weeding-passes)
6. [Phase 5 — Symbol Table Extensions](#phase-5--symbol-table-extensions)
7. [Phase 6 — Type Shape Inference (Pass A)](#phase-6--type-shape-inference-pass-a)
8. [Phase 7 — Schema Generalization (Pass B)](#phase-7--schema-generalization-pass-b)
9. [Phase 8 — Ownership Classification (Pass C)](#phase-8--ownership-classification-pass-c)
10. [Phase 9 — Move/State Analysis (Pass D)](#phase-9--movestate-analysis-pass-d)
11. [Phase 10 — Borrow/Lifetime Validity (Pass E)](#phase-10--borrowlifetime-validity-pass-e)
12. [Phase 11 — Destruction Insertion (Pass F)](#phase-11--destruction-insertion-pass-f)
13. [Design Decisions (all resolved)](#13-design-decisions-all-resolved)

---

## Phase 0 — Baseline audit

**Goal:** Establish a fully green, measured baseline before any TOP changes are introduced.
This phase produces no new language features; it exists to make regressions detectable.

### Tasks

- Run `bin/runtests.sh` (unit + system) and confirm all tests pass.
- Run `bin/gencov.sh` and record the initial line/branch coverage numbers.
- Tag the repository `v0-tip-baseline`.
- **[Q1 — resolved]** Build config: `Debug` + `-fsanitize=address` for the topc compiler
  binary throughout development.  Generated-binary ASan strategy is two-level:
  - **Option A (test harness):** all system tests compile and link generated bitcode with
    `clang -fsanitize=address`.  Confirm and standardise in Phase 0.
  - **Option B (compiler flag):** add a `-asan` flag to topc that inserts
    `llvm::AddressSanitizerPass` into the optimisation pipeline.  Implement in Phase 0.

### Deliverables

- A clean `git tag v0-tip-baseline` at HEAD.
- Recorded baseline coverage numbers (unit tests, `build/src` scope, excluding ANTLR-generated code):
  - **Lines:** 77.4% (937 / 1210)
  - **Functions:** 73.9% (283 / 383)
- All system tests confirmed to link generated bitcode with `-fsanitize=address` (Option A). ✓
- topc `-asan` flag implemented and smoke-tested (Option B). ✓

---

## Phase 1 — TOP Grammar

**Goal:** Extend `src/frontend/TIP.g4` and (if needed) `tipg4/TIP.g4` to parse TOP surface
syntax.  Semantics are **not** implemented in this phase — new grammar rules are hooked to stub
AST visitor methods that emit `InternalError("not yet implemented")`.

### Background

The existing grammar already parses `&expr` as `refExpr`.  TOP repurposes `&` as a borrow
operator with different static semantics; the surface syntax is unchanged but the meaning
diverges.  **[Q2 — resolved]** `alloc` and `*` (dereference) are retained unchanged;
`alloc E` becomes the owning allocation in TOP.

### New grammar constructs

The following constructs are added in this phase.  Each is listed with the grammar rule name
and the minimal example program that must parse correctly.

#### 1a. Sum type declarations (top-level)

```
typeDecl : KTYPE IDENTIFIER '=' sumVariant ('|' sumVariant)* ';' ;
sumVariant : IDENTIFIER ('(' nameDeclaration (',' nameDeclaration)* ')')? ;
```

Example:
```
type Option = Some(x) | None;
```

**Design decision (locked):** Constructor payloads are **positional** (ML/Haskell style).
The binder names in the declaration (`x` in `Some(x)`, `a, b` in `MkPair(a, b)`) are
placeholders used in case arm patterns — they are not persistent field names.  There is no
nominal record-inside-constructor syntax.  For constructors that genuinely need named fields,
pass a TIP record as the single payload:

```
type Shape = Circle(int) | Rect({width: int, height: int});
case s of {
  Circle(r)  -> output r * r;
  Rect(dims) -> output dims.width * dims.height;
}
```

This reuses existing record syntax at no grammar cost.

**[Q3 — resolved]** Sum type declarations are **top-level only**.

#### 1b. Case / pattern-match expression

```
caseExpr : KCASE expr KOF '{' caseArm+ '}' ;
caseArm  : IDENTIFIER ('(' IDENTIFIER (',' IDENTIFIER)* ')')? ARROW statement ;
```

Example:
```
case p of {
  Some(v) -> output v;
  None    -> output 0;
}
```

**[Q4 — resolved]** `case` is a **statement only**; expression form is explicitly out of scope.

**[Q5 — resolved]** New `ARROW : '->'` token.

#### 1c. For-loop (SOP stub — grammar rule added now, AST building deferred to sopc)

```
forStmt : KFOR '(' nameDeclaration ':' expr ')' statement ;
```

#### 1d. Range expression (SOP stub — grammar rule added now, AST building deferred to sopc)

```
rangeExpr : expr DOTDOT expr (KBY expr)? ;
```

Token: `DOTDOT : '..' ;`

Example: `for (i : 1 .. 10 by 2) output i;`

### New keyword tokens

```
KTYPE   : 'type' ;
KCASE   : 'case' ;
KOF     : 'of' ;
KFOR    : 'for' ;
KBY     : 'by' ;
ARROW   : '->' ;
DOTDOT  : '..' ;
```

**[Q6 — task]** Audit existing test programs for identifier conflicts with new keywords.

**Prerequisite task (before merging grammar change):** run
```sh
grep -r '\btype\b\|\bcase\b\|\bof\b\|\bfor\b\|\bby\b' test/system/ test/unit/
```
and rename any identifier that collides with a new keyword.

### Source file extension

**[Q7 — resolved]** TOP source files use the **`.top`** extension.

**Prerequisite task (Phase 1):** update all of the following before the first TOP test program is added:
- `test/system/run.sh` — change glob pattern from `*.tip` to `*.top` (or support both)
- `test/system/selftests/`, `iotests/`, `polytests/` — rename any new TOP golden files to `.top`
- `CMakeLists.txt` test registration — any hardcoded `.tip` references for new tests
- `README.md` and `docs/` examples that cite source file names

Existing `.tip` test programs (pure TIP, not TOP) keep their `.tip` extension and continue to pass unchanged.

### Compiler binary name

**[Q8 — resolved]** Rename binary to **`topc`**; keep `tipc` as a symlink alias.

**Prerequisite task (Phase 1):**
- `CMakeLists.txt` — rename the executable target from `tipc` to `topc`; add a symlink install rule so `tipc` resolves to `topc`
- `test/system/run.sh` — update the `TIPC` variable to point to `topc` (existing `.tip` tests continue to work via the alias)
- `bin/build.sh`, `bin/runtests.sh` — update any hardcoded `tipc` binary references
- `README.md` — update usage examples

### Testing

All tests in this phase live in `test/unit/frontend/`.

#### Parser positive tests (add to `TIPParserTest.cpp`)

| Test name | Program fragment | Expected outcome |
|---|---|---|
| `parseSumTypeDecl` | `type Option = Some(x) \| None;` | Parses without error |
| `parseSumTypeDeclMultiField` | `type Pair = MkPair(a, b);` | Parses without error |
| `parseSumTypeDeclNoPayload` | `type Color = Red \| Green \| Blue;` | Parses without error |
| `parseCaseStmt` | `case p of { Some(v) -> output v; None -> output 0; }` | Parses without error |
| `parseForStmt` | `for (x : s) output x;` | Parses without error |
| `parseRangeExpr` | `for (i : 1 .. 10) output i;` | Parses without error |
| `parseRangeByExpr` | `for (i : 1 .. 10 by 2) output i;` | Parses without error |

#### Parser negative tests (add to `TIPParserTest.cpp`)

| Test name | Program fragment | Expected outcome |
|---|---|---|
| `parseRejectCaseMissingOf` | `case p { Some(v) -> output v; }` | Parse error |
| `parseRejectSumTypeDeclMissingEq` | `type Option Some(x) \| None;` | Parse error |
| `parseRejectRangeExprMissingDot` | `for (i : 1 . 10) output i;` | Parse error |

#### Regression

- All existing TIP parser tests must pass unchanged.
- Run `bin/runtests.sh -u` (unit only) after grammar change.

---

## Phase 2 — AST Nodes

**Goal:** Add one AST node class per new grammar construct, following the existing pattern in
`src/frontend/ast/treetypes/`.  Update `ASTBuilder.cpp` visitor methods to construct the new
nodes.  No semantic content yet.

### Existing pattern

Each node has:
- a header (`ASTFoo.h`) declaring the class, constructor, and `accept(ASTVisitor &)` override,
- a source file (`ASTFoo.cpp`) implementing print and accept,
- a registration in `AST.h` and `ASTinternal.h`,
- a corresponding visitor method in `ASTVisitor.h`.

### New nodes

| Class | Parent | Grammar rule | Key fields |
|---|---|---|---|
| `ASTSumTypeDecl` | `ASTNode` | `typeDecl` | name: `std::string`; variants: `vector<ASTSumVariant*>` |
| `ASTSumVariant` | `ASTNode` | `sumVariant` | tag: `std::string`; params: `vector<ASTDeclNode*>` |
| `ASTCaseStmt` | `ASTStmt` | `caseExpr` (stmt form) | scrutinee: `ASTExpr*`; arms: `vector<ASTCaseArm*>` |
| `ASTCaseArm` | `ASTNode` | `caseArm` | tag: `std::string`; bindings: `vector<ASTDeclNode*>`; body: `ASTStmt*` |
| `ASTBorrowExpr` | `ASTExpr` | `refExpr` (reinterpret) | operand: `ASTExpr*` |
| `ASTForStmt` | `ASTStmt` | `forStmt` (SOP stub) | var: `ASTDeclNode*`; iterable: `ASTExpr*`; body: `ASTStmt*` |
| `ASTRangeExpr` | `ASTExpr` | `rangeExpr` (SOP stub) | lo: `ASTExpr*`; hi: `ASTExpr*`; step: `ASTExpr*` (nullable) |

**[Q9 — resolved]** Rename `ASTRefExpr` → `ASTBorrowExpr` throughout; `&x` is always a
borrow in TOP and `TipRef` is retired (see Q15).

**[Q10 — resolved]** `ASTProgram` gains a separate `vector<ASTSumTypeDecl*>` field; type
declarations appear before functions in source and are independently accessible.

### ASTBuilder updates

For each new visitor method in `ASTBuilder.cpp`:
- `visitTypeDecl` → constructs `ASTSumTypeDecl`
- `visitSumVariant` → constructs `ASTSumVariant`
- `visitCaseExpr` (or `visitCaseStmt`) → constructs `ASTCaseStmt`
- `visitCaseArm` → constructs `ASTCaseArm`
- `visitForStmt` → constructs `ASTForStmt` (SOP stub; body deferred to sopc)
- `visitRangeExpr` → constructs `ASTRangeExpr` (SOP stub)

For the borrow/address-of distinction: `ASTRefExpr` is renamed `ASTBorrowExpr` (Q9).

### Testing

#### Unit tests (add to `test/unit/frontend/ASTBuilderTest.cpp`)

For each new node:
- Positive construction test: parse a minimal program containing that construct, verify the
  AST contains a node of the correct type with correct field values.
- Visitor traversal test: use `PreOrderIterator` to walk the constructed AST and confirm the
  new node appears in the iteration order.

#### Visitor coverage test

- Add a test that visits every AST node type via the visitor interface.  Any new node whose
  `accept()` is not called should cause a test failure (catch via a mock visitor that tracks
  which `visit` methods were invoked).

#### Regression

- All existing `ASTBuilderTest.cpp`, `ASTNodeNoLLVMTest.cpp`, and `SyntaxTreeTest.cpp` tests
  must pass unchanged.

---

## Phase 3 — Pretty Printer and Visualizer

**Goal:** Give every new AST node a canonical textual representation.  This drives the
selftest oracle generation in Phase 1/2.

### Files to update

- `src/frontend/prettyprint/PrettyPrinter.cpp` — add a `visit` method for each new node.
- `src/frontend/prettyprint/ASTVisualizer.cpp` — add DOT-graph support for each new node.

### Pretty-print format

The printed form must round-trip: `parse(prettyPrint(ast))` must produce an equivalent AST.
This is tested below.

Proposed format for new nodes:

```
// ASTSumTypeDecl
type Option = Some(x) | None;

// ASTCaseStmt
case p of {
  Some(v) -> output v;
  None -> output 0;
}

// ASTForStmt (SOP)
for (x : s) output x;

// ASTRangeExpr (SOP)
1 .. 10
1 .. 10 by 2
```

**[Q11 — resolved]** Brace-aligned arms with 2-space indentation; no arrow alignment.

### Selftest golden files

For each new construct, add a file pair to `test/system/selftests/`:
- `<name>.top` — the source program
- `<name>.top.pppt` — the expected pretty-printed output

Proposed initial selftest programs:

| File | Construct covered |
|---|---|
| `sumtype.top` | Sum type declaration, case statement |
| `borrow.top` | Borrow expression in function call |
| `ownership-move.top` | Alloc, move via assignment, scope exit |

### Testing

#### Unit tests

- Extend `test/unit/frontend/PrettyPrinterTest.cpp` and `ASTVisualizerTest.cpp` with one test
  per new node type.
- Add a round-trip property test to `SyntaxTreeTest.cpp`: for each new selftest program,
  assert `parse(prettyPrint(ast))` yields a structurally identical AST.

#### System tests

- Run each new selftest program through the pretty-printer mode (existing `-pp` flag) and
  diff against its `.pppt` file.

---

## Phase 4 — Weeding Passes

**Goal:** Reject programs that are syntactically valid but statically ill-formed in ways that
can be checked before type inference.  Each rule is a separate, small pass following the
existing pattern of `CheckAssignable`.

### Existing weeding infrastructure

`src/semantic/weeding/CheckAssignable.{h,cpp}` validates l-value positions.  New passes
follow the same structure: a visitor that calls `throw SemanticError(...)` on violation.
`SemanticAnalysis::analyze` invokes all weeding passes sequentially.

### New weeding passes

#### 4a. `CheckBorrowPositions`

**File:** `src/semantic/weeding/CheckBorrowPositions.{h,cpp}`

**Rule:** A borrow expression `&x` must appear in a position that accepts a read-only
reference.  In TOP v1 this means:
- the argument of a function call, or
- the right-hand side of an assignment to a variable typed as a borrow (after type inference
  this is enforced more precisely, but the weeding pass catches obvious misuse early).

**[Q12 — resolved]** Reject only unambiguously wrong positions: `output &x`, arithmetic on
a borrow, `return &x`.  All other positions are deferred to the type/borrow checker.

#### 4b. `CheckCaseCompleteness`

**File:** `src/semantic/weeding/CheckCaseCompleteness.{h,cpp}`

**Rule:** Every arm of a `case` expression must bind variables that are consistent in arity
with the corresponding constructor declared in the enclosing program's type declarations.

**[Q13 — resolved]** Missing or duplicate arms are a **hard error** (`SemanticError`);
incomplete case risks ownership leaks.

#### 4c. `CheckSumTypeNames` and `CheckConstructorCase`

**Files:** `src/semantic/weeding/CheckSumTypeNames.{h,cpp}`, `CheckConstructorCase.{h,cpp}`

**`CheckSumTypeNames` rule:** No two sum type declarations in the same program may declare a
constructor with the same name (constructors are globally unambiguous).

**`CheckConstructorCase` rule (Q14 locked):** Every type name and every constructor name must
begin with an uppercase letter; every function name and variable name must begin with a
lowercase letter.  This replaces the need for a separate constructor namespace — at any use
site, capitalisation unambiguously identifies constructor calls vs function calls.

```
// valid
type Option = Some(int) | None;
// rejected by CheckConstructorCase
type option = some(int) | none;   // type and constructors must be uppercase
```

The check walks `ASTSumTypeDecl` nodes (type name + all variant names) and
`ASTFunDecl`/`ASTDeclStmt` nodes (function and variable names).

### Testing

For each weeding pass, add tests to `test/unit/semantic/`:
- One `.cpp` test file per pass (e.g., `CheckBorrowPositionsTest.cpp`).
- Each test file contains at minimum:
  - one **positive** test: a program that should pass the pass without error,
  - one **negative** test per rule: a program that should throw a `SemanticError` with an
    exact expected message string.

Error message strings should be `EXPECT_THROW` + string comparison, not just type-check.

---

## Phase 5 — Symbol Table Extensions

**Goal:** Extend name resolution to cover sum type constructors, type declaration names, and
for-loop iteration variables.

### Existing symbol table structure

`SymbolTable` maps `ASTDeclNode*` to scoped names, built by:
- `FunctionNameCollector` — top-level function names,
- `LocalNameCollector` — function-local variable declarations,
- `FieldNameCollector` — record field names.

### Extensions needed

#### 5a. Type declaration collector

**New class:** `TypeNameCollector` (parallel to `FunctionNameCollector`)

Responsibility: walk `ASTSumTypeDecl` nodes and record:
- the type name (e.g., `Option`),
- each constructor name and its arity (e.g., `Some/1`, `None/0`).

#### 5b. Constructor resolution in case expressions

`SymbolTable` must resolve constructor names used in `ASTCaseArm` to their `ASTSumVariant`
declaration.  The existing `lookup` interface will be extended with a constructor-specific
query.

#### 5c. For-loop variable scope

Iteration variables declared in `ASTForStmt` must be scoped to the loop body.  The
`LocalNameCollector` visitor is extended to open/close a scope around `ASTForStmt`.

#### 5d. Sum type name conflict checks

`TypeNameCollector` raises `SemanticError` if a constructor name is duplicated across type
declarations.  Constructor/function conflicts are impossible by construction (capitalisation
convention enforced in Phase 4), so no cross-namespace conflict check is needed here.

### Testing

Extend `test/unit/semantic/SymbolTableTest.cpp`:

| Test name | Scenario | Expected outcome |
|---|---|---|
| `resolveConstructorName` | `type T = A(x); case p of { A(v) -> ... }` | `A` resolves to `ASTSumVariant` |
| `rejectUnknownConstructor` | `case p of { B(v) -> ... }` with no `type` decl | `SemanticError` |
| `rejectDuplicateConstructor` | `type T = A(x) \| A(y);` | `SemanticError` |
| `forLoopVarScoped` | iteration variable `x` not visible after loop body | lookup returns null |

---

## Phase 6 — Type Shape Inference (Pass A)

**Goal:** Extend the constraint-based type inference in `src/semantic/types/` to include new
type forms needed by TOP.

### Existing type infrastructure

Type terms are defined as `TipType` subclasses in `src/semantic/types/concrete/`:
- `TipInt`, `TipFunction`, `TipRecord`, `TipRef`, `TipAlpha` (type variable), `TipMu`
  (recursive type), `TipVar` (unification variable), `TipAbsentField`, `TipCons`.

Constraints are generated by `TypeConstraintCollectVisitor` and solved by `TermUnifier`
(union-find based).

### New type terms

#### 6a. `TipOwningRef`

Distinguishes an owning heap pointer from a raw TIP pointer (`TipRef`).

`alloc E` should generate `TipOwningRef(T)` rather than `TipRef(T)`.

**[Q15 — resolved]** `TipRef` is **retired** in TOP.  `alloc` → `TipOwningRef`, `&` →
`TipBorrowRef`.  No raw `TipRef` in TOP programs.

#### 6b. `TipBorrowRef`

Represents a read-only borrow of a value of type T.  Used for the result type of `&x`.

#### 6c. `TipSumType`

Represents a sum type with a map from constructor tags to payload types.

```
type Option = Some(x) | None
```

gives `TipSumType({ "Some" -> TipAlpha("x"), "None" -> void })`.

**[Q16 — resolved]** No unit type.  Zero-arity constructors use no parens (`None`, `Nil`);
empty parens `None()` are a parse error; the payload is represented as void internally.

#### 6d. `TipSeq` (SOP stub — type term defined now, constraints added in sopc)

Represents an owned sequence of elements of type T.

### Constraint generation changes

`TypeConstraintCollectVisitor` is extended:

- `visitASTBorrowExpr` → generates `[[&x]] = TipBorrowRef([[x]])` and adds a lifetime
  constraint (the lifetime constraints themselves are solved in Phase 10; here we only
  record them).
- `visitASTCaseStmt` → generates equality constraints unifying the scrutinee type against the
  sum type and unifying each arm's binding types against the corresponding constructor payload
  types.
- `visitASTSumTypeDecl` → registers the `TipSumType` in the type environment.
- `visitASTAssignStmt` → for owning types, generates a `move` constraint rather than
  an equality constraint (the move constraint is consumed by Phase 9; type shape inference
  still requires the types to unify).

### Testing

Add to `test/unit/semantic/types/`:

| Test name | Scenario | Expected inferred type |
|---|---|---|
| `inferSumTypeDecl` | `type T = A(x) \| B` | `TipSumType({"A" -> α, "B" -> unit})` |
| `inferCaseScrutinee` | scrutinee unified against declared sum type | scrutinee has `TipSumType(...)` |
| `inferBorrowExpr` | `&p` where `p : TipOwningRef(int)` | `TipBorrowRef(TipOwningRef(int))` |
| `inferAllocOwning` | `alloc 5` | `TipOwningRef(TipInt)` |
| `rejectBorrowMismatch` | arm payload type mismatch | `SemanticError` / `UnificationError` |

Existing TIP type-inference tests (`test/unit/semantic/types/`) must pass unchanged.

---

## Phase 7 — Schema Generalization (Pass B)

**Goal:** Replace the explicit `KPOLY` keyword with implicit generalization for eligible
non-recursive globals, as specified in §4.7 of the design doc.

### Current mechanism

`TypeInference::run` accepts a `bool polyInf` flag.  When true, it uses
`PolyTypeConstraintCollectVisitor` / `PolyTypeConstraintVisitor`.  The flag is set when a
function is marked with `KPOLY` in the grammar.

### Proposed change

1. **Phase 7 development:** accept `KPOLY` in the grammar but silently ignore it — existing
   poly selftest programs parse and run unchanged while the auto-generalization logic is
   being built and tested.
2. After building the call graph, compute SCCs.
3. For each SCC of size 1 whose sole member is not self-recursive: automatically apply
   polymorphic inference (i.e., set `polyInf = true` for that function).
4. For all other functions: apply monomorphic inference as today.
5. **Final Phase 7 step (removal):** once all poly selftests pass with auto-generalization,
   strip `poly` from `polyfactorial.tip`, `polyfun.tip`, `polyprog.tip`, then delete the
   `KPOLY` token from the grammar and the `polyInf` flag path.  Confirm all tests still
   green before closing the phase.

The call graph infrastructure (`src/semantic/cfa/CallGraph*`) already exists and is tested
in `test/unit/semantic/cfa/CallGraphTest.cpp`.

**[Q17 — resolved]** Silently ignore `KPOLY` during Phase 7 development; remove the token
entirely as the final Phase 7 step once all poly selftests pass without it.

### Testing

- Strip `poly` from `test/system/selftests/polyfactorial.tip`, `polyfun.tip`, `polyprog.tip`
  and confirm they still typecheck and produce correct output.
- Add new selftest programs: `apply.top`, `fold-scalar.top` — higher-order helpers that
  are auto-generalized.
- Negative test: a recursive function that cannot be generalized produces the same type error
  as today when used polymorphically.
- Unit tests: extend `CallGraphTest.cpp` to verify SCC computation identifies singleton
  non-recursive functions correctly.

---

## Phase 8 — Ownership Classification (Pass C)

**Goal:** After type shape inference, classify every resolved type as `Copy` or `Own`.

### Classification rules (initial)

| Type term | Classification |
|---|---|
| `TipInt` | Copy |
| `TipFunction(...)` | Copy (functions are not heap resources in v1) |
| `TipRecord(...)` | Copy if all fields are Copy, Own otherwise |
| `TipOwningRef(T)` | Own |
| `TipBorrowRef(T)` | Copy (borrows are not owned) |
| `TipSumType(...)` | Own if any constructor payload is Own, otherwise Copy |
| `TipSeq(T)` | Own |
| `TipAlpha` / `TipVar` | Copy until resolved; re-classified after unification |

**[Q18 — resolved]** Closures are out of scope for all TOP phases (confirmed by FQ3).
`TipFunction` is always `Copy`.

### New pass

**New class:** `OwnershipClassifier` in `src/semantic/`

Input: the `TypeInference` result (the solved type for every `ASTDeclNode`).
Output: a map `ASTDeclNode* → OwnershipClass { Copy, Own }`.

This pass is a single structural traversal of the solved types — no fixed-point is needed
given the rules above.

### Integration

`SemanticAnalysis` stores the `OwnershipClassifier` result alongside `TypeInference` and
`SymbolTable`.  Phases 9 and 10 consume it.

### Testing

**New file:** `test/unit/semantic/OwnershipClassifierTest.cpp`

Table-driven tests: for each type term in the table above, construct the type directly
(without parsing), run the classifier, and assert the expected `OwnershipClass`.

Integration test: run the classifier on a selftest program containing both Copy and Own
variables; assert the classification snapshot matches a recorded expected output.

---

## Phase 9 — Move/State Analysis (Pass D)

**Goal:** For every variable classified as `Own`, track ownership state across all control
flow paths and reject invalid uses.

### Ownership states

```
Owned   — variable holds a live resource
Moved   — resource has been transferred; variable is invalid
Borrowed — temporarily lent; cannot be moved while a live borrow exists (Phase 10)
```

### Analysis design

This is a forward dataflow analysis:

- **Domain:** `map<ASTDeclNode*, OwnershipState>` per program point.
- **Initial state:** all `Own` variables start as `Owned` after their declaration.
- **Transfer functions:**
  - `x = y` where `y : Own` → `y` transitions `Owned → Moved`; `x` transitions to `Owned`
    (or stays `Owned` if it was already owning — that would be a double-assign that drops
    the previous resource; that is a hard error per Q19).
  - `x = y` where `y : Copy` → no ownership state change.
  - Use of `x` in any expression position → if `x` is `Moved`, emit `use-after-move` error.
  - Return statement with `x : Own` → `x` transitions `Owned → Moved` (resource transferred
    to caller).
- **Join:** at control-flow merge points, the state is the join of all incoming paths.
  - `Owned ⊔ Owned = Owned`
  - `Moved ⊔ Moved = Moved`
  - `Owned ⊔ Moved = hard error` — **[Q20 — resolved]** Both paths must leave every variable
    in the same ownership state at the join (Rust-consistent).

### Error messages

| Violation | Error message (exact text to be decided) |
|---|---|
| Use-after-move | `"variable 'x' used after move at line N"` |
| Move-while-borrowed | deferred to Phase 10 |
| Double-move | `"variable 'x' moved more than once"` |
| Assign-over-live-own | `"variable 'x' assigned while still owned — free or move first"` |

**[Q19 — resolved]** Assigning over a live `Own` variable is a **hard error** in v1.
Free-before-overwrite is a trivial Phase 11 extension (FP2) but hard error is pedagogically
clearer and makes Phase 11 easier to test incrementally.

### New pass

**New class:** `MoveAnalysis` in `src/semantic/`

Input: `ASTProgram`, `SymbolTable`, `OwnershipClassifier` result.
Output: map from program point (representable as `ASTNode*`) to ownership state map; error
list.

### Testing

**New file:** `test/unit/semantic/MoveAnalysisTest.cpp`

| Test name | Program | Expected outcome |
|---|---|---|
| `moveTransfersOwnership` | `p = alloc 5; q = p; output *q;` | accepted; `p` is Moved after assignment |
| `rejectUseAfterMove` | `p = alloc 5; q = p; output *p;` | `use-after-move` error on `*p` |
| `rejectDoubleMove` | `p = alloc 5; q = p; r = p;` | `double-move` error on second `p` |
| `moveInBothBranches` | `if (c) q = p; else r = p;` | accepted if `p` moved on both paths |
| `moveInOneBranchReject` | `if (c) q = p;` (no else) | hard error: paths disagree on ownership state |
| `copyDoesNotMove` | `x = 5; y = x; output x;` | accepted; `x : int` is Copy |

System tests: compile and run programs that perform valid moves; confirm correct output.

---

## Phase 10 — Borrow/Lifetime Validity (Pass E)

**Goal:** Verify that borrowed references do not outlive their owner and that owners are not
moved while a live borrow exists.

### Lifetime model (TOP v1)

Lifetimes in v1 are implicit and lexical.  A borrow `&x` is valid from the borrow point to
its last use.  The owner `x` must not be moved within that interval.

This is a **last-use liveness** analysis:

1. Compute the last use of each borrow variable.
2. For each owner `x`, check that no move of `x` occurs between the creation of any active
   borrow `&x` and the last use of that borrow.

**[Q21 — resolved]** Immediate function arguments only for all TOP phases (`f(&x)` is legal;
storing `&x` in a variable is a weeding error, caught by `CheckBorrowPositions`).
First-class borrows are deferred to §FP (FP1).

### Error messages

| Violation | Error message |
|---|---|
| Owner moved while borrow live | `"variable 'x' moved while borrow is still live"` |
| Borrow escapes owner scope | `"borrow of 'x' outlives owner"` |

### New pass

**New class:** `BorrowChecker` in `src/semantic/`

Input: `ASTProgram`, `SymbolTable`, `OwnershipClassifier`, `MoveAnalysis` result.
Output: error list.

### Testing

**New file:** `test/unit/semantic/BorrowCheckerTest.cpp`

| Test name | Program | Expected outcome |
|---|---|---|
| `validBorrowBeforeMove` | `inspect(&p); q = p;` | accepted |
| `rejectMoveWhileBorrowed` | `b = &p; q = p; use(b);` | error: move while borrow live |
| `rejectBorrowOutlivesOwner` | store `&p` into variable that outlives `p` | error: borrow escapes owner |
| `borrowInFunctionArgOk` | `f(&p)` where `f` does not store the borrow | accepted |

System tests: programs from the design doc `inspect(&p)` example compile and run correctly.

---

## Phase 11 — Destruction Insertion (Pass F)

**Goal:** Insert `free` calls at the correct program points for `Own` variables that exit
scope without being moved.

### Design

After move analysis (Phase 9), for each function:

1. Walk all scope-exit points (closing `}` of blocks, return statements, early exits through
   conditionals).
2. For each `Own` variable in scope at that exit point:
   - If the variable's ownership state at that point is `Owned` → emit `free`.
   - If the variable's ownership state is `Moved` → do not emit `free` (resource was
     transferred).
3. For conditional paths: a free must be emitted on each path where the variable is `Owned`
   at scope exit.  This may require inserting conditional free code.

**[Q22 — resolved]** Destruction is **recursive**.  `ASTDestroyStmt` lowering walks the
type structure at compile time and emits child frees (depth-first) before the parent free.

**[Q23 — resolved]** `ASTDestroyStmt` nodes are inserted by the destruction pass; codegen
lowers them to `free()` calls.  Keeps the insertion pass independently testable.

### Files to update

- New `ASTDestroyStmt.{h,cpp}` node inserted by the destruction pass.
- `src/codegen/CodeGenerator.cpp` and `CodeGenFunctions.cpp` — lower `ASTDestroyStmt` to
  `free()` IR calls.

### Testing

- All existing leak tests in `test/system/leak/` must pass.
- New leak test programs (add to `test/system/leak/`):
  - `basicOwnershipFree.top` — alloc, no move, scope exit → free called
  - `moveNoDoubleFree.top` — alloc, move to other variable, scope exit of original → no free
  - `conditionalFree.top` — alloc, conditional move → free only on the no-move branch
  - `nestedFree.top` — alloc inside alloc (record with owning field) → both freed
- All new leak tests run under AddressSanitizer (`-fsanitize=address`):
  - No memory leaks reported.
  - No double-free reported.
  - No use-after-free reported.
- **[Q24 — resolved]** ASan is the sole memory safety checker.  Valgrind is not required
  and not part of the plan; macOS toolchain support is unreliable.

---

## 13. Design Decisions (all resolved)

All questions are closed.  The table below records each decision for reference.
No open questions remain; the agent proceeds autonomously without seeking further input
on these topics.

| # | Phase | Question | Decision |
|---|---|---|---|
| Q1 | 0 | Build config + ASan strategy for compiler binary and generated programs? | `Debug`+ASan for topc binary; all system tests link generated bitcode with `-fsanitize=address`; `-asan` compiler flag instruments generated IR via `llvm::AddressSanitizerPass` |
| Q2 | 1 | Are `alloc` and `*` (dereference) retained unchanged in TOP, or replaced by owned-allocation syntax? | Retained; `alloc E` becomes the owning allocation |
| Q3 | 1 | Can sum type declarations appear inside a function, or are they top-level only? | **Locked: top-level only** |
| Q3-payload | 1 | Are constructor payloads positional or nominal (named fields)? | **Locked: positional**; record as single payload is the escape hatch for named fields |
| Q4 | 1 | Is `case` an expression or a statement? | **Locked: statement only**; expression form explicitly out of scope |
| Q5 | 1 | What token for `->` in case arms — new `ARROW` lexeme or reuse an existing symbol? | **Locked: new `ARROW : '->'` token** |
| Q7 | 1 | File extension for TOP programs: `.top` or `.tip`? | **Locked: `.top`**; existing `.tip` tests unchanged |
| Q8 | 1 | Compiler binary name: keep `tipc` or rename to `topc`? | **Locked: rename to `topc`; `tipc` symlink alias retained** |
| Q9 | 2 | How to distinguish borrow `&x` from TIP address-of `&x` at the AST level? | **Locked: no distinction — `&x` is always a borrow in TOP; `ASTRefExpr` repurposed as `ASTBorrowExpr`; `TipRef` retired (see Q15)** |
| Q10 | 2 | How should `ASTProgram` hold type declarations alongside functions? | **Locked: separate `vector<ASTSumTypeDecl*>` field; type decls before functions in source; both lists independently accessible** |
| Q11 | 3 | Pretty-printer indentation style for `case` arms? | **Locked: braces with 2-space indented arms, no arrow alignment** |
| Q12 | 4 | Which borrow positions are rejected syntactically by `CheckBorrowPositions`? | **Locked: only unambiguously wrong positions (`output &x`, arithmetic on borrow, `return &x`); rest deferred to type/borrow checker** |
| Q13 | 4 | Are incomplete `case` arms a hard error or warning in v1? | **Locked: hard error**; missing arms risk ownership leaks |
| Q14 | 4 | Do constructor names share a namespace with function names? | **Locked: capitalisation convention — type names and constructor names must start with uppercase (`Option`, `Some`), functions/variables with lowercase; enforced by `CheckConstructorCase` weeding pass; no separate constructor namespace needed** |
| Q15 | 6 | Should `TipRef` be retired in TOP or kept for backward TIP compatibility? | **Locked: retired**; `alloc` → `TipOwningRef`, `&` → `TipBorrowRef`, no raw `TipRef` |
| Q16 | 6 | Is there a unit type, or are zero-arity constructors represented differently? | **Locked: no unit type; zero-arity constructors use no parens (`None`, `Nil`); empty parens `None()` are a parse error; payload represented as void internally** |
| Q17 | 7 | Keep `KPOLY` keyword or remove it? | **Locked: silently ignore during Phase 7 development; remove `KPOLY` entirely as the final Phase 7 step once all poly selftests pass without it** |
| Q18 | 8 | Can closures capture owning values, making the closure itself `Own`? | **N/A: closures are out of scope for all TOP phases (resolved by FQ3); `TipFunction` is always Copy** |
| Q19 | 9 | Assigning over a live `Own` variable: hard error, warning, or silent free-before-overwrite? | **Locked: hard error in v1. Free-before-overwrite is a trivial Phase 11 extension (one extra insertion site reusing the same `ASTDestroyStmt` infrastructure), but hard error is pedagogically clearer and makes Phase 11 easier to test incrementally.** |
| Q20 | 9 | Ownership state at a join where one path moved a variable and another did not? | **Locked: hard error; both paths must leave every variable in the same ownership state at the join (Rust-consistent).** |
| Q21 | 10 | Are borrows tracked as first-class variables or only as immediate function arguments? | **Locked: immediate function arguments only for all TOP phases (`f(&x)` only; storing `&x` in a variable is a weeding error). First-class borrows require CFG infrastructure and region inference — deferred to Final Project Options (see §FP).** |
| Q22 | 11 | Is destruction of owned aggregate types recursive? | **Locked: yes. `ASTDestroyStmt` lowering walks the type structure at compile time and emits child frees (depth-first) before the parent free.** |
| Q23 | 11 | Insert destruction as LLVM IR directly or as an `ASTDestroyStmt` node? | **Locked: `ASTDestroyStmt` nodes inserted by the destruction pass; codegen lowers them to `free()` calls. Keeps the insertion pass independently testable.** |
| Q24 | 11 | Is Valgrind available in CI, or is ASan the sole memory safety checker? | **Locked: ASan only (superseded by Q1 strategy). Valgrind is not required and not part of the plan; macOS toolchain support is unreliable.** |

---

## 14. Follow-up Questions from Design Doc Review

The design answers in `docs/design/TOP_SOP_design_consolidation.md` §10 Q1–Q10 resolve some
dev-plan questions and open new follow-ups.  Each subsection below: (a) states what was
answered, (b) provides the technical analysis where the user asked "what are the tradeoffs /
complications", and (c) poses the residual decision question.

---

### Design Q1 — Implicit generalization confirmed

**Answer:** Automatic generalization for eligible globals; minimal annotation only if automatic
is not possible.

**Analysis:** Automatic generalization for singleton non-recursive SCCs is implementable with
the existing call-graph infrastructure (Phase 7).  "Not possible" means a recursive function
that the user expects to be polymorphic — the only fallback would be a `poly` marker.  The
default plan already handles this correctly.

**FQ1:** Should the `KPOLY` keyword be **removed entirely** from the grammar (breaking the
existing `polyfactorial.tip`, `polyfun.tip`, `polyprog.tip` selftests which will need their
`poly` annotation stripped), or should the compiler **accept and silently ignore** `poly` for
a transition period before removing it?  The former is cleaner; the latter avoids editing
the existing test corpus immediately.

---

### Design Q2 — Copy/Own boundary aligns with Rust

**Answer:** Primitive scalar types (`int`, `bool`) are Copy; everything involving
heap allocation (`alloc`, arrays, tuples, records, sequences) is Own.

**Analysis:** This aligns well with the dev-plan Phase 8 classification rules.  Two edge
cases need a ruling:

1. **Record literals.** TIP record literals such as `{id: 1, balance: 100}` do not involve
   `alloc` at the source level, but the compiler may allocate them on the heap internally.
   In Rust, a struct is `Copy` only if all fields are `Copy` **and** the type explicitly
   opts in.  For TOP the simplest rule is: records are **always Own**, consistent with the
   user's "anything deeper is owned" intent.  Alternatively: records are `Copy` when all
   fields are `Copy` (matching Rust's structural rule).

2. **Function values.** TOP v1 has no closures (see FQ3 below); all higher-order values
   are pointers to named global functions.  These are naturally `Copy` (function pointers
   in Rust are `Copy`).

**FQ2a:** Are record literals (`{f: e, ...}`) **always Own**, or **Copy when all fields are
Copy**?  The "always Own" rule is simpler to implement; the structural rule is more expressive.

**FQ2b:** Is it confirmed that **function values are Copy** in TOP v1 (no closures, so a
function value is a plain function pointer)?

---

### Design Q3 — Closure complexity

**Answer:** User asks what the complications are and says "if too complicated, go with globals."

**Analysis:** TIP already has no lambda syntax; all functions are named globals.  Closures
would require:

1. **New grammar:** a `lambda` or anonymous function expression that does not exist in TIP.
2. **Environment struct:** a heap-allocated record capturing the enclosing variables; the
   closure value is a fat pointer (function pointer + data pointer).
3. **Ownership of captures:** if a closure captures an `Own` variable, the capture is a
   move (the closure is `FnOnce` in Rust terms); if it captures a borrow, the closure carries
   a lifetime (the closure is `Fn`).  This requires a three-tier distinction similar to
   Rust's `Fn`/`FnMut`/`FnOnce`.
4. **Type system:** each closure has a unique structural type; supporting polymorphic
   higher-order functions over closures requires either trait-object erasure or full HM
   with closure types — neither is trivial.
5. **Auto-generalization:** local closures cannot be auto-generalized (Phase 7 is restricted
   to eligible globals).

**Recommendation:** Keep globals-only for all TOP and SOP phases.  The existing TIP
higher-order programming style (passing named global functions like `inc`, `apply`, `fold`)
is already pedagogically rich and sufficient for the course goals.  Closures can be a separate
extension outside the TOP / SOP scope.

**FQ3:** Should closures be **explicitly out of scope** for all phases (TOP through SOP
v3), with the understanding that all higher-order programming goes through named global
functions?  Confirming this resolves dev-plan Q18 (closures capturing Own values = not
applicable).

---

### Design Q4 — Only `&` in source; implicit lifetimes

**Answer:** User wants only `&` in source with all lifetime detail inferred internally.
Asks: "what are the challenges?"

**Analysis:**

1. **Internal representation.** Even with no source annotations, the compiler must track
   lifetime/region variables internally — each borrow gets a fresh region variable, and
   region constraints (outlives relationships) are recorded alongside type constraints.
   This is a second constraint dimension beyond the existing equality unifier.

2. **Constraint solving.** Region constraints are ordering constraints (`r1 outlives r2`),
   not equality constraints.  They require a separate solver (e.g., a transitive-closure
   check or a simple topological sort), distinct from the `TermUnifier`.

3. **Error messages.** Violations must be reported in source terms ("variable `p` is moved
   while borrowed at line N") without exposing region variable names.  This requires a
   translation from internal region terms back to source variable names.

4. **CFG dependency.** Use-based lifetimes (confirmed in design Q7) require liveness
   analysis on a per-function control-flow graph.  This graph does not currently exist as a
   standalone data structure in tipc (see FQ7a).

**TOP v1 pragmatic restriction:** For the initial version, restrict borrows to
**immediately-passed function arguments only** — `f(&x)` is legal; `var b; b = &x;` is not.
Under this restriction, the lifetime of every borrow is exactly the duration of the enclosing
function call, which is trivially checkable without a CFG or a region solver.  This delivers
the core ownership/borrow experience with minimal infrastructure, and the restriction is lifted
in Phase 10 when the CFG is available.

**FQ4:** For TOP v1 (Phase 10 of the dev-plan), are you comfortable restricting borrows to
**immediately-passed function arguments** (no storing `&x` in a variable)?  This makes the
v1 borrow checker trivial; stored borrow variables become available once the CFG sub-phase
(FQ7a) is complete.

---

### Design Q7 — Use-based lifetimes; CFG refactor needed

**Answer:** Use-based lifetimes confirmed.  User will provide the borrow checker.  Notes that
the CFG may need to be refactored into a standalone representation.

**Analysis:** The current tipc compiler has a `CallGraph` for inter-procedural CFA, but no
**per-function control-flow graph** (basic blocks, edges, dominators) for intra-procedural
dataflow analysis.  Both the move analysis (Phase 9) and the borrow checker (Phase 10) require
intra-procedural dataflow.

Good news: TOP's control flow is **fully structured** — only `if/else` and `while`, no
`goto`, `break`, `continue`, or exception dispatch.  The CFG for a structured program is
deterministic from the AST and can be constructed as a simple recursive traversal.  A
lightweight representation (basic block list + successor edges, no SSA needed) is sufficient
for liveness and ownership analysis.

**FQ7a:** Should the dev-plan include a **Phase 9a: CFG construction** sub-phase — before
move analysis — that builds a per-function CFG from the AST?  This phase would produce a new
class `CFG` (list of `BasicBlock` nodes with predecessor/successor edges) used by Phases 9
and 10.  Given structured control flow, this is a medium-complexity task that could be a
student project milestone.

**FQ7b:** Since you plan to provide the borrow checker implementation: should the dev-plan
explicitly designate Phase 10 (Borrow/Lifetime Validity) as **instructor-provided
infrastructure**, with student work focused on testing, integration, and using the checker
to write correct TOP programs?  This affects how Phase 10 is scoped in the course.

---

## Summary of new follow-up questions

| # | Design Q | Follow-up question | Decision needed by |
|---|---|---|---|
| FQ1 | Q1 | Remove `KPOLY` entirely or silently ignore for a transition period? | **Resolved by Q17: silently ignore during Phase 7; remove entirely at end of Phase 7** |
| FQ2a | Q2 | Are record literals always Own, or Copy when all fields are Copy? | **Resolved: always Own. Simple uniform rule; avoids silent breakage when an Own field is added later.** |
| FQ2b | Q2 | Are function values confirmed as Copy in TOP v1? | **Resolved by Q18/FQ3: yes, `TipFunction` is always Copy (no closures, no captured environment).** |
| FQ3 | Q3 | Are closures explicitly out of scope for all phases through SOP v3? | **Resolved by Q18: yes, closures out of scope for all TOP phases; `TipFunction` always Copy** |
| FQ4 | Q4 | For TOP v1, restrict borrows to immediately-passed function arguments only? | **Resolved by Q21: yes, immediate-arg only for all TOP phases; first-class borrows deferred to §FP** |
| FQ7a | Q7 | Add Phase 9a (CFG construction) as an explicit sub-phase before move analysis? | **Resolved: not needed for base TOP (Phase 9 move analysis works via structured AST traversal). CFG construction added as FP3 in §FP to enable advanced final projects.** |
| FQ7b | Q7 | Is Phase 10 (borrow checker) instructor-provided infrastructure or student-built? | **Resolved: student-built. Phase 10 borrow checker is a weeding pass (immediate-arg restriction); no CFG required. Straightforward milestone.** |

---

## §FP — Final Project Options

These extensions are explicitly out of scope for the standard TOP phase sequence but are well-scoped, interesting projects suitable for a course final project or independent study. Each builds directly on the Phase 0–11 infrastructure.

| ID | Title | Prerequisite phases | Brief description |
|---|---|---|---|
| FP1 | First-class borrow variables | 9, 10, FP3 | Lift the immediate-arg restriction (Q21). Requires CFG construction (FP3), borrow liveness dataflow, and region/lifetime inference to prove no borrow outlives the borrowed value. The ownership checker (Phase 9) must query borrow liveness before allowing moves/destroys. |
| FP2 | Free-before-overwrite (implicit destruction at assignment) | 11 | Lift the hard-error-on-overwrite rule (Q19). Reuse `ASTDestroyStmt` insertion from Phase 11, adding overwrite sites as a second insertion trigger. Special-case self-assignment (`p = p`). |
| FP3 | CFG construction | 3 | Build a per-function control-flow graph (`CFG` / `BasicBlock` classes, successor edges) from the structured TOP AST. No SSA needed; structured control flow (`if/else`, `while` only) makes this a clean recursive traversal. Prerequisite for FP1 and any future dataflow-based analyses (escape, alias, dead-code). |

---

## Appendix A — Test matrix summary

| Phase | New unit test files | New system test programs | Property enforced |
|---|---|---|---|
| 0 | none | none | all existing tests green; coverage baseline recorded |
| 1 | `TIPParserTest.cpp` (extended) | none | new constructs parse; TIP tests unchanged |
| 2 | `ASTBuilderTest.cpp` (extended) | none | node construction; visitor coverage |
| 3 | `PrettyPrinterTest.cpp` (extended) | `sumtype`, `borrow`, `ownership-move` selftest pairs | `parse(print(ast)) == ast` |
| 4 | `CheckBorrowPositionsTest.cpp`, `CheckCaseCompletenessTest.cpp`, `CheckSumTypeNamesTest.cpp` | none | exact error message per violation |
| 5 | `SymbolTableTest.cpp` (extended) | none | no unknown names survive analysis |
| 6 | types/ subtests (extended) | none | TIP type tests unchanged; new type terms infer correctly |
| 7 | `CallGraphTest.cpp` (extended) | `apply`, `fold-scalar` selftests | `poly` selftests pass without keyword |
| 8 | `OwnershipClassifierTest.cpp` | classification snapshot test | every resolved type has a classification |
| 9 | `MoveAnalysisTest.cpp` | valid-move programs run correctly | `use-after-move` at correct source location |
| 10 | `BorrowCheckerTest.cpp` | borrow-validity programs run correctly | borrow errors at correct source location |
| 11 | none (codegen) | `test/system/leak/` additions | zero leaks and zero double-frees under ASan |

---

## Appendix B — File change summary per phase

| Phase | New source files | Modified source files |
|---|---|---|
| 1 | none | `src/frontend/TIP.g4`, `tipg4/TIP.g4` |
| 2 | `ASTSumTypeDecl.{h,cpp}`, `ASTSumVariant.{h,cpp}`, `ASTCaseStmt.{h,cpp}`, `ASTCaseArm.{h,cpp}`, `ASTBorrowExpr.{h,cpp}`, `ASTForStmt.{h,cpp}`, `ASTRangeExpr.{h,cpp}` | `ASTBuilder.{h,cpp}`, `AST.h`, `ASTinternal.h`, `ASTVisitor.h`, `ASTProgram.{h,cpp}` |
| 3 | none | `PrettyPrinter.{h,cpp}`, `ASTVisualizer.{h,cpp}` |
| 4 | `CheckBorrowPositions.{h,cpp}`, `CheckCaseCompleteness.{h,cpp}`, `CheckSumTypeNames.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| 5 | `TypeNameCollector.{h,cpp}` | `SymbolTable.{h,cpp}`, `LocalNameCollector.{h,cpp}`, `SemanticAnalysis.{h,cpp}` |
| 6 | `TipOwningRef.{h,cpp}`, `TipBorrowRef.{h,cpp}`, `TipSumType.{h,cpp}`, `TipSeq.{h,cpp}` (stub) | `TypeConstraintCollectVisitor.{h,cpp}`, `TipTypeVisitor.h`, `TypeInference.{h,cpp}` |
| 7 | none | `TypeInference.{h,cpp}`, `SemanticAnalysis.{h,cpp}`, `CallGraph.{h,cpp}` |
| 8 | `OwnershipClassifier.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| 9 | `MoveAnalysis.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| 10 | `BorrowChecker.{h,cpp}` | `SemanticAnalysis.{h,cpp}` |
| 11 | `ASTDestroyStmt.{h,cpp}` | `CodeGenerator.cpp`, `CodeGenFunctions.cpp`, `SemanticAnalysis.{h,cpp}` |
