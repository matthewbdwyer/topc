# topc Upgrade Plan — Phase B: Nested Patterns

## Objective

Extend the case-arm pattern language to support wildcard patterns, record patterns in payload positions, and nested constructor patterns.  Build on the Phase A product-type baseline to allow destructuring a constructor's record-typed payload directly inside the arm, eliminating the need to bind it to a temporary and then access fields in the body.

Phase B is a completeness and ergonomics layer.  It must not weaken any ownership, destruction, or type-safety guarantee introduced in Phase A.

**Prerequisite**: all four Phase A commit tags (`phaseA1-parser-ast` through `phaseA4-codegen-e2e`) must exist and the full test suite must pass before Phase B begins.

---

## Design Decisions (Locked Before Implementation)

These decisions keep Phase B implementable and prevent hidden coupling back into Phase A.

1. **No change to Phase A record representation**: Phase B consumes Phase A record/ownership semantics as-is.
2. **Case arm shape in Phase B**: keep top-level arm dispatch keyed by constructor tag (`CONID`) and introduce patterns only in payload positions.
3. **Pattern typing discipline**: no row-polymorphic pattern constraints in Phase B.
4. **Record-pattern policy**: record patterns are exact-field in Phase B (all fields must be mentioned).
5. **Binding uniqueness**: variable names must be unique within one arm; re-use across different arms remains allowed.
6. **Coverage policy**: top-level constructor exhaustiveness remains mandatory as in Phase A.
7. **Nested-pattern completeness policy**: apply conservative checks only (see B3); defer full pattern-matrix algorithm.

---

## Explicit Non-Goals for Phase B

1. Full ML-style pattern matrix compilation.
2. Or-patterns, guards, as-patterns, or typed patterns.
3. Partial record patterns with implicit wildcard fields.
4. Cross-arm unification of binding sets.
5. Changes to borrow model beyond pattern binding/plumbing.

---

## Background: What Phase A Gives You

After Phase A, a case arm looks like this (grammar-level):

```antlr
caseArm : CONID ('(' IDENTIFIER (',' IDENTIFIER)* ')')? ARROW statement ;
```

Payload positions are flat `IDENTIFIER` tokens — plain variable bindings.  That means you can write:

```top
case opt of {
  Some(r) -> output r.x;   // r is bound; field access happens in the body
  None    -> output 0;
}
```

But you cannot write the semantically equivalent:

```top
case opt of {
  Some({x:v}) -> output v;   // record pattern in payload — Phase B
  None        -> output 0;
}
```

Phase B adds a first-class `pattern` grammar construct and replaces `IDENTIFIER` payload positions with arbitrary patterns.

---

## Pattern Language Design

A pattern at any position is one of:

| Form | Syntax | Semantics |
|---|---|---|
| Variable | `x` (lowercase IDENTIFIER) | Binds the matched value to `x`; always succeeds |
| Wildcard | `_` | Discards the matched value; always succeeds |
| No-arg ctor | `Ctor` (uppercase CONID) | Succeeds only if matched value has tag `Ctor` |
| Ctor with sub-patterns | `Ctor(p1, p2, ...)` | Succeeds if tag matches and each payload position matches its sub-pattern |
| Record | `{f1:p1, f2:p2, ...}` | Succeeds if the value is a record and each field matches its sub-pattern |

A case arm uses this grammar:

```antlr
caseArm : CONID ('(' pattern (',' pattern)* ')')? ARROW statement ;
pattern : IDENTIFIER | '_' | CONID | CONID '(' pattern (',' pattern)* ')' 
        | '{' IDENTIFIER ':' pattern (',' IDENTIFIER ':' pattern)* '}' ;
```

Variable and wildcard patterns are **irrefutable** (always match); constructor and record patterns are **refutable** (may fail to match).  All variable names in a single arm must be distinct.

---

## Phase B1 — Pattern Grammar, AST, and Pretty-Printer

**Objective**: The parser accepts all pattern forms; AST nodes for patterns exist; the pretty-printer emits canonical pattern syntax; no semantic checking yet.

### 1.1 Grammar changes (`src/frontend/TOP.g4`)

- Add the `pattern` rule as defined above.
- Change `caseArm` to use `pattern` instead of `IDENTIFIER` in payload positions.
- The top-level of a case arm is still `CONID → statement`; only the payload positions become patterns.

### 1.2 New AST pattern node files

| New file | Purpose |
|---|---|
| `src/frontend/ast/treetypes/ASTPattern.h` | Abstract base for all pattern nodes |
| `src/frontend/ast/treetypes/ASTVarPattern.h/.cpp` | Variable-binding pattern `x` |
| `src/frontend/ast/treetypes/ASTWildcardPattern.h/.cpp` | Wildcard pattern `_` |
| `src/frontend/ast/treetypes/ASTCtorPattern.h/.cpp` | Constructor pattern `Ctor(p,...)` |
| `src/frontend/ast/treetypes/ASTRecordPattern.h/.cpp` | Record pattern `{f:p,...}` |

All pattern nodes must implement `accept(ASTVisitor*)`.  Add visitor hooks `visit(ASTVarPattern*)`, `visit(ASTWildcardPattern*)`, `visit(ASTCtorPattern*)`, `visit(ASTRecordPattern*)` to `ASTVisitor.h`.

### 1.3 ASTCaseArm changes (`src/frontend/ast/treetypes/ASTCaseArm.h/.cpp`)

Replace `BINDINGS: vector<shared_ptr<ASTDeclNode>>` with `PATTERNS: vector<shared_ptr<ASTPattern>>`.

Update `getBindings()` / `getChildren()` accordingly.  All callers (`LocalNameCollector`, `MoveAnalysis`, `DestructionPass`, `CheckCaseCompleteness`, `CodeGenVisitor`) will need to be updated in later phases; for now, temporarily preserve the old interface by deriving flat bindings from the pattern tree.

### 1.4 ASTBuilder changes (`src/frontend/ast/ASTBuilder.cpp`)

Wire `visitPattern` and its sub-rules to the new AST nodes.  Wire the updated `visitCaseArm` to produce `ASTPattern` objects for payload positions.

### 1.5 Pretty-printer changes

Emit canonical pattern syntax from each pattern node type:

| Node | Emitted text |
|---|---|
| `ASTVarPattern` | `x` |
| `ASTWildcardPattern` | `_` |
| `ASTCtorPattern` (no-arg) | `Ctor` |
| `ASTCtorPattern` (with sub-patterns) | `Ctor(p1, p2)` |
| `ASTRecordPattern` | `{f1:p1, f2:p2}` |

### 1.6 New test programs

Write the following `.top` programs **and** their corresponding `.pppt` golden files (pretty-print output of `topc -pp -pt`):

| Source to create | Content |
|---|---|
| `test/system/selftests/pattern-wildcard.top` | Case arm with `_` wildcard in payload position |
| `test/system/selftests/pattern-record.top` | Case arm with `{f:v}` record pattern in constructor payload |
| `test/system/selftests/pattern-nested-ctor.top` | Case arm with `Ctor(p)` nested constructor pattern in payload position |
| `test/system/selftests/pattern-mixed.top` | Single function mixing variable, wildcard, and record patterns across arms |

Create matching `.pppt` golden files alongside each source.

### 1.7 New unit tests

**`test/unit/frontend/TOPParserTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `TOP Parser: wildcard pattern in case arm` | `case x of { Some(_) -> ...; None -> ...; }` parses |
| `TOP Parser: record pattern in case arm payload` | `case x of { Some({a:v}) -> ...; None -> ...; }` parses |
| `TOP Parser: nested ctor pattern` | `case x of { Pair(Fst(v), _) -> ...; }` parses |
| `TOP Parser: flat variable pattern still works` | Existing flat-binding syntax unchanged |
| `TOP Parser: bare ctor payload pattern parses` | `Some(X)` parses as a constructor sub-pattern (semantic validity checked later) |

**`test/unit/frontend/ASTBuilderTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `ASTBuilder: wildcard pattern node` | `_` produces `ASTWildcardPattern` |
| `ASTBuilder: var pattern node` | `x` produces `ASTVarPattern` with name `"x"` |
| `ASTBuilder: record pattern node` | `{a:v}` produces `ASTRecordPattern` with one field |
| `ASTBuilder: nested ctor pattern node` | `Ctor(Sub(x))` produces `ASTCtorPattern` containing `ASTCtorPattern` |

**`test/unit/frontend/ASTPrinterTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `ASTPrinter: wildcard pattern round-trip` | `_` emitted as `_` |
| `ASTPrinter: record pattern round-trip` | `{a:v}` emitted as `{a:v}` |
| `ASTPrinter: nested ctor pattern round-trip` | `Ctor(Sub(x))` emitted as `Ctor(Sub(x))` |

### 1.8 Validation gate — Phase B1

- All new parser, ASTBuilder, and ASTPrinter test cases pass.
- Pretty-printer system tests for the four new programs match their `.pppt` golden files.
- All existing Phase A tests still pass (no regressions in existing case-arm handling).
- `sumtype-basic.top`, `sumtype-nested.top`, `sumtype-poly.top`, `shapes.top`, `tree.top`, `option.top` all still pass.

### 1.9 Commit point B1

```
feat(frontend): pattern grammar, AST hierarchy, and pretty-printer (Phase B1)
```

Tag: `phaseB1-pattern-ast`

---

## Phase B2 — Semantic Typing of Patterns

**Objective**: Patterns are type-checked against the matched type; variable bindings receive correct types; record patterns reject unknown fields and arity mismatches; `LocalNameCollector` collects bindings from nested patterns.

### 2.1 Pattern type compatibility rules

Add `CheckPatternTypes` (new file `src/semantic/weeding/CheckPatternTypes.h/.cpp`) as a post-type-inference weeding pass:

| Pattern | Matched against type | Compatibility rule |
|---|---|---|
| `ASTVarPattern(x)` | any `T` | always compatible; `x` gets type `T` |
| `ASTWildcardPattern` | any `T` | always compatible; no binding |
| `ASTCtorPattern(Ctor)` no-arg | sum type containing `Ctor` | tag must match; arity must be 0 |
| `ASTCtorPattern(Ctor, [p1,...])` | sum type containing `Ctor` | tag must match; each `pi` compatible with `Ctor`'s `i`-th payload type |
| `ASTRecordPattern([f:p,...])` | record type `{f:T,...}` | each field name must exist; each `pi` compatible with field `i`'s type; no extra fields |

Errors raised by this pass:

| Error | Message substring |
|---|---|
| Unknown constructor in nested ctor pattern | `"unknown constructor"` |
| Arity mismatch in nested ctor pattern | `"expects N binding(s)"` |
| Unknown field name in record pattern | `"unknown field"` |
| Duplicate binding name within one arm | `"duplicate binding"` |
| Record pattern applied to non-record type | `"record pattern requires a record type"` |
| Ctor pattern applied to non-sum type | `"constructor pattern requires a sum type"` |

### 2.2 LocalNameCollector updates (`src/semantic/symboltable/LocalNameCollector.cpp`)

Currently collects names from `ASTCaseArm::getBindings()` (flat `ASTDeclNode` list).  Update to walk the pattern tree recursively and collect every `ASTVarPattern` name found.  Wildcards contribute no names.

Conflict detection: two `ASTVarPattern` nodes with the same name in the same arm → error `"duplicate binding 'x' in case arm"`.

### 2.3 Symbol table / type environment

After `LocalNameCollector` runs, the symbol table contains all pattern-bound names.  The type inference pass must assign each pattern-bound name the type derived by the compatibility rule above.  Confirm that the type annotations in the `.pppt` golden files for B1 programs are correct after this phase runs.

### 2.4 New unit tests

Add a new file **`test/unit/semantic/PatternTypingTest.cpp`**:

| Test case name | What it checks |
|---|---|
| `PatternTyping: variable pattern gets type from matched payload` | `Some(x)` — `x` has the payload type of `Some` |
| `PatternTyping: wildcard pattern accepted with no binding` | `Some(_)` accepted; no `_` in symbol table |
| `PatternTyping: record pattern fields match record type` | `Some({a:v})` where `Some` carries `{a:int}` — `v` typed `int` |
| `PatternTyping: record pattern unknown field rejected` | `Some({z:v})` where `Some` carries `{a:int}` → error `"unknown field"` |
| `PatternTyping: record pattern arity mismatch rejected` | `Some({a:v,b:w})` where `Some` carries `{a:int}` → error |
| `PatternTyping: nested ctor pattern type-checks` | `Outer(Inner(x))` where `Inner` has one `int` payload — `x` typed `int` |
| `PatternTyping: nested ctor unknown constructor rejected` | `Some(Bogus(x))` — `Bogus` not declared → error `"unknown constructor"` |
| `PatternTyping: duplicate binding in arm rejected` | `Some({a:x, b:x})` → error `"duplicate binding"` |
| `PatternTyping: record pattern on non-record type rejected` | `Some({a:v})` where `Some` carries `int` → error `"record pattern requires"` |

**`test/unit/semantic/LocalNameCollectorTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `LocalNameCollector: wildcard contributes no binding` | `case x of { Some(_) -> ...; None -> ...; }` — `_` not in symbol table |
| `LocalNameCollector: record pattern bindings collected` | `Some({a:v,b:w})` — `v` and `w` in symbol table |
| `LocalNameCollector: nested ctor bindings collected` | `Outer(Inner(x))` — `x` in symbol table |
| `LocalNameCollector: duplicate binding in arm rejected` | `Some({a:x, b:x})` — error |

**`test/unit/semantic/Phase4WeedingTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `CheckPatternTypes: valid record pattern accepted` | `Some({a:v})` matching declared payload type — no error |
| `CheckPatternTypes: nested ctor valid` | `Outer(Inner(x))` — no error |
| `CheckPatternTypes: record pattern unknown field rejected` | → error `"unknown field"` |
| `CheckPatternTypes: ctor pattern arity mismatch rejected` | → error `"expects"` |
| `CheckPatternTypes: record pattern on non-record rejected` | → error `"record pattern requires"` |

### 2.5 System test gate

After B2, `topc -pp -pt` on all four B1 programs must produce type annotation blocks that match the `.pppt` golden files (now including binding-type annotations for pattern-bound variables).

### 2.6 Validation gate — Phase B2

- All new `PatternTypingTest` cases pass.
- New `LocalNameCollectorTest` cases pass.
- New `Phase4WeedingTest` cases for `CheckPatternTypes` pass.
- All four B1 pretty-print system tests pass (with correct type annotations).
- All Phase A and Phase B1 tests still pass.

### 2.8 Mandatory file touch list — Phase B2

At minimum, Phase B2 is expected to modify/create in:

1. `src/semantic/weeding/CheckPatternTypes.h/.cpp`
2. `src/semantic/weeding/CMakeLists.txt`
3. `src/semantic/SemanticAnalysis.cpp` (wire the new pass in correct order)
4. `src/semantic/symboltable/LocalNameCollector.h/.cpp`
5. semantic unit tests listed in §2.4

### 2.7 Commit point B2

```
feat(semantic): pattern type checking and LocalNameCollector nested-pattern support (Phase B2)
```

Tag: `phaseB2-pattern-typing`

---

## Phase B3 — Exhaustiveness and Redundancy Checking for Nested Patterns

**Objective**: `CheckCaseCompleteness` correctly determines whether a set of arms with nested patterns is exhaustive; redundant arms are flagged; the existing flat-constructor checks remain unaffected.

### 3.1 Exhaustiveness algorithm update (`src/semantic/weeding/CheckCaseCompleteness.cpp`)

The current algorithm checks: (a) all constructors of the scrutinee's sum type appear exactly once, (b) each arm's binding arity matches the declared arity.

Phase B extends this to handle **irrefutable sub-patterns**:

- A `_` or variable pattern in a payload position is irrefutable — it always matches that position.
- A refutable sub-pattern (nested ctor or record pattern) narrows the match space.

The extended algorithm (conservative Phase B form):

1. The top-level arm coverage check (which constructors appear, how many times) is unchanged.
2. For duplicate top-level constructor arms, apply only **syntactic redundancy checks**:
  - If an earlier arm for the same top-level constructor has irrefutable payload patterns (`_` or vars at every payload position), later arms for that constructor are unreachable.
  - If two adjacent arms for the same constructor have syntactically identical payload patterns, the later one is unreachable.
3. Full sub-pattern-space completeness for duplicate-constructor arms is deferred to a later phase.

**Important simplification**: duplicate-constructor arm analysis is intentionally conservative in Phase B.  This keeps the checker sound-with-respect-to-diagnostics while avoiding a full pattern-matrix implementation.

### 3.2 Error messages

| Situation | Error substring |
|---|---|
| Nested pattern leaves a constructor uncovered | `"not exhaustive"` |
| Redundant arm (fully shadowed by earlier arm) | `"unreachable case arm"` |

### 3.3 New unit tests

**`test/unit/semantic/Phase4WeedingTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `CheckCaseCompleteness: wildcard payload does not break coverage` | `case x of { Some(_) -> ...; None -> ...; }` — still exhaustive |
| `CheckCaseCompleteness: record pattern arm exhaustive` | `Some({a:v})` — single arm for `Some` is exhaustive for that ctor |
| `CheckCaseCompleteness: redundant wildcard arm rejected` | Two arms for `Some`, second using `_` after a general `Some(v)` arm → `"unreachable case arm"` |
| `CheckCaseCompleteness: duplicate ctor with catch-all first arm rejects later specific arm` | `Some(x)` followed by `Some(Inner(y))` → `"unreachable case arm"` |
| `CheckCaseCompleteness: identical duplicate ctor arm rejected` | two equivalent `Some(Inner(x))` arms → `"unreachable case arm"` |

Add a new file **`test/unit/semantic/ExhaustivenessNestedTest.cpp`**:

| Test case name | What it checks |
|---|---|
| `ExhaustivenessNested: irrefutable sub-pattern covers any payload` | `Ctor(x)` subsumes all sub-patterns for that ctor |
| `ExhaustivenessNested: wildcard covers any position` | `Ctor(_)` is irrefutable at that position |
| `ExhaustivenessNested: record pattern with wildcard field covers that field` | `{a:_, b:v}` — `a` position covered by wildcard |
| `ExhaustivenessNested: partial record pattern missing field rejected` | `{a:v}` for type `{a:int,b:int}` → error (all fields required in Phase B) |
| `ExhaustivenessNested: redundant arm after catch-all rejected` | Arm after `Ctor(x)` (irrefutable) that also matches `Ctor` → `"unreachable case arm"` |

Partial record patterns are not supported in Phase B; all fields must appear.

### 3.4 Negative system tests

Write four error programs in `test/system/selftests/`:

| File | Expected failure |
|---|---|
| `pattern-nonexhaustive-nested.top` | compile fails, message contains `"not exhaustive"` |
| `pattern-redundant-arm.top` | compile fails, message contains `"unreachable case arm"` |
| `pattern-record-unknown-field.top` | compile fails, message contains `"unknown field"` |
| `pattern-dup-binding.top` | compile fails, message contains `"duplicate binding"` |

### 3.5 Validation gate — Phase B3

- All new `Phase4WeedingTest` cases pass.
- All new `ExhaustivenessNestedTest` cases pass.
- All four negative system tests fail to compile with the expected message substring.
- All Phase A, B1, and B2 tests still pass.

### 3.6 Commit point B3

```
feat(semantic): exhaustiveness and redundancy checking for nested patterns (Phase B3)
```

Tag: `phaseB3-exhaustiveness`

### 3.7 Mandatory file touch list — Phase B3

At minimum, Phase B3 is expected to modify/create in:

1. `src/semantic/weeding/CheckCaseCompleteness.h/.cpp`
2. semantic test files listed in §3.3
3. negative system tests listed in §3.4

---

## Phase B4 — Codegen for Nested Pattern Destructuring

**Objective**: All positive pattern programs compile, link, and execute correctly; ownership/destruction semantics are preserved for values bound through patterns.

### 4.1 Codegen strategy

A case statement is currently lowered as a dispatch on the top-level tag (encoded as the first word of the sum-type value).  Phase B extends each arm's entry block:

1. **Top-level tag check**: unchanged from Phase A.
2. **Payload position extraction**: for each payload position `i`:
   - If the pattern is `ASTVarPattern(x)` or `ASTWildcardPattern`: extract the `i`-th payload word and, for variable patterns, store it in `ctx.namedValues[x]`.
   - If the pattern is `ASTRecordPattern({f:p,...})`: extract the payload word as a pointer to the record struct; for each field `f_j`, GEP to `f_j`; recurse on sub-pattern `p_j`.
   - If the pattern is `ASTCtorPattern(Ctor, [p,...])`: the payload is a nested sum-type value; emit a nested tag test; if it fails, jump to the arm's failure block (which falls through to the next arm or to an exhaustiveness error path); if it passes, recurse on the sub-patterns.
3. **Arm body**: executed only if all pattern tests pass; all pattern-bound names are in `ctx.namedValues`.
4. **Failure path**: for refutable sub-patterns, the compiler emits a conditional branch to the next arm.  This is new — the current codegen assumes a top-level tag check is sufficient to commit to an arm.

### 4.2 Ownership/destruction in pattern bindings

- A variable pattern binding that receives an Own value is a **move**: the source payload slot must be nulled/invalidated.
- A wildcard at an Own payload position must still trigger destruction of the discarded value (the value is matched but not bound — it must still be freed).
- Variable patterns binding Copy values are simple loads.

Update `DestructionPass` and `MoveAnalysis` to recognise pattern-bound names and apply the same rules as for assignment-bound names.

### 4.3 New unit tests

**`test/unit/codegen/CodegenFunctionsTest.cpp`** — add cases:

| Test case name | What it checks |
|---|---|
| `CodegenFunctions: wildcard pattern generates no named-value store` | `_` in payload — no `namedValues` entry for `_` |
| `CodegenFunctions: record pattern generates GEP per field` | `{a:v}` — IR contains `getelementptr` for field `a` |
| `CodegenFunctions: nested ctor pattern generates inner tag check` | `Outer(Inner(x))` — IR contains two tag-comparison branches |
| `CodegenFunctions: wildcard on Own payload generates free` | `_` at Own payload position — IR contains `call free` |

### 4.4 New positive system tests

Write the following self-test programs (and matching `.pppt` golden files):

| Program | Primary behavior validated |
|---|---|
| `pattern-wildcard.top` | `_` in payload; arm body executes; wildcard doesn't bind |
| `pattern-record.top` | `{a:v}` in `Some` payload; `v` is used in arm body; exits 0 |
| `pattern-nested-ctor.top` | `Outer(Inner(x))` — correct path taken, `x` bound |
| `pattern-mixed.top` | Mix of variable, `_`, and record patterns across arms; all arms reachable |
| `pattern-wildcard-own.top` | `_` at an Own (owning-pointer) payload position — owned value freed, no leak |

Run via `bin/runtests.sh -s`.

### 4.5 Validation gate — Phase B4

- All four new `CodegenFunctionsTest` cases pass.
- All five positive pattern self-tests exit 0.
- All four negative system tests (from B3) still compile-fail with expected messages.
- Full test suite (`bin/runtests.sh`) passes with no regressions against Phase A baseline.

### 4.6 Commit point B4

```
feat(codegen): nested pattern destructuring, ownership in bindings, wildcard destruction (Phase B4)
```

Tag: `phaseB4-codegen-patterns`

### 4.7 Mandatory file touch list — Phase B4

At minimum, Phase B4 is expected to modify/create in:

1. `src/codegen/CodeGenVisitor.h/.cpp`
2. `src/semantic/MoveAnalysis.cpp`
3. `src/semantic/DestructionPass.cpp`
4. `src/semantic/BorrowChecker.cpp` (only if needed for pattern-bound values)
5. `test/unit/codegen/CodegenFunctionsTest.cpp`
6. positive pattern selftests and `.pppt` golden files

---

## Phase B Complete — Acceptance Criteria

Phase B is closed when all four sub-phase commit tags exist and the following hold:

1. All five positive pattern system tests exit 0.
2. All four negative pattern system tests fail at compile with the expected diagnostic substrings.
3. All unit tests in `test/unit/` pass (no regressions against the Phase A baseline).
4. `CheckCaseCompleteness` correctly handles exhaustiveness for arms that use `_`, variable, record, and nested-ctor patterns.
5. Redundant arms are flagged by the exhaustiveness checker.
6. Ownership/destruction rules apply correctly to values bound via patterns and to Own values matched by `_`.
7. Documentation (`README.md` and this plan) is updated with at least one canonical nested-pattern example.

---

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| `ASTCaseArm` BINDINGS refactor breaks many callers at once | Introduce a temporary `getFlatBindings()` adapter at the start of B1; remove it only once all callers are updated in B2/B3 |
| Exhaustiveness algorithm for nested patterns becomes unsound | Start with the conservative simplification in §3.1 (irrefutable sub-patterns short-circuit); add soundness tests before expanding to full pattern matrix |
| Codegen failure-path emission for refutable sub-patterns interacts with existing case lowering | Write `pattern-nested-ctor.top` runtime test before touching `CodeGenVisitor::generate(ASTCaseStmt*)`; it is a regression guard for the existing case codegen |
| Wildcard at Own position leaks memory | Add `pattern-wildcard-own.top` as an explicit no-leak test before codegen changes; verify with sanitizers |
| Partial-record-pattern design choice causes ambiguity | Decide and document in B3 commit message; write a test that confirms the chosen rule |

---

## Implementation Order Within Each Phase

1. Grammar / AST node addition (with temporary adapter for old callers).
2. Visitor/printer wiring (pretty-print tests pass).
3. `LocalNameCollector` update (semantic tests for binding collection pass).
4. Pattern type checking / `CheckPatternTypes` (semantic typing tests pass).
5. Exhaustiveness / redundancy extension.
6. Codegen.

Never commit a step whose unit tests are failing.

---

## Autonomous Execution Protocol

This section defines what "implement Phase B autonomously" means.

### A. Pre-flight checks

1. Confirm current branch and clean/known working tree state.
2. Confirm all Phase A tags exist locally.
3. Run one baseline build/test pass before edits.

### B. Phase gate command set

Use this command sequence at each phase gate:

1. Build: `cmake --build build -j$(sysctl -n hw.ncpu)`
2. Run relevant frontend/semantic/codegen unit suites for changed areas.
3. Run `bin/runtests.sh -s` for selftest/golden validation.
4. Run `bin/runtests.sh` at B4 final gate.

### C. Stop conditions (must pause and ask)

Stop and request direction if any occur:

1. A required B3 check needs full pattern-matrix reasoning to be correct.
2. Existing Phase A ownership/destruction behavior conflicts with wildcard discard semantics.
3. Parser changes require changing top-level case-arm syntax (beyond payload patterns).
4. Any change would break locked Phase A semantics.

### D. Commit discipline

1. One gate commit per phase (B1–B4), optional fixups before each gate tag.
2. Do not squash across B1–B4 gates.
3. Tag each successful gate with the tags defined in this plan.
