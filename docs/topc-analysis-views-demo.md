# TOPC Analysis Views Demo

This demo shows how to inspect the main intermediate representations and
analysis results produced by `topc` without compiling all the way to an output
artifact.

The views are useful when debugging type inference, call relationships,
control flow, ownership transfer, borrow provenance, and the constraints behind
those analyses.

## Demo Program

```top
type Flag = On | Off;

addone(x) {
  return x + 1;
}

identity(x) {
  return x;
}

readBorrow(b) {
  return *b;
}

makeBox(seed) {
  return alloc *seed;
}

main() {
  var x, value, incResult, box;

  x = 17;
  value = readBorrow(identity(&x));
  incResult = addone(value);
  box = makeBox(&x);

  if (value != 17) error value;
  if (incResult != 18) error incResult;
  if (*box != 17) error *box;

  return 0;
}
```

Save as `/tmp/topc-analysis-demo.top`.

## What Each View Shows

| View | What to inspect |
| --- | --- |
| `--psource` | Pretty-printed source after parsing. |
| `--past` / `--past=ascii` | AST graph artifacts. |
| `--psym` | Symbol table and scoped declarations. |
| `--ptype` | Inferred function and local types. |
| `--pcallgraph` / `--pcallgraph=ascii` | Function call relationships. |
| `--pcfg` / `--pcfg=ascii` | Control-flow graph for each function. |
| `--pownership` | Ownership classes and destruction insertion summary. |
| `--pborrow` | Approved borrow expressions; rejected programs use diagnostics. |
| `--constraint` | Underlying constraints for supported analysis views. |

## Core Analysis Views

```bash
./build/src/topc --psource /tmp/topc-analysis-demo.top
./build/src/topc --past=ascii /tmp/topc-analysis-demo.top
./build/src/topc --psym /tmp/topc-analysis-demo.top
./build/src/topc --ptype /tmp/topc-analysis-demo.top
./build/src/topc --pcallgraph=ascii /tmp/topc-analysis-demo.top
./build/src/topc --pcfg=ascii /tmp/topc-analysis-demo.top
./build/src/topc --pownership /tmp/topc-analysis-demo.top
./build/src/topc --pborrow /tmp/topc-analysis-demo.top
```

Some views print to stdout. Graph-oriented views write artifacts by default.

Default graph artifacts include:

1. `--past` writes `/tmp/topc-analysis-demo.top.ast.dot`.
2. `--pcallgraph` writes `/tmp/topc-analysis-demo.top.callgraph.dot`.
3. `--pcfg` writes one file per function, for example:
   1. `/tmp/topc-analysis-demo.top.main.cfg.dot`
   2. `/tmp/topc-analysis-demo.top.identity.cfg.dot`
   3. `/tmp/topc-analysis-demo.top.makeBox.cfg.dot`

Use `--output-dir` to relocate graph artifacts:

```bash
./build/src/topc --past --pcallgraph --pcfg --output-dir /tmp/top-graphs /tmp/topc-analysis-demo.top
```

## Ownership And Borrow Views

The demo program exercises two useful inter-procedural cases:

1. `readBorrow(identity(&x))` passes a borrow-derived value through immediate
   function arguments. This is accepted because the borrowed alias is not stored
   or returned.
2. `makeBox(&x)` receives a borrow but returns a new owning reference. The
  result bound to `box` is classified as `Own`, and the destruction pass
  accounts for it.

Inspect those facts with:

```bash
./build/src/topc --pborrow /tmp/topc-analysis-demo.top
./build/src/topc --pownership /tmp/topc-analysis-demo.top
```

Expected output shape:

1. `--pborrow` prints a `[borrow-result]` section with approved borrow
   expressions.
2. `--pownership` prints `[ownership-result]` and `[destruction-summary]`
   sections.

## Constraint Views

`--constraint` augments supported result views with the facts that drove the
analysis.

```bash
./build/src/topc --ptype --constraint /tmp/topc-analysis-demo.top
./build/src/topc --pcallgraph --constraint /tmp/topc-analysis-demo.top
./build/src/topc --pownership --constraint /tmp/topc-analysis-demo.top
./build/src/topc --pborrow --constraint /tmp/topc-analysis-demo.top
```

For borrows, `[borrow-constraints]` names the function and argument position
that directly receive each `&expression`. A nested pass-through such as
`readBorrow(identity(&x))` also produces ordered `[borrow-flow]` records for
each call-to-call hop. These records explain immediate call-chain provenance;
they are not a general lifetime or region graph.

Combined example with fixed result order, including borrow provenance:

```bash
./build/src/topc --psource --ptype --pcallgraph --pownership --pborrow --constraint /tmp/topc-analysis-demo.top
```

## Compilation Coexistence

Inspection-only invocation emits no code artifacts:

```bash
./build/src/topc --ptype /tmp/topc-analysis-demo.top
```

Mixed inspect and compile in one run:

```bash
./build/src/topc --ptype -o /tmp/topc-analysis-demo.bc /tmp/topc-analysis-demo.top
./build/src/topc --past --asm -o /tmp/topc-analysis-demo.ll /tmp/topc-analysis-demo.top
```