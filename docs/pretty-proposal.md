# Pretty-Printing Proposal for TOPC

This document proposes a unified set of pretty-print and inspection views for TOPC.

The goal is to make the compiler's internal state easier for students to understand by exposing not only final results, but also the intermediate analysis artifacts that lead to those results.

The feature set should be developed first in `topc`, validated with examples and tests, and then carried forward into `sopc`.

## Motivation

TOPC already has some inspection support, such as `--pp`, `--ps`, and `--pt`.

That is useful, but it is not yet enough for teaching the full reasoning process inside the compiler. Students will benefit from being able to see:

1. the pretty-printed source form,
2. the symbol table,
3. inferred types,
4. generalized type schemes and call-site instantiations,
5. type and ownership constraints,
6. call-graph / CFA results,
7. borrow and lifetime / region relationships when they exist,
8. move and destruction reasoning.

The important design point is that these are different views of the compiler, not one monolithic dump.

There is also a useful recurring pattern across these views: for each analysis we can often separate (a) the constraints built by the compiler and (b) the derived result produced by solving or interpreting those constraints. That is true for type inference, CFA/call-graph construction, and likely for future borrow and lifetime reasoning as well.

## Proposal Summary

Create a coherent family of pretty-print and inspection options that cover the major compiler subsystems.

The current flags can remain, but they should be treated as only the first layer:

1. `--pp` for pretty-printing source structure.
2. `--ps` for symbol-table visibility.
3. `--pt` for final inferred types.

On top of that, TOPC should grow inspection views for internal reasoning data, especially constraints and analysis results. The existing AST visualizer (`--pa`) is a separate source-structure view and should remain distinct from `--pp`.

## What Should Be Printable

### 1. Pretty-printed source structure

This is the most familiar view and already exists with `--pp`.

It should continue to show the source program in a normalized, readable form.

### 2. Symbols

`--ps` should remain the lightweight inventory of declared names.

It is useful for seeing:

1. function names,
2. field names,
3. local names.

### 3. Final inferred types

`--pt` should remain the final answer view for semantic analysis.

This is the most compact way to see the result of inference, and it already exposes enough structure to make ownership and borrowing visible in many cases.

For example, the current output can distinguish ordinary types, references, owned references, and type variables.

### 4. Generalized types and call-site instantiations

This is one of the most useful additions for teaching polymorphism.

Students should be able to see both:

1. the generalized scheme stored for a function or global, and
2. the instantiated type used at each call site.

This makes the following subtlety visible:

1. one definition can have one generalized form,
2. but many fresh instantiations when it is used.

This is especially helpful for explaining why polymorphic functions work, and where inference fails when instantiation does not satisfy the constraints. In other words, the scheme/instantiation split is the type-level version of the broader constraint/result pattern.

### 5. Type constraints and inference results

These are the equations and relationships that drive inference.

Useful items include:

1. fresh type variables,
2. equality / unification constraints,
3. generalization boundaries,
4. instantiation points,
5. constraints introduced by records, sums, sequences, and borrowing.

The corresponding result view is already `--pt`, which shows the final inferred types after the constraints are solved.

### 6. Call-graph / CFA constraints and results

Students should be able to see the compiler's control-flow and call-graph reasoning.

Useful output might include both the constraints and the resulting call-graph:

1. CFA constraints that seed call-graph construction,
2. direct call edges,
3. indirect or conservative targets,
4. reachability information,
5. source spans for each edge or summary.

The current `--pcg` view is the result-side representation; a future constraint view should show the starting point used to derive it.

### 7. Borrow, lifetime, and region relations

When the language or analysis introduces explicit lifetime-style reasoning, it should be printable in a form that explains:

1. which region depends on which other region,
2. why a borrow is valid or invalid,
3. why a region must outlive another region,
4. where a borrow ends.

### 8. Ownership, move, and destruction reasoning

This should make visible:

1. where a value is moved,
2. where a value becomes unavailable,
3. where destruction is inserted,
4. why a use-after-move or double-destroy is rejected.

## Suggested CLI Shape

The exact command-line syntax does not need to be fixed now, but the design should support a small number of focused views.

The intended layering is:

1. `--pX` shows the information `X` computed by the compiler.
2. `--pc=X` shows the constraints used to compute `X`.

In this proposal, the main result views are:

1. `--pp` for pretty-printed source structure.
2. `--ps` for symbol-table visibility.
3. `--pt` for final inferred types.
4. `--pcg` for the computed call graph.

One reasonable direction is:

1. `--pc=type` for type constraints.
2. `--pc=cg` for call-graph / CFA constraints.
3. `--pc=ownership` for move and destruction reasoning.
4. `--pc=borrow` for borrow and lifetime / region data.
5. `--pc=all` for a complete multi-section dump.

For AST structure, a dual-mode visualizer is a good fit:

1. `--pa=dot` for Graphviz dot output, which remains the richest structural view.
2. `--pa=ascii` for a terminal-friendly indented tree view that is easier to read and diff.

The exact notation is intentionally a design choice we are free to make.

## Notation Principles

The pretty-printed output should be:

1. easy to read in class,
2. stable enough for golden tests,
3. compact enough for terminal output,
4. precise enough to diagnose compiler behavior,
5. consistent across different analysis subsystems.

This is important because TOPC is a teaching compiler. The notation should help students reason about the compiler, not just expose internal jargon.

## Output Format Guidelines

Each printed line or section should ideally include:

1. the subsystem or phase that generated it,
2. the source span or AST origin,
3. the printed item itself,
4. enough structure to support regression tests.

An example of the overall style might be:

```text
[type]      test.top:12:5-12:9   α3 = Seq(α4)
[scheme]    test.top:20:1-20:12  forall a. (a) -> a
[inst]      test.top:21:3-21:7   (Int) -> Int
[cfa]       test.top:27:1-27:8   call f -> {g, h}
[borrow]    test.top:33:7-33:14  region(r1) outlives region(r2)
[ownership] test.top:40:2-40:9   value x is moved here
```

The final formatting can and should evolve, but it should remain structured and predictable.

## Why This Should Be Built First in TOPC

There are several reasons to develop this in `topc` before carrying it to `sopc`:

1. It forces us to define the user-facing notation early.
2. It gives us a teaching interface to refine while the compiler is still relatively small.
3. It lets us decide which views are genuinely helpful before SOP adds more complexity.
4. It creates a reusable inspection layer that can move into SOPC later.

This is particularly valuable because SOP will introduce more ownership and lifetime complexity, and those features will be much easier to teach if the inspection story already exists.

## Suggested Development Order

1. Keep the current `--pp`, `--ps`, and `--pt` views.
2. Add a constraint-printing family for type reasoning.
3. Add a polymorphism view that separates generalized schemes from call-site instantiations.
4. Add CFA / call-graph views.
5. Add ownership and destruction views.
6. Add borrow / region views as that analysis becomes explicit.
7. Stabilize the notation with tests and examples.
8. Carry the final design into `sopc`.

## Success Criteria

This proposal is successful if students can use the compiler output to answer questions like:

1. What did the compiler know about this program?
2. Why did this type infer the way it did?
3. What general type was stored for this function?
4. What type did this call site instantiate?
5. What constraints were generated?
6. Why was this program accepted or rejected?
7. Which analysis phase introduced the relevant decision?

If the output helps students understand those questions, then the pretty-printing layer is doing its job.

## Implementation Plan

This section is based on a codebase audit of the current driver, semantic passes, and test harness.

### Key research findings

1. CLI behavior is currently split between stdout booleans and file-valued flags in `src/topc.cpp`:
	- `--pp`, `--ps`, `--pt` print to stdout.
	- `--pa=<file>` and `--pcg=<file>` write dot files.
2. The current `--pa` contract is output-file based, and system tests rely on it (`test/system/run.py` compares generated dot files). This means `--pa=dot|ascii` cannot be introduced by repurposing `--pa` alone without breaking behavior.
3. `SemanticAnalysis` currently retains only result objects: symbol table, type results, call graph, ownership classifier (`src/semantic/SemanticAnalysis.h`).
4. Type constraints are already first-class during collection and heavily tested (`TypeConstraintCollectVisitor`, `PolyTypeConstraintCollectVisitor`), so type-constraint dumping is low-risk.
5. CFA constraints are currently embedded in `CFAnalyzer`/solver flow and not retained as a separate printable artifact; call-graph results are retained and printable.
6. Borrow checker and move analysis currently enforce validity by throwing diagnostics; they do not expose a stable, queryable constraint/result object for printing.
7. Existing testing is strong and should be extended, not replaced:
	- unit tests for `--pp`, `--ps`, AST visualizer, symbol table, type constraints,
	- system golden snapshots for `-pp -pt`, `-pp -ps`, `--pa`, and `--pcg`.

### Design decisions flowing from findings

1. Keep existing result-view flags and semantics stable: `--pp`, `--ps`, `--pt`, `--pcg`, `--pa=<file>`.
2. Add constraint views under `--pc=<kind>` without changing existing output modes.
3. Support ASCII AST output by adding a format selector, not by breaking file-output semantics for `--pa`.
4. Prefer a single inspection rendering layer with domain adapters, not separate ad-hoc printers for each pass.
5. Introduce new persisted artifacts only where needed (notably CFA constraints, and later ownership/borrow traces).

### Phase 1 (lock taxonomy and CLI contracts)

1. Freeze result view semantics for existing flags.
2. Introduce `--pc=<kind>` parser surface for `type`, `cg`, `ownership`, `borrow`, `all`.
3. For AST mode selection, adopt a non-breaking contract such as:
	- `--pa=<file>` (existing)
	- `--pa-format=dot|ascii` (new; default `dot`)
4. Document the two-layer model explicitly in help text: constraints via `--pc`, results via existing `--pX` flags.

### Phase 2 (shared rendering infrastructure)

1. Add a small shared "inspection record" abstraction used by all new dumps:
	- subsystem tag,
	- optional source span,
	- payload text,
	- stable sort key.
2. Build one renderer for text output and reuse it across type/CFA/ownership/borrow dumps.
3. Extend AST visualizer with an ASCII emitter while reusing the same traversal strategy.
4. Add deterministic ordering in all emitters to support golden tests.

### Phase 3 (type constraints + polymorphism visibility)

1. Reuse existing type constraint collectors to implement `--pc=type` output.
2. Keep `--pt` unchanged as the solved result view.
3. Add explicit scheme/instantiation sections in `--pc=type` output (or a closely related mode) to expose polymorphic subtleties.
4. Tests:
	- unit tests with exact constraint-string assertions for small examples,
	- at least one polymorphic identity example showing generalized scheme vs call-site instantiations,
	- system golden snapshots for one or two representative programs.

### Phase 4 (CFA constraints + call-graph result pairing)

1. Introduce a retained CFA-constraint artifact from `CFAnalyzer` suitable for printing.
2. Implement `--pc=cg` using that retained artifact.
3. Keep `--pcg` as-is for result graph output.
4. Tests:
	- unit tests for CFA-constraint extraction and formatting,
	- existing `--pcg` driver/system tests remain unchanged,
	- add paired golden tests demonstrating `--pc=cg` and `--pcg` on the same program.

### Phase 5 (ownership + borrow/move printable traces)

1. Add lightweight retained traces to ownership/move/borrow passes:
	- transitions (Owned/Moved),
	- borrow approvals/rejections context,
	- destruction insertion summary points.
2. Implement `--pc=ownership` and `--pc=borrow` using those traces.
3. Keep error diagnostics unchanged; printed traces are explanatory, not a replacement for errors.
4. Tests:
	- unit tests for trace generation on positive and negative programs,
	- targeted regression tests for move-after-move and borrow-position failures.

### Phase 6 (test harness integration and stabilization)

1. Extend `test/system/run.py` with new snapshot checks for `--pc=type` and `--pc=cg`.
2. Add AST ASCII snapshot checks while preserving existing dot checks.
3. Keep current `--pp`, `--ps`, `--pt`, `--pcg` tests intact to prevent regressions.
4. Add dedicated golden suffixes for new outputs (for example, `.pc.type`, `.pc.cg`, `.ast.txt`) and document regeneration commands in `test/system/README.md`.
5. Enforce deterministic output in CI by sorting records prior to rendering.

### Lean implementation guardrails (avoid clunky duplication)

1. Do not fork existing result printers; wrap them where possible.
2. Do not create multiple text formats for the same domain unless required.
3. Keep one renderer and many adapters.
4. Introduce new persistent analysis objects only when current pass outputs are ephemeral.
5. Add tests concurrently with each phase so format churn is intentional and reviewable.

## Detailed Task Checklist (Test-First)

This checklist is intended to be implementation-ready. It is organized as a sequence of small, test-gated tasks so progress is measured by green tests.

### Working agreement

1. For every new user-visible output, add or update tests first.
2. It is expected that first-pass output formatting may not match final golden files; adjust and regenerate intentionally.
3. No task is complete until all relevant unit + system tests are green.
4. Preserve existing behavior for `--pp`, `--ps`, `--pt`, `--pcg`, and `--pa=<file>` unless explicitly changed by this proposal.

### Pre-flight baseline

1. Run a full baseline test suite and capture current status.
2. Record which tests cover each existing view:
	- `test/unit/frontend/PrettyPrinterTest.cpp`
	- `test/unit/frontend/ASTVisualizerTest.cpp`
	- `test/unit/semantic/SymbolTableTest.cpp`
	- `test/unit/semantic/types/constraints/*.cpp`
	- system snapshots in `test/system/selftests/*.top.pppt`, `test/system/iotests/fib.ppps`, `test/system/**/*.dot`
3. Do not proceed until baseline is reproducible.

Acceptance:

1. Baseline test run is green.
2. No unrelated test failures are introduced.

### Task Group A: CLI surface and mode plumbing (tests first)

1. Add CLI parsing tests for new flags/modes:
	- `--pc=type`, `--pc=cg`, `--pc=ownership`, `--pc=borrow`, `--pc=all`
	- `--pa-format=dot|ascii` with `--pa=<file>`
2. Add driver/system negative tests for invalid values:
	- unknown `--pc` kind
	- unknown `--pa-format`
3. Implement parser options in `src/topc.cpp` without changing old flag semantics.
4. Ensure help text reflects two-layer model (constraints vs results).

Files to touch:

1. `src/topc.cpp`
2. `test/system/run.py`
3. `README.md` (only after behavior is stable)

Acceptance:

1. Existing driver tests remain green.
2. New CLI tests pass for valid and invalid options.

### Task Group B: AST ascii mode (tests first)

1. Add unit tests for ASCII AST output:
	- basic function tree
	- nested expressions/statements
2. Add system snapshot test for at least one known program (`ptr4.top`) in ASCII mode.
3. Implement ASCII emission in AST visualizer using current traversal order.
4. Keep existing dot mode unchanged.

Files to touch:

1. `src/frontend/prettyprint/ASTVisualizer.h`
2. `src/frontend/prettyprint/ASTVisualizer.cpp`
3. `src/frontend/FrontEnd.cpp` (if API widening is needed)
4. `test/unit/frontend/ASTVisualizerTest.cpp`
5. `test/system/run.py`
6. new golden file(s), for example `test/system/selftests/ptr4.top.ast.txt`

Acceptance:

1. Dot output tests still pass unchanged.
2. ASCII output tests pass and snapshots are stable.

### Task Group C: shared constraint rendering layer (tests first)

1. Add unit tests for renderer behavior only:
	- deterministic ordering
	- source span formatting
	- section labeling
2. Implement a small reusable renderer utility (records in, text out).
3. Do not connect to any analysis yet; keep this layer isolated and testable.

Files to touch:

1. new files under `src/frontend/prettyprint/` or `src/semantic/` for renderer
2. new unit test file under `test/unit/frontend/` or `test/unit/semantic/`

Acceptance:

1. Renderer tests are green and deterministic.
2. No domain logic duplicated.

### Task Group D: type constraints (`--pc=type`) + polymorphism sections (tests first)

1. Add unit tests for `--pc=type` output using small programs:
	- arithmetic/integer
	- refs/alloc/deref
	- record and sum examples
	- polymorphic identity with scheme + instantiation lines
2. Add system snapshot(s) for representative `.top` files.
3. Implement adapter from existing type-constraint collectors to shared renderer.
4. Keep `--pt` output unchanged and independently tested.

Files to touch:

1. `src/semantic/types/constraints/TypeConstraintCollectVisitor.*` (read-only if possible)
2. `src/semantic/types/constraints/PolyTypeConstraintCollectVisitor.*` (read-only if possible)
3. new printer/adapter files
4. `src/topc.cpp` wiring
5. new/updated tests under `test/unit/semantic/types/constraints/`
6. system snapshot additions in `test/system/`

Acceptance:

1. `--pc=type` tests pass.
2. Existing `--pt` tests and snapshots remain green.

### Task Group E: CFA constraints (`--pc=cg`) paired with `--pcg` result (tests first)

1. Add unit tests specifying expected CFA constraint dump shape for simple programs.
2. Add system paired snapshots on same input:
	- constraints via `--pc=cg`
	- graph result via `--pcg`
3. Introduce retained CFA-constraint artifact from `CFAnalyzer` (or equivalent adapter-safe structure).
4. Implement `--pc=cg` rendering using shared renderer.

Files to touch:

1. `src/semantic/cfa/CFAnalyzer.h`
2. `src/semantic/cfa/CFAnalyzer.cpp`
3. optional new artifact struct/class under `src/semantic/cfa/`
4. `src/topc.cpp`
5. tests under `test/unit/semantic/cfa/`
6. system snapshots in `test/system/iotests/` or `test/system/selftests/`

Acceptance:

1. New `--pc=cg` tests pass.
2. Existing `--pcg` tests pass unchanged.

### Task Group F: ownership and borrow traces (`--pc=ownership`, `--pc=borrow`) (tests first)

1. Add unit tests for trace events:
	- move transitions
	- borrow-approval contexts
	- rejection contexts for known error programs
2. Add focused system tests for at least one move and one borrow case.
3. Add lightweight retained trace structures in:
	- move analysis
	- borrow checker
	- (optional) destruction pass summary
4. Wire new trace dumps to `--pc=ownership` and `--pc=borrow`.

Files to touch:

1. `src/semantic/MoveAnalysis.h`
2. `src/semantic/MoveAnalysis.cpp`
3. `src/semantic/BorrowChecker.h`
4. `src/semantic/BorrowChecker.cpp`
5. optional `src/semantic/DestructionPass.*`
6. `src/topc.cpp`
7. tests under `test/unit/semantic/`
8. system snapshots in `test/system/`

Acceptance:

1. New ownership/borrow dump tests pass.
2. Existing semantic error behavior and messages are preserved unless intentionally revised.

### Task Group G: system harness and docs stabilization

1. Extend `test/system/run.py` snapshot matrix to include new `--pc=*` and ASCII AST checks.
2. Choose and document golden file suffixes consistently, for example:
	- `.pc.type`
	- `.pc.cg`
	- `.pc.ownership`
	- `.pc.borrow`
	- `.ast.txt`
3. Update `test/system/README.md` regeneration commands.
4. Update top-level `README.md` option docs only after all outputs stabilize.

Acceptance:

1. Full unit + system test run is green.
2. Regeneration steps are documented and reproducible.

### Final acceptance criteria (phases 1-6)

1. Existing result views remain intact and tested.
2. New constraint views are available, deterministic, and covered by unit + system tests.
3. AST visualizer supports dot and ascii without breaking existing dot workflows.
4. The system-test runner validates new outputs via golden snapshots.
5. A clean test run is the release gate.
