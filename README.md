topc
=========
A compiler from TOP to LLVM bitcode


## Heritage

TOP extends [TIP](https://cs.au.dk/~amoeller/spa/ "Static Program Analysis") (Tiny Imperative Programming), the pedagogical language designed by Anders M&#248;ller and Michael I. Schwartzbach for their _Static Program Analysis_ lecture notes.  The original TIP compiler, [`tipc`](https://github.com/matthewbdwyer/tipc), compiles TIP programs to LLVM bitcode; `topc` builds on that foundation, adding sum types, a static ownership type system, and a borrow mechanism for controlled aliasing.

`topc` now implements a single TOP semantics path. There is no separate TIP
compatibility typing mode in semantic analysis.


## TOP Language Quick Reference

This section is a compact reference. See the [TOP language tutorial](docs/TOP-tutorial.md)
for detailed explanations, diagnostics, examples, and current limitations.

### TOP Core Syntax Subset

TOP includes a compact imperative core syntax: functions, optional `var`
declarations for locals before statements, assignments, `while`, `if`/`else`, `output`, `error`,
and a final `return`. The heap allocation expression `alloc expr` allocates a
cell holding the initial value. Function values are first-class: a function
name evaluates to a pointer that can be called through a variable. Comparison
operators are `>`, `==`, and `!=`.

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

TOP adds algebraic sum types with a file-level `type` declaration and pattern-matching `case` statements. Constructors have zero or more positional payloads, such as `None`, `Some(value)`, or `Pair(first, second)`.

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

`alloc expr` produces a value of the owned pointer type `own&T`. The compiler tracks ownership statically: at a function return, a _destruction pass_ inserts destruction for each live owned local. Code generation lowers destruction to recursive resource cleanup.

```
main() {
  var p;
  p = alloc 42;
  // destruction pass inserts cleanup for p before return
  return *p;
}
```

Ownership is linear: assigning an owned value _moves_ it; the original binding becomes inaccessible.  The same applies to sum-type values, which are heap-boxed and therefore owned: passing an owned value to a function **by value moves it** (the callee then owns and destroys it unless it moves it out), while a **borrow** (`&x`) lets a function read or modify a value in place without taking ownership, so the caller keeps it.  A `case` on an owned value consumes it, moving its payloads into the arm bindings; to traverse a value you want to keep, borrow it (`case *p`).  Destruction is lowered to recursive per-type cleanup, and the `--san` flag instruments the generated IR with Address/LeakSanitizer so that correct automatic deallocation can be verified at runtime.

An owned pointer is **single-level**: the payload of `alloc expr` must be a non-owning (`Copy`) value, so `own&own&T` is rejected — an owned pointer cannot own another owned pointer.  This keeps every owned resource with exactly one owner and freed exactly once.  To own structured or heap-allocated data, use a sum type, whose payloads are typed individually and freed recursively; a mutable cell inside such a structure is an `own&int` (or other `own&<Copy>`) payload.  For example, a mutable sequence is `type Seq = None | Cons(v, n)` with `v : own&int`, built as `Cons(alloc 1, Cons(alloc 2, None))`, traversed by borrow (`case *p`), mutated in place (`*v = *v + 1`), and freed automatically.

### Borrows

A borrow `&expr` creates a non-owning reference to a storage location. A borrow expression may **only** appear as an immediate function call argument. A borrow-derived call result may continue through nested immediate call arguments, but it cannot be stored, returned, or used in another expression. Inside the callee, `*p` reads through the reference and `*p = v` writes through it without transferring ownership.

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
| Ownership | Owned values (including sum-type values) are linear: passing by value moves; use `&x` to borrow for read/write without moving |
| Owned pointers | Single-level: `alloc expr` requires a non-owning payload; `own&own&T` is rejected. Own structured data with a sum type instead |
| Borrows | `&expr` must be a direct call argument; borrow-derived results may flow only through nested immediate call arguments |
| Call statements | Every call result must be assigned — there are no void call statements |


## Teaching Goals

`topc` is a teaching and research compiler designed to make the construction of a typed, memory-safe language concrete and approachable.  The implementation layers a sequence of static analyses on top of a standard compiler pipeline — parser → AST → type inference → semantic checks → code generation — with each phase building on the results of the previous:

1. **Type inference** resolves the types of all expressions and functions using Hindley-Milner-style unification, extended to handle sum types, owned pointers, and borrow references.
2. **Ownership classification** structurally labels inferred values as `Own` or `Copy`, including algebraic values whose payloads own resources.
3. **Borrow checking** restricts direct borrows and tracks borrow-derived values through immediate call chains, preventing those aliases from escaping without a full lifetime system.
4. **Destruction pass** uses ownership and move information to insert `destroy` calls for live owned locals before each function return.

Together these analyses enforce TOP's ownership and borrow model within the supported language boundaries described below. The compiler remains small enough to read in full, and its unit and system tests are structured to be readable alongside the corresponding analysis code.


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
cmake -S . -B build -G Ninja
cmake --build build
```

The build downloads and compiles ANTLR4's C++ runtime if needed, builds the runtime library (`build/rtlib/top_rtlib.bc`), and compiles `topc` together with its unit tests.  The `topc` executable lands in `build/src/`.


## Testing topc

After building, run the CTest suite from the repository root:

```
ctest --test-dir build --output-on-failure --progress
```

The `--progress` flag keeps successful runs compact while `--output-on-failure` still prints failing test output when needed. For an even quieter summary-only run, use `ctest --test-dir build --output-on-failure -Q`.

Run the system harness with the same `clang` used by the CMake/LLVM build:

```
TOPCLANG=/opt/homebrew/opt/llvm/bin/clang python3 test/system/run.py -j 2
```

On non-Homebrew systems, replace `TOPCLANG` with the path to the matching LLVM `clang`. The bootstrap script writes `TOPCLANG` into your shell startup file, so this is often just:

```
python3 test/system/run.py -j 2
```

The convenience wrapper runs both unit binaries and system tests:

```
./bin/runtests.sh
```

Use `./bin/runtests.sh -s` for only system tests or `./bin/runtests.sh -u` for only CTest. The wrapper accepts system harness arguments after `--`, for example `TOPCLANG=/opt/homebrew/opt/llvm/bin/clang ./bin/runtests.sh -s -- -j 2`. By default `runtests.sh` cleans stale `*.gcda` files before running tests to avoid `gcov` merge warnings; set `TOPC_KEEP_COVERAGE=1` to preserve coverage artifacts across runs.


## Using topc

```
OVERVIEW: topc - a TOP to llvm compiler

USAGE: topc [options] <top source file>

OPTIONS:

  --san                          - instrument generated IR with Address/LeakSanitizer
  --asm                          - emit human-readable LLVM assembly language
  --constraint                   - include constraint/trace details
  --do                           - disable bitcode optimization
  --log=<logfile>                - log all messages to logfile (enables --verbose 3)
  -o <outputfile>                - write output to <outputfile>
  --output-dir=<directory>       - output directory for graph artifacts
  --past[=<dot|ascii>]           - print source AST (default format: dot)
  --pborrow                      - print borrow validity result
  --pcallgraph[=<dot|ascii>]     - print call graph result (default format: dot)
  --pcfg[=<dot|ascii>]           - print source CFG result (default format: dot)
  --pownership                   - print ownership analysis result
  --psource                      - print normalized source
  --psym                         - print symbols and scopes
  --ptype                        - print inferred types
  --verbose=<int>                - enable log messages (Levels 1-3)
```

By default `topc` accepts a `.top` file, runs the full analysis pipeline, generates LLVM bitcode, and emits a `.bc` file. Inspection-only invocations (for example `--ptype` or `--pcfg`) do not emit `.bc`/`.ll` artifacts unless compile output is also requested via `-o` or `--asm`. Linking the bitcode with the runtime library produces an executable.  The [build.sh](bin/build.sh) script handles the compile and link steps in one command:

```
$ cat hello.top
main() { return 42; }
$ ./bin/build.sh hello.top
$ ./hello
Program output: 42
$ ./build/src/topc --psource --ptype hello.top
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

During development only the initial build steps need to run once. Thereafter, run `cmake --build build` from the repository root to rebuild only what changed. CMake automatically regenerates when `CMakeLists.txt` files change.

If you add a source file, edit the relevant `CMakeLists.txt`, then rebuild:

```
cmake -S . -B build
cmake --build build --parallel
```

The `topg4` directory contains a standalone ANTLR4 grammar; its README describes how to build and run it in isolation with the ANTLR4 jar.

### The bin directory

| Script | Purpose |
|--------|---------|
| `bin/bootstrap.sh` | Install dependencies and configure the shell environment |
| `bin/build.sh` | Compile and link a single TOP program |
| `bin/runtests.sh` | Run the full unit and system test suite |
| `bin/gencov.sh` | Generate a code-coverage report |
| `bin/gendocs.sh` | Generate Doxygen documentation |

See [Testing topc](#testing-topc) for the recommended test commands. Run `./bin/runtests.sh -h` for wrapper options.

### Log Messages

The `--verbose=<1-3>` flag enables compiler logs at increasing detail:

- **1** — semantic phase lifecycle and bounded summaries
- **2** — level 1 plus per-function and per-declaration semantic decisions
- **3** — level 2 plus constraints, solver operations, and dataflow mechanics

For example, `topc --verbose=2 program.top` writes levels 1 and 2 to stderr.
`topc --log=topc.log program.top` appends all levels to a file while retaining
compiler errors on stderr. Prefer the deterministic `--pX` inspection views in
documentation and tests; verbose logs target compiler implementation debugging.

Semantic analyses add messages with `SEMANTIC_LOG(level, phase)` so phase names
and filtering remain consistent.


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

Start with the document that matches the question you are asking:

* [TOP language tutorial](docs/TOP-tutorial.md) — language concepts, examples,
  compiler diagnostics, ownership, borrowing, and current limitations
* [Compiler architecture](docs/compiler-architecture.md) — semantic pipeline,
  reference representation, effect summaries, and analysis contracts
* [Analysis views demo](docs/topc-analysis-views-demo.md) — focused examples of
  the `--p*` inspection options and constraint traces
* [CLion remote development](docs/clion_thin_client.md) and
  [VS Code Remote SSH](docs/vscode_remote_ssh.md) — UVA course environment setup

The TOP grammar, [topg4/TOP.g4](./topg4/TOP.g4), is implemented using ANTLR4 and is free of semantic actions; ANTLR4 rule features control the tree visitors that form key parts of the compiler, keeping the grammar relatively clean without factoring or stratification.

The `topc` compiler follows a classic pipeline:

 * [frontend](./src/frontend) — parsing, AST construction, pretty printing
 * [semantic analysis](./src/semantic) — assignability, symbol, type inference, ownership classification, borrow checking, and the destruction pass
 * [code generation](./src/codegen) — LLVM bitcode emission from the AST
 * [optimization](./src/optimizer) — standard LLVM optimization passes

The `topc` driver produces a `.bc` bitcode file.  Linking it with the [runtime library](./rtlib/top_rtlib.c) (built automatically by CMake as `build/rtlib/top_rtlib.bc`) produces an executable.  Use [bin/build.sh](./bin/build.sh) to perform both steps in one command.

### API Documentation

API documentation is generated by [Doxygen](https://www.doxygen.nl) from inline source comments.  To build it, first ensure the project has been configured with CMake (see [Building topc](#building-topc)), then run:

```
bin/gendocs.sh
```

The generated HTML is written to `build/docs/html/`.  Open `build/docs/html/index.html` in a browser to browse the documentation.  If the Python package [coverxygen](https://pypi.org/project/coverxygen/) is installed, the script also appends a documentation-coverage summary to the generated HTML.

## Differences from TIP and Limitations

`topc` implements a variant of TIP semantics in a few ways.

The comparison operators are `>`, `==`, and `!=`.  `tipc` added `!=` for convenience in self-contained tests; `topc` preserves that choice.

`topc` follows [C operator precedence rules](https://en.cppreference.com/w/c/language/operator_precedence).  Add parentheses to make intent explicit.

Polymorphic type inference is always on: all non-recursive functions are auto-generalised so call sites instantiate fresh type variables.

**Anonymous records replaced by algebraic sum types.**  TIP supports anonymous record expressions `{field: expr, ...}` and field access `expr.field`.  TOP removes this syntax entirely and replaces structured data with algebraic sum types.  The motivation is soundness under unification-based type inference: TIP's record type system uses an *uber-record* strategy — a single global record type is inferred that contains every field name mentioned anywhere in the program.  Fields absent from a given record expression are zero-initialised on heap allocation and undefined otherwise.  This makes it impossible to detect field-name typos at compile time, conflates structurally distinct record uses, and produces misleading inferred types.  Algebraic sum types avoid these problems.  Each `type` declaration introduces a distinct named type, constructor payloads are typed individually, and `case` analysis checks constructor patterns and exhaustive coverage.  Constructor expressions are validated the same way as case arms: an unknown constructor or a wrong argument count is rejected during weeding.

Memory management: unlike TIP, TOP has no source-level manual deallocation. The destruction pass inserts destruction for live owned locals before a function returns, and code generation recursively lowers that destruction to resource cleanup. Run with `--san` to check the generated cleanup for leaks, double frees, and invalid accesses.


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
