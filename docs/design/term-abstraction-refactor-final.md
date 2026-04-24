# Term Abstraction Refactoring Design Document

## 1. Problem Statement

### Current Architecture

The type inference subsystem in TIP currently has tight coupling between:

1. **The Unifier** (`src/semantic/types/Unifier.h/.cpp`) - implements the unification algorithm
2. **Concrete TIP Types** (`src/semantic/types/TipType.h` and subclasses) - represent TIP's type system
3. **Type Constraints** (`src/semantic/types/constraints/TypeConstraint.h`) - pairs of types to unify

### Specific Issues

1. **Unifier depends on TipType**: The `Unifier` class directly references `TipType` and its subclasses, making it impossible to reuse for other term unification problems.

2. **No abstract term interface**: There's no separation between "what a term is" (structure) and "what TIP types are" (semantics). The unification algorithm only needs to know:
   - Is this term a variable?
   - What is the constructor/functor?
   - What are the subterms?

3. **Testing limitations**: Unit tests in `test/unit/semantic/types/` must use concrete TIP types even when testing unification logic that should be type-agnostic.

4. **Extensibility concerns**: Adding new type constructors or modifying the type system requires understanding and potentially modifying the Unifier.

---

## 2. Goals

### Primary Goals

1. **Decouple the Unifier from TipType**: The unification algorithm should operate over an abstract `Term` interface.

2. **Enable Unifier reuse**: The solver should be usable for other term unification problems (e.g., other type systems, symbolic computation).

3. **Improve testability**: Enable unit testing of the Unifier with simple mock term implementations.

4. **Maintain backward compatibility**: Existing code using `TypeInference` should continue to work without changes.

### Non-Goals

1. We are **not** changing the TIP type system itself.
2. We are **not** modifying the constraint collection visitors.
3. We are **not** changing the external API of `TypeInference`.

---

## 3. Architecture

### 3.1 Directory Structure

```
src/semantic/types/
├── solver/                           # Generic, reusable unification & TIP helpers
│   ├── TermInterface.h               # Abstract term interface
│   ├── TermUnifier.h / .cpp          # Generic unification algorithm
│   ├── Unifier.h / .cpp              # TIP-specific unifier & closure
│   └── Substitution.h / .cpp         # Generic substitution
├── concrete/                         # TIP-specific types
│   ├── TipType.h                     # Now extends Term
│   ├── TipAlpha.h / .cpp
│   ├── TipInt.h / .cpp
│   ├── TipFunction.h / .cpp
│   ├── TipMu.h / .cpp
│   ├── TipRef.h / .cpp
│   ├── TipRecord.h / .cpp
│   ├── TipAbsentField.h / .cpp
│   ├── TipVar.h / .cpp
│   └── TipCons.h / .cpp
├── constraints/                      # Unchanged location
└── TypeInference.h / .cpp            # Orchestration
```

---

## 4. Development Phases

**Phase 1: Core Abstraction** ✅ COMPLETE
- [x] Create `TermInterface.h` - abstract interface
- [x] Create `MockTerm.h` - mock implementations for testing
- [x] Create `TermUnifier.h/.cpp` - generic unifier
- [x] Create `TermUnifierTest.cpp` - unit tests for generic unifier

**Phase 2: Validation** ✅ COMPLETE
- [x] Review error handling (`TermUnificationError`)
- [x] Verify occurs check works correctly
- [x] Verify `close()` resolves transitive bindings
- [ ] ⚠️ `TermUnifierTest.cpp` was listed as complete but does **not exist on disk** — to be written in Phase 4a

**Phase 3: TipType Integration** ✅ COMPLETE
- [x] Make `TipType` extend `Term`
- [x] Implement `Term` interface in all `TipType` subclasses
- [x] Create `TipTypeTermTest.cpp` to validate implementations
- [x] Renamed `Term.h` to `TermInterface.h` to avoid system header collision
- [x] All TipType Term interface tests passing (50+ tests)

**Phase 4a: Test Infrastructure (TDD Setup)** ← NEXT
- [ ] Move `solvers/UnifierTest.cpp` → `solver/UnifierTest.cpp` (fixes broken `CMakeLists.txt` reference)
- [ ] Move `solvers/UnionFindTest.cpp` → `solver/UnionFindTest.cpp` (fixes broken `CMakeLists.txt` reference)
- [ ] Verify full build and all tests pass — this is the green baseline before any production code changes
- [ ] Write `solver/TermUnifierTest.cpp` using only `MockVar` / `MockCons` (no AST, no TIP types), covering:
  - Basic variable unification (one variable binds to another)
  - Variable unifies with a constructor term
  - Two identical constructors unify; subterms unified recursively
  - Functor clash throws `TermUnificationError`
  - Arity mismatch throws `TermUnificationError`
  - Occurs check throws when variable would unify with a term containing itself
  - `close()` resolves transitive variable chains
  - `close()` detects and handles cycles
- [ ] All `TermUnifierTest.cpp` tests pass (target: ≥17 tests)

**Phase 4b: Unifier Refactoring (TDD Execution)** 
- [ ] Refactor `Unifier::unify()` to use Term interface (`isVariable()`, `matchesFunctor()`, `getSubterms()`) — zero `dynamic_cast` calls remaining in the `unify()` function body.
- [ ] Keep `Unifier::close()` unchanged — it requires TIP-specific `TipAlpha`/`TipMu` construction.
- [ ] Keep `UnionFind` and TIP-specific helpers (`TypeVars`, `Copier`, `Substituter`) unchanged.
- [ ] Document the boundary between `TermUnifier` (generic) and `Unifier` (TIP-specific) in headers.
- [ ] All `UnifierTest.cpp` integration tests pass (acceptance gate — verifies backward compatibility per Non-Goals §2)
- [ ] Done criterion: `grep 'dynamic_cast' src/semantic/types/solver/Unifier.cpp` returns no matches inside `unify()`

**Test layer roles after Phase 4:**
- `solver/TermUnifierTest.cpp` — fast, isolated unit tests for generic unification logic (no TIP dependency)
- `concrete/TipTypeTermTest.cpp` — Term interface compliance tests for every TipType subclass
- `solver/UnifierTest.cpp` — integration/acceptance tests; validate end-to-end TIP type inference pipeline
- `solver/UnionFindTest.cpp` — unit tests for the UnionFind data structure

---

## 5. Implementation State Assessment

### Current State (as of latest review)

**Build Status:** ⚠️ Broken — `CMakeLists.txt` references `solver/UnifierTest.cpp` and `solver/UnionFindTest.cpp` but both files reside in `solvers/`. Fix is the first step of Phase 4a.

**Test Status (known issues):**
- `solver/TermUnifierTest.cpp` — listed as complete in Phase 2, does **not exist on disk**
- `solver/UnifierTest.cpp` — exists as `solvers/UnifierTest.cpp` (wrong directory)
- `solver/UnionFindTest.cpp` — exists as `solvers/UnionFindTest.cpp` (wrong directory)
- `concrete/TipTypeTermTest.cpp` — ✅ exists and passes (50+ tests)
- All concrete `Tip*Test.cpp` files — ✅ exist and pass

**Completed Details:**
- `TermInterface.h` — abstract Term interface
- `MockTerm.h` with `MockVar` and `MockCons` implementations (in `test/unit/semantic/types/solver/`)
- `TermUnifier.h/.cpp` — generic unification algorithm
- All TipType classes successfully implement the Term interface

**To run all Term-related tests (once Phase 4a is complete):**
```bash
./test/unit/semantic/types/typeinference_unit_tests "[Term]"
./test/unit/semantic/types/typeinference_unit_tests "[TermUnifier]"
./test/unit/semantic/types/typeinference_unit_tests "[TipType][Term]"
```

---

## Appendix A: Current Unifier Analysis

Based on examination of `src/semantic/types/solver/Unifier.cpp`:

### A.1 Core Unification Algorithm: Equivalent ✅

| Feature | Unifier | TermUnifier |
|---------|---------|-------------|
| Variable binding | `unionFind->quick_union()` | `substitution[varName] = term` |
| Occurs check | ❌ Not implemented | ✅ `occursIn()` |
| Structural matching | `TipCons::doMatch()` | `term->matchesFunctor()` |
| Variable test | `dynamic_cast<TipVar*>` | `term->isVariable()` |
| Compound recursion | Direct recursion | Constraint worklist |

### A.2 Closure: NOT Equivalent - TIP-Specific ❌

The TIP-specific closure (`Unifier::close()`) does several things that cannot be easily generalized into the `TermUnifier`:

| Feature | `Unifier::close()` | `TermUnifier::close()` |
|---------|-----------------|---------------------|
| Follow variable chains | ✅ via `unionFind->find()` | ✅ via `substitution` lookup |
| Detect cycles | ✅ via `visited` set | ✅ via `resolving` set |
| Create `TipAlpha` | ✅ `make_shared<TipAlpha>(node)` | ❌ Not supported |
| Create `TipMu` | ✅ `make_shared<TipMu>(v, closed)` | ❌ Callback only - insufficient |
| Substitute in terms | ✅ `Substituter::substitute()` | ❌ Relies on rebuilding |
| Collect free vars | ✅ `TypeVars::collect()` | ❌ Not supported |

### A.3 Conclusion and Final Recommendation

`TermUnifier` is a **complete, correct, generic unifier** suitable for unit testing, future reuse, and non-TIP term unifications.

However, `Unifier` must **remain as the main TIP solver** in production because:
1. `close()` requires dynamically creating `TipAlpha` (for unconstrained variables in general type schemes).
2. It requires creating `TipMu` for recursive cycles.
3. It uses `TypeVars::collect()` and `Substituter::substitute()` to accurately deduce if a variable is genuinely free before generating recursive types.
4. Altering the algorithm to abstract away these TIP-specific constraints would require a labyrinth of callbacks which yields high regression risk with no architectural benefit.

**Phase 4 Goal Revised:** 
The transition should just refactor `Unifier::unify()` to depend heavily on the new abstract `TermInterface` (by utilizing properties like `isVariable()` and `getSubterms()`) to eliminate type casting, while maintaining `Unifier::close()` exactly as it is for language-specific type constraints.
