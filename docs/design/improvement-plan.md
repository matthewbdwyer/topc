# tipc Improvement Plan

## Overview

This document is the single authoritative roadmap for bringing tipc to architectural
coherence, idiomatic C++17, and a healthy test pyramid.  It supersedes and replaces:

- `term-abstraction-refactor-final.md`
- `term-unifier-compiler-integration.md`
- `types-unit-test-plan.md`
- `codebase-health-analysis.md`

### Scope

The plan covers three interleaved workstreams:

1. **Types subsystem migration** — Replace `Unifier`/`UnionFind` with `TermUnifier`
   + `TipTermBridge`, guided by unit-tests written first.
2. **C++ and LLVM health** — Fix LLVM compatibility blockers, idiomatic C++ patterns,
   and encapsulation problems.
3. **Codegen decoupling** — Extract global state into `CodeGenContext` and decouple
   the AST from LLVM headers.

### TDD discipline

Every phase **begins by writing tests** that capture the desired or existing
behaviour, then makes the production change, then verifies green.  No production
change is merged without a prior or simultaneous test that would catch a regression.
Tests that reveal a bug not yet fixed are tagged `[!mayfail]` so CI stays green
during multi-phase work; the tag is removed in the phase that fixes the bug.

### What not to test

`Unifier` and `UnionFind` are **deleted** in Phase 7.  Do **not** write new tests
for them — bugs will be caught by existing integration tests and the new
`TipTermBridgeTest`.

---

## Historical context (Phases 1–4b, complete)

The term-abstraction refactoring that preceded this plan:

- Created `TermInterface.h` — abstract `Term` base for unification.
- Made `TipType` extend `Term`; all subclasses implement the interface.
- Created `TermUnifier.h/.cpp` — generic, TIP-agnostic unifier (17 tests passing).
- Created `TipTypeTermTest.cpp` — Term interface compliance for every `TipType`
  subclass (50+ tests).
- Refactored `Unifier::unify()` to use `matchesFunctor()` and `getSubterms()`;
  zero `dynamic_pointer_cast` calls remain inside `unify()`.
- Established a green baseline: 266 assertions / 142 test cases on LLVM 17.

Build command (LLVM 17, macOS Apple Silicon):
```bash
cmake .. \
  -DLLVM_DIR=/opt/homebrew/opt/llvm@17/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@17/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@17/bin/clang++
```

---

## Dependency map

```
[done] Term abstraction Phases 1–4b
         │
         ▼
    Phase 1: LLVM API compatibility (unblocks LLVM 19+)
         │
         ▼
    Phase 2: Baseline tests + [[nodiscard]]
         │
         ├──────────────────────────┐
         ▼                          ▼
    Phase 3: Types helper tests     (independent of types migration)
         │
         ▼
    Phase 4: TipVarRegistry + TipTermClosure  (migration A–B)
         │
         ▼
    Phase 5: TipTermBridge facade             (migration C)
         │
         ├──────────────────────────┐
         ▼                          ▼
    Phase 6: C++ idiom cleanup      │
         │                          │
         ▼                          │
    Phase 7: Wire + delete legacy ◄─┘   (migration D–F + semantic encapsulation)
         │
         ▼
    Phase 8: CodeGenContext struct
         │
         ▼
    Phase 9: Decouple AST from LLVM
```

---

## Phase 1 — LLVM API Compatibility ✅ COMPLETE

**Prerequisite:** Green baseline (done).  
**Goal:** Make the codebase compile cleanly against LLVM 19+ and prepare for LLVM 22.
These are one-line changes; doing them first unblocks any future LLVM upgrade.

### Test Goals

**Existing coverage (no new tests needed):**

The five changes in this phase are mechanical substitutions with no behavioural
difference on LLVM 17.  The system-test suite already exercises every affected code
path; the regression guard is:

```bash
./bin/runtests.sh
```

Specific mapping of affected code to existing tests:

| Code site | What exercises it | System test |
|-----------|------------------|-------------|
| `Intrinsic::getDeclaration` — sets up `nop` | Any program; nop is always declared | All iotests |
| `inputIntrinsic`, `outputIntrinsic`, `errorIntrinsic` | Programs using `input`, `output`, `error` | `iotests/ioe.tip` |
| `ConstantExpr::getPointerCast` — function dispatch table | Programs that pass functions as values | `polytests/apply.tip`, `polytests/ident.tip` |
| `_tip_input_array` with `CommonLinkage` | Programs whose `main` receives command-line arguments | `iotests/mainparams.tip` |
| Hardcoded Java path | CMake configure step | Build succeeds on any JDK 11+ |

**Phase complete when:**
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

All changes are in `src/codegen/CodeGenFunctions.cpp` unless noted.

### 1.1 Replace `Intrinsic::getDeclaration` (breaks on LLVM 20)

**Regression guard:** Existing system tests cover all IO, arithmetic, and
control-flow paths.  No new test needed.

**Change:**
```cpp
// Before
nop = llvm::Intrinsic::getDeclaration(TheModule.get(),
                                      llvm::Intrinsic::donothing);
// After
nop = llvm::Intrinsic::getOrInsertDeclaration(TheModule.get(),
                                               llvm::Intrinsic::donothing);
```

`getOrInsertDeclaration` has the same signature and semantics on LLVM 17.

---

### 1.2 Remove `ConstantExpr::getPointerCast` (breaks on LLVM 19)

**Regression guard:** Function-dispatch system tests (programs with first-class
function calls).

**Change:**
```cpp
// Before
castProgramFunctions.push_back(
    llvm::ConstantExpr::getPointerCast(pf, FunctionOpaquePtrType));
// After
castProgramFunctions.push_back(pf);
```

With opaque pointers (the default since LLVM 15), all pointer types are the same
type; the cast is already a no-op on LLVM 17.

---

### 1.3 Remove dead `LegacyPassManager.h` include

**Change:** Delete line:
```cpp
#include "llvm/IR/LegacyPassManager.h"
```

---

### 1.4 ~~Fix `CommonLinkage` for `_tip_input_array`~~  *(reverted — not a valid change)*

`_tip_input_array` is defined in the TIP module but **written by the rtlib**
(`tip_rtlib.c`), so it must have external visibility.  `CommonLinkage` is the
correct linkage for this cross-module tentative definition.  Changing to
`InternalLinkage` hides the symbol from the linker and breaks all programs that
receive command-line arguments.  This item is removed from Phase 1.

---

### 1.5 Fix hardcoded Java path in `CMakeLists.txt`

**Change:** Replace the hardcoded Darwin branch path with a `find_program` probe:

```cmake
# Before (Darwin branch)
set(JAVA_HOME
    "/Library/Java/JavaVirtualMachines/amazon-corretto-11.jdk/Contents/Home")

# After
find_program(JAVA_EXECUTABLE NAMES java)
if(NOT JAVA_EXECUTABLE)
  message(FATAL_ERROR
    "Java not found. Install a JDK 11+ or set JAVA_HOME manually.")
endif()
get_filename_component(JAVA_BIN_DIR "${JAVA_EXECUTABLE}" DIRECTORY)
set(JAVA_HOME "${JAVA_BIN_DIR}/..")
```

**Acceptance:** Build succeeds with any JDK 11+ installation, not just Amazon Corretto.

---

**Phase 1 acceptance gate:**
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass on LLVM 17.  Code
compiles without error on LLVM 19+ (verify opportunistically; not a hard gate if
LLVM 19 is unavailable in CI).

---

## Phase 2 — Baseline Tests and `[[nodiscard]]` ✅ COMPLETE

**Prerequisite:** Phase 1 complete.  
**Goal:** Before touching any logic, write characterization tests and annotate
factory methods.  These tests either pass immediately (documenting current behaviour)
or are tagged `[!mayfail]` (documenting a known bug to be fixed later).

### Test Goals

**Existing coverage:**

- 142 unit test cases passing.  No existing test exercises back-to-back program
  compilation in a single process, so global-state bleed is undetected.
- No existing test directly exercises `ASTBinaryExpr` getters in isolation.
- No existing test proves the `std::set<shared_ptr<TipVar>>` pointer-identity bug.

**Research findings that shape the tests:**

*Codegen globals (22 file-scope variables in anonymous namespace of
`CodeGenFunctions.cpp`):*

| Variable | Type | Bleed risk |
|----------|------|-----------|
| `llvmContext` | `LLVMContext` | `zeroV`/`oneV` constants are computed from it at static init |
| `irBuilder` | `IRBuilder<>` | Points into llvmContext |
| `labelNum` | `int = 0` | Never reset; increments across compilations |
| `lValueGen`, `allocFlag` | `bool = false` | Not reset between functions |
| `functionIndex` | `map<string,int>` | Accumulates across compilations; old entries survive |
| `functionFormalNames` | `map<string,vector<string>>` | Same |
| `namedValues` | `map<string,AllocaInst*>` | Old alloca ptrs remain after module is destroyed |
| `fieldIndex`, `fieldVector` | map / vector | Accumulate |
| `CurrentModule` | `shared_ptr<Module>` | Re-set per compilation, but old entries in maps reference the old module |
| `nop`, `*Intrinsic`, `callocFun` | `Function*` | Reset per compilation; lower risk |
| `tipFunctionTable`, `tipNum*`, `tipInputArray` | `GlobalVariable*` | Reset per compilation |

The two highest-risk bleeds are `labelNum` (causes non-deterministic IR labels when
tests run in the same process) and `namedValues`/`functionIndex` (stale entries from
program A remain when compiling program B).

*`TipVar` identity:*

`TipVar::operator==` compares `node` pointer addresses (same `ASTNode*` ⟹ equal).
`std::set<shared_ptr<TipVar>>` uses `std::less<shared_ptr<TipVar>>` which compares the
raw `TipVar*` address stored in each `shared_ptr`.  Two independently constructed
`shared_ptr<TipVar>` wrapping the same `ASTNode*` hold different `TipVar` objects →
different raw pointers → **treated as distinct elements** even though they are logically
equal.  This means a set built with pointer-identity ordering can contain multiple
"equal" entries.

**New test files at phase start:**

*`test/unit/codegen/CodegenStateIsolationTest.cpp`* — tagged `[!mayfail]` until Phase 8

```
SECTION: struct isolation (pure, no LLVM)
  Construct CodeGenContext A; set A.labelNum = 5
  Construct CodeGenContext B; assert B.labelNum == 0
  (Fails today because CodeGenContext doesn't exist yet;
   proves the struct separates state once it does)

SECTION: labelNum bleed between compilations
  Compile program-A (contains a while loop + if — two label-consuming constructs)
  Compile program-B (same structure)
  Inspect B's IR: first basic block label is "entry", first conditional
  label is "while.cond0" (numbering starts at 0 in B, not continuing from A)
  [!mayfail] — fails today because labelNum never resets

SECTION: namedValues bleed
  Compile program-A with local variable "x"
  Compile program-B with NO local "x"
  Inspect B's IR: namedValues does not contain a dangling alloca for "x"
  [!mayfail] — fails today because map is never cleared

SECTION: functionIndex bleed
  Compile program-A with function "foo"
  Compile program-B with NO function "foo"
  Assert B's function table contains exactly B's functions
  [!mayfail]
```

*`test/unit/frontend/treetypes/ASTBinaryExprTest.cpp`* — all pass immediately

```
SECTION: operator strings
  For each of: "+", "-", "*", "/", ">", "==", "!="
    Construct ASTBinaryExpr with that operator
    REQUIRE(node.getOp() == "<op>")

SECTION: operand access
  Construct ASTBinaryExpr("+", left, right)
  REQUIRE(node.getLeft()  == left.get())
  REQUIRE(node.getRight() == right.get())

SECTION: comparison via getOp()
  Two ASTBinaryExpr nodes with "+" both return the same string
  REQUIRE(nodeA.getOp() == nodeB.getOp())
  (Documents that string comparison works; holds after Phase 6 return-type change)
```

*`test/unit/semantic/types/concrete/TipVarSetTest.cpp`*

```
SECTION: value equality (passes immediately)
  ASTNumberExpr node;
  auto v1 = make_shared<TipVar>(&node);
  auto v2 = make_shared<TipVar>(&node);
  REQUIRE(*v1 == *v2)  // same ASTNode* → equal

SECTION: set pointer-identity bug [!mayfail]
  set<shared_ptr<TipVar>> s;
  s.insert(v1);
  s.insert(v2);
  REQUIRE(s.size() == 2)  // two pointers → two entries; proves the bug
  // After Phase 3 introduces TipVarValueCmp this becomes REQUIRE(s.size() == 1)

SECTION: contains() linear scan (passes immediately)
  // The workaround in Unifier.cpp does value comparison
  REQUIRE(contains(s, v1))
  REQUIRE(contains(s, v2))
```

**Phase complete when:** new `[!mayfail]` tests are visible in CI output and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

Pure additive annotation — no tests needed; the compiler enforces correctness at
call sites.

| Method | File |
|--------|------|
| `SemanticAnalysis::analyze()` | `src/semantic/SemanticAnalysis.h` |
| `TypeInference::run()` | `src/semantic/types/TypeInference.h` |
| `TermUnifier::solve()` | `src/semantic/types/solver/TermUnifier.h` |
| `FrontEnd::parse()` | `src/frontend/FrontEnd.h` |
| `CodeGenerator::generate()` | `src/codegen/CodeGenerator.h` |
| `CallGraph::build()` | `src/semantic/cfa/CallGraph.h` |

**Acceptance:** Any caller that discards the return value now fails to compile.

---

### 2.2 Codegen global-state isolation test (tagged `[!mayfail]`)

**New file:** `test/unit/codegen/CodegenStateIsolationTest.cpp`

These tests expose the global-state bug.  They are tagged `[!mayfail]` until Phase 8
is complete.

Scenarios:
- Compile program A (defines function `f`), then compile program B (also defines `f`)
  in the same process.  Assert the second module does not contain symbols from the first.
- Compile a program with a while loop followed by an if, then compile it again in the
  same process.  Assert that label numbers in the second module start from 0, not
  carrying over from the first.

---

### 2.3 `ASTBinaryExpr` characterization test

**New file:** `test/unit/frontend/treetypes/ASTBinaryExprTest.cpp`

These tests document current behaviour and continue to pass after Phase 6 changes the
return type of `getOp()`.

Scenarios:
- `getOp()` returns the correct operator string for each of the 7 operators
  (`+`, `-`, `*`, `/`, `>`, `==`, `!=`)
- `getLeft()` / `getRight()` return the correct operands
- Two nodes with the same operator string compare equal via `getOp() == getOp()`

---

### 2.4 `TipVarSet` pointer-identity bug characterization (tagged `[!mayfail]`)

Add to `test/unit/semantic/types/solver/TermUnifierTest.cpp` or a new
`test/unit/semantic/types/concrete/TipVarSetTest.cpp`:

Scenarios:
- Two `TipVar` objects wrapping the same `ASTNode*` are equal via `operator==`
- A `std::set<shared_ptr<TipVar>>` (keyed by pointer identity) does **not**
  deduplicate two `TipVar` objects with equal values but different `shared_ptr`
  owners — proving the current bug
- Tagged `[!mayfail]` until Phase 3 introduces a value comparator; after that the
  test is rewritten to confirm deduplication **does** happen and the tag is removed

---

**Phase 2 acceptance gate:** New `[!mayfail]`-tagged tests are visible in CI output and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

## Phase 3 — Types Helper Tests and `TipVarSet` Fix ✅ COMPLETE

**Prerequisite:** Phase 2 complete.  
**Goal:** Add direct unit tests for `Substituter`, `Copier`, `FreshAlphaCopier`, and
`TypeVars`.  Fix the `TipVarSet` pointer-identity bug.  Extend `TipTypeTermTest` and
`TipConsDoMatchTest`.

### Test Goals

**Existing coverage:**

`SubstituterTest`, `CopierTest`, `TypeVarsTest` do not exist.  Every bug in these
helpers currently surfaces only as a wrong inferred type in a complex integration
test.  The Term-interface compliance tests (`TipTypeTermTest.cpp`) cover six of eight
`TipType` subclasses; `TipMu` and `TipRecord` are missing.  `TipConsDoMatchTest`
tests only mismatch cases.

**Research findings that shape the tests:**

*`Substituter::substitute(TipType *t, TipVar *v, shared_ptr<TipType> s)`:*

Visitor post-order traversal pushes results onto `visitedTypes`.  Key behaviours:
- `endVisit(TipVar)`: if `*element == *target` push a `Copier::copy(s)`; else push
  new `TipVar(element->getNode())`.
- `endVisit(TipAlpha)`: same check — TipAlpha is substituted if it equals the target
  (possible because TipAlpha extends TipVar and operator== is based on node pointer).
- `endVisit(TipMu)`: pops two items, casts the second to `TipVar*`.  If the binding
  variable **is** the target, Copier::copy returns a non-TipVar type and the cast
  returns nullptr → undefined behaviour.  In practice this never happens because
  Substituter is always called on closed types where the binding var ≠ free target.
- All compound types (TipFunction, TipRef, TipRecord) reconstruct fresh nodes from
  visited subtypes; field names on TipRecord are preserved from the original.
- Substitution itself is always a `Copier::copy` of `s` — so each replaced site gets
  a fresh copy, preventing accidental sharing.

*`Copier::copy(shared_ptr<TipType> t)`:*

Default-constructs a `Substituter` (target = nullptr, substitution = nullptr).
Overrides only `endVisit(TipVar)` and `endVisit(TipAlpha)` so those two types create
fresh objects rather than checking against a null target.  All other node types go
through Substituter's endVisit methods, which reconstruct fresh nodes — meaning every
node in the tree gets a fresh allocation.  `TipVar` nodes in a copy preserve the
**same** `ASTNode*` pointer (identity is the node, not the TipVar wrapper).

*`FreshAlphaCopier::copy(TipType *t, ASTNode *context)`:*

Extends Copier; overrides `endVisit(TipAlpha)` to replace each TipAlpha with a new
`TipAlpha(element->getNode(), context, element->getName())` — the second constructor
argument pins the copy to a specific call-site context.

*`TypeVars::collect(TipType *t)`:*

Key findings from reading the source:
1. **TipAlpha IS included** in the collected set — `endVisit(TipAlpha)` inserts a
   `make_shared<TipAlpha>`.  The existing test-plan draft incorrectly stated otherwise.
2. **`vars.erase(element->getV())` is a no-op** in `endVisit(TipMu)`.  The set
   holds newly-created `shared_ptr`s (different raw pointers than the TipMu's stored
   `shared_ptr`), so `std::set::erase` (which uses pointer address) never finds them.
   TipMu bound variables **remain** in the collected set.
3. Despite finding #2, this does not cause incorrect inference because `contains()`
   in `Unifier.cpp` uses linear scan with value equality (`operator==`), and
   `newV` is a `TipAlpha` whereas the bound variable is a `TipVar` — they compare
   unequal even with the same node pointer.
4. If the same logical variable appears twice in a type expression, the set gets two
   pointer-distinct entries (same logical value).  Again harmless because `contains()`
   uses value equality.

**New test files at phase start:**

*`test/unit/semantic/types/concrete/SubstituterTest.cpp`*

```
SECTION: nullary type unchanged
  substitute(TipInt, x, TipFloat) → result is TipInt (different ptr)

SECTION: target variable replaced
  substitute(TipVar(nodeA), TipVar(nodeA), TipInt) → TipInt

SECTION: non-target variable unchanged
  substitute(TipVar(nodeA), TipVar(nodeB), TipInt) → TipVar(nodeA) (same node)

SECTION: compound type — one occurrence
  substitute(TipRef(TipVar(x)), x, TipInt) → TipRef(TipInt)

SECTION: multiple occurrences all replaced
  substitute(TipFunction([TipVar(x)], TipVar(x)), x, TipInt)
  → TipFunction([TipInt], TipInt)

SECTION: compound substitution preserved
  substitute(TipRef(TipVar(x)), x, TipRef(TipInt)) → TipRef(TipRef(TipInt))

SECTION: TipRecord — field names preserved
  substitute(TipRecord({f: TipVar(x), g: TipVar(y)}), x, TipInt)
  → TipRecord({f: TipInt, g: TipVar(y)})  — field names identical

SECTION: substituted value is a fresh copy (not the original shared_ptr)
  auto s = make_shared<TipInt>();
  auto result = substitute(TipVar(x), x, s);
  REQUIRE(result.get() != s.get())  // Copier::copy produced a new pointer

SECTION: TipMu with DIFFERENT bound variable (safe case)
  TipMu(v, TipRef(TipVar(x))) where v != x
  substitute(..., x, TipInt) → TipMu(v, TipRef(TipInt))
  (binding variable v is unchanged; only free x is replaced)
```

*`test/unit/semantic/types/concrete/CopierTest.cpp`*

```
SECTION: copy TipInt
  auto orig = make_shared<TipInt>();
  auto copy = Copier::copy(orig);
  REQUIRE(*copy == *orig)          // same logical type
  REQUIRE(copy.get() != orig.get()) // fresh pointer

SECTION: copy compound type — all pointers fresh
  auto orig = make_shared<TipRef>(make_shared<TipInt>());
  auto copy = Copier::copy(orig);
  REQUIRE(*copy == *orig)
  auto origRef = dynamic_pointer_cast<TipRef>(orig);
  auto copyRef = dynamic_pointer_cast<TipRef>(copy);
  REQUIRE(copyRef->getReferencedType().get() !=
          origRef->getReferencedType().get())  // inner node is also fresh

SECTION: copy TipVar — node pointer preserved
  ASTNumberExpr node;
  auto orig = make_shared<TipVar>(&node);
  auto copy = Copier::copy(orig);
  auto copyVar = dynamic_pointer_cast<TipVar>(copy);
  REQUIRE(copyVar->getNode() == &node)  // same ASTNode*, different TipVar

SECTION: copy TipAlpha — node and name preserved
  auto orig = make_shared<TipAlpha>(&node, "α");
  auto copy = Copier::copy(orig);
  auto copyAlpha = dynamic_pointer_cast<TipAlpha>(copy);
  REQUIRE(copyAlpha->getNode() == orig->getNode())
  REQUIRE(copyAlpha->getName() == orig->getName())
  REQUIRE(copy.get() != orig.get())

SECTION: FreshAlphaCopier replaces alpha context
  ASTNode *ctxA = ..., *ctxB = ...;
  auto orig = make_shared<TipAlpha>(ctxA, "α");
  auto copy = FreshAlphaCopier::copy(orig.get(), ctxB);
  auto copyAlpha = dynamic_pointer_cast<TipAlpha>(copy);
  REQUIRE(copyAlpha->getNode() == ctxA)     // original node preserved
  REQUIRE(copyAlpha->getName() == "α")       // name preserved
  // second constructor arg (context) is ctxB — verify via a context accessor
  // if no accessor exists, verify that two copies with different contexts
  // are not equal (TipAlpha equality includes context)

SECTION: FreshAlphaCopier with no alphas
  auto orig = make_shared<TipInt>();
  auto copy = FreshAlphaCopier::copy(orig.get(), ctxB);
  REQUIRE(*copy == *orig)
```

*`test/unit/semantic/types/concrete/TypeVarsTest.cpp`*

```
SECTION: nullary type — empty
  TypeVars::collect(TipInt) → set of size 0

SECTION: single TipVar
  TypeVars::collect(TipVar(node1)) → set of size 1
  contains a TipVar with node == node1

SECTION: TipAlpha IS included (not empty)
  TypeVars::collect(TipAlpha(node, "α")) → set of size 1
  contains a TipAlpha (not empty — this is the correct behaviour)

SECTION: compound type — one TipVar
  TypeVars::collect(TipRef(TipVar(node1))) → set of size 1

SECTION: same logical variable appearing twice
  auto f = TipFunction([TipVar(nodeX)], TipVar(nodeX))
  TypeVars::collect(f) → set of size 2 (two pointer-distinct entries with same node)
  // Documents the pointer-identity behaviour of the underlying set

SECTION: TipMu — bound variable remains in set (erase is a no-op)
  TypeVars::collect(TipMu(sharedPtr_v, TipRef(TipVar_v)))
  → set CONTAINS the variable v (erase in endVisit(TipMu) fails due to pointer mismatch)
  // This documents actual behaviour; it does not cause incorrect inference because
  // contains() in Unifier uses linear scan with value equality

SECTION: free variable inside TipMu body IS collected
  TipMu(v, TipFunction([TipVar(v), TipVar(x)], TipInt)) where x != v
  TypeVars::collect → set contains both v and x
```

*Extensions to `TipTypeTermTest.cpp`* — detailed in Phase 3.5 of the plan body.

*Extensions to `TipConsDoMatchTest`* — detailed in Phase 3.6 of the plan body.

**Phase complete when:** all new test files compile and pass; Phase 2.4 `[!mayfail]`
set-size test now passes without the tag (deduplication works);
`grep contains src/semantic/types/solver/Unifier.cpp` returns empty; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

These helpers are type-algebra operations (visitor-based traversal and transformation
of `TipType` trees), not solver machinery.  Their tests are placed under `concrete/`
even though the source files currently live in `solver/`.  Relocating the source files
to `concrete/` or a new `typeops/` directory is a clean-up step that can accompany
writing these tests.

This phase also unblocks Phase 4 — `TipTermClosure` uses all three helpers and its
tests assume the helpers' behaviour is already verified.

---

### 3.1 `test/unit/semantic/types/concrete/SubstituterTest.cpp`

Tests `Substituter::substitute()` with hand-built `TipType` values.  No AST, no
visitor, no unifier.

Scenarios:
- Substitute into a nullary type that is not the target → unchanged
- Substitute into a `TipVar` that is the target → substitution returned
- Substitute into a `TipVar` that is NOT the target → unchanged
- Substitute into a compound type (`TipFunction`, `TipRef`, `TipRecord`) where the
  target appears once → result has the substituted subterm
- Substitute into a type where the target appears multiple times → all occurrences
  replaced
- Substitute into a `TipMu` wrapping the target variable → only free occurrences
  replaced (the bound occurrence is not substituted)
- Substitution is itself a compound type → compound substitution preserved correctly

---

### 3.2 `test/unit/semantic/types/concrete/CopierTest.cpp`

Tests `Copier::copy()` and `FreshAlphaCopier::copy()`.

*`Copier`:*
- Copy a nullary type → same functor, different pointer
- Copy a compound type → same structure, all pointers fresh
- Copy a type containing `TipVar` → `TipVar` node pointer preserved (not cloned)
- Copy a type containing `TipAlpha` → `TipAlpha` preserved with same node/name

*`FreshAlphaCopier`:*
- Copy a type containing `TipAlpha` with context A → alphas replaced with
  context-specific alphas for context B
- Copy a type with no alphas → identical structure returned

---

### 3.3 `test/unit/semantic/types/concrete/TypeVarsTest.cpp`

Tests `TypeVars::collect()`.

Scenarios:
- `TipInt` → empty set
- Single `TipVar` → set of size 1
- `TipAlpha` → empty set (alpha is not a free unification variable)
- Compound type with one embedded `TipVar` → that var collected
- Compound type with the same `TipVar` appearing twice → collected once (set semantics)
- `TipMu(v, T)` where `v` appears free in `T` → `v` is NOT collected (it is bound)
- `TipMu(v, T)` where a different var `x` appears free in `T` → `x` is collected
- Nested compound type → all free vars at any depth collected

---

### 3.4 `TipVarSet` value comparator and `contains()` deletion

**Change 1 — `operator<` on `TipVar`:**
```cpp
bool TipVar::operator<(const TipVar &other) const {
    return node < other.node;   // ASTNode* pointer ordering
}
```

**Change 2 — named comparator and typedef:**
```cpp
struct TipVarValueCmp {
    bool operator()(const std::shared_ptr<TipVar> &a,
                    const std::shared_ptr<TipVar> &b) const {
        return *a < *b;
    }
};
using TipVarSet = std::set<std::shared_ptr<TipVar>, TipVarValueCmp>;
```

**Change 3 — update usages** in `Unifier.cpp`, `TypeVars.h`, `TypeVars.cpp`,
`Substituter.h`.

**Change 4 — delete `contains()`** from `Unifier.cpp`'s anonymous namespace.
Replace the single call site with `visited.count(v)` (correct now that the set has
value semantics).

Remove the `[!mayfail]` tag from Phase 2.4 tests and verify they pass.

---

### 3.5 `TipTypeTermTest.cpp` additions — TipMu and TipRecord

Extend `test/unit/semantic/types/concrete/TipTypeTermTest.cpp`:

*TipMu Term interface:*
- `isVariable()` → false
- `getFunctor()` → `"μ"`
- `arity()` → 2
- `getSubterms()` → `[v, T]`
- `withSubterms()` with exactly 2 terms → new `TipMu` with updated structure
- `withSubterms()` wrong count → throws `std::invalid_argument`
- `matchesFunctor()` → `TipMu` matches `TipMu`; does not match `TipRef`

*TipRecord Term interface:*
- `isVariable()` → false
- `getFunctor()` → encodes field names (records with different field names differ)
- `arity()` → number of fields
- `getSubterms()` → field types in order
- `withSubterms()` correct count → new `TipRecord` with updated field types
- `withSubterms()` wrong count → throws `std::invalid_argument`
- `matchesFunctor()` → same field names + same arity matches; different field names
  does not match even with same arity (critical field-name semantics)

---

### 3.6 `TipConsDoMatchTest` additions

Extend coverage of `TipCons::doMatch()`:

- `TipInt.doMatch(TipInt)` → true
- `TipRef(TipInt).doMatch(TipRef(TipInt))` → true
- `TipFunction([int], int).doMatch(TipFunction([int], int))` → true
- `TipRecord({f: int}).doMatch(TipRecord({f: int}))` → true
- `TipRecord({f: int}).doMatch(TipRecord({g: int}))` → false (same arity, different
  field name — the critical case ensuring field-name identity is enforced)

---

**Phase 3 acceptance gate:** All new test files pass; Phase 2.4 `[!mayfail]` tests
now pass without the tag; `grep contains src/semantic/types/solver/Unifier.cpp`
returns empty; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

## Phase 4 — TipVarRegistry and TipTermClosure ✅ COMPLETE

**Prerequisite:** Phase 3 complete.  
**Goal:** Build and test the two new types-subsystem classes that replace
`Unifier::close()`.  No changes to `TypeInference` or callers yet.

### Test Goals

**Existing coverage:** None.  Both classes are new.  Tests are written TDD-style
before any production code.

**Research findings that shape the tests:**

*`TipVar::getFunctor()` implementation:*
```cpp
std::string TipVar::getFunctor() const {
    std::ostringstream oss;
    oss << "var@" << node;   // node is ASTNode*; address used as unique id
    return oss.str();
}
```
The registry maps this string → the original `shared_ptr<TipVar>`.  Because
`getFunctor()` returns a different string for every distinct `ASTNode*`, no two
different declaration nodes can collide.

*`Unifier::close()` branch inventory (complete list):*

| Branch | Trigger condition | Test scenario |
|--------|------------------|---------------|
| **Var: unbound** | `isVar(type)` and `unionFind->find(v) == v` | Variable never constrained → TipAlpha |
| **Var: bound, no cycle** | `isVar(type)`, `find(v) != v`, not in visited, `contains(freeV, newV)` is false | x = TipInt → TipInt |
| **Var: transitive chain** | x → y → TipInt | Two-hop chain → TipInt |
| **Var: isAlpha reuse** | `isAlpha(v)` → reuse v itself as newV | TipAlpha input → same TipAlpha out |
| **Var: cycle → TipMu** | `find(v) != v` and `contains(freeV, newV)` | x → TipRef(x) → TipMu(α, ref(α)) |
| **Cons: no free vars** | `isCons(type)` and `TypeVars::collect(c)` is empty | TipInt passthrough |
| **Cons: with free vars** | `isCons(type)` and freeV non-empty | TipRef(TipVar(x)) with x=TipInt → TipRef(TipInt) |
| **Mu passthrough** | `isMu(type)` | TipMu(v, body) → TipMu(v, closed body) |

*Critical subtlety:* `TipTermClosure` reads from `TermUnifier::getSubstitution()`
(a `map<string, shared_ptr<Term>>`) instead of `unionFind->find()`.  The "bound"
check becomes `substitution.count(key) > 0` instead of `find(v) != v`.  The visited
set guards cycles the same way.

**New test files at phase start (TDD — write before production code):**

*`test/unit/semantic/types/solver/TipVarRegistryTest.cpp`*

```
SECTION: register and lookup
  ASTNumberExpr nodeA;
  auto v = make_shared<TipVar>(&nodeA);
  TipVarRegistry reg;
  reg.register_(v);
  auto key = v->getFunctor();              // "var@<addr>"
  REQUIRE(reg.lookup(key) == v)            // same shared_ptr returned

SECTION: multiple independent vars
  ASTNumberExpr nodeA, nodeB;
  auto vA = make_shared<TipVar>(&nodeA);
  auto vB = make_shared<TipVar>(&nodeB);
  reg.register_(vA); reg.register_(vB);
  REQUIRE(reg.lookup(vA->getFunctor()) == vA)
  REQUIRE(reg.lookup(vB->getFunctor()) == vB)

SECTION: unregistered key returns null
  REQUIRE(reg.lookup("var@0xdeadbeef") == nullptr)

SECTION: idempotent registration
  reg.register_(v); reg.register_(v);
  REQUIRE(reg.lookup(v->getFunctor()) == v)  // no crash, no duplicate

SECTION: TermUnifier round-trip
  Register vA and vB.
  Add constraint vA == vB to TermUnifier (both cast to Term).
  Add constraint vB == TipInt (cast to Term).
  solve().
  From getSubstitution(), look up vA->getFunctor().
  The result key chains through: follow substitution until TipInt is reached.
  Verify: reg.lookup(vA->getFunctor()) == vA (registry preserves the original TipVar)
```

*`test/unit/semantic/types/solver/TipTermClosureTest.cpp`*

```
SECTION: unbound variable → TipAlpha
  Setup: empty TermUnifier (no constraints involving x)
  close(TipVar(x)) → result is TipAlpha
  REQUIRE(dynamic_pointer_cast<TipAlpha>(result) != nullptr)

SECTION: variable bound to TipInt → TipInt
  Add constraint x == TipInt; solve()
  close(TipVar(x)) → TipInt
  REQUIRE(*result == *make_shared<TipInt>())

SECTION: two-hop chain
  Add x == y, y == TipInt; solve()
  close(TipVar(x)) → TipInt

SECTION: TipAlpha input — reuse path
  close(TipAlpha(node, "α")) where α is in substitution mapping to TipAlpha
  → the same TipAlpha is returned (isAlpha reuse path)
  REQUIRE(dynamic_pointer_cast<TipAlpha>(result) != nullptr)

SECTION: cycle → TipMu
  Add x == TipRef(TipVar(x)); solve()
  close(TipVar(x)) → TipMu(α, TipRef(α))
  auto mu = dynamic_pointer_cast<TipMu>(result);
  REQUIRE(mu != nullptr)
  auto inner = dynamic_pointer_cast<TipRef>(mu->getT());
  REQUIRE(inner != nullptr)
  REQUIRE(*inner->getReferencedType() == *mu->getV())  // α == α

SECTION: compound with free variable
  Add x == TipFunction([TipVar(y)], TipInt), y unconstrained; solve()
  close(TipVar(x)) → TipFunction([TipAlpha], TipInt)
  (y replaced by TipAlpha because y is unbound)

SECTION: TipRecord with free variable
  Add x == TipRecord({f: TipVar(y), g: TipInt}), y unconstrained; solve()
  close(TipVar(x)) → TipRecord({f: TipAlpha, g: TipInt})

SECTION: TipMu passthrough
  Input type is an already-formed TipMu(v, TipRef(v)); no variable binding in substitution
  close(mu) → TipMu(v, closed TipRef(v))
  Structure preserved; inner type is also closed
```

**Phase complete when:** `TipVarRegistryTest` and `TipTermClosureTest` all pass and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

---

### 4.1 `TipVarRegistry` (Migration Phase A)

#### Tests first: `test/unit/semantic/types/solver/TipVarRegistryTest.cpp`

`TipVarRegistry` does not yet exist.  Write the tests before writing the class.

Scenarios:
- Register a `TipVar`; `lookup(var->getFunctor())` returns the same pointer
- Register multiple vars; each is independently recoverable by its functor key
- Lookup of an unregistered key returns null / empty optional
- Registering the same var twice is idempotent
- Round-trip: submit a `TypeConstraint` pair to `TermUnifier` as
  `static_pointer_cast<Term>`, call `solve()`, look up the result key via the registry
  and confirm the original `TipVar` is recovered

#### Production: `src/semantic/types/solver/TipVarRegistry.h/.cpp`

```cpp
class TipVarRegistry {
public:
    void register_(std::shared_ptr<TipVar> var);
    std::shared_ptr<TipVar> lookup(const std::string &key) const;
};
```

The registry maps `var->getFunctor()` → `TipVar`.  `TipVar::getFunctor()` returns
`"var@<ASTNode*_address>"`, making the pointer address the unique string key.

**Acceptance:** All `TipVarRegistryTest` tests pass; no production callers changed.

---

### 4.2 `TipTermClosure` (Migration Phase B)

#### Tests first: `test/unit/semantic/types/solver/TipTermClosureTest.cpp`

Scenarios mirror `Unifier::close()` step-for-step, driven through
`TermUnifier::getSubstitution()` and `TipVarRegistry`:

- Unbound variable → `TipAlpha`
- Variable bound to `TipInt` → `TipInt`
- Two-hop chain: `x → y`, `y = TipInt` → `TipInt`
- Existing `TipAlpha` passed as type → same `TipAlpha` returned (isAlpha reuse path)
- Cycle: `x → TipRef(x)` → `TipMu(α, ref(α))`
- Variable bound to `TipFunction([x], TipInt)` → compound type closed correctly;
  `x` replaced by `TipAlpha`
- Variable bound to `TipRecord` with a free variable → record fields closed
- `TipMu` passed directly → inner type closed preserving mu structure

#### Production: `src/semantic/types/solver/TipTermClosure.h/.cpp`

```cpp
class TipTermClosure {
public:
    TipTermClosure(
        const TermUnifier::Substitution &substitution,
        const TipVarRegistry &registry);

    std::shared_ptr<TipType> close(
        std::shared_ptr<TipType> type,
        std::set<std::shared_ptr<TipVar>> visited = {});
};
```

Algorithm mirrors `Unifier::close()` step for step, reading from the
`TermUnifier::Substitution` map instead of calling `unionFind->find()`:

| `Unifier::close()` | `TipTermClosure::close()` |
|--------------------|--------------------------|
| `unionFind->find(v) != v` → follow chain | `substitution.count(key)` → follow chain |
| `isAlpha(v)` → reuse existing alpha | same, via `dynamic_pointer_cast<TipAlpha>` |
| `make_shared<TipAlpha>(v->getNode())` | same |
| `TypeVars::collect(closedV)` | same |
| `Substituter::substitute(...)` | same |
| `make_shared<TipMu>(newV, substClosedV)` | same |
| `Copier::copy(c)` + re-close arguments | same |

Note: Phase B is the highest-risk step.  If `Unifier::close()` has subtle behaviour
not captured by existing tests, write additional golden-output tests against `Unifier`
first (characterise its behaviour with known programs), then use the same expected
values to validate `TipTermClosure`.

**Acceptance:** All `TipTermClosureTest` tests pass; `TypeInference` and its callers
are unchanged.

---

**Phase 4 acceptance gate:** `TipVarRegistryTest` and `TipTermClosureTest` all pass and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

## Phase 5 — TipTermBridge Facade

**Prerequisite:** Phase 4 complete.  
**Goal:** Provide a drop-in replacement for `Unifier` backed by `TermUnifier` +
`TipTermClosure`.  No changes to `TypeInference` or callers yet.

### Test Goals

**Existing coverage:**

`UnifierTest.cpp` contains 7 test cases grouped into three `TEST_CASE` blocks:

| Block | Scenarios |
|-------|-----------|
| "Collect and then unify constraints" | short() program (infer x:int, y:&int, z:int, f:()->int); deref (infer TipMu/TipRef/TipAlpha); 3 UnificationError cases |
| "Unify constraints on the fly" | short(); record2 (alloc record, field write+read); record4 (nested record, double-deref); deref; 4 UnificationError cases |
| "Test unifying TipCons with different arities" | TipFunction(1 param) vs TipFunction(2 params) → UnificationError |

**Research findings that shape the tests:**

All `TipTermBridgeTest` scenarios are ported directly from `UnifierTest.cpp`.  This
is intentional: the goal is to prove that `TipTermBridge` is a drop-in replacement
before `TypeInference` is wired.  The test is written by copying `UnifierTest.cpp`
and substituting:
- `#include "Unifier.h"` → `#include "TipTermBridge.h"`
- `Unifier unifier(...)` → `TipTermBridge unifier(...)`
- `Unifier unifier` → `TipTermBridge unifier` (default constructor)

The `TypeConstraintUnifyVisitor` cases in the "on-the-fly" block require `TipTermBridge`
to also expose `TypeConstraintUnifyVisitor` compatibility — check whether that visitor
holds a `Unifier*` or something more abstract, and update accordingly.

**New test file at phase start (TDD):**

*`test/unit/semantic/types/solver/TipTermBridgeTest.cpp`*

```
TEST_CASE: "Collect and then unify — short()"
  [copy of UnifierTest SECTION "Test type-safe program 1"]
  Verify inferred types: x:TipInt, y:TipRef(TipInt), z:TipInt, f:TipFunction(→TipInt)

TEST_CASE: "Collect and then unify — deref"
  [copy of UnifierTest SECTION "Test type-safe deref"]
  Verify TipFunction([TipRef(TipAlpha)], TipAlpha) inferred shape

TEST_CASE: "Collect and then unify — UnificationError cases 1-3"
  [copy of three error SECTIONs from UnifierTest]
  REQUIRE_THROWS_AS(bridge.solve(), UnificationError)  — or equivalent bridge error

TEST_CASE: "On-the-fly — type-safe programs"
  [copy of four safe SECTIONs from "Unify on the fly" block]
  short(), record2, record4, deref — REQUIRE_NOTHROW

TEST_CASE: "On-the-fly — UnificationError cases 1-4"
  [copy of four error SECTIONs]
  REQUIRE_THROWS_AS

TEST_CASE: "Arity mismatch"
  [copy of "Test unifying TipCons with different arities"]
  REQUIRE_THROWS_AS
```

Additionally, one new scenario not in `UnifierTest`:

```
TEST_CASE: "Static helpers are accessible"
  REQUIRE(TipTermBridge::isCons(make_shared<TipInt>()))
  REQUIRE(TipTermBridge::isVar(make_shared<TipVar>(&node)))
  REQUIRE(!TipTermBridge::isMu(make_shared<TipInt>()))
  REQUIRE(TipTermBridge::isAlpha(make_shared<TipAlpha>(&node)))
  REQUIRE(TipTermBridge::isProperType(make_shared<TipInt>()))
```

**Phase complete when:** all `TipTermBridgeTest` tests pass; `UnifierTest.cpp` still
passes unchanged (it remains the regression guard until Phase 7); and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

Write by copying every test case from `solver/UnifierTest.cpp` and substituting
`Unifier` → `TipTermBridge`.  All scenarios must pass without changing the test logic.
This proves `TipTermBridge` is a drop-in replacement.

### Production: `src/semantic/types/solver/TipTermBridge.h/.cpp`

Public API mirrors `Unifier` exactly:

```cpp
class TipTermBridge {
public:
    TipTermBridge();
    explicit TipTermBridge(std::vector<TypeConstraint>);
    void unify(std::shared_ptr<TipType>, std::shared_ptr<TipType>);
    void add(std::vector<TypeConstraint>);
    void solve();
    std::shared_ptr<TipType> inferred(std::shared_ptr<TipType>);

    // Static helpers — kept for any code that calls Unifier::isCons etc.
    static bool isCons(std::shared_ptr<TipType>);
    static bool isMu(std::shared_ptr<TipType>);
    static bool isVar(std::shared_ptr<TipType>);
    static bool isAlpha(std::shared_ptr<TipType>);
    static bool isProperType(std::shared_ptr<TipType>);
};
```

Implementation details:
- Constructor populates `TipVarRegistry` from all `TipVar` terms in constraints
- `add()` / `unify()` forward to `TermUnifier::addConstraint()`
- `solve()` calls `TermUnifier::solve()`
- `inferred(v)` builds a `TipTermClosure` over the current substitution and calls
  `close(v, {})`

**Acceptance:** All `TipTermBridgeTest` tests pass.  `UnifierTest.cpp` still passes
unchanged (it remains the regression guard until Phase 7).

---

## Phase 6 — C++ Idiom Cleanup

**Prerequisite:** Phase 5 complete (Phase 2 characterization tests in place).  
**Goal:** Eliminate non-idiomatic C++ patterns that cause hidden copies, hard-coded
dispatch, or misleading ownership semantics.

### Test Goals

**Existing coverage:**

`ASTBinaryExprTest.cpp` (written in Phase 2) already covers `getOp()` return values
and operand access.  Those tests pass before and after this phase — the observable
behaviour is unchanged; only the return type and dispatch method change.

**Research findings that shape the tests:**

*`getOp()` / `getName()` / `getField()` call-site audit:*

All callers in the production code use the result in one of three safe patterns:
1. Immediate comparison: `if (getOp() == "+")` — safe with `const string &`
2. Copy into `auto`: `auto op = element->getOp()` — `auto` copies; safe
3. String concatenation: `"..." + element->getName()` — safe

No caller binds the result to a `string &` (non-const), so changing the return
type to `const std::string &` is safe in all cases.

Affected getters and their files:

| Getter | Declared in | Call-site files |
|--------|------------|-----------------|
| `ASTBinaryExpr::getOp()` | `ASTBinaryExpr.h` | `CodeGenFunctions.cpp` (7 comparisons), `TypeConstraintVisitor.cpp` (1), `PrettyPrinter.cpp` (1), `ASTBinaryExpr.cpp` (1) |
| `ASTVariableExpr::getName()` | `ASTVariableExpr.h` | `CodeGenFunctions.cpp` (3), `TypeConstraintVisitor.cpp` (2) |
| `ASTDeclNode::getName()` | `ASTDeclNode.h` | `CodeGenFunctions.cpp` (2), `LocalNameCollector.cpp` (5+), `SymbolTable.cpp` (1), `FunctionNameCollector.cpp` (3), `TypeInference.cpp` (1) |
| `ASTFunction::getName()` | `ASTFunction.h` | `CodeGenFunctions.cpp` (3), `ASTProgram.cpp` (1), `PolyTypeConstraintVisitor.cpp` (1) |
| `ASTFieldExpr::getField()` | `ASTFieldExpr.h` | `CodeGenFunctions.cpp` (2), `FieldNameCollector.cpp` (2), `TypeConstraintVisitor.cpp` (1) |
| `ASTAccessExpr::getField()` | `ASTAccessExpr.h` | `CodeGenFunctions.cpp` (1), `TypeConstraintVisitor.cpp` (1) |
| `ASTProgram::getName()` | `ASTProgram.h` | `ASTProgram.cpp` (2) |

*`BinaryOp` enum — current dispatch (7-branch `else if` chain):*
```cpp
if (getOp() == "+")  ...
else if (getOp() == "-")  ...
else if (getOp() == "*")  ...
else if (getOp() == "/")  ...
else if (getOp() == ">")  ...
else if (getOp() == "==") ...
else if (getOp() == "!=") ...
```
This chain has no exhaustiveness check.  Replacing with `switch (getOpKind())` and
a `default: throw InternalError(...)` enables `-Wswitch` warnings for future additions.

**New/extended tests at phase start:**

Extend `ASTBinaryExprTest.cpp` with a `BinaryOp` enum section (written before the
production change):

```
SECTION: BinaryOp enum mapping (to be added before production change)
  REQUIRE(ASTBinaryExpr("+",  l, r).getOpKind() == BinaryOp::Add)
  REQUIRE(ASTBinaryExpr("-",  l, r).getOpKind() == BinaryOp::Sub)
  REQUIRE(ASTBinaryExpr("*",  l, r).getOpKind() == BinaryOp::Mul)
  REQUIRE(ASTBinaryExpr("/",  l, r).getOpKind() == BinaryOp::Div)
  REQUIRE(ASTBinaryExpr(">",  l, r).getOpKind() == BinaryOp::Gt)
  REQUIRE(ASTBinaryExpr("==", l, r).getOpKind() == BinaryOp::Eq)
  REQUIRE(ASTBinaryExpr("!=", l, r).getOpKind() == BinaryOp::Neq)

SECTION: string getter still works after enum addition
  REQUIRE(ASTBinaryExpr("+", l, r).getOp() == "+")

SECTION: unknown operator throws at construction
  REQUIRE_THROWS_AS(ASTBinaryExpr("??", l, r), InternalError)
```

These tests fail (class doesn't have `getOpKind()` yet), tagged `[!mayfail]` when
written, then the `[!mayfail]` is removed once the enum is added.

The existing `ASTBinaryExprTest` return-type and operand tests continue to pass
throughout — they are the regression guard for the `const string &` change.

**Phase complete when:** no new `-Wswitch` warnings;
`grep 'std::move(std::make_shared' src/` returns empty; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

The Phase 2.3 `ASTBinaryExprTest` characterization tests pass before and after this
change — the observable behaviour is identical; only unnecessary copies are removed.

Files to update (return type only):

| File | Getter |
|------|--------|
| `ASTBinaryExpr.h` | `getOp()` |
| `ASTVariableExpr.h` | `getName()` |
| `ASTDeclNode.h` | `getName()` |
| `ASTFunction.h` | `getName()` — delegates to `DECL->getName()`; can return `const std::string &` because `DECL` outlives the call |
| `ASTFieldExpr.h` | `getField()` |
| `ASTAccessExpr.h` | `getField()` |
| `ASTProgram.h` | `getName()` |

**Acceptance:** All unit and system tests pass.  Codegen string copies in the
operator dispatch chain are eliminated.

---

### 6.2 `BinaryOp` enum on `ASTBinaryExpr`

#### Tests first (extend `ASTBinaryExprTest.cpp`):

- For each operator string `+`, `-`, `*`, `/`, `>`, `==`, `!=`: constructing an
  `ASTBinaryExpr` with that string yields the expected `BinaryOp` enum value
- `getOp()` still returns the correct string (for pretty-printer compatibility)
- An unknown operator string throws `InternalError` at construction time (fail-fast
  at the AST boundary)

#### Production:

```cpp
// ASTBinaryExpr.h
enum class BinaryOp { Add, Sub, Mul, Div, Gt, Eq, Neq };

class ASTBinaryExpr : public ASTExpr {
    std::string OP;   // kept for pretty-printing
    BinaryOp opKind;  // derived from OP at construction
    ...
public:
    BinaryOp getOpKind() const { return opKind; }
    const std::string &getOp() const { return OP; }
};
```

Replace the 7-branch `else if (getOp() == "...")` in codegen with `switch (getOpKind())`.
The compiler will warn (`-Wswitch`) if a new operator is added to `BinaryOp` without
a case.

**Acceptance:** `switch` statement compiles without `-Wswitch` warnings.  All codegen
tests pass.

---

### 6.3 Remove `std::move` on `make_shared` prvalue

File: `src/semantic/types/solver/Unifier.cpp`

```cpp
// Before
Unifier::Unifier() : unionFind(std::move(std::make_shared<UnionFind>())) {}
// After
Unifier::Unifier() : unionFind(std::make_shared<UnionFind>()) {}
```

No observable effect; removes a misleading anti-pattern.

---

### 6.4 `SymbolTable` constructor: pass-by-value-and-move

```cpp
SymbolTable(
    std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap,
    std::map<ASTDeclNode *, std::map<std::string, ASTDeclNode *>> lMap,
    std::vector<std::string> fSet)
    : functionNames(std::move(fMap)),
      localNames(std::move(lMap)),
      fieldNames(std::move(fSet)) {}
```

Eliminates the double-copy at every construction site.

---

### 6.5 Ownership-semantics comments on raw observer pointers

Add `// non-owning observer` to each raw-pointer declaration in:
- `TypeInference.h` (`AbsentFieldChecker` member)
- `SemanticAnalysis.h` (any raw pointer members)
- Anonymous namespace in `CodeGenFunctions.cpp`

---

### 6.6 `GlobalVariable` LLVM ownership comment

At each `new llvm::GlobalVariable(...)` call:
```cpp
// The Module takes ownership; do not delete.
new llvm::GlobalVariable(...)
```

---

**Phase 6 acceptance gate:** `grep 'std::move(std::make_shared' src/` returns empty;
no new `-Wswitch` warnings; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
464 assertions / 265 unit test cases + 102 system tests pass.

---

## Phase 7 — Wire Integration, Delete Legacy, and Semantic Encapsulation

**Prerequisite:** Phases 5 and 6 complete.  
**Goal:** Replace `Unifier` with `TipTermBridge` in `TypeInference`, delete the
legacy classes, and fix semantic-subsystem encapsulation issues.

### Test Goals

**Existing coverage:**

- `UnifierTest.cpp` (7 test cases) guards the monomorphic and polymorphic inference
  paths end-to-end.
- `TypeConstraintCollectTest.cpp` (13 cases), `PolyTypeConstraintCollectTest.cpp`
  (2 cases), `AbsentFieldCheckerTest.cpp` (11 cases) exercise the full pipeline
  through `TypeInference`.
- No direct tests of `CubicSolver`'s API exist.
- No structured error-hierarchy tests exist.
- Parser unit tests do not exist.

**Research findings that shape the tests:**

*`TypeInference` wiring — how `Unifier` is used today:*

`runMono` (in `TypeInference.cpp`):
```cpp
auto unifier = make_shared<Unifier>(visitor.getCollectedConstraints());
unifier->solve();
AbsentFieldChecker::check(ast, unifier.get());
return make_shared<TypeInference>(symbols, unifier);
```

`runPoly`:
```cpp
auto unifier = make_shared<Unifier>();   // empty ctor
for each non-recursive function f:
    unifier->add(polyVisitor.getCollectedConstraints());
    unifier->solve();  // incremental
for each residual function f:
    unifier->add(monoVisitor.getCollectedConstraints());
unifier->solve();
AbsentFieldChecker::check(ast, unifier.get());
return make_shared<TypeInference>(symbols, unifier);
```

`TypeInference::getInferredType(ASTDeclNode *)` calls `unifier->inferred(var)`.
`AbsentFieldChecker::check(ASTProgram*, Unifier*)` takes a raw `Unifier*`.

For Phase D: replace `shared_ptr<Unifier>` → `shared_ptr<TipTermBridge>` in `runMono`
and change `AbsentFieldChecker` to accept `TipTermBridge*`.  The incremental
`add()`/`solve()` pattern in `runPoly` (Phase E) is already supported by
`TipTermBridge` since it wraps `TermUnifier::addConstraint()`.

*`CubicSolver` public API:*
```cpp
CubicSolver(vector<ASTFunction*> functions);
void addElementofConstraint(ASTFunction *fn, ASTNode *node);
void addConditionalConstraint(ASTFunction *condition, ASTNode *in,
                              ASTNode *from, ASTNode *to);
void addSubseteqConstraint(ASTNode *from, ASTNode *to);
vector<ASTFunction*> getPossibleFunctionsForExpr(ASTNode *);
```

`CubicSolverNode` is currently a **public top-level class** in `CubicSolver.h`,
with all fields private and `friend CubicSolver`.  Moving it to a private nested
struct changes only the header, not the public interface.

**New test files at phase start:**

*`test/unit/semantic/CubicSolverTest.cpp`* — written against the public API only;
passes before and after the encapsulation change

```
SECTION: no constraints — no callees
  CubicSolver solver({&funcF});
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeX).empty())

SECTION: elementof constraint
  solver.addElementofConstraint(&funcF, &nodeX);
  auto fns = solver.getPossibleFunctionsForExpr(&nodeX);
  REQUIRE(fns.size() == 1)
  REQUIRE(fns[0] == &funcF)

SECTION: subset propagation
  solver.addElementofConstraint(&funcF, &nodeA);
  solver.addSubseteqConstraint(&nodeA, &nodeB);
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeB).size() == 1)

SECTION: conditional constraint fires
  solver.addConditionalConstraint(&funcF, &nodeIn, &nodeFrom, &nodeTo);
  // condition not yet met — nodeTo is empty
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeTo).empty())
  // now satisfy the condition
  solver.addElementofConstraint(&funcF, &nodeIn);
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeTo).size() >= 1)

SECTION: mutual subset cycle converges
  solver.addSubseteqConstraint(&nodeA, &nodeB);
  solver.addSubseteqConstraint(&nodeB, &nodeA);
  solver.addElementofConstraint(&funcF, &nodeA);
  // Both A and B should contain funcF without hanging
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeA).size() == 1)
  REQUIRE(solver.getPossibleFunctionsForExpr(&nodeB).size() == 1)
```

*`test/unit/semantic/SemanticErrorHierarchyTest.cpp`*

```
SECTION: WeedingError is-a SemanticError
  try { throw WeedingError("msg"); }
  catch (SemanticError &e) { REQUIRE(string(e.what()).find("msg") != npos) }

SECTION: SymbolError is-a SemanticError
  [same pattern]

SECTION: TypeCheckError is-a SemanticError
  [same pattern]

SECTION: ParseError is NOT a SemanticError
  try { throw ParseError("msg"); }
  catch (SemanticError &) { FAIL("should not catch as SemanticError"); }
  catch (std::exception &) { SUCCEED(); }

SECTION: each error preserves what() message
  WeedingError e("detail"); REQUIRE(string(e.what()) == "detail")
  SymbolError e2("sym"); REQUIRE(string(e2.what()) == "sym")
  TypeCheckError e3("type"); REQUIRE(string(e3.what()) == "type")
```

*`test/unit/frontend/TIPParserUnitTest.cpp`*

```
SECTION: minimal function
  auto ast = ASTHelper::build_ast("f() { return 0; }");
  REQUIRE(ast->getFunctions().size() == 1)
  REQUIRE(ast->getFunctions()[0]->getName() == "f")
  auto body = ast->getFunctions()[0]->getBody();
  // body contains one ASTReturnStmt
  REQUIRE(dynamic_cast<ASTReturnStmt*>(...) != nullptr)

SECTION: function with formals
  auto ast = ASTHelper::build_ast("f(a, b) { return a + b; }");
  REQUIRE(ast->getFunctions()[0]->getFormals().size() == 2)

SECTION: each binary operator parses correctly
  For each op in {"+", "-", "*", "/", ">", "==", "!="}:
    auto ast = ASTHelper::build_ast("f(a,b) { return a " + op + " b; }");
    auto binExpr = ... (find the ASTBinaryExpr in the return stmt)
    REQUIRE(binExpr->getOp() == op)

SECTION: alloc expression
  auto ast = ASTHelper::build_ast("f(x) { return alloc x; }");
  REQUIRE(dynamic_cast<ASTAllocExpr*>(...) != nullptr)

SECTION: record expression
  auto ast = ASTHelper::build_ast("f() { return {a: 1, b: 2}; }");
  auto rec = dynamic_cast<ASTRecordExpr*>(...);
  REQUIRE(rec->getFields().size() == 2)

SECTION: malformed program throws ParseError
  REQUIRE_THROWS_AS(ASTHelper::build_ast("f() {"), ParseError)
```

**Phase complete when:** `grep 'UnionFind\|Unifier\b' src/` returns empty;
`CubicSolverNode` not visible in any header; error hierarchy tests pass;
parser unit tests pass; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

**Changes:**
- `TypeInference.h`: change `shared_ptr<Unifier> unifier` →
  `shared_ptr<TipTermBridge> unifier`
- `TypeInference.cpp` (`runMono`): construct `TipTermBridge` instead of `Unifier`
- `AbsentFieldChecker.h/.cpp`: accept `TipTermBridge*` instead of `Unifier*`
- `TypeInference::getInferredType()` unchanged (method exists on both)

**Acceptance gate:**
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

### 7.2 Wire `runPoly` to `TipTermBridge` (Migration Phase E)

The polymorphic path does incremental `add()` / `solve()` calls on a single unifier
instance; `TipTermBridge` supports this because `TermUnifier::addConstraint()` can
be called multiple times before successive `solve()` calls.

**Changes:**
- `TypeInference.cpp` (`runPoly`): construct `TipTermBridge` instead of `Unifier`

**Acceptance gate:**
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

### 7.3 Delete legacy `Unifier` and `UnionFind` (Migration Phase F)

```bash
grep -r 'Unifier\b' src/   # confirm zero non-test references
```

**Steps:**
1. Remove `Unifier.h/.cpp`, `UnionFind.h/.cpp`, `UnificationError.h`
2. Remove `solver/UnifierTest.cpp` and `solver/UnionFindTest.cpp`
3. Update `src/semantic/types/solver/CMakeLists.txt`
4. Confirm `Copier`, `TypeVars`, `Substituter` are still consumed by `TipTermClosure`;
   remove any that are truly unused

**Acceptance gate:** No references to deleted classes and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

### 7.4 Nest `CubicSolverNode` inside `CubicSolver`

#### Tests first: `test/unit/semantic/CubicSolverTest.cpp`

All scenarios use only the public `CubicSolver` API, never touching the node directly:

- Add a function; `getPossibleFunctionsForExpr` returns it
- Add an element-of constraint; function appears as a possible callee
- Add a conditional constraint; fires when the condition element is added
- Add a subset constraint; elements propagate transitively
- Cycle: two nodes that mutually subset each other converge correctly

These tests pass both before and after the structural change.

#### Production:

Move `CubicSolverNode` from the header into `CubicSolver.cpp` as a private nested
struct:

```cpp
class CubicSolver {
    struct Node {
        std::set<Node *> supsets;
        std::set<Node *> subsets;
        std::vector<bool> bitvector;
        int size;
        std::vector<std::vector<std::pair<ASTNode *, ASTNode *>>>
            conditionalConstraints;
    };
    ...
```

Using `unique_ptr<Node>` inside `CubicSolver` for owned allocation.

**Acceptance:** `CubicSolverNode` name appears only in `CubicSolver.cpp`.

---

### 7.5 Structured error hierarchy

#### Tests first

- `test/unit/frontend/ParseErrorTest.cpp`
- `test/unit/semantic/SemanticErrorHierarchyTest.cpp`

Scenarios:
- `WeedingError` is-a `SemanticError`
- `SymbolError` is-a `SemanticError`
- `TypeCheckError` is-a `SemanticError`
- `UnificationError` is-a `TypeCheckError` (if not already deleted)
- `ParseError` is its own hierarchy, not `SemanticError`
- Each error type carries the original `what()` message
- Catch sites catching `SemanticError&` still compile without change

#### Target hierarchy:

```
Error
├── ParseError               (already exists)
├── InternalError            (already exists)
└── SemanticError            (already exists)
    ├── WeedingError         (new — currently just SemanticError)
    ├── SymbolError          (new — currently just SemanticError)
    └── TypeCheckError       (new)
```

Add three header-only subclasses.  Update throw sites:
- `CheckAssignable.cpp` → throw `WeedingError`
- `SymbolTable.cpp` and name-collector visitors → throw `SymbolError`
- `TypeInference.cpp` → throw `TypeCheckError`

No catch sites need updating — base-class catch still works.

---

### 7.6 Parser unit tests

**New file:** `test/unit/frontend/TIPParserUnitTest.cpp`

Use `ParserHelper::build_ast` (already exists in `test/unit/helpers`) to test grammar
constructs without running full semantic analysis.

Scenarios:
- Minimal function `f() { return 0; }` → `ASTFunction` with one `ASTReturnStmt`
- Function with formals → `getFormals()` count correct
- Each binary operator → correct `ASTBinaryExpr` operator string
- `alloc(x)` → `ASTAllocExpr`
- Record expression `{a: 1, b: 2}` → correct field count and names
- Malformed program → `ParseError` is thrown, not a crash

---

**Phase 7 acceptance gate:** `grep 'UnionFind\|Unifier\b' src/` returns empty;
`CubicSolverNode` not visible in any header; error hierarchy tests pass;
parser unit tests pass; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

## Phase 8 — `CodeGenContext`: Eliminate Global State

**Prerequisite:** Phase 7 complete.  
**Goal:** Replace the 22 mutable file-scope globals in `CodeGenFunctions.cpp` with a
`CodeGenContext` struct.  Highest-impact readability and correctness change in the
codebase — makes re-entrant use of the code generator possible and all data flow
visible.

### Test Goals

**Existing coverage:**

`CodegenStateIsolationTest.cpp` was written in Phase 2 with `[!mayfail]` tags on
scenarios that expose the global bleed.  Those tags are removed here once the struct
is in place.

**Research findings that shape the tests:**

*Complete list of file-scope variables (22 total):*

```
llvmContext             LLVMContext
irBuilder               IRBuilder<> — initialized from llvmContext
functionIndex           map<string, int>
functionFormalNames     map<string, vector<string>>
namedValues             map<string, AllocaInst*>
globalRecordType        StructType*
pointerToGlobalRecordType  PointerType*
fieldIndex              map<string, int>
fieldVector             vector<string>
CurrentModule           shared_ptr<Module>
nop                     Function*
inputIntrinsic          Function*
outputIntrinsic         Function*
errorIntrinsic          Function*
callocFun               Function*
labelNum                int = 0
lValueGen               bool = false
allocFlag               bool = false
tipFunctionTable        GlobalVariable*
numTIPArgs              int64_t = 0
tipNumInputs            GlobalVariable*
tipInputArray           GlobalVariable*
```

Additionally: `zeroV` and `oneV` are constants derived from `llvmContext` at static
initialization.  They must move to per-context computed values after the refactor.

*Complete `codegen()` call tree (22 node types):*

```
ASTProgram::codegen
  └─ ASTFunction::codegen (per function)
       ├─ ASTDeclNode::codegen (per local decl)
       ├─ ASTDeclStmt::codegen
       ├─ ASTAssignStmt::codegen
       │    ├─ getLHS()->codegen()
       │    └─ getRHS()->codegen()
       ├─ ASTBlockStmt::codegen → each stmt->codegen()
       ├─ ASTWhileStmt::codegen
       │    ├─ getCondition()->codegen()
       │    └─ getBody()->codegen()
       ├─ ASTIfStmt::codegen
       │    ├─ getCondition()->codegen()
       │    ├─ getThen()->codegen()
       │    └─ getElse()->codegen() [optional]
       ├─ ASTOutputStmt / ASTErrorStmt / ASTReturnStmt
       │    └─ getArg()->codegen()
       └─ Expression nodes (all leaves or single-child):
            ASTNumberExpr, ASTBinaryExpr, ASTVariableExpr, ASTInputExpr,
            ASTFunAppExpr, ASTAllocExpr, ASTNullExpr, ASTRefExpr,
            ASTDeRefExpr, ASTRecordExpr, ASTFieldExpr, ASTAccessExpr
```

Every method in this tree reads from or writes to the file-scope globals.
The mechanical migration (Steps 8a–8f) threads `CodeGenContext &ctx` through all
22 methods.

**Tests at phase start:**

Remove `[!mayfail]` from `CodegenStateIsolationTest.cpp` and add one new pure-struct
test that does not require LLVM:

```
SECTION: struct isolation — no LLVM required  [add new, passes immediately after 8a]
  CodeGenContext ctxA, ctxB;
  ctxA.labelNum = 7;
  REQUIRE(ctxB.labelNum == 0)    // default-initialized
  ctxA.lValueGen = true;
  REQUIRE(!ctxB.lValueGen)

SECTION: labelNum bleed  [remove [!mayfail] — passes after Step 8e]
  [existing scenario from Phase 2]

SECTION: namedValues bleed  [remove [!mayfail]]
  [existing scenario from Phase 2]

SECTION: functionIndex bleed  [remove [!mayfail]]
  [existing scenario from Phase 2]
```

**Phase complete when:**
```bash
grep -n 'llvmContext\|irBuilder\|functionIndex\|namedValues\|lValueGen\|allocFlag' \
  src/codegen/CodeGenFunctions.cpp
```
Returns only lines inside function bodies (no file-scope declarations);
all isolation tests pass without `[!mayfail]`; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

```cpp
// src/codegen/CodeGenContext.h
class CodeGenContext {
public:
    llvm::LLVMContext             llvmContext;
    llvm::IRBuilder<>             irBuilder{llvmContext};
    std::shared_ptr<llvm::Module> module;

    // Per-program state
    std::map<std::string, int>                          functionIndex;
    std::map<std::string, std::vector<std::string>>     functionFormalNames;
    std::map<std::string, llvm::AllocaInst *>           namedValues;
    llvm::StructType *   globalRecordType       = nullptr;
    llvm::PointerType *  ptrToGlobalRecordType  = nullptr;
    std::map<std::string, int>   fieldIndex;
    std::vector<std::string>     fieldVector;
    llvm::GlobalVariable *tipFunctionTable = nullptr;
    llvm::GlobalVariable *tipNumInputs     = nullptr;
    llvm::GlobalVariable *tipInputArray    = nullptr;
    llvm::Function *     nop               = nullptr;
    llvm::Function *     inputIntrinsic    = nullptr;
    llvm::Function *     outputIntrinsic   = nullptr;
    llvm::Function *     errorIntrinsic    = nullptr;
    llvm::Function *     callocFun         = nullptr;
    int     labelNum   = 0;
    int64_t numTIPArgs = 0;
    bool    lValueGen  = false;
    bool    allocFlag  = false;

    CodeGenContext() = default;
    CodeGenContext(const CodeGenContext &) = delete;
    CodeGenContext &operator=(const CodeGenContext &) = delete;
};
```

---

### 8.2 Tests required before refactor

Extend `CodegenStateIsolationTest.cpp` (the `[!mayfail]` tag is removed here):

- Construct two `CodeGenContext` objects; modify `labelNum` in one; verify the other
  is unaffected (pure struct test, no LLVM required)
- Compile program A and program B back-to-back using separate `CodeGenContext`
  instances; verify symbol names from A do not appear in B's module

---

### 8.3 Mechanical migration steps

Each step is a separate commit, reviewable in isolation:

**Step 8a:** Add `CodeGenContext.h` with the struct definition.  Add to
`src/codegen/CMakeLists.txt`.  No behaviour change.

**Step 8b:** Add `CodeGenContext &ctx` as the first parameter of every static
helper function in `CodeGenFunctions.cpp`'s anonymous namespace
(`getFunction`, `CreateEntryBlockAlloca`).  Pass globals through `ctx` at each
call site.  The file-scope globals remain.  No behaviour change.

**Step 8c:** Add `CodeGenContext &ctx` as the first parameter of each
`codegen()` method override, working leaf-to-root.  At each site replace global
reads/writes with `ctx.member` accesses.

**Step 8d:** Update `ASTProgram::codegen(SemanticAnalysis*, string)` to construct
a `CodeGenContext`, thread it through recursive calls, and return the module from
`ctx.module`.  Update `CodeGenerator::generate()` accordingly.

**Step 8e:** Delete the anonymous-namespace globals.  Build fails if any usage was
missed.

**Step 8f:** Remove `[!mayfail]` from Phase 2.2 tests and verify they pass.

---

**Phase 8 acceptance gate:**
```bash
grep -n 'llvmContext\|irBuilder\|functionIndex\|namedValues\|lValueGen\|allocFlag' \
  src/codegen/CodeGenFunctions.cpp
```
Returns only lines inside function bodies (no file-scope declarations); all
isolation tests pass without `[!mayfail]`; and:
```bash
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
102 system tests pass.

---

## Phase 9 — Decouple AST from LLVM

**Prerequisite:** Phase 8 complete.  
**Goal:** Remove `virtual llvm::Value *codegen() = 0` from `ASTNode`.  Make the AST
compilable and readable without any LLVM headers.  This is the largest architectural
change; it requires replacing virtual dispatch with an explicit `CodeGenVisitor`.

### Test Goals

**Existing coverage:**

- All AST unit tests (frontend/treetypes/) currently compile with LLVM headers because
  `ASTNode.h` includes `llvm/IR/Value.h`, `llvm/IR/Function.h`, `llvm/IR/Module.h`.
  These tests do not exercise codegen; they just happen to pull in LLVM transitively.
- There are no tests that verify the AST is compilable without LLVM.

**Research findings that shape the tests:**

*LLVM includes in AST headers — full enumeration:*

Only `ASTNode.h` directly includes LLVM:
```cpp
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
```
All other AST headers (`ASTBinaryExpr.h`, `ASTVariableExpr.h`, etc.) inherit this
pollution transitively via `ASTNode.h`.  Removing the three includes from `ASTNode.h`
and deleting `virtual llvm::Value *codegen() = 0` should be sufficient to break the
coupling — no other AST header directly includes LLVM.

*`ASTNode` methods that reference LLVM:*
```cpp
virtual llvm::Value *codegen() = 0;
```
Plus `ASTProgram` has the additional override:
```cpp
virtual std::shared_ptr<llvm::Module> codegen(SemanticAnalysis*, const std::string&);
```

*`CodeGenVisitor` will need one method per node type (22 total):*
`ASTProgram`, `ASTFunction`, `ASTDeclNode`, `ASTDeclStmt`, `ASTAssignStmt`,
`ASTBlockStmt`, `ASTWhileStmt`, `ASTIfStmt`, `ASTOutputStmt`, `ASTErrorStmt`,
`ASTReturnStmt`, `ASTNumberExpr`, `ASTBinaryExpr`, `ASTVariableExpr`,
`ASTInputExpr`, `ASTFunAppExpr`, `ASTAllocExpr`, `ASTNullExpr`, `ASTRefExpr`,
`ASTDeRefExpr`, `ASTRecordExpr`, `ASTFieldExpr`, `ASTAccessExpr`.

**New test file at phase start:**

*`test/unit/frontend/ASTNodeNoLLVMTest.cpp`* — tagged `[!mayfail]` until Step 9d

```
// This file must NOT include any llvm header, directly or transitively.
// It tests that the AST can be used from a translation unit that has no
// LLVM dependency.

SECTION: ASTBinaryExpr construction and getter — no LLVM
  [include only ASTBinaryExpr.h, ASTDeclNode.h, ASTNumberExpr.h]
  ASTNumberExpr left(42), right(1);
  ASTBinaryExpr node("+", make_shared<ASTNumberExpr>(42),
                          make_shared<ASTNumberExpr>(1));
  REQUIRE(node.getOp() == "+")
  REQUIRE(node.getLeft() != nullptr)

SECTION: ASTVariableExpr construction
  ASTDeclNode decl("x");
  ASTVariableExpr varExpr(&decl);
  REQUIRE(varExpr.getName() == "x")

SECTION: ASTProgram construction and traversal — no LLVM
  Build a minimal program AST by hand (one function, one return stmt)
  Create a custom ASTVisitor subclass that records visit order
  ast->accept(&visitor)
  Verify: visit order is ASTProgram → ASTFunction → ASTReturnStmt → ASTNumberExpr

SECTION: compile-time check — no LLVM types visible
  // If this file compiles without error when llvm/IR/*.h are absent from the
  // include path, the test "passes".  Use a static_assert or simply rely on
  // the build system check.
  static_assert(!std::is_base_of_v<SomeNonExistentLLVMType, ASTNode>,
                "ASTNode must not require LLVM");
  // Realistically: the test file compiling is the assertion.
```

The `[!mayfail]` tag ensures CI stays green while the migration proceeds; it is
removed in Step 9d once LLVM includes are stripped from `ASTNode.h`.

**Phase complete when:**
```bash
grep -r 'llvm/IR' src/frontend/   # empty
grep -r 'llvm/IR' src/semantic/   # empty
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
`ASTNodeNoLLVMTest.cpp` passes without `[!mayfail]`; 102 system tests pass.

---

`CodeGenVisitor` is a non-virtual-dispatch class that holds a `CodeGenContext &` and
implements one `generate(ASTNodeType*, CodeGenContext&)` method per node type.  These
methods call each other directly, mirroring the current recursive `codegen()` call
structure.  This minimises behavioural change during migration.

(Alternatively, `ASTCodeGenVisitor` extending `ASTVisitor` with a `result` field
works and is more consistent with the visitor pattern students have already seen in
the codebase.  Either choice is acceptable; pick based on teaching goals.)

---

### 9.2 Tests before refactor: `ASTNodeNoLLVMTest.cpp`

**New file:** `test/unit/frontend/ASTNodeNoLLVMTest.cpp`

These tests compile and run without including any LLVM header.  Because `ASTNode.h`
currently pulls in LLVM, they will fail to compile until the refactor is done.  Tag
`[!mayfail]`.

Scenarios:
- Construct `ASTBinaryExpr`, `ASTVariableExpr`, `ASTDeclNode`, `ASTFunction`,
  `ASTProgram` and verify getters work without an LLVM context
- Build a full mini-program AST by hand; visit it with a custom `ASTVisitor` subclass
  that records visited nodes in order; assert traversal order

---

### 9.3 Migration steps

**Step 9a:** Create `src/codegen/CodeGenVisitor.h/.cpp` with one
`generate(ASTNodeType*, CodeGenContext&)` method per AST node type, bodies copied
verbatim from the existing `codegen()` implementations.  Full test suite passes
(no callers changed yet).

**Step 9b:** Update `ASTProgram::codegen(SemanticAnalysis*, string)` to construct
a `CodeGenVisitor` and delegate to it rather than calling `this->codegen()`
recursively.  Run tests.

**Step 9c:** Remove LLVM includes from each AST node header one at a time.
Forward-declare where necessary.  For each header removed, verify the compiler error
list shrinks.

**Step 9d:** Once all LLVM includes are out of `ASTNode.h`, remove
`virtual llvm::Value *codegen() = 0` from `ASTNode` entirely.

**Step 9e:** Remove the `[!mayfail]` tags from `ASTNodeNoLLVMTest.cpp` and verify
the tests compile and pass.

---

**Phase 9 acceptance gate:**
```bash
grep -r 'llvm/IR' src/frontend/   # empty
grep -r 'llvm/IR' src/semantic/   # empty
TIPCLANG=/opt/homebrew/opt/llvm@17/bin/clang ./bin/runtests.sh
```
`ASTNodeNoLLVMTest.cpp` tests pass without `[!mayfail]`; 102 system tests pass.

---

## Phase summary

| Phase | Key deliverable | New test files | Prod files changed |
|-------|----------------|----------------|--------------------|
| 1 | LLVM 19+ compatibility | — (system tests guard) | `CodeGenFunctions.cpp`, `CMakeLists.txt` |
| 2 | Baseline tests + `[[nodiscard]]` | `CodegenStateIsolationTest`, `ASTBinaryExprTest`, `TipVarSetTest` | Header annotations only |
| 3 | Types helper tests + `TipVarSet` fix | `SubstituterTest`, `CopierTest`, `TypeVarsTest`; extend `TipTypeTermTest`, `TipConsDoMatchTest` | `TipVar.h`, `TypeVars.h/.cpp`, `Substituter.h`, `Unifier.cpp` |
| 4 | TipVarRegistry + TipTermClosure | `TipVarRegistryTest`, `TipTermClosureTest` | `TipVarRegistry.h/.cpp`, `TipTermClosure.h/.cpp` |
| 5 | TipTermBridge facade | `TipTermBridgeTest` | `TipTermBridge.h/.cpp` |
| 6 | C++ idiom cleanup | Extend `ASTBinaryExprTest` | 7 AST headers, `SymbolTable.h`, `Unifier.cpp` |
| 7 | Wire + delete legacy + semantic encapsulation | `CubicSolverTest`, `SemanticErrorHierarchyTest`, `TIPParserUnitTest`, `ParseErrorTest` | `TypeInference.h/.cpp`, delete `Unifier`/`UnionFind`, `CubicSolver.h/.cpp`, 3 error headers |
| 8 | `CodeGenContext` struct | Extend `CodegenStateIsolationTest` | `CodeGenContext.h/.cpp`, `CodeGenFunctions.cpp`, `CodeGenerator.cpp` |
| 9 | Decouple AST from LLVM | `ASTNodeNoLLVMTest` | All AST headers + CPPs, new `CodeGenVisitor.h/.cpp` |

