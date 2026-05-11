# System Tests

System tests compile TIP programs with `tipc`, link them against the runtime
library, execute them, and compare output or exit codes against golden
reference files.

## Running

From the repository root:

```bash
TIPCLANG=/path/to/clang ./bin/runtests.sh -s
```

Or directly (from any directory):

```bash
TIPCLANG=/path/to/clang ./test/system/run.sh
```

`TIPCLANG` must point to the clang binary used to link `.bc` bitcode with the
runtime library (e.g. `/opt/homebrew/opt/llvm@17/bin/clang`).

The runtime library (`rtlib/tip_rtlib.bc`) is built automatically by
`bin/runtests.sh`. When running `run.sh` directly, build it first:

```bash
cd rtlib && ./build.sh
```

## Test categories

### `selftests/`

Self-contained TIP programs that exit 0 on success and non-zero on failure.
Each program encodes its own expected behavior (no external oracle needed).
Every program is run twice: once normally and once with the `-do` dead-code
elimination pass.

Golden reference files:
- `*.tip.pppt` — expected output of `tipc -pp -pt` (pretty-print with types)
- `*.tip.dot`  — expected AST visualizer output (`tipc --pa`)

### `iotests/`

Programs that read input from `argv[1]` and produce output on stdout.
Expected output is stored in `*.expected` files named
`<program>-<input>.expected`.

Golden reference files:
- `fib.ppps`   — expected output of `tipc -pp -ps fib.tip`
- `fib.tip.ll` — expected human-readable LLVM IR (`tipc --asm`)
- `fib.tip.dot` — expected call graph dot output (`tipc --pcg`)
- `linkedlist.tip.dot` — expected AST visualizer output
- `main.tip.ll` — expected default assembly output
- `unwritable`  — zero-byte file kept non-writable; used to test the
  "failed to open output file" error path. **Do not delete or chmod this file.**

`*error.tip` files are TIP programs expected to produce a non-zero exit from
`tipc` (e.g. programs with type errors). The test verifies that `tipc` rejects
them.

### `polytests/`

TIP programs that exercise the polymorphic type inference (`--pi`) extension.
Each is compiled and run similarly to `selftests`, and the pretty-printed type
output is diffed against a `*.pppt` golden reference.

### `leak/`

TIP programs intended for memory-leak analysis (Valgrind / AddressSanitizer).
These files are **not yet run automatically** — they are inputs for future CI
integration. See `docs/test-analysis.md` item SYS-3 for details.

## Regenerating golden reference files

When a deliberate change to compiler output is made, regenerate the affected
reference files:

```bash
# Pretty-print with types for a selftest
tipc -pp -pt test/system/selftests/<name>.tip > test/system/selftests/<name>.tip.pppt

# Assembly output
tipc --asm test/system/iotests/fib.tip -o test/system/iotests/fib.tip.ll

# Call graph
tipc --pcg=test/system/iotests/fib.tip.dot test/system/iotests/fib.tip

# AST visualizer
tipc --pa=test/system/iotests/linkedlist.tip.dot test/system/iotests/linkedlist.tip
tipc --pa=test/system/selftests/ptr4.tip.dot test/system/selftests/ptr4.tip
```

Commit the updated reference files alongside the compiler change.
