# TOP Language Tutorial

## Purpose

This tutorial describes TOP as implemented by topc, including the full product-and-sum language surface and nested pattern matching support.

Use this document as the language reference for writing and reading TOP programs.

## 1. Language Overview

TOP keeps TIP's compact imperative core and adds:

1. Sum data types with pattern matching
2. Product data types (structural records with field access/update)
3. Ownership-aware pointers with automatic destruction
4. Borrow expressions for controlled aliasing
5. Nested case-arm patterns (constructor, record, wildcard, variable)

## 2. Core Program Shape

A TOP program is a set of top-level type and function declarations.

```top
fib(n) {
  var f1, f2, i, temp;
  f1 = 1;  f2 = 1;  i = n;
  while (i > 1) {
    temp = f1 + f2;  f1 = f2;  f2 = temp;  i = i - 1;
  }
  return f2;
}

main(n) { return fib(n); }
```

TOP statements and expressions include:

1. var declarations
2. assignment, if/else, while, block
3. output and error
4. pointer operations alloc, dereference, borrow
5. first-class function values and function calls
6. sum constructors and case statements
7. record construction and field access/update

## 3. Sum Types

Declare sum types with type and constructor alternatives:

```top
type Expr = Const(n) | Add(lhs, rhs) | Unit;
```

Constructors in the same type may have different arities:

1. `Const` has arity 1
2. `Add` has arity 2
3. `Unit` has arity 0

Sum declarations are concrete language declarations, not generic type constructors.

Construct values using constructor syntax:

```top
main() {
  var a, b, c;
  a = Const(42);
  b = Add(1, 2);
  c = Unit;
  return 0;
}
```

## 4. Product Types (Records)

TOP supports structural records as product values.

```top
main() {
  var r;
  r = {x:1, y:2};
  r.x = r.x + 10;
  output r.y;
  return r.x;
}
```

Record operations:

1. Construction: `{f:e, g:e2, ...}`
2. Read: `e.f`
3. Write: `e.f = v`
4. Field address: `&e.f`

Records are structurally typed, and field names must be valid for the inferred record shape.

## 5. Case Statements and Patterns

Case arms dispatch on constructors and can destructure payloads with nested patterns.

Supported pattern forms:

1. Variable pattern: `x`
2. Wildcard pattern: `_`
3. Constructor pattern: `Ctor(...)`
4. Record pattern: `{f:p, g:q}`

Example:

```top
type Expr = Const(n) | Add(lhs, rhs) | Unit;

eval(e) {
  var r;
  case e of {
    Const(n) -> r = n;
    Add(l, r2) -> r = eval(l) + eval(r2);
    Unit -> r = 0;
  }
  return r;
}
```

Pattern rules:

1. Bindings in one arm must be unique.
2. Constructor arity must match the declaration.
3. Record-pattern fields must match the record type exactly.
4. Case coverage must be exhaustive for top-level constructors.
5. Redundant unreachable arms are rejected.

## 6. Ownership and Move Semantics

TOP classifies values into two ownership classes:

1. Copy values: assignment copies the value.
2. Own values: assignment moves ownership.

Quick intuition:

1. `int` is Copy.
2. `alloc expr` results are Own.
3. Records are Copy only when all fields are Copy.
4. Records become Own if any field is Own.

Example (Copy assignment):

```top
main() {
  var a, b;
  a = 10;
  b = a;      // copy
  output a;   // still valid
  return b;
}
```

alloc creates owning pointers:

```top
main() {
  var p;
  p = alloc 42;
  return *p;
}
```

Ownership rules:

1. Assigning an owning value moves it.
2. Moved-from values cannot be used.
3. Ownership classification is structural and compositional.
4. Records are Own when any field is Own; otherwise Copy.

Example (move on Own value):

```top
main() {
  var p, q;
  p = alloc 42;
  q = p;      // move
  // output *p;   // rejected: use after move
  return *q;
}
```

Automatic destruction:

1. Owned values live at function exit are destroyed automatically.
2. Destruction recurses through owned fields and nested structures.
3. Manual free is not required in source programs.

## 7. Borrows

Borrow expressions create non-owning references:

```top
increment(p) {
  *p = *p + 1;
  return 0;
}

main() {
  var x, d;
  x = 10;
  d = increment(&x);
  return x;
}
```

Borrow intuition:

1. Borrow does not transfer ownership.
2. Borrow lets another function read/write through the referenced location.
3. The owner remains responsible for lifetime and destruction.

Borrow restrictions:

1. A borrow must be an immediate call argument.
2. Borrow values cannot be stored in variables.
3. Borrow values cannot be returned.
4. Borrowing a field address is allowed when it appears in a valid borrow position.

## 8. Static Checks You Should Expect

TOP rejects programs with:

1. Unknown constructor tags
2. Non-exhaustive case coverage
3. Unknown record fields
4. Duplicate bindings in one pattern arm
5. Use-after-move of owning values
6. Illegal borrow positions

## 9. Constraints and Limits

TOP intentionally keeps some constraints explicit:

1. Comparisons are limited to `>`, `==`, and `!=`.
2. Return must be the final statement in a function body.
3. Call results must be assigned (no void call statement form).

## 10. End-to-End Example

```top
type Command = Sum(pair) | Zero | Shift(dx, dy);

apply(c) {
  var r;
  case c of {
    Sum({a:x, b:y}) -> r = x + y;
    Zero -> r = 0;
    Shift(dx, dy) -> r = dx + dy;
  }
  return r;
}

main() {
  var c;
  c = Sum({a:20, b:22});
  if (apply(c) != 42) error 1;
  output 0;
  return 0;
}
```

## 11. Summary

TOP supports sum and product data types with pattern matching, plus:

1. Ownership-aware safety checks
2. Automatic destruction of owned values
3. Compile-time checks for moves, borrows, and pattern correctness

The result is a compact imperative language with expressive data modeling and compile-time memory-safety enforcement.
