# TOP Language Tutorial

## Purpose

This tutorial is a walkthrough of the TOP (tiny ownership programming) language as implemented by `topc`. It follows
the successful compilation path through syntax and names, inferred types,
ownership, borrow validity, moves, automatic destruction, and LLVM code
generation. It then shows how static errors stop that path and identifies the
current language boundaries.

TOP combines a compact imperative language with inferred types, parametric
polymorphic functions, algebraic data, first-class functions, owning heap
references, and checked borrows. The final sections state the language
boundaries explicitly so unsupported syntax is not mistaken for an
undocumented feature.

## 1. Your First TOP Program

A TOP file contains one or more top-level function or type declarations. Every
function returns a value, and its final statement must be `return`.

```top
double(n) {
  return n * 2;
}

main() {
  var answer;
  answer = double(21);
  output answer;
  return 0;
}
```

TOP has no source-level type annotations. The compiler infers from
multiplication that `n`, `2`, and the result of `double` have type `int`:

```text
double : (int) -> int
main : () -> int
```

After building the compiler, inspect those types with:

```sh
build/src/topc --ptype program.top
```

Useful inspection options include:

| Option | What it shows |
| --- | --- |
| `--psource` | Normalized source from the pretty-printer |
| `--past=ascii` | Abstract syntax tree |
| `--psym` | Functions, local declarations, and scopes |
| `--ptype` | Inferred global, function, and local types |
| `--pcallgraph=ascii` | Possible call relationships |
| `--pcfg=ascii` | Control-flow graphs |
| `--pownership` | Ownership and destruction analysis |
| `--pborrow` | Borrow validity |
| `--constraint` | Constraints or traces for compatible views |

These views expose compiler reasoning; they do not add syntax to TOP.

For example, `--psource` prints the program after parsing and normalization:

```sh
build/src/topc --psource program.top
```

The beginning of the result is:

```text
double(n)
{
  return (n * 2);
}
```

The added parentheses and consistent braces reflect the syntax understood by
the compiler. They do not change the program's behavior. This view is useful
when checking how `topc` parsed an expression before examining later analyses.

## 2. Values, Variables, and Control Flow

The only primitive data type is `int`. Integer literals, negative integer
literals, and `input` all have type `int`. Arithmetic operators are `+`, `-`,
`*`, and `/`. Comparisons return an integer used as false or true.

```top
main() {
  var n, total;
  n = input;
  total = 0;
  while (n > 0) {
    if (n != 2) {
      total = total + n;
    } else {
      output n;
    }
    n = n - 1;
  }
  return total;
}
```

Local variables are declared at the beginning of a function with `var`.
Parameters and locals acquire one consistent inferred type within that
function. TOP has no separate Boolean type: conditions are integers, and
comparison results have type `int`.

The comparison operators are `>`, `==`, and `!=`. Arithmetic and `>` require
integer operands. Equality and disequality require both operands to have the
same type.

Statements include assignment, blocks, `if`/`else`, `while`, `case`, `output`,
`error`, and `return`. `error e` reports an error value and terminates execution.

The control-flow graph makes the loop and branch structure explicit:

```sh
build/src/topc --pcfg=ascii program.top
```

An abridged result is:

```text
[cfg main]
  b1: while ((n>0)) {...}
    true -> b5
    false -> b0
  b5: if ((n!=2)) {...}
    true -> b3
    false -> b4
  b0: return total;
    return -> exit
```

A basic block is a sequence of operations with one entry and one exit. The
arrows show possible transfers of control: the `while` either enters the body
or reaches the return, and the `if` chooses one of its two arms.

## 3. Functions and Inferred Types

Function types record parameter types and a return type, for example
`(int, int) -> int`. TOP functions are first-class values: they can be passed,
returned, stored in variables, and called through expressions.

```top
inc(x) {
  return x + 1;
}

twice(f, x) {
  return f(f(x));
}

main() {
  return twice(inc, 20);
}
```

Inspect the inferred types with:

```sh
build/src/topc --ptype program.top
```

Ignoring the source-location details in `topc`'s type-variable names, the
result has this shape:

```text
inc   : (int) -> int
twice : ((α) -> α, α) -> α
main  : () -> int
```

`α` is a type variable: it stands for a type that inference has not fixed
to one concrete type in the definition of `twice`. Every occurrence of the
same type variable must denote the same type. Therefore:

1. `x` has type `α`.
2. `f` accepts an `α` and returns an `α`.
3. Both nested calls to `f` preserve that type.
4. `twice` returns an `α`.

The actual inspection output adds an origin annotation such as `@6:9` to `α`.
Those details distinguish compiler inference variables; repeated occurrences
of the same annotated name are the important part when reading the output.

For now, read the repeated `α` as a relationship among parts of one function
type. Section 4 explains when `topc` generalizes such a variable and how
separate call sites can choose different concrete types for it.

Calls are expressions, not standalone statements. If a call is needed only for
its effect, assign its result to a local variable.

The symbol view answers a different question from the type view: which names
are declared, and in which function scope?

```sh
build/src/topc --psym program.top
```

```text
Functions : {inc, main, twice}
Locals for function inc : {x}
Locals for function twice : {f, x}
Locals for function main : {}
```

Parameters appear with local declarations because both are names in a
function's local scope. Compare this output with `--ptype`: `--psym` shows where
`f` and `x` are declared, while `--ptype` shows the types inferred for them.

## 4. Parametric Polymorphic Functions

Type inference generalizes eligible top-level functions. A generalized type
variable lets one definition be instantiated at multiple types without generic
syntax.

```top
identity(x) {
  return x;
}

inc(x) {
  return x + 1;
}

main() {
  var n, f;
  n = identity(41);
  f = identity(inc);
  return f(n);
}
```

Conceptually, `identity` has the inferred type `(α) -> α`. The calls
instantiate `α` once as `int` and once as `(int) -> int`. This is implicit
parametric polymorphism. TOP does not declare type parameters in source, and
polymorphism does not let one local variable change type during its lifetime.

A higher-order function accepts or returns a function. A polymorphic function
can be instantiated at more than one type. A function may be either, both, or
neither.

Add `--constraint` to see where polymorphic functions are instantiated:

```sh
build/src/topc --ptype --constraint program.top
```

The relevant sections are:

```text
[type-schemes]
  [scheme] - identity : (α<x@1:9>) -> α<x@1:9>

[type-instantiations]
  [instantiation] 11:6 call identity(41) instantiates identity
  [instantiation] 12:6 call identity(inc) instantiates identity
```

A type scheme is the generalized type available for instantiation. Each call
receives a fresh instance of its `α`, allowing one call to use `int` and the
other to use the function type `(int) -> int`. Source locations in your output
will depend on where the example appears in your file.

## 5. Algebraic Data Types

A top-level `type` declaration defines a sum type. Each alternative is a
constructor with zero or more payloads.

```top
type Shape = Point | Circle(radius) | Rectangle(width, height);

areaHint(shape) {
  var result;
  case shape of {
    Point -> result = 0;
    Circle(r) -> result = r * r;
    Rectangle(w, h) -> result = w * h;
  }
  return result;
}

main() {
  return areaHint(Rectangle(6, 7));
}
```

Type and constructor names begin with an uppercase letter. Payload names do not
declare fields or introduce variables that a function can use. They are
placeholders that give each positional payload slot a stable identity during
type inference. Construction and pattern matching constrain the type of that
slot by position. Pattern validation checks constructor names and arities;
payload types are checked through inference.

A case pattern chooses its own binding names; they do not need to match the
payload placeholders in the declaration. Above, `radius`, `width`, and
`height` name declaration-side payload slots, while `r`, `w`, and `h` are
arm-local variables. The relationship is positional: `w` receives the first
`Rectangle` payload and `h` receives the second. A pattern may use `_` instead
when it does not need a payload value.

Inspecting the program shows the payload types inferred for the declaration:

```sh
build/src/topc --ptype program.top
```

```text
Types : {
  Shape : Point | Circle(int) | Rectangle(int, int)
}
```

The output retains declaration order. It reports that `Point` has no payload,
`Circle` has one `int` payload, and `Rectangle` has two `int` payloads. The
names `radius`, `width`, and `height` are not field names in this inferred
type; the payload positions and their types are what matter.

Each payload slot begins with an unknown type. The compiler infers its type from
the constraints imposed by its uses. For example, after
`type Option = None | Some(value);`, constructing `Some(41)` requires `value`
to be `int`. A pattern that performs integer arithmetic on the value would
impose the same requirement. Constructing `None` alone does not constrain the
payload slot.

Inference considers all uses together; runtime and source order do not matter.
There is one `value` slot shared by every use of that `Option` declaration, and
all constraints on it must agree. If one use requires `int`, another use such
as `Some(inc)`, where `inc` is a function, is a type error.

This differs from the parametric polymorphism of functions in Section 4.
`identity` can be generalized and instantiated with a fresh type at each call,
but a sum declaration is not generalized that way. TOP has no source-level
parameterized type such as `Option<T>`: the declaration above defines one
`Option` type with one inferred payload type for the program.

## 6. Pattern Matching

A `case` statement selects an arm by constructor. Patterns can bind payloads,
ignore them, or inspect nested constructors.

```top
type Inner = Lit(value) | Neg(value);
type Outer = Empty | Wrap(inner);

evaluate(outer) {
  var result;
  case outer of {
    Empty -> result = 0;
    Wrap(Lit(n)) -> result = n;
    Wrap(Neg(n)) -> result = 0 - n;
  }
  return result;
}

main() {
  return evaluate(Wrap(Neg(5)));
}
```

The pattern forms are variable binding, wildcard `_`, and nullary or payload
constructors. Patterns may be nested. Bindings in one arm must be unique,
constructor arity must match its declaration, and arms must cover all reachable
top-level constructor cases. Redundant arms are rejected.

Pattern bindings are local to their arm. A wildcard binds no name. When a
wildcard discards a payload classified as `Own`, the compiler arranges to
destroy it.

The ASCII abstract syntax tree shows the nesting that the parser assigned to
constructor patterns and expressions:

```sh
build/src/topc --past=ascii --output-dir analysis program.top
```

The command writes `analysis/program.top.ast.txt`. An abridged portion is:

```text
CaseStmt
├── case-expression: VariableExpr: outer
├── arm[0]: CaseArm
│   ├── pattern: ConstructorPattern: Empty
│   └── body: AssignStmt
│       ├── lhs: VariableExpr: result
│       └── rhs: IntegerLiteral: 0
├── arm[1]: CaseArm
│   ├── pattern: ConstructorPattern: Wrap
│   │   └── payload[0]: ConstructorPattern: Lit
│   │       └── payload[0]: BindingPattern: n
│   └── body: AssignStmt
│       ├── lhs: VariableExpr: result
│       └── rhs: VariableExpr: n
└── arm[2]: CaseArm
    ├── pattern: ConstructorPattern: Wrap
    │   └── payload[0]: ConstructorPattern: Neg
    │       └── payload[0]: BindingPattern: n
    └── body: AssignStmt
        ├── lhs: VariableExpr: result
        └── rhs: BinaryExpr: -
            ├── lhs: IntegerLiteral: 0
            └── rhs: VariableExpr: n
```

The labeled edges distinguish each relationship. The `case-expression` is the
expression being matched, and each `arm` has a `pattern` and a `body`. In the
second arm, the pattern descends from `Wrap` through its first payload to `Lit`,
then through that constructor's first payload to the binding pattern `n`.

The body of that arm is an assignment whose `lhs` is `result` and whose `rhs`
reads the `n` bound by the pattern. The final arm has the same shape until its
assignment's `rhs`, which is a binary subtraction expression with its own
left-hand and right-hand operands. The tree therefore separates the `->`
relationship between pattern and body from the `=` relationship between an
assignment's two expressions.

## 7. Recursive Algebraic Data

Algebraic types may be recursive. Recursion is inferred from payloads and use
sites; there is no explicit recursive-type syntax.

```top
type List = Nil | Cons(head, tail);

length(list) {
  var result;
  case list of {
    Nil -> result = 0;
    Cons(_, tail) -> result = 1 + length(tail);
  }
  return result;
}

sum(list) {
  var result;
  case list of {
    Nil -> result = 0;
    Cons(head, tail) -> result = head + sum(tail);
  }
  return result;
}

main() {
  var list;
  list = Cons(1, Cons(2, Cons(3, Nil)));
  return sum(list) - length(list);
}
```

Running `build/src/topc --ptype program.top` prints the inferred global type as:

```text
Types : {
  List : Nil | Cons(int, List)
}
```

This says that `Nil` has no payload and `Cons` has two payloads: an `int` and
another `List`. The first payload is inferred as `int` from construction and
from the addition in `sum`; the second is recursive because `length` and `sum`
process the tail as another list. TOP source does not write these inferred
payload types explicitly.

Recursive lists, trees, and functions over them are supported. A recursive
function such as `length` handles a base constructor such as `Nil`, then makes
progress by calling itself with the smaller value stored in `Cons`. This pattern
is the usual way to process recursive algebraic data in TOP.

## 8. References and Reference Modes

TOP references have two concrete modes: an owning reference owns a heap
allocation, while a borrow reference aliases storage owned elsewhere. The
compiler models both with the conceptual type constructor `Ref(mode, pointee)`.

| Inferred type | Meaning |
| --- | --- |
| `own&T` | Owning reference to `T` |
| `borrow&T` | Non-owning reference to `T` |
| `ref&T` | Reference to `T` whose mode is polymorphic |

These spellings appear in compiler output; they are not source annotations.

The two reference modes use the same dereference syntax. `alloc` creates an
owning reference, while `&` creates a borrowing reference. The expression
`*pointer` reads through either mode, and an assignment to `*pointer` writes
through either mode.

```top
read(pointer) {
  return *pointer;
}

increment(pointer) {
  *pointer = *pointer + 1;
  return 0;
}

main() {
  var value, owner, ignored;
  value = 10;
  owner = alloc 41;
  ignored = increment(&value);
  return read(owner) + value;
}
```

The assignment `owner = alloc 41` creates an owning reference. The expression
`&value` creates a borrow of `value` and passes it to `increment`. Inside
`increment`, the right-hand `*pointer` reads the current value through that
borrow, while the left-hand `*pointer` writes the incremented value back to the
same storage. Finally, `read(owner)` reads through the owning reference. The
program therefore demonstrates both creation forms, a dereference read through
each mode, and a dereference write through a borrow.

Inspect the inferred reference modes and borrow expression together:

```sh
build/src/topc --ptype --pborrow program.top
```

Ignoring source-origin details on `α`, the salient output is:

```text
Functions : {
  increment : (borrow&int) -> int,
  read : (ref&α) -> α
}

Locals for function main : {
  owner : own&int
}

[borrow-result]
  14:22 &value -> approved
```

The call with `&value` fixes `increment`'s parameter mode to `Borrow`, while
`alloc` gives the reference bound to `owner` mode `Own`. The body of `read`
only dereferences its parameter and does not require either concrete mode, so
its principal type remains mode-polymorphic as `ref&α`. The approved borrow
record points back to the `&value` expression in the example. For now, it is
enough to recognize these labels in the output: Section 9 explains why a
reference with mode `Own` is classified as `Own`, and Section 11 explains why
this borrow is approved and what restrictions apply to borrowed references.

## 9. Ownership Classes

A binding is a name, such as a local variable or function parameter, associated
with a value. Some values also represent resources that the compiler must
eventually destroy, such as heap allocations created by `alloc`.

After type inference, `topc` classifies each value as `Copy` or `Own`. A value
classified as `Copy` can be duplicated: assigning it to another binding does
not change the source binding. A value classified as `Own` carries
responsibility for destroying a resource and has exactly one owner at a time.
Assigning it transfers ownership to the destination binding; this is called a
*move*. The move invalidates the source binding, meaning later expressions
cannot read, dereference, or move its old value. These are ownership
classifications, not a second source-level type system.

| Inferred type shape | Ownership class |
| --- | --- |
| `int` | `Copy` |
| Function type | `Copy` |
| `borrow&T` | `Copy` |
| `own&T` | `Own` |
| Algebraic (sum) data | `Own` — a constructor value is heap-boxed, so the box is an owned resource |

Because owned values are linear, passing one to a function **by value moves it**:
the callee takes ownership and destroys it when it is done, unless it moves the
value out again (returns it or passes it on). To let a function read (or modify
in place) a value without taking ownership, pass a **borrow** (`&x`); the caller
keeps ownership and may lend the value repeatedly. Matching an owned value with
`case` consumes it, moving its payloads into the arm bindings; traverse a value
you want to keep by borrowing it (`case *p`).

The following example contrasts copying a value classified as `Copy` with
moving one classified as `Own`:

```top
main() {
  var number, copied, first, second;
  number = 10;
  copied = number;
  first = alloc 10;
  second = first;
  return number + copied + *second;
}
```

The assignment `copied = number` copies the value because `number` is an
`int`, which the table classifies as `Copy`. Both `number` and `copied` may be
read afterward. The assignment `first = alloc 10` binds a new value of type
`own&int`, classified as `Own`, to `first`. The assignment `second = first`
then moves that value, transferring ownership from `first` to `second` and
invalidating `first`.

Classification is structural. If a constructor carries a value classified as
`Own`, the enclosing algebraic value is also classified as `Own`:

```top
type Box = Empty | Full(value);

main() {
  var box;
  box = Full(alloc 10);
  return 0;
}
```

Inference determines that `Full` carries an owning reference, so the value
bound to `box` is classified as `Own` and must eventually be destroyed.

`--pownership` displays ownership classes and a destruction summary. For the
move example, it reports:

```sh
build/src/topc --pownership program.top
```

```text
[ownership-result]
  local main.copied : Copy
  local main.first : Own
  local main.number : Copy
  local main.second : Own
[destruction-summary]
  main : 1 destroy
```

The classification records the kind of value each binding can hold, so both
`first` and `second` are reported as `Own` even though `first` has moved. Move
analysis separately records `first` as invalid and `second` as the current
owner. Only the value held by `second` is destroyed at function exit. For now,
read `main : 1 destroy` as confirmation that the compiler will clean up one
live resource; Section 12 explains where automatic destruction is inserted and
how it follows structured values. Running the same command on the `Box`
example reports `local main.box : Own`, showing that ownership classification
follows payloads through algebraic data.

## 10. Moves and Function Boundaries

A value classified as `Own` has exactly one owner at a time. Assignment, or
passing the value to a call that consumes it, transfers ownership to a new
binding. This move invalidates the previous binding; reading, moving, or
dereferencing that binding afterward is a compile-time error.

```top
identity(value) {
  return value;
}

main() {
  var first, second;
  first = alloc 42;
  second = identity(first);
  // return *first;  // rejected: first was consumed by the call
  return *second;
}
```

After type inference, the compiler derives a function-effect summary. It
records whether each formal is classified as `Copy`, classified as `Own`, or
dependent on polymorphic instantiation, and whether the return is fresh
ownership, a value from a formal, a borrow from a formal, or a pure copy.

This explains observable polymorphic behavior: `identity(42)` copies an
integer, while `identity(pointer)` moves a value classified as `Own` into the
call and returns ownership with the result. Effect analysis does not change the
function's principal inferred type.

Inspect the inferred types, ownership classes, move trace, and destruction
summary together:

```sh
build/src/topc --ptype --pownership --constraint program.top
```

The salient records are:

```text
Functions : {
  identity : (α<value@1:9>) -> α<value@1:9>,
  main : () -> int
}

Locals for function main : {
  first : own&int,
  second : own&int
}

[ownership-result]
  local main.first : Own
  local main.second : Own
[destruction-summary]
  identity : 0 destroys
  main : 1 destroy

[ownership-constraints]
  [move] 7:1 own first (ownership established by assignment)
  [move] 8:1 move first (ownership moved via function argument)
  [move] 8:1 own second (ownership established by assignment)
```

Read these records in source order. `identity` retains the polymorphic
principal type `(α) -> α`. The allocation binds a new value classified as
`Own` to `first`, making `first` its owner. At this call site,
`identity(first)` instantiates `α` with `own&int`, so the call moves the value
out of `first`. Because `identity` returns its formal unchanged, the returned
value is bound to `second`, making `second` its owner. Both bindings can hold
values of owning reference type and are therefore reported as `Own`, but
`first` is invalid after the move. Only `second` remains an owner at function
exit, so the compiler inserts one automatic destruction in `main`. Section 12
shows where that destruction is inserted.

Control-flow joins must agree about ownership state. If one branch moves a
value out of a binding and another leaves that binding as its owner, later use
is rejected. Overwriting a binding that still owns a live value is also
rejected because it would abandon a resource.

Uncommenting `return *first` in the example asks the compiler to dereference a
binding after the value it owned moved to `second`. Full compilation reports:

```text
topc: variable 'first' used after move on line 9
topc: semantic error
```

The important phrase is `used after move`. Start from the reported use and
look backward for the assignment or call that transferred ownership. Here that
event is the call `identity(first)`, whose result is assigned to `second`.

## 11. Borrows and Lifetimes

A borrow is a temporary non-owning alias. TOP has no lifetime annotations or
general region inference. Instead, it enforces a narrow rule: a borrow
expression must be an immediate function argument.

```top
increment(pointer) {
  *pointer = *pointer + 1;
  return 0;
}

main() {
  var value, ignored;
  value = 10;
  ignored = increment(&value);
  return value;
}
```

The borrow does not move `value`. Its usable lifetime is confined to the call
chain, and the original storage remains responsible for its lifetime. Storing
`&value` in a variable or returning it is rejected.

Borrow analysis can show the accepted borrow site:

```sh
build/src/topc --pborrow --constraint program.top
```

```text
[borrow-result]
  9:22 &value -> approved

[borrow-constraints]
  [borrow] 9:22 &value -> approved; direct argument 0 of increment
```

Read each record from left to right. `9:22` is the source position where the
borrow expression begins: line 9 and zero-based column 22. `&value` is the
borrow expression at that position. `approved` means the expression is an
immediate call argument and the completed analysis found no forbidden escape.
`direct argument 0 of increment` identifies the zero-based argument position
and the first function receiving the alias.

`[borrow-result]` is the concise result view. With `--constraint`, the
`[borrow-constraints]` section provides the call-chain evidence behind that
result. A compilation that rejects a borrow exits with a diagnostic instead of
printing a stable rejected-result section.

Borrow provenance is tracked across calls. A borrow-derived result may continue
through nested immediate call arguments:

```top
identity(value) {
  return value;
}

read(pointer) {
  return *pointer;
}

main() {
  var value, result;
  value = 17;
  result = read(identity(&value));
  return result;
}
```

For this program, `--pborrow --constraint` reports:

```text
[borrow-result]
  12:25 &value -> approved

[borrow-constraints]
  [borrow] 12:25 &value -> approved; direct argument 0 of identity
  [borrow-flow] 12:25 hop 1 identity(&value) -> argument 0 of read at 12:11
```

The `[borrow]` record establishes the origin: `identity` receives `&value` as
argument 0. The `[borrow-flow]` record then shows that the result of `identity`
retains that provenance and becomes argument 0 of `read`. `hop 1` orders this
step after the direct borrow even though the outer call begins earlier on the
source line. Longer immediate call chains produce `hop 2`, `hop 3`, and so on.
Every record in one chain repeats the originating borrow position, while the
final `at 12:11` identifies the receiving outer call.

But `identity(&value)` may not be stored, returned, used in arithmetic, emitted
with `output` or `error`, or used as a condition. In compiler terms, a
borrow-derived value may flow through immediate call arguments but cannot
escape into another sink. This evidence traces immediate call chains; it is not
a general lifetime or region proof.

A function receiving a borrow may return newly allocated ownership:

```top
makeBox(seed) {
  return alloc *seed;
}

main() {
  var value, pointer;
  value = 23;
  pointer = makeBox(&value);
  return *pointer;
}
```

Inspect the inferred types, ownership class, destruction, and borrow result:

```sh
build/src/topc --ptype --pownership --pborrow program.top
```

Ignoring source-origin annotations and the borrow expression's source
position, the salient records are:

```text
Functions : {
  main : () -> int,
  makeBox : (borrow&α) -> own&α
}

Locals for function main : {
  pointer : own&int,
  value : int
}

[ownership-result]
  local main.pointer : Own
  local main.value : Copy
[destruction-summary]
  makeBox : 0 destroys
  main : 1 destroy
[borrow-result]
  &value -> approved
```

The return type `own&α` shows that `makeBox` produces a new owning reference,
not a borrow-derived result. At this call site, `α` is `int`, so `pointer` has
type `own&int` and holds a value classified as `Own`. The approved borrow is
confined to the call, while `main : 1 destroy` confirms that the returned
allocation remains live in `pointer` and is destroyed when `main` exits.

## 12. Automatic Destruction

TOP source has no manual `free`. After move analysis determines which bindings
remain owners of values classified as `Own`, the destruction pass inserts
destruction at exits and other ownership-sensitive points.

```top
main() {
  var pointer;
  pointer = alloc 5;
  return 0;
}
```

The allocation is freed automatically. A moved-from binding is not destroyed;
the value in the destination binding is destroyed instead.

Destruction follows structure recursively:

1. An owning reference releases its heap allocation.
2. Algebraic data destroys constructor payloads classified as `Own`.
3. Recursive algebraic data recursively destroys contents classified as `Own`.
4. A wildcard destroys a discarded payload classified as `Own`.
5. Values classified as `Copy`, including borrows, never release another
  value's storage.

`--pownership` exposes ownership and destruction results. `--san` instruments
generated code with Address/LeakSanitizer, which helps reveal leaks, double frees,
and invalid accesses while testing compiler behavior.

For this example, `--pownership` makes the inserted cleanup visible:

```text
[ownership-result]
  local main.pointer : Own
[destruction-summary]
  main : 1 destroy
```

The source contains no destruction statement, but the summary reports one
destroy inserted for the live value classified as `Own` and bound to `pointer`.
Compile with `--san` to have Address/LeakSanitizer check at runtime that the
generated cleanup does not leak, free an allocation twice, or access it after
it has been freed.

## 13. LLVM Code Generation

After semantic analysis validates the program and the destruction pass inserts
required cleanup, `topc` generates an LLVM module. By default, the compiler
optimizes that module and serializes it as LLVM bitcode in a `.bc` file. LLVM's
textual `.ll` form makes the generated instructions readable.

Consider the `double` function from Section 1 in its complete program:

```top
double(n) {
  return n * 2;
}

main() {
  var answer;
  answer = double(21);
  output answer;
  return 0;
}
```

Compile it to bitcode, then use LLVM's disassembler to produce the textual
form. The following commands use the Homebrew LLVM installation path on macOS:

```sh
build/src/topc -o program.bc program.top
/opt/homebrew/opt/llvm/bin/llvm-dis program.bc -o program.ll
```

The selected function in `program.ll` is:

```llvm
define internal i64 @double(i64 %n) {
entry:
  %multiply = shl i64 %n, 1
  ret i64 %multiply
}
```

The source parameter `n` becomes the LLVM parameter `%n`, and TOP's `int`
becomes LLVM's 64-bit integer type `i64`. The optimizer replaces `n * 2` with
the equivalent left shift `shl i64 %n, 1`. The final `ret` implements the TOP
return statement.

The generated entry point links this function back to the rest of the source:

```llvm
define i64 @_top_main() {
entry:
  %call.result = call i64 @double(i64 21)
  %0 = call i64 @_top_output(i64 %call.result)
  ret i64 0
}
```

The argument `21` comes from `double(21)`, and the call to `_top_output`
implements `output answer`. The `_top_main` name distinguishes TOP's entry
function from the native entry point supplied when the bitcode is linked with
the TOP runtime library.

For inspection, `topc` can write the same generated LLVM module directly in
textual form without a separate `llvm-dis` step:

```sh
build/src/topc --asm -o program.ll program.top
```

Use `--do` with `--asm` to disable optimization when investigating the more
literal allocation, load, and store instructions emitted before LLVM
optimization. The optimized output is usually the clearer view for connecting
a TOP expression to the instructions that will execute.

## 14. Static Errors as Part of the Model

TOP rejects programs before code generation when it cannot prove a required
type, ownership, or borrow property.

For example, `inc` requires an integer argument, so passing the function itself
is a type error:

```text
inc(value) {
  return value + 1;
}

main() {
  return inc(inc);
}
```

Compilation reports:

```text
topc: Cannot unify int with (⟦inc@1:0⟧) -> ⟦inc(inc)@6:9⟧: different structure
topc: semantic error
```

To unify two types is to make them equal while solving inference constraints.
This diagnostic says that one use requires `int`, while the supplied value has
a function type. The source locations identify the declarations and
expressions that contributed those incompatible requirements.

| Diagnostic idea | Meaning |
| --- | --- |
| Type mismatch | One inference variable was forced to incompatible types |
| Unknown constructor pattern | No declared sum alternative has that pattern name |
| Pattern arity mismatch | A constructor pattern has the wrong payload count |
| Non-exhaustive case | Some top-level constructor is not covered |
| Redundant case arm | An earlier pattern already covers the arm |
| Used after move | The value was moved out of this binding earlier |
| Moved more than once | A move was attempted from an invalidated binding |
| Assigned while still owned | Assignment would overwrite a binding that still owns a live value |
| Ownership state disagreement | Control-flow paths reach a join where a binding has incompatible ownership states |
| Borrow must be an immediate argument | A direct borrow appears outside a call argument |
| Borrow-derived value escapes | Borrow provenance reaches a forbidden sink |

Debug these errors from the first relevant event:

1. Inspect inferred types with `--ptype`.
2. Find the assignment or call that first moved the value.
3. Distinguish direct syntax such as `&value` from a call result carrying borrow
   provenance.
4. Check every branch and loop path reaching the reported use.
5. Use `--pownership` for classes and destruction, or `--pborrow --constraint`
  for the evidence behind borrow validity.

## 15. Current Language Boundaries

TOP is an intentionally small language that incorporates a number of features of modern languages, like Rust.
The following table is a quick
reference for constructs that may be familiar from other languages but are not
part of TOP. The examples after the table explain two less obvious boundaries
in the current compiler implementation.

### 15.1 Surface and Data-Model Boundaries

| Area | Current behavior |
| --- | --- |
| Primitive types | `int` only; no separate Boolean, character, or string type |
| Comparisons | `>`, `==`, and `!=` only |
| Local declarations | `var` declarations precede statements in a function |
| Return | Every function has a final return; no early return form |
| Calls | Calls are expressions; no void call statement |
| Type annotations | Types and generic parameters are inferred, not written |
| Algebraic data | Concrete sum declarations; no source-level parameterized ADTs |
| Constructor expressions | Unknown names and extra payloads are not yet rejected consistently |
| Structured data | Constructor payloads; no tuples, anonymous records, or field projection |
| References | Created with `alloc` or `&`; no source reference type annotations |
| Reference polymorphism | One helper cannot currently be instantiated at both Own and Borrow within one program |
| Borrows | Immediate call arguments only; not storable or returnable |
| Lifetimes | No lifetime syntax or general long-lived borrow inference |
| Deallocation | Automatic destruction; no source-level `free` |
| Null | No null-reference expression |
| Recursive types | Recursive algebraic data works; recursively self-referential function types do not |
| Patterns | Constructor, variable, wildcard, and nested patterns |

Use constructor patterns to access structured values. Represent absence with a
nullary constructor such as `None` or `Empty`.

### 15.2 Recursive Function Types and Ownership Analysis

Section 7 used two supported forms of recursion:

1. `List` is recursive data because `Cons` contains another `List`.
2. `length` is a recursive function because it calls itself on the tail.

Passing a function to itself as an argument can make its inferred function type
contain itself. For example, this program is rejected during full compilation:

```text
foo(n, function) {
  var result;
  if (n == 0) {
    result = 1;
  } else {
    result = n * function(n - 1, function);
  }
  return result;
}

main() {
  return foo(3, foo);
}
```

The recursive call is not the problem by itself. The call `foo(3, foo)` also
uses `foo` as the value of its own `function` parameter, which requires a
recursive function type. Type inference can describe that type, and `--ptype`
displays it using `μ` notation. The later ownership analysis cannot yet
classify recursive function types, so full compilation rejects the program.
Ordinary recursive functions such as `length` do not create this kind of type
and are supported.

The two commands expose the compiler-phase boundary. First, type inspection
succeeds:

```sh
build/src/topc --ptype program.top
```

```text
foo : μα<foo@3:0>.(int, α<foo@3:0>) -> int
```

Here `μ` binds the recursively occurring type variable. Full compilation then
continues to ownership analysis and reports:

```text
topc: recursive types are not yet supported in ownership analysis: function foo
topc: semantic error
```

### 15.3 Higher-Order Inference with Recursive Data

A higher-order helper can be used with recursive data. Section 16, for example,
uses `apply(sum, list)`. A current inference limitation appears when the same
helper is also used at an unrelated type in that program:

```text
total = apply(sum, list);
next = apply(inc, 1);
```

The first call requires `apply` to accept a function over `List`; the second
requires it to accept a function over `int`. `topc` currently reports a type
unification error instead of generalizing `apply` for both uses.

```text
topc: Cannot unify List{...} with int: different structure
topc: semantic error
```

When this occurs, give the unrelated uses separate helper definitions:

```text
applyToList(function, value) {
  return function(value);
}

applyToInt(function, value) {
  return function(value);
}

total = applyToList(sum, list);
next = applyToInt(inc, 1);
```

This is an implementation boundary, not a claim that lists and integers have
the same type. Direct polymorphic functions such as `identity` can still be
used once with a recursive value and once with an integer.

## 16. End-to-End Example

This example combines recursive algebraic data, a polymorphic higher-order
function, pattern matching, allocation, borrowing, moves, and destruction.

```top
type List = Nil | Cons(head, tail);

apply(function, value) {
  return function(value);
}

sum(list) {
  var result;
  case list of {
    Nil -> result = 0;
    Cons(head, tail) -> result = head + sum(tail);
  }
  return result;
}

read(pointer) {
  return *pointer;
}

readBorrow(pointer) {
  return *pointer;
}

main() {
  var list, total, owner;
  list = Cons(10, Cons(20, Cons(12, Nil)));
  total = apply(sum, list);
  owner = alloc total;
  if (readBorrow(&total) != 42) error total;
  if (read(owner) != 42) error *owner;
  return 0;
}
```

What `topc` establishes:

1. `apply` is a higher-order helper with a parametric type shape; this program
  instantiates it for the recursive `List` type.
2. `List` is recursive, with integer heads and list tails.
3. Values of this `List` type are classified as `Copy` because none of its
  payloads is classified as `Own`.
4. `owner` has inferred type `own&int` and holds a value classified as `Own`.
5. This program constrains `read` to `(own&int) -> int` and `readBorrow` to
  `(borrow&int) -> int`. An otherwise unconstrained dereference-only helper can
  display a principal type such as `(ref&int) -> int`.
6. `&total` is valid because it is an immediate argument to `readBorrow`.
7. The value bound to `owner` is not consumed by `read`; dereference does not
  transfer ownership.
8. Before `main` exits, the destruction pass destroys the value bound to
  `owner`, releasing its allocation.

Inspect the program with:

```sh
build/src/topc --ptype --pownership --pborrow program.top
```

Across those views, the most relevant lines are:

```text
List : Nil | Cons(int, List)
read : (own&int) -> int
readBorrow : (borrow&int) -> int
local main.owner : Own
29:17 &total -> approved
main : 1 destroy
```

These lines follow the compiler's reasoning through several phases. Type
inference determines the recursive `List` shape and the two reference modes.
Ownership analysis classifies the value bound to `owner` as `Own` and inserts
its destruction. Borrow analysis approves the temporary alias passed to
`readBorrow`. Line and column numbers depend on the source file.

The call graph shows which functions may call which other functions:

```sh
build/src/topc --pcallgraph=ascii --output-dir analysis program.top
```

The generated `analysis/program.top.callgraph.txt` contains:

```text
[callgraph]
  apply -> {sum}
  sum -> {sum}
  read -> {}
  readBorrow -> {}
  main -> {apply, read, readBorrow}
```

The self-edge `sum -> {sum}` records recursion. Although `apply` receives a
function as a parameter, call-graph analysis determines that this program can
invoke `sum` at that call site.

Build and run it with:

```sh
bin/build.sh program.top
./program
```

## 17. Mental Model

When reading a TOP program, ask:

1. What types do expressions, variables, and functions infer?
2. Where are polymorphic functions instantiated, and at which types?
3. Which constructor and pattern payload types must agree?
4. Which values classify as Copy, and which as Own?
5. Where does ownership move through assignments and calls?
6. Does every borrow remain inside an immediate call chain?
7. Which binding owns each allocation at every control-flow point?
8. What will the destruction pass release when each path exits?
9. How does the generated LLVM IR implement the source operations?

Choose an inspection view based on the question you are asking:

| Question | Compiler option |
| --- | --- |
| How did the parser normalize the source? | `--psource` |
| What tree structure did the parser build? | `--past=ascii` |
| Where is each name declared? | `--psym` |
| What types were inferred? | `--ptype` |
| Which functions may call one another? | `--pcallgraph=ascii` |
| How can control move through a function? | `--pcfg=ascii` |
| Which values are Copy or Own, and where is destruction inserted? | `--pownership` |
| Which borrow expressions were accepted? | `--pborrow` |
| Which constraints or trace records produced a result? | Add `--constraint` to a compatible view |
| What LLVM IR will execute after optimization? | `--asm -o program.ll` |

ASCII tree and graph views write files when used with `--output-dir`. The
ownership, borrow, and type views print to standard output. Start with the view
closest to the concept you are investigating, then add `--constraint` when you
need the evidence behind a type, call-graph, ownership, or borrow result.

That sequence mirrors the observable responsibilities of `topc`: infer types,
derive effects, validate ownership and borrow behavior, and insert destruction
before generating executable code.