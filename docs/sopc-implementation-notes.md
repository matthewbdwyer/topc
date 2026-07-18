# SOPC Implementation Notes (Internal)

## Purpose

This file is for internal instructor/developer use while building the private SOPC implementation. It is not student-facing.

## Capture-Free Lambda Strategy

Core SOPC supports anonymous lambdas with no environment capture.

### User-facing model

1. Lambdas are first-class function values.
2. Lambdas can be assigned to variables and passed as arguments.
3. Lambdas cannot reference outer local variables.

### Internal lowering model

Use a desugaring pass that rewrites each lambda into a fresh generated top-level function.

Example conceptual rewrite:

```top
map(&nums, (x) => x * 2)
```

becomes:

```top
__lambda_1(x) { return x * 2; }
map(&nums, __lambda_1)
```

## Suggested Compiler Responsibilities

1. Parse lambda literals and function application through variables.
2. Build AST nodes for lambda expression and lambda call sites.
3. Reject captures in semantic analysis.
4. Run lambda desugaring before codegen.
5. Preserve source-location mapping for diagnostics after desugaring.

## Suggested Diagnostics

1. Capturing lambda:
   - "capturing lambdas are not supported in core SOPC"
2. Unknown variable in lambda body:
   - standard unresolved-name diagnostic
3. Arity mismatch in lambda call:
   - standard function-arity diagnostic

## Tests To Include

1. Positive: assign lambda to variable and call it.
2. Positive: pass lambda variable into `map` and `fold`.
3. Positive: nested lambda expressions with no captures.
4. Negative: lambda captures local variable from outer scope.
5. Negative: lambda call arity mismatch.
6. Negative: lambda return type incompatible with helper signature.

## Future Extension (Optional)

If closures are later enabled, introduce explicit closure environments and capture-mode rules as a separate project track. Keep this out of core SOPC.

## SOP Standard Library Architecture Extension

Treat SOP standard library support as a first-class architecture extension, not just a language feature.

### Goal

Support library-level functions such as `fold`, `map`, and `subseq` through separate compilation units and a real link stage.

### Recommended split

1. Keep language-level helpers in SOP source files (for example, `sop_stdlib.sop`).
2. Keep C runtime files focused on low-level runtime services (entry, IO, memory helpers, panic/bounds helpers).
3. Link user program units + SOP stdlib unit(s) + runtime unit(s) into the final executable.

### Pipeline shape

1. Compile each SOP translation unit independently to IR/object.
2. Compile/prepare runtime bitcode/object.
3. Run link stage across all units.
4. Emit final executable.

### Assignment-quality milestones

1. Single-unit compile is stable.
2. SOP stdlib compiles as a separate unit.
3. Multi-unit linking succeeds for user code calling stdlib functions.
4. Unresolved-symbol and duplicate-symbol diagnostics are clear.
5. One-command build flow covers compile+link for representative programs.

### Diagnostics to require

1. Missing symbol at link time (undefined reference).
2. Duplicate symbol definition across units.
3. Signature/ABI mismatch at call boundaries.
4. Missing required stdlib unit in build invocation.

### Tests to include

1. Positive: user file calls `map`/`fold` implemented in separate stdlib file.
2. Positive: multi-file project with user helper functions split across files.
3. Negative: deliberately omitted stdlib unit causes unresolved symbol.
4. Negative: duplicate function definition across two units.
5. Negative: call-site arity/type mismatch across unit boundaries.

### Why this matters

1. It mirrors real compiler toolchains and build systems.
2. It forces end-to-end reasoning (frontend, codegen, linker, runtime integration).
3. It creates robust debugging scenarios beyond parser/type-only work.
