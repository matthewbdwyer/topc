# TOPC Inspection Feature Demo

This demo shows the new result-first inspection interface and the optional
`--constraint` modifier.

## Demo Program

```top
type Flag = On | Off;

addone(x) {
  return x + 1;
}

identity(f) {
  return f;
}

touch(b) {
  return 0;
}

main() {
  var x, p, q, touchResult, incResult, polyResult;

  x = 41;
  p = alloc 41;

  touchResult = touch(&x);
  incResult = addone(x);
  polyResult = identity(addone)(x);

  q = p;

  if (incResult != 42) error incResult;
  if (polyResult != 42) error polyResult;
  if (*q != 41) error *q;

  return 0;
}
```

Save as `/tmp/feature-demo.top`.

## Core Result Views

```bash
./build/src/topc --psource /tmp/feature-demo.top
./build/src/topc --past=ascii /tmp/feature-demo.top
./build/src/topc --psym /tmp/feature-demo.top
./build/src/topc --ptype /tmp/feature-demo.top
./build/src/topc --pcallgraph /tmp/feature-demo.top
./build/src/topc --pcallgraph=ascii /tmp/feature-demo.top
./build/src/topc --pcfg=ascii /tmp/feature-demo.top
./build/src/topc --pownership /tmp/feature-demo.top
./build/src/topc --pborrow /tmp/feature-demo.top
```

Default graph artifacts:

- `--past` writes `/tmp/feature-demo.top.ast.dot`
- `--pcallgraph` writes `/tmp/feature-demo.top.callgraph.dot`
- `--pcfg` writes one file per function, e.g.:
  - `/tmp/feature-demo.top.main.cfg.dot`
  - `/tmp/feature-demo.top.addone.cfg.dot`

Use `--output-dir` to relocate graph artifacts:

```bash
./build/src/topc --past --pcallgraph --pcfg --output-dir /tmp/top-graphs /tmp/feature-demo.top
```

## Constraint Views

`--constraint` augments supported result views.

```bash
./build/src/topc --ptype --constraint /tmp/feature-demo.top
./build/src/topc --pcallgraph --constraint /tmp/feature-demo.top
./build/src/topc --pownership --constraint /tmp/feature-demo.top
./build/src/topc --pborrow --constraint /tmp/feature-demo.top
```

Combined example (fixed result order):

```bash
./build/src/topc --psource --ptype --pcallgraph --pownership --constraint /tmp/feature-demo.top
```

## Compilation Coexistence

Inspection-only invocation emits no code artifacts:

```bash
./build/src/topc --ptype /tmp/feature-demo.top
```

Mixed inspect + compile in one run:

```bash
./build/src/topc --ptype -o /tmp/feature-demo.bc /tmp/feature-demo.top
./build/src/topc --past --asm -o /tmp/feature-demo.ll /tmp/feature-demo.top
```
