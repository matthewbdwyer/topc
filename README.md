topc
=========
A compiler from TOP to LLVM bitcode


## Heritage

TOP extends [TIP](https://cs.au.dk/~amoeller/spa/ "Static Program Analysis") (Tiny Imperative Programming), the pedagogical language designed by Anders M&#248;ller and Michael I. Schwartzbach for their _Static Program Analysis_ lecture notes.  The original TIP compiler, [`tipc`](https://github.com/matthewbdwyer/tipc), compiles TIP programs to LLVM bitcode; `topc` builds on that foundation, adding sum types, a static ownership type system, and a borrow mechanism for controlled aliasing.


## TOP Language Quick Reference

### TIP: The Core

TIP programs consist of functions.  Each function declares its locals in a single `var` statement, then performs assignments, `while` loops, `if`/`else`, `output` (print to stdout), `error` (runtime abort), and ends with a `return`.  The heap allocation expression `alloc expr` allocates a cell holding the initial value.  Function values are first-class: a function name evaluates to a pointer that can be called through a variable.  Comparison operators are `>`, `==`, and `!=`.

```
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

### Sum Types

TOP adds algebraic sum types with a file-level `type` declaration and pattern-matching `case` statements.  Constructors may be nullary (`None`) or carry a single payload (`Some(x)`).

```
type Color = Red | Green | Blue;

luminance(c) {
  var v;
  case c of {
    Red   -> v = 299;
    Green -> v = 587;
    Blue  -> v = 114;
  }
  return v;
}

main() {
  if (luminance(Green) != 587) error 1;
  return 0;
}
```

### Ownership

`alloc expr` produces a value of the owned pointer type `own&T`.  The compiler tracks ownership statically: when an owned value goes out of scope at a function return, a _destruction pass_ automatically inserts the corresponding `free`.

```
main() {
  var p;
  p = alloc 42;
  // destruction pass inserts free(p) before return — no explicit deallocation needed
  return *p;
}
```

Ownership is linear: assigning an owned pointer _moves_ it; the original binding becomes inaccessible.  The `--asan` flag instruments the generated IR with AddressSanitizer so that correct automatic deallocation can be verified at runtime.

### Borrows

A borrow `&expr` creates a non-owning reference to a heap cell.  Borrows may **only** appear as immediate function call arguments — they cannot be stored in variables or returned.  Inside the callee, `*p` reads through the reference and `*p = v` writes through it without transferring ownership.

```
increment(p) {
  *p = *p + 1;
  return 0;
}

main() {
  var x, dummy;
  x = 10;
  dummy = increment(&x);
  if (x != 11) error x;
  return 0;
}
```

Attempting to store a borrow in a variable is rejected at compile time:

```
main() {
  var x, p;
  x = 5;
  p = &x;   // error: borrow expression must be an immediate function argument
  return 0;
}
```

### Notable Constraints

| Feature | Constraint |
|---------|------------|
| Comparisons | Only `>`, `==`, `!=` — no `<`, `<=`, `>=` |
| `return` | Must be the final statement in a function body — no early return |
| Borrows | `&expr` is only valid as a direct function call argument |
| Call statements | Every call result must be assigned — there are no void call statements |


## Teaching Goals

`topc` is a teaching and research compiler designed to make the construction of a typed, memory-safe language concrete and approachable.  The implementation layers a sequence of static analyses on top of a standard compiler pipeline — parser → AST → type inference → semantic checks → code generation — with each phase building on the results of the previous:

1. **Type inference** resolves the types of all expressions and functions using Hindley-Milner-style unification, extended to handle sum types, owned pointers, and borrow references.
2. **Ownership classification** labels every pointer-typed value as `Own` or `Copy` based on its inferred type, providing the foundation for safe allocation tracking.
3. **Borrow checking** enforces that borrow expressions are used only as immediate call arguments, preventing dangling-reference patterns without a full lifetime system.
4. **Destruction pass** uses the ownership information to insert `destroy` calls before each function return, automatically freeing any owned values still live at that point.

Together these analyses realize TOP's memory-safety guarantees within a compiler small enough to read in full.  The test suite (435 unit tests, 156 system tests) is structured to be readable alongside the corresponding analysis code.


## Dependencies

`topc` is implemented in C++17 and depends on: [ANTLR4](https://www.antlr.org), [Catch2](https://github.com/catchorg/Catch2), [CMake](https://cmake.org/), [Doxygen](https://www.doxygen.nl/), [loguru](https://github.com/emilk/loguru), [Java](https://www.java.com), and [LLVM](https://www.llvm.org) (LLVM 22+ required).

To simplify dependency management the project provides a [bootstrap](bin/bootstrap.sh) script that installs all required dependencies on Ubuntu and macOS and configures the shell variables (`LLVM_DIR`, `TOPCLANG`) used by the build and test scripts:

```
./bin/bootstrap.sh
. ~/.zshrc      # or . ~/.bashrc on Linux
```

`TOPCLANG` must be the `clang` binary from the same LLVM installation used by CMake — for example, on macOS with Homebrew: `/opt/homebrew/opt/llvm/bin/clang`.


## Building topc

After cloning the repository and running bootstrap:

```
mkdir build
cd build
cmake ..
cmake --build . -j$(nproc)
```

The build downloads and compiles ANTLR4's C++ runtime if needed, builds the runtime library (`build/rtlib/top_rtlib.bc`), and compiles `topc` together with its unit tests.  The `topc` executable lands in `build/src/`.

You may see CMake policy warnings from third-party packages; these are harmless and will disappear as those packages are updated.


## Using topc

```
OVERVIEW: topc - a TOP to llvm compiler

USAGE: topc [options] <top source file>

OPTIONS:

  --asan                         - instrument generated IR with AddressSanitizer
  --asm                          - emit human-readable LLVM assembly language
  --do                           - disable bitcode optimization
  --log=<logfile>                - log all messages to logfile (enables --verbose 3)
  -o <outputfile>                - write output to <outputfile>
  --pa=<AST output file>         - print AST to a file in dot syntax
  --pcg=<call graph output file> - print call graph to a file in dot syntax
  --pp                           - pretty print
  --ps                           - print symbols
  --pt                           - print symbols with types (supercedes --ps)
  --verbose=<int>                - enable log messages (Levels 1-3)
```

By default `topc` accepts a `.top` file, runs the full analysis pipeline, generates LLVM bitcode, and emits a `.bc` file.  Linking the bitcode with the runtime library produces an executable.  The [build.sh](bin/build.sh) script handles the compile and link steps in one command:

```
$ cat hello.top
main() { return 42; }
$ $TOPDIR/bin/build.sh hello.top
$ ./hello
Program output: 42
$ $TOPDIR/bin/build.sh -pp -pt hello.top
main()
{
  return 42;
}

Functions : {
  main : () -> int
}

Locals for function main : {

}
```

Set `TOPDIR` to the repository root, or run `build.sh` from within the repository (it falls back to `git rev-parse --show-toplevel`).


## Working with topc

### Command Line

During development only the initial build steps need to run once (unless a `CMakeLists.txt` file changes).  Thereafter, run `cmake --build build` from the repository root to rebuild only what changed.

If you add a source file, edit the relevant `CMakeLists.txt`, then regenerate:

```
cd build
rm CMakeCache.txt
cmake ..
cmake --build . -j$(nproc)
```

The `tipg4` directory contains a standalone ANTLR4 grammar; its README describes how to build and run it in isolation with the ANTLR4 jar.

### The bin directory

| Script | Purpose |
|--------|---------|
| `bin/bootstrap.sh` | Install dependencies and configure the shell environment |
| `bin/build.sh` | Compile and link a single TOP program |
| `bin/runtests.sh` | Run the full unit and system test suite |
| `bin/gencov.sh` | Generate a code-coverage report |
| `bin/gendocs.sh` | Generate Doxygen documentation |

Run `./bin/runtests.sh -h` for options.  By default `runtests.sh` cleans stale `*.gcda` files before running tests to avoid `gcov` merge warnings; set `TIPC_KEEP_COVERAGE=1` to preserve coverage artifacts across runs.

### Log Messages

The `--verbose [1-3]` flag enables loguru log messages at increasing detail:

- **1** — phase boundaries, symbol table insertions, and type constraint generation
- **2** — level 1 plus type constraints as they are unified
- **3** — level 2 plus union-find solving steps

New messages can be added with `LOG_S(level)` at any point in the source.


## Code Style

`topc` follows [LLVM coding standards](https://llvm.org/docs/CodingStandards.html).  Apply the style across `src/`:

```bash
find src -iname '*.h' -o -iname '*.cpp' | xargs clang-format -style=llvm -i
```

Using [pre-commit](https://pre-commit.com/) enforces style before each commit:

```bash
pre-commit install
```


## Documentation

The TOP grammar, [topg4/TOP.g4](./topg4/TOP.g4), is implemented using ANTLR4 and is free of semantic actions; ANTLR4 rule features control the tree visitors that form key parts of the compiler, keeping the grammar relatively clean without factoring or stratification.

The `topc` compiler follows a classic pipeline:

 * [frontend](./src/frontend) — parsing, AST construction, pretty printing
 * [semantic analysis](./src/semantic) — assignability, symbol, type inference, ownership classification, borrow checking, and the destruction pass
 * [code generation](./src/codegen) — LLVM bitcode emission from the AST
 * [optimization](./src/optimizer) — standard LLVM optimization passes

The `topc` driver produces a `.bc` bitcode file.  Linking it with the [runtime library](./rtlib/top_rtlib.c) (built automatically by CMake as `build/rtlib/top_rtlib.bc`) produces an executable.  Use [bin/build.sh](./bin/build.sh) to perform both steps in one command.

### API Documentation

API documentation is generated by [Doxygen](https://www.doxygen.nl) from inline source comments.  To build it, first ensure the project has been configured with CMake (see [Getting Started](#getting-started)), then run:

```
bin/gendocs.sh
```

The generated HTML is written to `build/docs/html/`.  Open `build/docs/html/index.html` in a browser to browse the documentation.  If the Python package [coverxygen](https://pypi.org/project/coverxygen/) is installed, the script also appends a documentation-coverage summary to the generated HTML.

## Differences from TIP and Limitations

`topc` implements a variant of TIP semantics in a few ways.

The comparison operators are `>`, `==`, and `!=`.  `tipc` added `!=` for convenience in self-contained tests; `topc` preserves that choice.

`topc` follows [C operator precedence rules](https://en.cppreference.com/w/c/language/operator_precedence).  Add parentheses to make intent explicit.

Polymorphic type inference is always on: all non-recursive functions are auto-generalised so call sites instantiate fresh type variables.

**Anonymous records replaced by algebraic sum types.**  TIP supports anonymous record expressions `{field: expr, ...}` and field access `expr.field`.  TOP removes this syntax entirely and replaces structured data with algebraic sum types.  The motivation is soundness under unification-based type inference: TIP's record type system uses an *uber-record* strategy — a single global record type is inferred that contains every field name mentioned anywhere in the program.  Fields absent from a given record expression are zero-initialised on heap allocation and undefined otherwise.  This makes it impossible to detect field-name typos at compile time, conflates structurally distinct record uses, and produces misleading inferred types.  Algebraic sum types avoid all of these problems.  Each `type` declaration introduces a distinct named type; every constructor is closed over exactly the fields it declares; the type checker rejects unknown constructors and missing cases in `case` statements.  Programs that previously used records to model variant data — option types, linked-list nodes, tree nodes — are expressed more precisely with sum types: the compiler enforces exhaustive case coverage, constructor payloads are typed individually, and there is no sharing of field names across unrelated types.

Memory management: unlike TIP, TOP programs that allocate with `alloc` are not responsible for manually freeing memory.  The destruction pass inserts `free` automatically for every owned pointer before the function returns, so allocation-heavy programs do not leak.  Run with `--asan` to verify this guarantee at runtime.


## Resources

### C++ Resources

#### Move Semantics
+ [Move Semantics (part 1 of 2)](https://youtu.be/St0MNEU5b0o)
+ [Move Semantics (part 2 of 2)](https://youtu.be/pIzaZbKUw2s)

#### Value Categories
+ [Understanding Value Categories](https://youtu.be/XS2JddPq7GQ)
+ ["New" Value Terminology](https://www.stroustrup.com/terminology.pdf)

#### Smart pointers
+ [Smart Pointers](https://youtu.be/xGDLkt-jBJ4)

### CMake Resources
+ [CMake docs](https://cmake.org/cmake/help/latest/)
+ [More Modern CMake](https://youtu.be/y7ndUhdQuU8)
+ [Oh No! More Modern CMake](https://youtu.be/y9kSr5enrSk)

### Catch2 and Unit Testing Resources
+ [Catch2 docs](https://github.com/catchorg/Catch2/tree/master/docs)
+ [Modern C++ Testing with Catch2](https://youtu.be/Ob5_XZrFQH0)

### LLVM Resources

To understand and extend code generation, familiarise yourself with the [core LLVM class hierarchy](http://llvm.org/docs/ProgrammersManual.html#the-core-llvm-class-hierarchy-reference).  The [LLVM tutorial](https://llvm.org/docs/tutorial/) is a good starting point; `topc` uses idioms and strategies from it.

Useful references:
  * https://www.cs.cornell.edu/~asampson/blog/llvm.html
  * [LLVM Programmer's Manual](http://llvm.org/docs/ProgrammersManual.html)
  * [GEP instruction](https://llvm.org/docs/GetElementPtr.html) — used in `topc` for function-table dispatch
  * [Opaque Pointers](https://llvm.org/docs/OpaquePointers.html) — required since LLVM 15; see also [docs/OpaquePointers.md](docs/OpaquePointers.md)

### Git Resources
+ [Pro Git Book](https://git-scm.com/book/en/v2)
+ [Git For Ages 4 And Up](https://www.youtube.com/watch?v=1ffBJ4sVUb4)
