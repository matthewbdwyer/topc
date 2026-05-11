# O-TIP / O-SIP Design Consolidation Draft

## 1. Purpose

This document consolidates the main design principles that emerged from the discussion about extending TIP to **O-TIP** and then to **O-SIP**.

The overall objective is to design:

- a **clean surface language**,
- a **sound, non-leaking implementation** built from multiple small analyses and translators,
- an ownership / borrow / lifetime discipline that supports idiomatic programs,
- strong interaction with **type inference** and **higher-order polymorphism**, and
- a natural extension path from **O-TIP** to **O-SIP**, where O-SIP adds **sequence types**, **iteration**, and useful sequence combinators.

This is intended as a working design memo rather than a final language specification.

---

## 2. High-level design goals

### 2.1 Language goals

O-TIP should preserve the virtues of TIP:

- small core language,
- implicit type inference,
- higher-order programming,
- pedagogically transparent implementation.

O-TIP should add:

- strong memory safety without a garbage collector,
- ownership and move semantics,
- borrowing with inferred lifetimes,
- no memory leaks by construction,
- a path toward precise structural data types.

O-SIP should then add:

- first-class sequence values,
- iteration over ranges and sequences,
- reusable parametric combinators such as `apply`, `fold`, and `map`,
- eventually slices/views and perhaps more advanced sequence operations.

### 2.2 Pedagogic goals

The implementation should reinforce a central course theme:

> static semantics can be decomposed into little languages, little solvers, and little translators.

That means the compiler should avoid one giant monolithic “advanced type system” implementation whenever possible. Instead, it should use several clean phases that separately:

1. generate constraints,
2. solve constraints,
3. interpret solutions.

This should remain true even when ownership, borrowing, polymorphism, and sequences are added.

---

## 3. Core design philosophy

### 3.1 Separate surface simplicity from internal richness

The source language should stay relatively small and annotation-light.

Internally, however, the compiler may track richer information, including:

- ordinary type structure,
- ownership class,
- borrow relationships,
- lifetime / region information,
- move validity,
- destruction obligations,
- sequence-view origins.

This allows the language to remain readable and teachable while still exposing modern statically enforced memory safety under the hood.

### 3.2 Distinguish type shape from resource discipline

A key design principle is to separate:

- **type shape** — e.g. `int`, function type, record type, sequence type,
- **resource discipline** — e.g. copyable vs owning vs borrowed,
- **program-point state** — e.g. owned / moved / borrowed,
- **control-flow-sensitive borrow validity**.

This separation is what keeps the system modular and pedagogically transparent.

### 3.3 Favor staged complexity

The design should support a staged path:

- O-TIP v1: ownership, moves, read-only borrows, implicit inference.
- O-SIP v1: owned sequences, read-only iteration, combinators over borrowed input and owned output.
- O-SIP v2: slices / borrowed views.
- O-SIP v3: exclusive mutable sequence borrows for in-place algorithms.

---

## 4. Proposed O-TIP core

### 4.1 Copy vs Own split

A foundational decision is that not all values should be treated as affine/owning.

Instead, O-TIP should distinguish at least:

- **Copy** values: e.g. `int`, `bool`, perhaps `unit`,
- **Own** values: heap-owning references, closure values with owning environments, and future resource-carrying data types.

This gives the most practical and extensible design:

- assignment of a Copy value copies,
- assignment of an Own value moves,
- use-after-move applies only to moved owning values.

This avoids the awkwardness of making *every* value affine while still keeping ownership central where it matters.

### 4.2 Ownership and move semantics

For owning values:

- each heap allocation has exactly one owner,
- assignment transfers ownership,
- moved-from variables become invalid,
- when the final owner exits scope, destruction is inserted automatically.

This gives a straightforward static memory-safety story and eliminates leaks by construction.

### 4.3 Borrowing model

For O-TIP v1, borrowing should be:

- explicit in syntax (for example `&x`),
- **read-only** only,
- inferred in lifetime/region internally,
- free of explicit lifetime syntax in the source language.

This keeps the initial ownership model much simpler than a full Rust clone.

### 4.4 Lifetimes in the surface language

Lifetimes should not be explicit in the initial source language.

Instead:

- lifetimes are inferred internally,
- diagnostics and internal representations may show them,
- but ordinary source programs should not require lifetime annotations.

### 4.5 Structural types

O-TIP should eventually support precise structural data types rather than TIP’s current record approximations.

Recommended structural core:

- **product types** (records / tuples),
- **sum types** (tagged unions / options),
- ordinary functions,
- references / owning heap cells.

These types provide a principled basis for later extensions, including sequences, strings, and vectors.

### 4.6 Higher-order functions and polymorphism

Higher-order functions remain central.

Recommended principle:

- preserve **implicit polymorphism** where possible,
- prefer **implicit generalization of eligible non-recursive globals**,
- instantiate those generalized schemas at call sites.

This is cleaner than requiring programmers to mark helpers like `apply` or `fold` with a `poly` keyword.

### 4.7 Recommended initial restriction on polymorphism

Generalize automatically for:

- top-level, non-recursive global functions,
- singleton non-recursive SCCs in the call graph.

Do not initially generalize:

- recursive functions,
- mutually recursive groups,
- local closures (unless later design work justifies it).

This matches the existing implementation strategy well while keeping the surface language cleaner.

---

## 5. Multiple little passes under the hood

A central implementation goal is to keep the system as a composition of small analyses.

### Pass A — Type-shape inference

Infer ordinary type structure:

- base types,
- function types,
- product/sum types,
- sequence and slice types (later),
- reference constructors.

This should remain as close as possible to the familiar HM-style equality-constraint architecture:

- generate equality constraints,
- solve with unification / union-find,
- record inferred type shapes.

### Pass B — Generalization / instantiation for eligible globals

For eligible non-recursive globals:

- compute an abstract type schema,
- record it in semantic state,
- instantiate fresh copies at call sites.

This pass is conceptually separate from ordinary unification even if it uses the same inferred type structures.

### Pass C — Ownership classification

Classify solved types as, for example:

- Copy,
- Own,
- Borrowed view (later).

This can be implemented as a small monotone fixed-point or structural classification pass over inferred types.

### Pass D — Move/state analysis

For variables whose types are owning:

- track ownership state across control flow,
- reject use-after-move,
- identify which variables still own resources at scope exits.

This is naturally a dataflow-style analysis.

### Pass E — Borrow/lifetime validity

For borrowed values:

- ensure owners remain live while borrows are valid,
- ensure moved values are not moved while borrowed,
- eventually support local liveness-based borrow regions.

This is best viewed as a separate borrow-validity analysis rather than part of the ordinary type unifier.

### Pass F — Destruction insertion

Once ownership state is known:

- insert destruction/free operations at the correct scope exits or control-flow points,
- avoid double-free by consulting move analysis results.

This should be framed as interpretation of prior analyses, not as part of type inference itself.

---

## 6. O-SIP extension direction

### 6.1 Why O-SIP matters

The SIP examples show that the real expressive power comes from combining:

- sequence values,
- iteration,
- higher-order combinators,
- polymorphism.

The design challenge is to add those cleanly while respecting ownership and borrowing.

### 6.2 Recommended sequence model

Introduce at least two sequence-related forms:

1. **Owned sequence**
   - heap-allocated sequence value that owns its storage,
   - length-carrying,
   - movable as an owning value.

2. **Borrowed read-only slice/view**
   - non-owning view into some contiguous portion of a sequence,
   - tied to the owner by an inferred lifetime/region,
   - suitable for read-only combinators and traversal.

A third form can come later:

3. **Borrowed exclusive mutable slice**
   - a unique mutable view for in-place algorithms.

### 6.3 O-SIP v1: keep what is easy and powerful

The first O-SIP version should prioritize patterns that fit well with ownership and read-only borrowing:

- `for (e : s)` iterator-style loops,
- range loops like `for (i : lo .. hi by step)`,
- `fold` over borrowed input returning a scalar,
- `map` over borrowed input returning a fresh owned sequence,
- sequence construction from literals or computed forms.

These patterns do **not** require full view-returning borrow machinery.

### 6.4 O-SIP v2: first-class slices/views

To support useful sequence views, O-SIP should later add:

- first-class read-only slice values,
- local slice variables,
- borrow liveness based on last use rather than just lexical scope,
- support for returning a borrowed view **derived from exactly one borrowed input**.

This “single-source returned borrow” rule appears to be a strong compromise:

- powerful enough for `subseq`, `take`, `drop`, `head`, etc.,
- but much simpler than full Rust lifetime polymorphism.

### 6.5 O-SIP v3: exclusive mutable sequence borrows

In-place algorithms such as sorting, partitioning, or updating a destination matrix naturally want exclusive mutable views.

That suggests a later stage with:

- unique mutable slices,
- stronger alias restrictions,
- explicit analysis for mutation safety.

This should probably be delayed until after read-only slices are understood.

---

## 7. What coding patterns should work naturally?

### 7.1 Patterns that should work well in O-SIP v1

These fit well with owned sequences and read-only borrowing:

- `fold`
- `map` returning a fresh owned sequence
- `sum`
- `all`, `any`, `count`
- iterator-based read-only traversals
- range-driven numeric loops

These patterns stress polymorphism and iteration, but not escaping borrows.

### 7.2 Patterns that need slices/views

These want borrowed views as first-class values:

- `subseq(s, i, j)`
- `take(s, n)`
- `drop(s, n)`
- `head(s)` returning a view
- non-copying string / sequence slices
- windows / chunks

These require slice values plus a borrow-origin discipline.

### 7.3 Patterns that likely want mutable/exclusive borrows

These stress in-place mutation and alias control:

- insertion sort in place,
- in-place reversal,
- destination-buffer matrix multiplication,
- mutable subrange updates.

Initially these can either be delayed or expressed by moving ownership in and returning ownership out.

---

## 8. Recommended surface-language principles

### 8.1 Keep the syntax clean

The surface language should avoid unnecessary annotations wherever possible.

That suggests:

- no explicit lifetimes in source,
- no explicit polymorphism marker for ordinary eligible globals,
- borrowing visible only where semantically important,
- sequence and iteration syntax that is concise and regular.

### 8.2 Prefer explicitness only where it carries semantic weight

Good places for explicit syntax:

- `&x` to indicate borrowing,
- sequence literals and constructors,
- perhaps later syntax for mutable/exclusive sequence borrows.

Less desirable places for explicit syntax:

- marking obviously polymorphic helper functions with `poly`,
- writing explicit lifetime parameters in ordinary source programs.

### 8.3 Preserve the feel of “modern static safety without ceremony”

The language should feel more like:

- a small, principled, safe language,

than like:

- a rough pedagogic prototype with implementation details leaking into the syntax.

---

## 9. Provisional design recommendations

### Recommendation 1
Adopt **implicit generalization for eligible non-recursive globals**.

### Recommendation 2
Use a **Copy / Own split** rather than making every value affine.

### Recommendation 3
Keep **borrows read-only in O-TIP v1**.

### Recommendation 4
Introduce **owned sequences** early in O-SIP.

### Recommendation 5
Prioritize **borrowed-input / owned-output combinators** (`fold`, `map`) before view-returning sequence functions.

### Recommendation 6
Add **read-only slices** once local borrow-liveness analysis is in place.

### Recommendation 7
Delay **exclusive mutable slices** until the read-only view model is understood.

### Recommendation 8
Maintain the compiler as a composition of small passes:

- type shape,
- schema generalization,
- ownership classification,
- move analysis,
- borrow validity,
- destruction insertion,
- later: slice-origin analysis.

---

## 10. Open design questions

Below are the most important unresolved questions, each paired with a concrete example.

### Q1. Should polymorphism be fully implicit for eligible non-recursive globals?

#### Example
```tip
apply(f, v) {
  return f(v);
}

inc(x) { return x + 1; }
get(p) { return *p; }

main() {
  var a, b;
  a = apply(inc, 13);
  b = apply(get, alloc 42);
  return a + b;
}
```

**Issue raised:**
Should `apply` be generalized automatically because it is a non-recursive global, or should some source annotation still control that behavior?

---

### Q2. What exactly counts as Copy vs Own?

#### Example
```tip
main() {
  var x, y, p, q;
  x = 1;
  y = x;         // copy?
  p = alloc 42;
  q = p;         // move?
  return y + *q;
}
```

**Issue raised:**
Which values should be freely copied, and which values should participate in move semantics?

---

### Q3. Should closures be part of the initial higher-order design?

#### Example
```tip
makeAdder(n) {
  return lambda(x) { return x + n; };
}
```

**Issue raised:**
Do we initially support only named global functions as higher-order values, or also closures with captured environments? If closures exist, how are captured owning values handled?

---

### Q4. How much of borrowing should be visible in the source language?

#### Example
```tip
inspect(p) {
  return *p;
}

main() {
  var p;
  p = alloc 10;
  return inspect(&p);
}
```

**Issue raised:**
Is `&` the only source-level marker we want initially, with all lifetime details inferred internally?

---

### Q5. Should O-SIP v1 include slices/views, or only owned sequences?

#### Example
```tip
subseq(s, i, j) {
  return ???
}
```

Possible uses:
```tip
x = subseq(a, 2, 5);
y = fold(x, 0, sum);
```

**Issue raised:**
If slices are deferred, many non-copying sequence idioms are awkward. If slices are included, borrow/lifetime analysis becomes more sophisticated.

---

### Q6. If slices are included, should returned borrows be limited to a single source input?

#### Example
```tip
head(s) {
  return subseq(s, 0, 1);
}
```

versus

```tip
pick(b, s1, s2) {
  return b ? s1 : s2;
}
```

**Issue raised:**
Is it enough in v1/v2 to allow returned borrowed views only when they are clearly derived from exactly one borrowed input?

---

### Q7. Should local borrow lifetimes be lexical or use-based?

#### Example
```tip
main() {
  var s, t, x;
  s = allocSeq(...);
  t = subseq(s, 0, 10);
  x = fold(t, 0, sum);
  // should the borrow of s be over here?
  return length(s);
}
```

**Issue raised:**
If borrow lifetime is purely lexical, `s` may appear borrowed longer than necessary. If borrow lifetime ends at last use of `t`, the language becomes more ergonomic but the borrow checker becomes more CFG-sensitive.

---

### Q8. When do we add exclusive mutable borrows?

#### Example
```tip
sort(a) {
  // in-place sort
}
```

Possible styles:
```tip
b = sort(a);      // move in / return owned result
sort(&mut a);     // exclusive mutable borrow style
```

**Issue raised:**
Should O-SIP initially force move-in / move-out style for mutating algorithms, or should it introduce exclusive mutable sequence borrows early?

---

### Q9. How should sequence iteration type element access?

#### Example
```tip
fold(a, init, f) {
  var s, e;
  s = init;
  for (e : a) {
    s = f(s, e);
  }
  return s;
}
```

**Issue raised:**
Does iteration bind `e` as:
- a copied element value,
- a borrowed element view,
- or something type-directed based on element ownership class?

---

### Q10. What is the smallest useful O-SIP core?

#### Example candidate feature set
- owned sequence type,
- `#s` length,
- indexing,
- sequence literals / computed constructors,
- iterator-style `for (e : s)`,
- `fold` and `map`,
- no slices yet.

**Issue raised:**
Is this already enough to demonstrate the desired combinator-style power, or do slices need to be in the first O-SIP design to make the language feel convincing?

---

## 11. Suggested next steps

A practical next step would be to turn this memo into:

1. a **design decision record** with explicit choices and rationale,
2. a **surface syntax sketch** for O-TIP and O-SIP,
3. a **pass-by-pass compiler architecture note**, and
4. a small set of **worked examples** showing TIP → O-TIP → O-SIP evolution.

---

## 12. Closing thought

The design space now looks encouraging.

The key opportunity is that much of the desired expressive power appears to come from a sweet spot where:

- polymorphism is implicit for eligible globals,
- sequences are owned values,
- iteration borrows read-only views,
- and combinators mostly consume borrowed input and return owned output.

That sweet spot appears rich enough to make O-SIP genuinely expressive *before* taking on the full complexity of general borrowed views and mutable slice borrowing.

That is likely the best path to a language that feels both modern and teachable.
