# Opaque Pointers in TOPC

LLVM's opaque pointer mode replaces typed pointer spellings such as `i32*` with
the generic `ptr` type. Loads, stores, calls, and GEPs still need the relevant
value types, but pointer values no longer carry a pointee type that frontend code
can recover with APIs such as `getPointerElementType()`.

## Practical Impact

TOPC code generation should track the value type it intends to load, store, or
call instead of asking LLVM pointer values for their element type.

Common migration patterns:

- Use `llvm::PointerType::get(context, 0)` when a generic pointer type is needed.
- Use explicit load types, for example `CreateLoad(i64Type, pointer)`.
- Build call-site `llvm::FunctionType` values from the inferred argument and
  result types, then call through the loaded `ptr` value directly.
- Avoid pointer casts that only existed to convert between typed pointer forms.

## Function Table Shape

Older typed-pointer IR represented the function table as an array of function
pointers and used bitcasts at each call site:

```ll
@_top_ftable = internal constant [2 x i64 ()*] [i64 ()* bitcast (i64 (i64)* @fib to i64 ()*), i64 ()* @_top_main]
```

Opaque-pointer IR stores generic `ptr` values instead:

```ll
@_top_ftable = internal constant [2 x ptr] [ptr @fib, ptr @_top_main]
```

The call site still constructs the precise `FunctionType`, but the loaded
function table entry can be used as a `ptr` without a typed-pointer bitcast.

## Maintenance Note

When updating LLVM APIs in TOPC, prefer carrying explicit type information from
the AST, semantic analysis, or codegen helper that already knows it. Treat any
remaining dependency on pointer element types as migration debt.