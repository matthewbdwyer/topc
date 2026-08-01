# System Tests

System tests compile TOP programs with `topc`, link them against the runtime
library, execute them, and compare output or exit codes against golden
reference files.

## Running

From the repository root:

```bash
TOPCLANG=/path/to/clang ./bin/runtests.sh -s -- -j 2
```

Or directly (from any directory):

```bash
TOPCLANG=/path/to/clang python3 ./test/system/run.py
```

`TOPCLANG` must point to the clang binary used to link `.bc` bitcode with the
runtime library (e.g. `/opt/homebrew/opt/llvm/bin/clang`).

The runtime library (`build/rtlib/top_rtlib.bc`) is built automatically by
`bin/runtests.sh`. When running `run.py` directly, build it first:

```bash
cmake --build build --target top_rtlib
```

## Test categories

### `selftests/`

Self-contained TOP core-feature programs that exit 0 on success and non-zero on failure.
Each program encodes its own expected behavior (no external oracle needed).
Every program is run twice: once normally and once with the `-do` dead-code
elimination pass.

Golden reference files:
- `*.top.pppt` — expected output of `topc --psource --ptype`
- `*.top.ast.txt` — expected ASCII AST output (`topc --past=ascii`)
- `*.top.pc.type` — expected type+constraint output (`topc --ptype --constraint`)
- `*.top.pc.cg` — expected call-graph constraints (`topc --pcallgraph --constraint`)
- `*.top.pc.ownership` — expected ownership/move constraints (`topc --pownership --constraint`)
- `*.top.pc.borrow` — expected borrow constraints (`topc --pborrow --constraint`)

### `iotests/`

Programs that read input from `argv[1]` and produce output on stdout.
Expected output is stored in `*.expected` files named
`<program>-<input>.expected`.
Programs that use TOP's `input` expression instead of `main` parameters can
provide stdin with a same-stem `*.stdin` fixture next to the `*.expected` file.

Golden reference files:
- `fib.ppps`   — expected output of `topc --psource --psym fib.top`
- `fib.top.ll` — expected human-readable LLVM IR (`topc --asm`)
- `fib.top.callgraph.dot` — expected call graph dot output (`topc --pcallgraph`)
- `linkedlist.top.dot` — expected AST visualizer output
- `main.top.ll` — expected default assembly output
- `unwritable`  — zero-byte file kept non-writable; used to test the
  "failed to open output file" error path. **Do not delete or chmod this file.**

`*error.top` files are TOP programs expected to produce a non-zero exit from
`topc` (e.g. programs with type errors). The test verifies that `topc` rejects
them.

### `polytests/`

TOP programs that exercise the polymorphic type inference extension.
Each is compiled and run similarly to `selftests`, and the pretty-printed type
output is diffed against a `*.pppt` golden reference.

### `leak/`

TOP programs intended for memory-leak analysis (Valgrind / AddressSanitizer).
These files are **not yet run automatically** — they are inputs for future CI
integration. See `docs/test-analysis.md` item SYS-3 for details.

## Regenerating golden reference files

When a deliberate change to compiler output is made, regenerate the affected
reference files:

```bash
# Source + inferred types for a selftest
topc --psource --ptype test/system/selftests/<name>.top > test/system/selftests/<name>.top.pppt

# Type constraints
topc --ptype --constraint test/system/selftests/<name>.top > test/system/selftests/<name>.top.pc.type

# Call-graph constraints
topc --pcallgraph --constraint test/system/selftests/<name>.top > test/system/selftests/<name>.top.pc.cg

# Ownership / move constraints
topc --pownership --constraint test/system/selftests/<name>.top > test/system/selftests/<name>.top.pc.ownership

# Borrow constraints
topc --pborrow --constraint test/system/selftests/<name>.top > test/system/selftests/<name>.top.pc.borrow

# Assembly output
topc --asm test/system/iotests/fib.top -o test/system/iotests/fib.top.ll

# Call graph
topc --pcallgraph --output-dir test/system/iotests test/system/iotests/fib.top

# AST visualizer
topc --past --output-dir test/system/iotests test/system/iotests/linkedlist.top
topc --past --output-dir test/system/selftests test/system/selftests/ptr4.top
topc --past=ascii --output-dir test/system/selftests test/system/selftests/ptr4.top
```

Commit the updated reference files alongside the compiler change.
