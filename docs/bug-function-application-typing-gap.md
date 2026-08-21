# Bug: function application escapes type checking under polymorphic inference

## Summary

Applying a non-function value type-checks successfully when the calling
function is handled by the polymorphic inference path. For example:

```top
main() {
  var v, out;
  v = 5;
  out = v(3);   // v is an int, yet this is accepted
  return out;
}
```

`v` is unified with `int` by `v = 5`, and then applied as a function in
`v(3)`. This should be a type error (`int` cannot unify with a function type),
but the program compiles and produces a binary. This is a soundness hole: the
compiler accepts a program that applies an integer as if it were a function.

## Minimal reproduction

```
$ printf 'main() { var v, out; v = 5; out = v(3); return out; }\n' > /tmp/bug.top
$ ./build/src/topc --asm --output-dir=/tmp /tmp/bug.top ; echo "exit $?"
exit 0        # accepted — should be a type error
```

Contrast with dereference, which is checked correctly:

```
$ printf 'main() { var v, out; v = 5; out = *v; return out; }\n' > /tmp/bug2.top
$ ./build/src/topc --asm --output-dir=/tmp /tmp/bug2.top
topc: Cannot unify ref&<(*v)@...> with int: different structure
topc: semantic error
```

## Root cause

`src/semantic/types/constraints/PolyTypeConstraintVisitor.cpp`,
`endVisit(ASTFunAppExpr)` (currently around lines 52-108) emits the
application-shape constraint — "the callee must be a function from the actual
argument types to the result type" — *inside a loop over the CFA may-call
set*:

```cpp
for (auto f : callGraph->getCalledFuns(element)) {
    ...
    constraintHandler->handle(
        astToVar(element->getFunction()),
        std::make_shared<TopFunction>(actuals, astToVar(element)));
}
```

When control-flow analysis finds **no** function flowing to the callee — which
is exactly the situation for `v(3)` where `v` holds an integer, since no
function value is ever assigned to `v` — `getCalledFuns(element)` returns the
empty set. The loop body never executes, so **no constraint is generated at
all** for the application. Nothing ever requires the callee to be a function,
so its `int` type stands unchallenged and the program is accepted.

Two observations confirm this is the mechanism:

1. **Dereference is checked** because `endVisit(ASTDeRefExpr)` emits its
   `callee = ref&<...>` constraint unconditionally, not gated on CFA. Function
   application is the only elimination form whose constraint is conditioned on
   the may-call set.

2. **The monomorphic path catches it.** `TypeConstraintVisitor::endVisit(
   ASTFunAppExpr)` (the mono collector, around line 141) emits the same
   constraint **unconditionally**, with no CFA loop. Placing the identical
   `v(3)` inside a *recursive* function (which routes through the mono/residual
   path rather than the polymorphic path) produces the expected error:

   ```top
   loop(k) {
     var v, out;
     v = 5;
     out = v(3);
     if (k > 0) { out = loop(k - 1); }
     return out;
   }
   main() { return loop(2); }
   ```
   ```
   topc: Cannot unify int with (<3@...>) -> <v(3)@...>: different structure
   ```

So the gap is specifically: **polymorphic path + empty CFA target set ⇒ the
application is left entirely unconstrained.**

## Why the constraint is gated (context for the fix)

The loop is not gratuitous. Its purpose is *polymorphic instantiation*: for a
direct named call to a function marked polymorphic, the visitor makes a fresh
copy of the callee's generic type (`FreshAlphaCopier`) and constrains the fresh
copy — not the function declaration's own type variable — against the actual
arguments. This is what keeps a generic function like `id` usable at multiple
types across call sites. Emitting the base structural constraint directly on
the callee's declaration variable in that case would unify the *generic* type
with concrete actuals and monomorphize the function, defeating polymorphism.
That is the reason the base (monomorphic) constraint is not simply emitted
unconditionally today.

The bug is that when the loop iterates zero times, the base constraint is not
emitted *either* — so the fundamental "the callee is a function of this shape"
rule, which must hold regardless of what CFA can resolve, is dropped.

## Proposed fix

The application rule `callee : (actuals) -> result` is a structural invariant of
the program and must always be stated for every call site. The minimal,
low-risk change that closes the hole without disturbing polymorphism:

> When `callGraph->getCalledFuns(element)` is empty, emit the monomorphic base
> constraint `astToVar(element->getFunction()) = TopFunction(actuals,
> astToVar(element))` as a fallback (the same constraint the mono collector
> already emits unconditionally).

Case analysis:

- Direct named call to a real function — the function is itself a CFA target,
  so the set is non-empty; behavior unchanged.
- Higher-order call that CFA resolves to one or more targets — set non-empty;
  behavior unchanged.
- Applying a non-function value (`v = 5; v(3)`), or any callee CFA cannot tie to
  a function — set previously empty and unconstrained; now the fallback
  constraint is emitted and the program is correctly rejected.

An alternative worth considering is to *always* emit the base structural
constraint but target it at the call-site's own fresh result/callee variables
(never the generic function declaration variable), so it composes with rather
than replaces poly instantiation. The empty-set fallback above is the smaller,
more obviously safe change and is recommended first.

## Test plan

Add a regression test asserting the negative case in a **non-recursive**
function (so it exercises the polymorphic path, where the bug lives):

```top
main() {
  var v, out;
  v = 5;
  out = v(3);
  return out;
}
```

This program must be rejected with a unification error between `int` and a
function type. Keep an existing positive higher-order test (e.g. `apply`) green
to confirm the fix does not over-constrain legitimate higher-order calls, and
confirm the polymorphic reuse tests (a generic function applied at multiple
types) still pass.

## Scope

This is core TOP type inference, independent of any SOP feature. The same code
is inherited by sopc through the backport, so the fix should be made here in
topc and then flow downstream; sopc gains the same regression coverage once it
picks up the change.
