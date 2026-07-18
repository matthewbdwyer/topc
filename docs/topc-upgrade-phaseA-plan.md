# topc Upgrade Plan — Phase A: Product Types (Records)

## Objective

Add first-class structural record (product) types to TOP with complete end-to-end compiler support:
grammar, AST, type inference, ownership classification, recursive destruction, codegen, and test coverage.

Phase A is the required baseline for SOPC. It focuses on correctness and test coverage, not on pattern-language completeness (that is Phase B). The phase is organized into four sequential sub-phases, each with explicit validation gates and a named commit point. No sub-phase begins until the previous gate passes fully.

---

## Background: What Already Exists

The existing `test/system/selftests/` directory contains `.pppt` golden files for record programs but no corresponding `.top` source files.  These golden files define the intended observable behavior and serve as the acceptance tests for this phase.  The source programs that produce them do not yet exist.

Existing golden files to drive the work:

| Golden file | What it exercises |
|---|---|
| `record1.top.pppt` | Basic construction `{p:5,q:2}`, field read, all-Copy record |
| `record2.top.pppt` | `alloc {p:4,q:2}`, whole-record assignment via `*n = {...}`, field read through pointer |
| `record4.top.pppt` | Nested record with owning-pointer field `{c:⭡{...},d:int}`, deep field deref `*n.c.a` |
| `record.top.pppt` | Multi-function program, all-int record with three fields |
| `fieldAssign.top.pppt` | Field mutation `rec.f = expr`, two heterogeneous records |
| `returnRecord.top.pppt` | Record constructed in callee, returned by value, caller reads fields |
| `returnAllocRecord.top.pppt` | Record allocated in callee, returned as owning pointer, caller reads fields |
| `recordArgument.top.pppt` | Record passed by value to function, callee mutates and reads fields |
| `borrow-record-field.top.pppt` | Borrow of a record field `&r.val` passed as pointer argument |
| `destroy-record.top.pppt` | Record with an owning-pointer field; ownership/destruction of pointer field |

---

## Language Semantics Summary

Records in TOP are **structurally typed**: the type `{a:int,b:int}` is inferred, not declared.

- Construction: `{f1:e1, f2:e2, ...}` produces a record value.
- Field read: `e.f` reads field `f` of record `e`.
- Field write: `e.f = v;` mutates field `f` of record `e` (l-value weeding required).
- Field address: `&e.f` takes the address of a field.
- `alloc {f:e}` heap-allocates a record, yielding `⭡{f:T}`.
- Ownership class: all-Copy fields → record is Copy; any Own field → record is Own.

---

## Design Decisions (Locked Before Implementation)

The following decisions are required so implementation can proceed autonomously without back-and-forth.

1. **Record typing model:** closed structural records only (exact field-set match).
2. **No row polymorphism in Phase A:** functions cannot be polymorphic over "has field x" constraints.
3. **Field order rule:** source declaration order is preserved in AST and codegen layout.
4. **Type canonicalization rule:** record type identity canonicalizes by field-name sort for unification keys, but preserves source order for layout/printing.
5. **Duplicate fields:** syntactically accepted by parser, rejected by semantic/weeding pass.
6. **Record l-values:** `e.f` is assignable iff `e` is assignable under existing rules.
7. **Borrowing field address:** `&e.f` is allowed where existing borrow-position rules allow `&expr`.
8. **No record pattern matching in Phase A:** this is deferred to Phase B.

---

## Explicit Non-Goals for Phase A

1. Row-polymorphic record constraints.
2. Width/depth subtyping between records.
3. Record patterns in `case` arms.
4. Anonymous tuple syntax (records only).
5. Optimized packed/bitfield layout in codegen.

---

## Phase A1 — Grammar, AST, and Pretty-Printer

**Objective:** The parser accepts all record programs; AST nodes exist; pretty-printer output matches the golden `.pppt` files.

### 1.1 Grammar changes (`src/frontend/TOP.g4`)

Add to the `expr` rule (before `parenExpr`):

```antlr
| '{' nameExpr ':' expr (',' nameExpr ':' expr)* '}'   #recordExpr
| expr '.' IDENTIFIER                                   #fieldAccessExpr
```

Add `IDENTIFIER` as the field-name terminal in `nameExpr` if not already separate.

Ensure the assignment weeder in `CheckAssignableTest` accepts `expr.field` as an l-value.

### 1.2 New AST node files

| New file | Purpose |
|---|---|
| `src/frontend/ast/treetypes/ASTRecordExpr.h/.cpp` | Record construction `{f:e,...}` |
| `src/frontend/ast/treetypes/ASTFieldAccessExpr.h/.cpp` | Field read/write `e.f` |

Both must implement `accept(ASTVisitor*)` and participate in the existing visitor/iterator infrastructure.

### 1.3 ASTBuilder changes (`src/frontend/ast/ASTBuilder.cpp`)

Wire the new grammar rules to the new AST nodes in `visitRecordExpr` and `visitFieldAccessExpr`.

### 1.4 Pretty-printer changes (`src/frontend/prettyprint/`)

Emit `{f:e,...}` for `ASTRecordExpr` and `e.f` for `ASTFieldAccessExpr`.

### 1.5 New test programs

Write the following `.top` source files (content must produce the corresponding golden when run through `topc -pp -pt`):

| Source to create | Drives golden |
|---|---|
| `test/system/selftests/record1.top` | `record1.top.pppt` |
| `test/system/selftests/record2.top` | `record2.top.pppt` |
| `test/system/selftests/record4.top` | `record4.top.pppt` |
| `test/system/selftests/record.top` | `record.top.pppt` |
| `test/system/selftests/fieldAssign.top` | `fieldAssign.top.pppt` |
| `test/system/selftests/returnRecord.top` | `returnRecord.top.pppt` |
| `test/system/selftests/returnAllocRecord.top` | `returnAllocRecord.top.pppt` |
| `test/system/selftests/recordArgument.top` | `recordArgument.top.pppt` |
| `test/system/selftests/borrow-record-field.top` | `borrow-record-field.top.pppt` |
| `test/system/selftests/destroy-record.top` | `destroy-record.top.pppt` |

### 1.6 New unit tests

**`test/unit/frontend/TOPParserTest.cpp`** — add test cases:

| Test case name | What it checks |
|---|---|
| `TOP Parser: record construction` | `{a:1, b:2}` parses without error |
| `TOP Parser: record field read` | `r.field` parses without error |
| `TOP Parser: record field write` | `r.field = 5;` parses without error |
| `TOP Parser: alloc record` | `alloc {x:3, y:4}` parses without error |
| `TOP Parser: borrow of record field` | `&r.val` parses without error |
| `TOP Parser: nested record field deref` | `*r.inner.x` parses without error |
| `TOP Parser: record construction reject — missing colon` | `{a 1}` is a parse error |

**`test/unit/frontend/ASTBuilderTest.cpp`** — add test cases:

| Test case name | What it checks |
|---|---|
| `ASTBuilder: record construction node` | Visitor walks `ASTRecordExpr`, sees field names and sub-expressions |
| `ASTBuilder: field access node` | Visitor walks `ASTFieldAccessExpr`, sees base expression and field name |

**`test/unit/frontend/ASTPrinterTest.cpp`** — add test cases:

| Test case name | What it checks |
|---|---|
| `ASTPrinter: record construction round-trip` | Pretty-print of `{a:1,b:2}` matches expected string |
| `ASTPrinter: field access round-trip` | Pretty-print of `r.a` matches expected string |
| `ASTPrinter: alloc record round-trip` | Pretty-print of `alloc {x:1}` matches expected string |

**`test/unit/semantic/CheckAssignableTest.cpp`** — add test cases:

| Test case name | What it checks |
|---|---|
| `CheckAssignable: record field is assignable` | `r.f = 5;` accepted as l-value |
| `CheckAssignable: record literal is not assignable` | `{a:1} = 5;` rejected |

### 1.7 Validation gate — Phase A1

All of the following must pass with no regressions on any previously-passing test:

- All new `TOPParserTest` cases listed in §1.6.
- All new `ASTBuilderTest` and `ASTPrinterTest` cases listed in §1.6.
- New `CheckAssignableTest` cases listed in §1.6.
- The pretty-printer system tests for all ten `.top` source files match their `.pppt` golden files (run via `bin/runtests.sh -s`).

### 1.9 Mandatory file touch list — Phase A1

At minimum, Phase A1 is expected to modify/create in:

1. `src/frontend/TOP.g4` and `topg4/TOP.g4`
2. `src/frontend/ast/ASTBuilder.h/.cpp`
3. `src/frontend/ast/ASTVisitor.h`
4. `src/frontend/ast/treetypes/ASTRecordExpr.h/.cpp`
5. `src/frontend/ast/treetypes/ASTFieldAccessExpr.h/.cpp`
6. `src/frontend/ast/treetypes/AST.h`
7. `src/frontend/ast/CMakeLists.txt`
8. `src/frontend/prettyprint/PrettyPrinter.h/.cpp`
9. `test/unit/frontend/TOPParserTest.cpp`
10. frontend AST/pretty-printer unit test files listed in §1.6

### 1.8 Commit point A1

```
feat(frontend): record construction, field access, and pretty-printer (Phase A1)
```

Tag: `phaseA1-parser-ast`

---

## Phase A2 — Type Inference and Semantic Analysis

**Objective:** Records have a proper type term; type inference infers field types; field-access and mutation are type-checked; duplicate and unknown-field errors are reported.

### 2.1 New type term

Create `src/semantic/types/concrete/TopRecordType.h/.cpp` modeled on `TopSumType`.

- Stores an ordered list of `(fieldName, shared_ptr<TopType>)` pairs.
- `getFunctor()` returns `"record_" + canonical_field_order_key` (field names sorted or in declaration order — choose one and document it).
- `arity()` returns the number of fields.
- `doMatch(TopType*)` returns true only for `TopRecordType` with same field names in same order.
- Visitor pattern: add `visitTopRecordType` to `TopTypeVisitor.h`.

### 2.2 Substituter and copier updates

`src/semantic/types/concrete/SubstituterTest.cpp` (and the production classes) must handle `TopRecordType` without skipping fields.

### 2.3 Type constraint generation

In the type inference/constraint generation pass:

- Record construction `{f1:e1,...}` generates a `TopRecordType([(f1,T1),...])` constraint.
- Field read `e.f` generates a constraint that the type of `e` must have field `f` of the result type.
- Field write `e.f = v` generates the same field constraint, with `v`'s type unified against the field type.

### 2.4 Semantic checks

In the semantic analysis pass:

- **Unknown field access**: `e.f` where the inferred type of `e` has no field `f` → error `"unknown field 'f'"`.
- **Type mismatch on write**: `e.f = v` where type of `v` does not unify with field type → error.
- **Duplicate field in construction**: `{a:1, a:2}` → error `"duplicate field 'a' in record expression"` (may already be caught by weeder — if so, confirm and note).

### 2.5 New type-term unit tests

**`test/unit/semantic/types/concrete/TopRecordTypeTest.cpp`** — new file:

| Test case name | What it checks |
|---|---|
| `TopRecordType: getFieldNames returns declared order` | Fields in declared order |
| `TopRecordType: doMatch true for same fields same order` | Structural identity |
| `TopRecordType: doMatch false for different field names` | `{a:int}` ≠ `{b:int}` |
| `TopRecordType: doMatch false for different arity` | `{a:int}` ≠ `{a:int,b:int}` |
| `TopRecordType: accepts TopTypeVisitor` | Visitor dispatch fires |

**`test/unit/semantic/types/concrete/SubstituterTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `Substituter: substitutes into record field types` | `{a:α}` under `{α→int}` yields `{a:int}` |
| `Substituter: identity on record with no vars` | `{a:int,b:int}` unchanged |

### 2.6 New semantic unit tests

**`test/unit/semantic/SymbolTableTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `SymbolTable: record type from construction` | Variable assigned `{p:5}` has record type in locals map |
| `SymbolTable: field access type resolved` | `r.p` in a typed program has type `int` |

Add a new file **`test/unit/semantic/RecordSemanticTest.cpp`**:

| Test case name | What it checks |
|---|---|
| `RecordSemantic: all-int record accepted` | `{a:1,b:2}` type-checks without error |
| `RecordSemantic: field read type-checks` | `r.a` resolves to `int` after `r = {a:1}` |
| `RecordSemantic: field write type-checks` | `r.a = 5;` accepted when `r:{a:int}` |
| `RecordSemantic: unknown field rejected` | `r.z` when `r:{a:int}` → error containing `"unknown field"` |
| `RecordSemantic: duplicate field in construction rejected` | `{a:1,a:2}` → error containing `"duplicate field"` |
| `RecordSemantic: record passed to and returned from function` | `foo(r)` where `foo` takes and returns `{l:int,m:int}` type-checks |
| `RecordSemantic: alloc record yields owning ref to record` | `alloc {p:4}` has type `⭡{p:int}` |
| `RecordSemantic: monomorphic record function accepted` | `foo(r) { return r.x; }` type-checks when call sites use one exact record shape |

### 2.7 System test gate

After A2, `topc -pp -pt` on each of the ten source programs must produce output that exactly matches the corresponding `.pppt` golden file (including the type annotation block at the bottom).

### 2.8 Validation gate — Phase A2

All of the following must pass with no regressions:

- All new `TopRecordTypeTest` cases.
- New `SubstituterTest` cases for records.
- New `SymbolTableTest` cases.
- All new `RecordSemanticTest` cases.
- All ten pretty-print system tests pass (type annotation block correct).
- All previously-passing Phase A1 tests still pass.

### 2.10 Mandatory file touch list — Phase A2

At minimum, Phase A2 is expected to modify/create in:

1. `src/semantic/types/concrete/TopRecordType.h/.cpp`
2. `src/semantic/types/concrete/Type.h`
3. `src/semantic/types/concrete/TopTypeVisitor.h`
4. `src/semantic/types/solver/Substituter.h/.cpp`
5. type-constraint visitor/collector files that currently handle sum constructors/case typing
6. `src/semantic/weeding/*` or semantic-check files for duplicate/unknown field diagnostics
7. `src/semantic/types/CMakeLists.txt` (if needed for new type term)
8. semantic/type unit test files listed in §2.5–§2.6

### 2.9 Commit point A2

```
feat(semantic): TopRecordType term, field inference, and record semantic checks (Phase A2)
```

Tag: `phaseA2-type-semantic`

---

## Phase A3 — Ownership Classification and Recursive Destruction

**Objective:** Records are classified as Copy or Own based on their field ownership composition; the destruction pass recursively frees owned fields; move and borrow rules apply correctly to record-containing values.

### 3.1 Ownership classifier

Update `src/semantic/OwnershipClassifier.cpp`:

- `classifyType(TopRecordType*)`: iterate fields; if all are Copy → return `Copy`; if any field is Own → return `Own`.
- The rule is compositional and recursive (a field of type `⭡T` is Own; a field of type `{a:⭡int}` makes the outer record Own).

### 3.2 Destruction pass

Update `src/semantic/DestructionPass.cpp`:

- When destroying a record value that is Own: emit destructor calls for each Own field in reverse declaration order before freeing/releasing the record itself.
- When destroying an `alloc`-allocated record (`⭡{...}`): free each Own field, then free the heap block.

### 3.3 Move analysis

Update `src/semantic/MoveAnalysis.cpp`:

- Assignment of an Own record (`e.f` is Own) is a move, not a copy; the source becomes invalid.
- `e.f` where `e` is Own and the field is Own must not be used as an r-value when `e` has already been moved.

### 3.4 Borrow checker

Update `src/semantic/BorrowChecker.cpp`:

- `&r.f` creates a borrow of field `f`; the borrow checker must prevent mutation of `r` while the borrow is live.
- A borrow of a Copy field is equivalent to borrowing the field value (no extra tracking needed beyond existing borrow semantics).

### 3.5 New unit tests

**`test/unit/semantic/OwnershipClassifierTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `OwnershipClassifier: all-Copy record is Copy` | `{a:int,b:int}` → `Copy` |
| `OwnershipClassifier: record with owning-ref field is Own` | `{p:⭡int,tag:int}` → `Own` |
| `OwnershipClassifier: nested all-Copy record is Copy` | `{x:{a:int}}` → `Copy` |
| `OwnershipClassifier: record with nested Own field is Own` | `{x:{p:⭡int}}` → `Own` |

**`test/unit/semantic/MoveAnalysisTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `MoveAnalysis: Copy record assignment is not a move` | `r2 = r1;` where `r1:{a:int}` — `r1` still usable |
| `MoveAnalysis: Own record assignment is a move` | `r2 = r1;` where `r1:{p:⭡int}` — `r1` invalid after |
| `MoveAnalysis: use after move of Own record rejected` | Access to `r1.p` after `r2 = r1` → error containing `"moved"` |
| `MoveAnalysis: Own field move out of record rejected` | `x = r.p;` where `r:{p:⭡int}` → error |

**`test/unit/semantic/BorrowCheckerTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `BorrowChecker: borrow of record field accepted` | `readField(&r.val)` where `val:int` — accepted |
| `BorrowChecker: mutation of record while field borrowed rejected` | `r = {val:0};` while `&r.val` borrow live → error |

Add a new file **`test/unit/semantic/DestructionPassRecordTest.cpp`**:

| Test case name | What it checks |
|---|---|
| `DestructionPassRecord: Copy record has no destructor` | `{a:int,b:int}` — destruction pass emits no free for the record |
| `DestructionPassRecord: Own record field freed` | `{payload:⭡int}` — destruction pass emits free of `payload` field |
| `DestructionPassRecord: alloc record fields freed before block` | `alloc {payload:⭡int}` — field freed, then block freed |
| `DestructionPassRecord: nested Own record fields freed recursively` | Three-level nested Own — all inner owners freed before outer |

### 3.6 System test gate

The following self-tests must pass as **end-to-end run tests** (compile, link, execute, exit 0):

- `test/system/selftests/destroy-record.top` — exercises Own field destruction.
- `test/system/selftests/borrow-record-field.top` — exercises field borrow.
- `test/system/selftests/record4.top` — exercises nested owning pointer field.

### 3.7 Validation gate — Phase A3

All of the following must pass with no regressions:

- All new `OwnershipClassifierTest` cases for records.
- All new `MoveAnalysisTest` cases for records.
- All new `BorrowCheckerTest` cases for records.
- All new `DestructionPassRecordTest` cases.
- Three run-time system tests listed in §3.6 exit 0.
- All Phase A1 and A2 tests still pass.

### 3.8 Commit point A3

```
feat(semantic): record ownership classification, move rules, and recursive destruction (Phase A3)
```

Tag: `phaseA3-ownership-destruction`

### 3.9 Mandatory file touch list — Phase A3

At minimum, Phase A3 is expected to modify/create in:

1. `src/semantic/OwnershipClassifier.cpp`
2. `src/semantic/MoveAnalysis.cpp`
3. `src/semantic/BorrowChecker.cpp`
4. `src/semantic/DestructionPass.cpp`
5. semantic unit tests listed in §3.5

---

## Phase A4 — Codegen and Full End-to-End System Tests

**Objective:** All ten record system tests compile, link, and execute correctly. Diagnostics for all error paths are reviewed.

### 4.1 Codegen — record value layout

In `src/codegen/CodeGenFunctions.cpp` / `CodeGenVisitor.cpp`:

- `ASTRecordExpr`: allocate an LLVM struct (or equivalent aggregate); store each field value in order.
- `ASTFieldAccessExpr` (r-value): GEP to the field offset; load.
- `ASTFieldAccessExpr` (l-value, assignment target): GEP to field offset; store.
- `alloc {f:e}`: heap-allocate (via `calloc`) a record struct; fill fields; return `⭡{...}`.
- `*r.f`: dereference owning-pointer field, then load.
- `&r.f`: GEP to field; do not load (produce pointer).

### 4.2 Codegen — destruction integration

Emit destruction code for Own record fields at the points determined by Phase A3:

- At scope exit for locally-owned records: free each Own field, then free the aggregate if heap-allocated.
- Match field-free order to what `DestructionPassRecordTest` validates.

### 4.3 New codegen unit tests

**`test/unit/codegen/CodegenFunctionsTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `CodegenFunctions: record construction emits struct store` | IR contains expected `store` for each field |
| `CodegenFunctions: field read emits GEP + load` | IR contains `getelementptr` and `load` for `r.f` |
| `CodegenFunctions: field write emits GEP + store` | IR contains `getelementptr` and `store` for `r.f = v` |
| `CodegenFunctions: alloc record emits calloc + stores` | IR contains `call calloc` and field stores |
| `CodegenFunctions: Own field destruction emits free` | IR contains `call free` for owned field at scope exit |

### 4.4 Full end-to-end system tests

All ten self-tests must produce correct output (exit 0):

| Test program | Primary behavior validated |
|---|---|
| `record1.top` | Basic construction and field read |
| `record2.top` | Heap-allocated record, whole-record rewrite, deref field read |
| `record4.top` | Nested record with owning pointer, deep deref field read |
| `record.top` | Multi-function program, three-field all-int record |
| `fieldAssign.top` | Field mutation, two heterogeneous records |
| `returnRecord.top` | Record returned by value from callee, fields read in caller |
| `returnAllocRecord.top` | Owning-pointer to record returned from callee |
| `recordArgument.top` | Record passed by value, callee mutates fields |
| `borrow-record-field.top` | Field borrow passed as pointer argument |
| `destroy-record.top` | Owned field correctly destroyed at scope exit |

Run via `bin/runtests.sh -s`.

### 4.5 Diagnostics review

For each of the following error programs, run `topc` and confirm the diagnostic message is actionable:

| Error case | Expected diagnostic substring |
|---|---|
| Unknown field read | `"unknown field"` |
| Duplicate field in construction | `"duplicate field"` |
| Use of record after move | `"moved"` |
| Mutation while field borrow live | `"borrow"` |

Write one negative system test per case in `test/system/selftests/`:

| File | Diagnostic expected |
|---|---|
| `record-unknown-field.top` | compile fails, message contains `"unknown field"` |
| `record-dup-field.top` | compile fails, message contains `"duplicate field"` |
| `record-use-after-move.top` | compile fails, message contains `"moved"` |
| `record-borrow-conflict.top` | compile fails, message contains `"borrow"` |

### 4.6 Validation gate — Phase A4

All of the following must pass:

- All five new `CodegenFunctionsTest` cases.
- All ten positive self-tests exit 0.
- All four negative system tests fail to compile with the expected diagnostic substrings.
- Full test suite run (`bin/runtests.sh`) shows no regressions on any prior test.

### 4.7 Commit point A4

```
feat(codegen): record lowering, destruction, and full end-to-end system tests (Phase A4)
```

Tag: `phaseA4-codegen-e2e`

### 4.8 Mandatory file touch list — Phase A4

At minimum, Phase A4 is expected to modify/create in:

1. `src/codegen/CodeGenVisitor.h/.cpp`
2. `src/codegen/CodeGenFunctions.cpp` (if record helpers are centralized there)
3. `src/codegen/CodeGenContext.h` (if new layout/type caches are needed)
4. `test/unit/codegen/CodegenFunctionsTest.cpp`
5. positive and negative system test files listed in §4.4–§4.5

---

## Phase A Complete — Acceptance Criteria

Phase A is closed when all four sub-phase commit tags exist and the following hold:

1. All ten positive record system tests exit 0.
2. All four negative diagnostic system tests fail at compile with the expected message substrings.
3. All unit tests in `test/unit/` pass (no regressions against any pre-Phase-A baseline).
4. `OwnershipClassifier` correctly classifies all-Copy and mixed-Own record types.
5. Recursive destruction is verified by `DestructionPassRecordTest` and by the `destroy-record.top` runtime test.
6. Documentation (`README.md` and this plan) reflects the record classification rules.

---

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Field-access l-value weeding interacts badly with existing pointer-deref l-value rules | Extend `CheckAssignableTest` with combined cases before touching the weeder |
| Record field order matters for GEP offsets but inference may reorder | Fix canonical field order at the point of type unification; document and test that `getFunctor` key is deterministic |
| Destruction ordering for nested Owns is tricky to emit correctly | Nail down order in `DestructionPassRecordTest` before writing codegen; tests are the spec |
| `alloc`-record interacts with existing owning-ref destruction paths | Write `returnAllocRecord.top` runtime test before touching `DestructionPass`; if it already passes, it is a regression guard |
| Grammar change breaks existing sum-type tests | Run full parser unit suite after every grammar edit; fix before proceeding |

---

## Implementation Order Within Each Phase

Work items within a phase should be ordered as follows to keep the test suite green at each step:

1. Grammar / type-term / data-structure change (no semantic logic yet).
2. Visitor/printer wiring (pretty-print tests can then pass).
3. Semantic/type logic.
4. Ownership/destruction logic.
5. Codegen.

Never commit a step whose unit tests are failing. Each step is independently buildable even if not all tests pass yet.

---

## Autonomous Execution Protocol

This section is the contract for autonomous implementation.

### A. Pre-flight checks (must run once)

1. Confirm branch is `product-types`.
2. Confirm clean working tree or explicitly list unrelated dirty files.
3. Build once before changes to ensure baseline status is known.

### B. Phase gate command set

Use these commands as the default validation sequence at each gate:

1. Build:
	- `cmake --build build -j$(sysctl -n hw.ncpu)`
2. Focused unit suites (new/modified test binaries):
	- run the relevant unit test executables for frontend, semantic, and codegen as each phase requires
3. System tests:
	- `bin/runtests.sh -s` for selftests/goldens
	- `bin/runtests.sh` for full regression at final gate

### C. Stop conditions (must pause and ask)

Implementation must stop and request direction if any of the following occur:

1. A design decision in "Design Decisions" must be violated to make tests pass.
2. Existing golden `.pppt` outputs conflict with structural record semantics.
3. A required rule implies row polymorphism or subtyping behavior.
4. A runtime layout choice would break previously passing non-record programs.

### D. Commit discipline

1. One commit per phase gate (A1–A4), plus optional small fixup commits before each gate tag.
2. Do not squash across phase gates.
3. Tag each successful phase using the tags defined in this plan.

### E. Definition of done for autonomous implementation request

An autonomous implementation request is complete only when:

1. All A1–A4 gates pass exactly as defined.
2. Required tags exist and point to passing commits.
3. The final report includes:
	- changed file list by phase,
	- tests run and pass/fail summary,
	- any deviations from the plan and rationale.
