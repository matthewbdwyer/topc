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

### Current File Structure

```
src/semantic/types/
├── Unifier.h / Unifier.cpp           # Unification algorithm (coupled to TipType)
├── TipType.h                          # Base class for all TIP types
├── TipAlpha.h / TipAlpha.cpp         # Type variables (α)
├── TipInt.h / TipInt.cpp             # Integer type
├── TipFunction.h / TipFunction.cpp   # Function types
├── TipMu.h / TipMu.cpp               # Recursive types (μ)
├── TipRef.h / TipRef.cpp             # Pointer/reference types
├── TipRecord.h / TipRecord.cpp       # Record types
├── TipAbsentField.h / TipAbsentField.cpp  # Absent field marker
├── TipVar.h / TipVar.cpp             # Type variables tied to AST nodes
├── TipCons.h / TipCons.cpp           # Type constructors (base)
├── TypeInference.h / TypeInference.cpp    # Orchestration
└── constraints/
    ├── TypeConstraint.h / TypeConstraint.cpp
    ├── TypeConstraintVisitor.h
    ├── TypeConstraintCollectVisitor.h / TypeConstraintCollectVisitor.cpp
    └── TypeConstraintUnifyVisitor.h / TypeConstraintUnifyVisitor.cpp
```

### Current Test Structure

```
test/unit/semantic/types/
├── UnifierTest.cpp           # Tests unification with TipType instances
├── TypeConstraintTest.cpp    # Tests constraint collection
└── SolverTest.cpp           # Integration tests for solving
```

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

## 3. Proposed Architecture

### 3.1 New Directory Structure

```
src/semantic/types/
├── solver/                           # NEW: Generic, reusable unification
│   ├── Term.h                        # Abstract term interface
│   ├── Substitution.h / .cpp         # Generic substitution
│   └── Unifier.h / .cpp              # Generic unification algorithm
├── concrete/                         # TIP-specific types (moved)
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
│   └── ...
└── TypeInference.h / .cpp            # Unchanged location
```

---

## 4. Parallel Development Approach

### 4.1 Strategy

To minimize risk and enable incremental testing, we develop the new abstraction **in parallel** with the existing implementation:

| Existing File | New Parallel File | Purpose |
|---------------|-------------------|---------|
| `Unifier.h/.cpp` | `TermUnifier.h/.cpp` | Generic unification algorithm |
| (none) | `Term.h` | Abstract term interface |
| (none) | `MockTerm.h` | Test mock implementations |
| `TipType.h` | (modify in place later) | Will extend `Term` after validation |

### 4.2 Development Phases

**Phase 1: Core Abstraction** ✅ COMPLETE
- [x] Create `Term.h` - abstract interface
- [x] Create `MockTerm.h` - mock implementations for testing
- [x] Create `TermUnifier.h/.cpp` - generic unifier
- [x] Create `TermUnifierTest.cpp` - unit tests for generic unifier

**Phase 2: Validation** ✅ COMPLETE
- [x] All `TermUnifierTest.cpp` tests pass (17 tests)
- [x] Review error handling (`TermUnificationError`)
- [x] Verify occurs check works correctly
- [x] Verify `close()` resolves transitive bindings

**Phase 3: TipType Integration** ✅ COMPLETE
- [x] Make `TipType` extend `Term`
- [x] Implement `Term` interface in all `TipType` subclasses
- [x] Create `TipTypeTermTest.cpp` to validate implementations
- [x] Renamed `Term.h` to `TermInterface.h` to avoid system header collision
- [x] All TipType Term interface tests passing (50+ tests)

**Phase 4: Cutover** ← NEXT
- [ ] All existing type inference tests pass with new implementation
- [ ] Remove old `Unifier.h/.cpp` (or keep both)
- [ ] Rename `TermUnifier` → `Unifier` (or keep as `TermUnifier`)
- [ ] Update all includes and references
- [ ] Run full system test suite

### 4.3 File Mapping

```
src/semantic/types/solver/          # New directory
├── Term.h                          # Abstract interface
├── TermUnifier.h                   # Generic unifier (parallel to Unifier.h)
├── TermUnifier.cpp                 # Implementation
└── TermUnificationError.h          # Error class (if separate)

test/unit/semantic/types/solver/    # New test directory
├── MockTerm.h                      # Mock implementations
└── TermUnifierTest.cpp             # Unit tests
```

---

## 5. Implementation State Assessment

### Current State (as of latest review)

**Build Status:** ✅ Compiles successfully
**Test Status:** ✅ All tests passing
- 17 TermUnifier tests passing
- 50+ TipType Term interface tests passing
- All existing type inference tests passing

**To run all Term-related tests:**
```bash
./test/unit/semantic/types/typeinference_unit_tests "[Term]"
./test/unit/semantic/types/typeinference_unit_tests "[TermUnifier]"
./test/unit/semantic/types/typeinference_unit_tests "[TipType][Term]"
```

**Completed:**
- `TermInterface.h` - abstract Term interface (renamed from `Term.h`)
- `MockTerm.h` with `MockVar` and `MockCons` implementations  
- `TermUnifier.h/.cpp` with generic unification
- `TermUnificationError` exception class
- All TipType classes implement Term interface:
  - `TipType` - base class extending `Term`
  - `TipVar` - `isVariable() = true`
  - `TipAlpha` - `isVariable() = false` (proper type)
  - `TipInt` - nullary constructor
  - `TipAbsentField` - nullary constructor
  - `TipRef` - unary constructor
  - `TipFunction` - n-ary constructor
  - `TipRecord` - n-ary constructor with sorted field names in functor
  - `TipMu` - binary constructor
  - `TipCons` - base class with `arity()` returning `std::size_t`

**Next Step: Phase 4 - Cutover**

1. Run full test suite to verify no regressions
2. Decide whether to replace `Unifier` with `TermUnifier` or keep both
3. Clean up any unused code

---

## Appendix A: Current Unifier Analysis

Based on examination of the current `src/semantic/types/Unifier.h` and `Unifier.cpp`:

### A.1 Current Constraint Representation

```cpp
// Current implementation uses TypeConstraint which holds pairs of shared_ptr<TipType>
// Constraints are collected by TypeConstraintCollectVisitor and stored in a vector
std::vector<TypeConstraint> constraints;
```

The current `TypeConstraint` class (in `constraints/TypeConstraint.h`) stores:
- `std::shared_ptr<TipType> lhs` - left-hand side of equality
- `std::shared_ptr<TipType> rhs` - right-hand side of equality

### A.2 Current Substitution Data Structure

```cpp
// The Unifier maintains a union-find structure for type variables
// Key insight: Uses TipVar's AST node pointer as the unique identifier
std::map<std::shared_ptr<TipType>, std::shared_ptr<TipType>> unionFind;
```

The substitution is implemented via:
1. **Union-Find**: Variables point to their representative
2. **Path compression**: `find()` operation flattens chains
3. **Close operation**: Resolves all variables to concrete types, creating `TipMu` for cycles

### A.3 Special Handling for TIP-Specific Types

The current Unifier has explicit knowledge of TIP types:

1. **TipVar handling**: 
   ```cpp
   if (auto *var = dynamic_cast<TipVar*>(t.get())) {
     // Variable-specific unification logic
   }
   ```

2. **TipAlpha handling**: `TipAlpha` is treated as a **proper type**, not a variable. It represents a universally quantified type variable in the output (e.g., `∀α.α→α`), not a unification variable.

3. **TipCons handling**: `TipCons` is a base class for compound types. The Unifier uses `dynamic_cast` to identify constructors:
   ```cpp
   if (auto *func = dynamic_cast<TipFunction*>(t.get())) { ... }
   if (auto *ref = dynamic_cast<TipRef*>(t.get())) { ... }
   if (auto *record = dynamic_cast<TipRecord*>(t.get())) { ... }
   ```

4. **TipMu creation**: During `close()`, cycles are detected and wrapped in `TipMu`:
   ```cpp
   // When a variable transitively refers to itself
   // Create μα.T where α is fresh and T is the cyclic type
   ```

5. **TipRecord special matching**: Records are unified by matching field names, not by position. Two records unify if they have compatible fields (using row polymorphism with `TipAbsentField`).

### A.4 Error Handling Approach

Current error handling:
```cpp
// Throws std::runtime_error with a message constructed from TipType::print()
throw std::runtime_error("Type error: cannot unify " + t1->print() + " with " + t2->print());
```

The error messages are TIP-specific and include source location information extracted from `TipVar` nodes.

### A.5 Key Methods to Preserve/Replicate

| Method | Purpose | Generic Equivalent |
|--------|---------|-------------------|
| `unify(TipType*, TipType*)` | Main unification | `unify(Term*, Term*)` |
| `find(TipType*)` | Follow union-find chain | `find(Term*)` |
| `union_(TipType*, TipType*)` | Merge equivalence classes | Part of `unify` |
| `close()` | Resolve all variables | `close()` - needs TIP-specific hook for TipMu |
| `getType(TipVar*)` | Get resolved type for variable | `apply(Term*)` |

### A.6 Dependencies on TipType Hierarchy

The current Unifier depends on:
- `TipType.h` - base class
- `TipVar.h` - unification variables
- `TipCons.h` - compound types (via subclasses)
- `TipFunction.h` - function types
- `TipRef.h` - pointer types  
- `TipRecord.h` - record types
- `TipAbsentField.h` - row polymorphism marker
- `TipMu.h` - recursive types (created during close)
- `TipAlpha.h` - type variables in output (created during close)

---

## Appendix B: Resolved Open Questions

### Q2 Resolution: TipAlpha vs TipVar

Based on code analysis:

- **TipVar**: `isVariable() = true` - These are unification variables created during constraint generation, one per AST node.
- **TipAlpha**: `isVariable() = false` - These are **proper types** representing universally quantified type variables in the final type scheme. They are created during `close()` when generalizing types.

```cpp
// TipAlpha.h - should implement:
bool isVariable() const override { return false; }  // NOT a unification variable
std::string getFunctor() const override { return "α" + std::to_string(id); }
```

### Q3 Resolution: TipMu Handling

The generic Unifier should **not** create `TipMu` directly. Instead:

1. Generic `Unifier::close()` detects cycles and marks them
2. A TIP-specific post-processing step (or callback) creates `TipMu` wrappers

**Design decision**: Add an optional callback to the generic Unifier:
```cpp
using CycleHandler = std::function<std::shared_ptr<Term>(
    const std::string& varName, std::shared_ptr<Term> cyclicTerm)>;
```

### Q4 Resolution: TipRecord Field Handling

Records should use **functor encoding** with sorted field names:

```cpp
// TipRecord implementation
std::string getFunctor() const override {
  std::ostringstream os;
  os << "record{";
  bool first = true;
  for (const auto& [name, _] : fields) {  // fields is sorted map
    if (!first) os << ",";
    os << name;
    first = false;
  }
  os << "}";
  return os.str();
}

std::size_t arity() const override { return fields.size(); }

std::vector<std::shared_ptr<Term>> getSubterms() const override {
  std::vector<std::shared_ptr<Term>> result;
  for (const auto& [_, type] : fields) {
    result.push_back(type);
  }
  return result;
}
```

This means two records with the same field names (in sorted order) will have matching functors.

---

## Appendix C: Enhanced Testing Strategy

### C.1 Generic Unifier Test Matrix

| Test Category | Test Cases |
|---------------|------------|
| **Variable binding** | Var=Const, Var=Var, Var=Compound |
| **Occurs check** | X=f(X), X=f(g(X)), mutual recursion |
| **Compound unification** | Same functor/arity, different functor, different arity |
| **Transitivity** | X=Y, Y=Z, Z=a ⟹ X=a |
| **Complex constraints** | Multiple constraints, order independence |
| **Edge cases** | Empty constraints, single constraint, already unified |

### C.2 TipType Term Implementation Tests

Each TIP type class needs tests verifying Term interface:

```cpp
// test/unit/semantic/types/concrete/TipTypeTermTest.cpp

TEST_CASE("TipInt implements Term correctly") {
  auto t = std::make_shared<TipInt>();
  REQUIRE_FALSE(t->isVariable());
  REQUIRE(t->getFunctor() == "int");
  REQUIRE(t->arity() == 0);
  REQUIRE(t->getSubterms().empty());
  REQUIRE(t->withSubterms({})->equals(*t));
}

TEST_CASE("TipVar implements Term correctly") {
  auto node = /* mock AST node */;
  auto t = std::make_shared<TipVar>(node);
  REQUIRE(t->isVariable());
  REQUIRE(t->arity() == 0);
  REQUIRE(t->getSubterms().empty());
}

TEST_CASE("TipFunction implements Term correctly") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
    std::vector<std::shared_ptr<TipType>>{intType}, intType);
  REQUIRE_FALSE(func->isVariable());
  REQUIRE(func->getFunctor() == "->");
  REQUIRE(func->arity() == 2);
  auto subs = func->getSubterms();
  REQUIRE(subs.size() == 2);
}

TEST_CASE("TipAlpha is NOT a variable") {
  auto alpha = std::make_shared<TipAlpha>(/* ... */);
  REQUIRE_FALSE(alpha->isVariable());  // Important!
}

TEST_CASE("TipRecord functor encodes field names") {
  // Record with fields {x: int, y: int}
  auto rec = std::make_shared<TipRecord>(/* fields */);
  REQUIRE(rec->getFunctor() == "record{x,y}");  // sorted
  REQUIRE(rec->arity() == 2);
}

TEST_CASE("TipMu implements Term correctly") {
  auto alpha = std::make_shared<TipAlpha>(/* ... */);
  auto body = /* some type containing alpha */;
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE_FALSE(mu->isVariable());
  REQUIRE(mu->getFunctor() == "μ");
  REQUIRE(mu->arity() == 2);  // binder and body
}
```

### C.3 Integration Tests

```cpp
// test/unit/semantic/types/TypeInferenceIntegrationTest.cpp

TEST_CASE("Type inference uses generic Unifier correctly") {
  // Parse a TIP program
  // Run type inference
  // Verify results match expected types
  // This validates the integration between:
  // - Constraint collection (unchanged)
  // - Generic Unifier (new)
  // - TipType Term implementations (new)
}
```

### C.4 Regression Test Suite

All existing tests in these files must pass:
- `test/unit/semantic/types/UnifierTest.cpp`
- `test/unit/semantic/types/TypeConstraintTest.cpp`
- `test/unit/semantic/types/SolverTest.cpp`
- `test/system/` type inference tests

---

## Appendix D: CMakeLists.txt Configuration

### Important: Single CMakeLists.txt Approach

The `solver/` directory should **NOT** have its own `CMakeLists.txt`. All source files are included directly in the parent `src/semantic/types/CMakeLists.txt`:

```cmake
# src/semantic/types/CMakeLists.txt
# All solver files are listed in target_sources() - no separate CMakeLists.txt in solver/
target_sources(types PRIVATE
    # ...existing code...
    ${CMAKE_CURRENT_SOURCE_DIR}/solver/Term.h
    ${CMAKE_CURRENT_SOURCE_DIR}/solver/TermUnifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver/TermUnifier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/solver/Unifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver/Unifier.h
    # ...etc...
)
```

### Test Configuration

Tests for the generic unifier are in `test/unit/semantic/types/solver/` and should be configured in the test CMakeLists.txt (not in the source tree).

<!-- ...rest of existing appendix content... -->