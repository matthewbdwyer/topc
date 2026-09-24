# TOPC Compiler Architecture

## Purpose

This document describes the durable architecture of `topc`: the order in which
compiler phases establish facts, the semantic products they produce, and how
later phases consume those products. For the TOP language surface and examples,
see [TOP-tutorial.md](TOP-tutorial.md). For commands that expose intermediate
results, see [topc-analysis-views-demo.md](topc-analysis-views-demo.md).

## Compilation Pipeline

A full compilation processes a program in this order:

1. Parse TOP source and construct the AST.
2. Build the symbol table.
3. Validate assignability and source-level borrow positions.
4. Validate algebraic type names, patterns, and case coverage.
5. Build source-level control-flow graphs.
6. Build the call graph.
7. Infer types and generalized function schemes.
8. Check that `alloc` payloads are not owned (`OwnershipTypeRules`).
9. Classify inferred types for ownership (`OwnershipClassifier`).
10. Build function-effect summaries: formal modes, return origins, and the
    copy requirements a body imposes (`FunctionEffectSummaries::build`).
11. Check positions: where borrow-derived values and aliases may appear,
    recording copy requirements for generic dereferences
    (`BorrowChecker::checkPositions`).
12. Resolve copy requirements to a fixed point, judge them at each call, and
    compute call effects (`FunctionEffectSummaries::resolveRequirements`).
13. Reject borrows stored as components of values or results
    (`OwnershipTypeRules`).
14. Analyze ownership moves and plan destruction (`MoveAnalysis`).
15. Insert automatic destruction from the plan (`DestructionPass`).
16. Generate and optionally optimize LLVM bitcode, executing the plan's frees.

The ownership passes are organized by the kind of fact each rule is:

| Kind | Decided by | Module |
| --- | --- | --- |
| read-off | a solved type | `OwnershipClassifier`, `OwnershipTypeRules` |
| position | an expression's parent | `BorrowChecker` |
| path | the statements before a point | `MoveAnalysis` (checking and planning), `DestructionPass` (rewriting) |
| instantiation | the types at a call site | `FunctionEffectSummaries` |

Every check reports through `RuleToggles::reject(id, message)`; the rule ids are
listed in `src/error/RuleToggles.cpp` (see `test/system/soundness/README.md`
for how they are used to test adequacy).

The source CFG is built before destruction insertion so analysis views continue
to represent the program the student wrote. Inspection-only driver paths run
the minimum stages needed by the requested view. The current driver and
`SemanticAnalysis` still encode parts of this ordering separately.

## Inferred Types

TOP has no source-level type annotations. Type constraints are generated from
the AST and solved by unification. Eligible non-recursive top-level functions
are generalized, and each call site receives an instantiation of the resulting
type scheme.

Algebraic declarations are nominal: two declarations with the same constructor
shape are still different types. Constructor payload slots begin as inference
variables and acquire one program-wide inferred type from all uses of that
declaration. They are not source-level type parameters.

Recursive algebraic data is closed into internal `TopMu` types. User-facing
global type output abbreviates recursive and nested algebraic payloads by their
nominal names, for example:

```text
List : Nil | Cons(int, List)
```

Recursive algebraic data is supported. A recursively self-referential function
type can be inferred and displayed, but ownership analysis currently rejects
that type shape with a semantic diagnostic.

## Reference Types

The solver-facing reference model is:

```text
Ref(mode, pointee)
```

`mode` is a separate kind of term and is either concrete or variable:

```text
Ref(Own, int)
Ref(Borrow, int)
Ref(m, int)
```

The C++ implementation uses `ReferenceType`, `ReferenceMode`, and `TopModeVar`.
`TopOwningRef` and `TopBorrowRef` provide concrete views used by existing
semantic and code-generation paths. Inspection output renders the three useful
source-facing shapes as:

```text
own&T
borrow&T
ref&T
```

Allocation generates `Ref(Own, T)`, address-of generates `Ref(Borrow, T)`, and
dereference uses `Ref(m, T)` with a fresh mode variable `m`, because reading or
writing through a reference works for either mode. The mode variable is
resolved by unification with the references that flow into it. Mode variables
are **not generalized**: a function's reference formals take one mode for the
whole program, fixed by whichever use determines it, and a function called with
an owning reference at one site and a borrow at another is rejected
(`Cannot unify Own with Borrow`). A dereference-only function that no call
constrains keeps `Ref(m, T)` and prints as `ref&T`.

## Ownership Classification

After type inference, `OwnershipClassifier` assigns each inferred type one of
two classes:

- `Copy`: integers, functions, and borrow references.
- `Own`: owning references and algebraic (sum) values. A constructor value is
  always heap-boxed, so its box is an owned resource regardless of payload class.

Classification is structural and does not change the inferred type. It supplies
facts used by effect-summary construction, move analysis, and destruction.

Owned values are linear. Passing one by value **moves** it: the callee takes
ownership and destroys it at scope exit unless it is moved out again. A callee
that only needs to read (or write through) a value takes a **borrow** (`&x`),
which does not move; the caller keeps ownership. Matching an owned by-value
scrutinee with `case` consumes it — its payloads move into the arm bindings and
its box is freed by the match. Placing an owned variable in a constructor
payload likewise moves it: the new box owns the payload, and destroying the box
destroys it.

Destruction of an owned sum is lowered to a per-type recursive destroy function
that reads the tag, destroys owned payload fields, and frees the box; recursive
types are handled by runtime recursion.

## Function-Effect Summaries

`FunctionEffectSummaries` records call-boundary behavior after type inference
and ownership classification. Summary indices refer only to formal parameters;
the return is represented separately by an origin relation.

Each formal has one mode:

- `Copy`: passing the actual does not consume ownership.
- `Own`: passing an owning actual consumes it.
- `DependsOnInstantiation`: the formal's type is not yet known to own
  anything (a type variable, or a reference of unresolved mode); what the
  call does is decided per call site, as with a polymorphic pass-through
  function.

A formal whose inferred type classifies as `Own` is `Own` even when the type
still contains variables (an owning reference's payload is always `Copy`, so
the callee can free it).

Each summary also records, per formal, whether the body **passes it on**: on
every path the value (or a local holding it) is an argument of some call. A
generic formal that receives an owned value must dispose of it, by returning
it (`FromFormal`) or passing it on, because a body compiled once for every
instantiation cannot free a value of variable type. Passing on only counts if
the receiving formal disposes of the value in turn; drops propagate backwards
through generic callees to a fixed point.

The same walk records, in evaluation order, a formal that is **used again
after it may have been passed on** (including under `&`, and in a second
iteration of a loop). A generic formal so used carries a `Self` copy
requirement: at an owned instance the first pass consumed it. Assigning to the
formal starts a new value. Every requirement also records its reasons
(`RequirementReason`: moves out of a borrow, overwrites through a borrow,
lends to a move-out, used after passed on), which select the diagnostic when a
call violates it.

From the summaries, `FunctionEffectSummaries` derives one **call effect** per
call site: which actuals the call consumes. An actual is classified from its
solved type (a variable reference resolves to its declaration). For each
possible callee, a named function or the call graph's targets for a call
through a function value, formal `i` consumes an owning actual when its mode
is `Own`, or when it is `DependsOnInstantiation` and the formal is disposed
of; an owning actual bound to a generic formal that is dropped is rejected
(`owned value passed to generic formal ...: the callee neither returns it nor
passes it on, and cannot free a value whose type it does not know`). Targets that disagree are rejected. Move
analysis and the destruction pass both read this table, so they cannot
diverge on what a call consumes.

Each return has one origin:

- `PureCopy`: the result is a copy-class value.
- `FreshOwn`: the function creates new ownership, such as with `alloc`.
- `FromFormal(i)`: the result comes from formal `i`.
- `BorrowFromFormal(i)`: the result is derived from a borrow of formal `i`.
- `Unknown`: the analysis cannot establish one of the preceding origins.

When body provenance yields no origin and the return type classifies as
`Own`, the summary records `FreshOwn`. That fallback is sound only because the
position check (below) excludes every way an *alias* of a caller's value -- the
value behind a borrowed formal, or a payload bound by matching through one --
could reach a return, an assignment, a payload, or a call argument. With those
excluded, an owned result of unknown provenance can only be a fresh box or a
call result whose own summary is sound.

Each formal also carries a **copy requirement**: `Referent` (the value behind a
borrowed actual must be `Copy`), `Self` (the actual itself must be `Copy`), or
both, with the reasons it was recorded. A generic body is compiled once for
every instantiation, so every use of a value of variable type that is only
valid for `Copy` values becomes a requirement:

| Reason | Body | Requirement |
| --- | --- | --- |
| moves out of a borrow | `take(p) { return *p; }` | `Referent` (from `BorrowChecker::checkPositions`) |
| overwrites through a borrow | `set(p, v) { *p = v; }` | `Referent` (same) |
| lends to a move-out | `lend(x) { return take(&x); }` | `Self` (inherited) |
| used after passed on | `twice(f, x) { f(x); f(x); }` | `Self` (body walk) |
| not disposed | `sink(p) { return 0; }` | `Self` (body walk) |

Requirements are recorded only where ownership depends on the instance (a type
variable, or a reference whose mode is unresolved). A generic body is accepted
at its definition; the requirement is judged at each call site against the
actual's solved type. Where the actual's type is still generic, the requirement
moves to the enclosing function's formal (found through the body walk's
forwarding log, which follows local copies) and the loop runs to a fixed point.
A concrete violation is rejected at the call with the reason's message.

For example, the principal type of `identity` remains polymorphic. Its summary
states that the return comes from formal 0. At an `int` instantiation the call
copies; at an owning-reference instantiation it transfers ownership into the
call and back through the result. Summary extraction reads inferred types and
body provenance; it does not add equations to type inference.

Summary extraction tracks straightforward assignment provenance through
blocks, `if`/`else`, and `case` joins. Matching branch origins are preserved.
Conflicting origins become `Unknown` rather than being treated as copies.

## Move, Borrow, and Destruction Analyses

`MoveAnalysis` applies formal modes at calls and uses return origins to determine
whether a call result carries ownership. An owned formal starts the function
Owned (the caller moved it in), and an owned variable used as a constructor
payload is moved, so both are tracked exactly like owned locals. Expressions
are walked in code-generation order (an assignment's `*e` target, then its
right-hand side; a callee, then its actuals left to right), and a move takes
effect where it happens. It rejects uses after move (including writes through
a moved owning pointer), repeated moves, overwriting a live owner, moving an
owner that an actual of the same call borrows, a move in a `while` condition,
a loop body that changes which variables are Owned, and a join where a
variable is Owned on some paths but not all. The Own binders of a by-value
`case` are Owned for their arm and leave scope at its end; binders are scoped
by arm, not by name, because uses resolve by name and the symbol table holds
one declaration per name.

The same walk records a **destruction plan**: owners still Owned at each
function's exit, owned binders still Owned at the end of each arm, owning
references used without being bound (`*mk()`), consuming (by-value) matches,
and owned payloads such a match discards with `_`. Checking and destruction
therefore cannot disagree.

Position rules live in `BorrowChecker`, in two stages:

1. `BorrowChecker::check`, with the weeding passes, enforces that `&x` is an
   immediate call argument, naming the position where it can (arithmetic,
   `output`, `error`, `return`).
2. `BorrowChecker::checkPositions`, between building and resolving summaries,
   walks each function with the position of every expression:
   - an *alias* (an `Own`-typed dereference of a borrow, or an `Own`-typed arm
     binding of `case *e`) may appear only under `&`, under another `*`, as a
     `case *e` scrutinee, or as an assignment target whose slot is `Copy`; a
     dereference whose type is still a variable becomes a copy requirement;
   - a borrow-derived call result may appear only as a call argument or under
     `*`; in an assignment, a return, or a constructor payload it escapes.

`OwnershipTypeRules::checkBorrowComponents` then rejects a borrow mode anywhere
inside a sum-type payload, an `alloc` payload, or a function's return type.
This is a type-level rule: a borrow-typed *variable* stored into a box is
invisible to the position walk, but its inferred payload type is not. Formals
and locals are not checked; that is where a borrow legitimately lives.

A borrow-derived result may continue through an immediate chain of call
arguments. A function that receives a borrow may independently return
`FreshOwn`; the caller then owns that result.

`DestructionPass` inserts the destroys `MoveAnalysis` planned: before each
return for variables still Owned at exit, and at the end of an arm for binders
still Owned there (the arm body is wrapped in a block that ends with the
destroys, while code generation still has the binders in scope). Algebraic
values are destroyed structurally, including owned payloads and recursive
contents.

Code generation executes the rest of the plan and decides nothing about
ownership: it frees an unbound owning reference after its load or store, and in
a consuming match it frees the box after the arm, the boxes of nested
constructor patterns, and discarded owned payloads. It tests all of an arm's
patterns before binding or freeing anything, so an arm that fails partway falls
through to the next with the scrutinee intact; a borrowed match frees nothing. It also guards every integer division: a zero divisor, or `INT64_MIN`
divided by `-1`, calls `_top_division_error` instead of executing an
undefined `sdiv`. With `--san`, every generated function is marked
`sanitize_address` before the AddressSanitizer pass runs (with or without
`-do`), so reads and writes are instrumented, not only allocation calls.

## Analysis Views

The driver exposes the major compiler products independently:

| Product | Option |
| --- | --- |
| Normalized source | `--psource` |
| AST | `--past=ascii` |
| Symbols and scopes | `--psym` |
| Inferred types | `--ptype` |
| Call graph | `--pcallgraph=ascii` |
| Control-flow graphs | `--pcfg=ascii` |
| Ownership and destruction | `--pownership` |
| Borrow validity | `--pborrow` |

`--constraint` augments type, call-graph, ownership, and borrow views with the
facts or traces that produced the displayed result.

## Maintained Boundaries

The architecture intentionally keeps these concerns separate:

1. Type inference computes principal types before ownership effects are
   interpreted.
2. Ownership classes describe type shapes; move state describes a binding at a
   program point.
3. Function summaries describe call-boundary effects without changing function
   types.
4. Borrow provenance is checked after summaries exist because it can flow
   through calls.
5. Destruction is inserted only after move and borrow validity are established.

Validation is duplicated in a few phases, and authoritative
constructor-expression checks remain a known gap.
