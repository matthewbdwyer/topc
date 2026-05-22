## TOP: A Memory-Safe Redesign of the TIP Language
TOP (Ownership-TIP) is a redesign of the Tiny Imperative Programming (TIP) language. It transforms TIP from a "leaky" minimalist language into a memory-safe, statically verified language by introducing Ownership, Lifetimes, and Structural Types, all while maintaining TIP’s signature implicit type inference.
------------------------------
## 1. Core Philosophy

* Zero-Cost Safety: Eliminate memory leaks without a Garbage Collector (GC).
* Explicit Intent: Use the & operator to distinguish between "borrowing" and "moving."
* Inferred Lifetimes: The compiler tracks how long data lives so the programmer doesn't have to.
* Resource Responsibility: Every allocation has exactly one "owner" at any given time.

------------------------------
## 2. Ownership and Move Semantics
In TOP, variables don't just hold values; they own resources.
## Move-on-Assignment
Standard assignment (p = q) is now a move.

* When q is assigned to p, p becomes the new owner of the memory.
* q is marked as invalid/uninitialized.
* Any further use of q results in a compile-time Use-after-move error.

## Automatic Deallocation
The compiler tracks the lexical scope of every "Owner."

* When an owner variable goes out of scope (at the closing }), the compiler automatically injects an LLVM free instruction.
* This ensures no memory leaks by construction.

------------------------------
## 3. Borrowing with &
To avoid the "Hot Potato" problem (repeatedly moving and returning pointers), TOP introduces Borrowing.

* The & Operator: Used to "lend" access to a resource without giving up ownership.
* Immutability: To keep the initial redesign simple, all borrows are read-only.
* The Rule of Lifetimes: A borrow (&p) cannot outlive the owner (p). The type system enforces this via Lifetime Inference.

main() {
  var p, x;
  p = alloc 10;   // p owns the memory
  inspect(&p);    // p is borrowed; inspect() cannot free it
  x = p;          // p is moved to x; p is now invalid
  // inspect(&p); // COMPILE ERROR: p no longer owns data
} // x goes out of scope; free(x) is called automatically

------------------------------
## 4. Strengthened Type System
TOP extends TIP’s unification-based inference to include Subtyping and Lifetime Constraints.

* Types as Pairs: A type is represented as (BaseType, OwnershipState).
* Constraint Solver: Instead of just checking if T1 == T2, the solver ensures that for any borrow, the Lifetime(Source) >= Lifetime(Borrow).
* Polymorphism: Supports implicit polymorphism. A function f(x) { return x; } is inferred as $\forall \alpha, \ell. \&^{\ell} \alpha \rightarrow \&^{\ell} \alpha$.

------------------------------
## 5. Enhanced Data Structures (Optional Additions)
To further strengthen the language, TOP introduces formal Product and Sum types, replacing raw pointer offsets.
## Product Types (Records)
Groups related data into a single owned unit.

type User = { id: int, balance: int }
var u;
u = { id: 1, balance: 100 };

## Sum Types (Tagged Unions)
Allows a variable to hold one of several distinct types. This eliminates null pointer errors by forcing the use of pattern matching or explicit checks.

type Option = Some(ptr) | None

var p;
p = Some(alloc 5);

// The type system forces you to handle the 'None' case
case p of
  Some(val) -> inspect(&val)
  None      -> print 0

------------------------------
## 6. Feature Summary: TIP vs. TOP

| Feature | Standard TIP | TOP |
|---|---|---|
| Memory Management | Manual (Leaky) | Automatic (RAII/Scope) |
| Pointer Safety | Unsafe / No checks | Ownership & Borrowing |
| Null Pointers | Allowed (Runtime Crash) | Eliminated (Sum Types) |
| Assignment | Copy (Aliasing) | Move (Unique Ownership) |
| Inference | Simple Unification | Constraint-based Lifetimes |

------------------------------
Next Steps: Should we define the Internal Representation (IR) changes needed for the tipc compiler to handle the automatic free injections?


