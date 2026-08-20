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
8. Classify inferred types for ownership.
9. Derive function-effect summaries.
10. Validate interprocedural borrow provenance.
11. Analyze ownership moves.
12. Insert automatic destruction.
13. Generate and optionally optimize LLVM bitcode.

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
dereference uses `Ref(m, T)` when either mode is acceptable. Mode variables let
a dereference-only function express that it does not require ownership.

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
its box is freed by the match.

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
- `DependsOnInstantiation`: behavior is determined from the call-site
  instantiation, as with a polymorphic pass-through function.

Each return has one origin:

- `PureCopy`: the result is a copy-class value.
- `FreshOwn`: the function creates new ownership, such as with `alloc`.
- `FromFormal(i)`: the result comes from formal `i`.
- `BorrowFromFormal(i)`: the result is derived from a borrow of formal `i`.
- `Unknown`: the analysis cannot establish one of the preceding origins.

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
whether a call result carries ownership. It rejects uses after move, repeated
moves, overwriting a live owner, and incompatible ownership state at
control-flow joins.

Borrow validation has two responsibilities at different stages:

1. The early check enforces the source rule that a direct borrow expression must
   be an immediate call argument.
2. The summary-aware check follows borrow-derived call results and rejects
   escape into storage, return, arithmetic, conditions, `output`, or `error`.

A borrow-derived result may continue through an immediate chain of call
arguments. A function that receives a borrow may independently return
`FreshOwn`; the caller then owns that result.

`DestructionPass` runs after move analysis. It uses ownership classification and
function summaries to destroy only values that remain active owners. It does not
destroy moved-from bindings, Copy values, or borrow-derived aliases. Algebraic
values are destroyed structurally, including owned payloads and recursive
contents.

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
