# TOP / SOP Design Consolidation Draft

## 1. Purpose

This document consolidates the main design principles that emerged from the discussion about extending TIP to **TOP** and then to **SOP**.

The overall objective is to design:

- a **clean surface language**,
- a **sound, non-leaking implementation** built from multiple small analyses and translators,
- an ownership / borrow / lifetime discipline that supports idiomatic programs,
- strong interaction with **type inference** and **higher-order polymorphism**, and
- a natural extension path from **TOP** to **SOP**, where SOP adds **sequence types**, **iteration**, and useful sequence combinators.

This is intended as a working design memo rather than a final language specification.

---

## 2. High-level design goals

### 2.1 Language goals

TOP should preserve the virtues of TIP:

- small core language,
- implicit type inference,
- higher-order programming,
- pedagogically transparent implementation.

TOP should add:

- strong memory safety without a garbage collector,
- ownership and move semantics,
- borrowing with inferred lifetimes,
- no memory leaks by construction,
- a path toward precise structural data types.

SOP should then add:

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

- TOP v1: ownership, moves, read-only borrows, implicit inference.
- SOP v1: owned sequences, read-only iteration, combinators over borrowed input and owned output.
- SOP v2: slices / borrowed views.
- SOP v3: exclusive mutable sequence borrows for in-place algorithms.

---

## 4. Proposed TOP core

### 4.1 Copy vs Own split

A foundational decision is that not all values should be treated as affine/owning.

Instead, TOP should distinguish at least:

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

For TOP v1, borrowing should be:

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

TOP should eventually support precise structural data types rather than TIP’s current record approximations.

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

## 6. SOP extension direction

### 6.1 Why SOP matters

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

### 6.3 SOP v1: keep what is easy and powerful

The first SOP version should prioritize patterns that fit well with ownership and read-only borrowing:

- `for (e : s)` iterator-style loops,
- range loops like `for (i : lo .. hi by step)`,
- `fold` over borrowed input returning a scalar,
- `map` over borrowed input returning a fresh owned sequence,
- sequence construction from literals or computed forms.

These patterns do **not** require full view-returning borrow machinery.

### 6.4 SOP v2: first-class slices/views

To support useful sequence views, SOP should later add:

- first-class read-only slice values,
- local slice variables,
- borrow liveness based on last use rather than just lexical scope,
- support for returning a borrowed view **derived from exactly one borrowed input**.

This “single-source returned borrow” rule appears to be a strong compromise:

- powerful enough for `subseq`, `take`, `drop`, `head`, etc.,
- but much simpler than full Rust lifetime polymorphism.

### 6.5 SOP v3: exclusive mutable sequence borrows

In-place algorithms such as sorting, partitioning, or updating a destination matrix naturally want exclusive mutable views.

That suggests a later stage with:

- unique mutable slices,
- stronger alias restrictions,
- explicit analysis for mutation safety.

This should probably be delayed until after read-only slices are understood.

---

## 7. What coding patterns should work naturally?

### 7.1 Patterns that should work well in SOP v1

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
Keep **borrows read-only in TOP v1**.

### Recommendation 4
Introduce **owned sequences** early in SOP.

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

ANSWER: I prefer automatic generalization to keep the syntax clean.  If this is not possible I want minimal annotation.

FOLLOWUP: The plan is to auto-generalize all singleton non-recursive SCCs; no source annotation is needed for the common case.  The only residual decision is what to do with the existing `KPOLY` keyword.  Options: (a) remove it entirely from the grammar — clean, but requires stripping `poly` from existing test programs such as `polyfactorial.tip`, `polyfun.tip`, `polyprog.tip`; (b) silently accept and ignore it during a transition period.  Recommendation: remove entirely for a cleaner language.

**FQ1:** Should `KPOLY` be **removed entirely** from the grammar, or **silently ignored** for a transition period before removal?

ANSWER: remove it entirely

FOLLOWUP: Confirmed.  Scheduling note: `KPOLY` can remain parseable through Phases 1–6 so the existing test files run unchanged, then the keyword is removed in **Phase 7** alongside the auto-generalization implementation.  This makes the removal a single contained commit: the existing `polyfactorial.tip`, `polyfun.tip`, and `polyprog.tip` selftests, once stripped of the `poly` annotation, become regression tests that validate auto-generalization produces identical behavior.

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

ANSWER: Values that can be efficiently copied, e.g., int, bool, etc., can be copied, but anything deeper like a heap cell, array, tuple, etc. should be owned.  I want this boundary to align with Rust.

FOLLOWUP: This aligns well with the planned ownership classifier.  Two edge cases need a ruling:

1. **Record literals** (`{id: 1, balance: 100}`) do not use `alloc` at the source level but represent structured heap values.  Simplest rule: records are **always Own**, consistent with "anything deeper is owned."  Alternative: records are Copy when all fields are Copy (Rust's structural rule, requiring an explicit opt-in).  The simpler always-Own rule is recommended.
2. **Function values** — in TOP v1 there are no closures, so all higher-order values are pointers to named globals, which are naturally **Copy** (like Rust function pointers).

**FQ2a:** Are record literals **always Own**, or **Copy when all fields are Copy**?

ANSWER: always Own.  So in effect a record literal is an implicit heap allocation of owned data.

FOLLOWUP: Confirmed.  Codegen implication: TIP's existing record codegen may currently produce stack-allocated struct values.  Since records are now always Own (heap-allocated), record literal codegen must emit a `malloc`-style allocation.  The right place to make this change is **Phase 11** (destruction insertion and codegen rework), when owned-allocation handling is already being extended.  Phases 1–10 treat records as Own in the type system without touching codegen.

**FQ2b:** Confirmed that **function values are Copy** in TOP v1 (no closures, function value = plain function pointer)?

ANSWER: yes.  

FOLLOWUP: Confirmed.  No further follow-up needed.

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

ANSWER: What complexities does adding closures entail?  If it is too complicated then we can go with globals.

FOLLOWUP: TIP already has no lambda syntax — all functions are named globals, so closures would be a wholly new feature.  The complexities are: (1) new grammar for anonymous function expressions; (2) heap-allocated environment structs (the closure is a fat pointer — function pointer plus data pointer — and the struct is Own if it captures any Own variable); (3) three-tier capture semantics analogous to Rust's `Fn` / `FnMut` / `FnOnce` to handle Copy captures, borrowed captures, and owned captures respectively; (4) each closure instance has a unique structural type, significantly complicating the type system and auto-generalization; (5) local closures cannot be auto-generalized (Phase 7 handles only eligible globals).  This complexity is disproportionate to the pedagogic gain, since named-global higher-order programming already covers `apply`, `fold`, `map`, and `compose` cleanly.

**FQ3:** Should closures be **explicitly out of scope** for all phases (TOP through SOP v3), with all higher-order programming through named global functions?

ANSWER: Yes let's push this as a potential feature for SOP

FOLLOWUP: Confirmed out of scope for all main phases.  Closures can be offered as an **open-ended final project extension track** alongside slices and `&mut`, suitable for students who want a more type-system-focused challenge.  All course-phase higher-order programming uses named global functions.

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

ANSWER: Ideally yes.  What are the challenges?

FOLLOWUP: The challenges of fully implicit lifetimes are: (1) the compiler must carry region/lifetime variables internally alongside type variables, even though they never appear in source; (2) lifetime constraints are ordering constraints (`r1 outlives r2`), not equality constraints — they require a separate solver beyond the existing equality unifier; (3) error messages must translate region violations back to source variable names and locations without exposing internal region variable names; (4) use-based lifetimes (confirmed in Q7) require per-function CFG liveness analysis, which the current compiler does not have as a standalone data structure.  For TOP v1, a pragmatic restriction removes most of this complexity: allow borrows **only as immediately-passed function arguments** — `f(&x)` is legal; `var b; b = &x;` is not.  Under this restriction the lifetime of every borrow equals the duration of its enclosing function call, trivially checkable without a CFG or region solver.  Stored borrow variables can be enabled once the per-function CFG is available.

**FQ4:** For TOP v1, are you comfortable restricting borrows to **immediately-passed function arguments only** (`f(&x)` legal; `var b; b = &x;` not), deferring stored-borrow variables to the phase when the per-function CFG is available?

ANSWER: Yes I think this is nice point in the design space that adds expressive power and ergonomics without increasing complexity.

FOLLOWUP: Confirmed.  This raises one grammar implication: to declare a borrow parameter at the definition site, `&IDENTIFIER` must be a valid parameter form.  The current grammar has `nameDeclaration : IDENTIFIER`; it must be extended so `inspect(&p) { return *p; }` is valid, with `&p` in the parameter list meaning `p : BorrowRef(T)`.  This change is consistent with the FQ9b answer and should be a single Phase 1 grammar addition.

**FFQ4:** Should borrow parameters be declared with `&` in the function definition parameter list (e.g., `inspect(&p) { ... }` declares `p : BorrowRef(T)`), making the borrow contract visible and self-documenting at the definition site, consistent with call-site syntax `inspect(&x)`?

ANSWER: Is this how Rust documents the borrow contract?  If so then let's do it this way.

FOLLOWUP: Confirmed — Rust uses exactly this convention: `fn inspect(p: &Point)` at the definition site.  In TOP the equivalent is `inspect(&p) { return *p; }` where `&p` in the parameter list declares `p : BorrowRef(T)`.  Decision locked for Phase 1: `&IDENTIFIER` parameter declarations are a first-class TOP grammar element, added alongside other TOP syntax in Phase 1.

---

### Q5. Should SOP v1 include slices/views, or only owned sequences?

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

ANSWER: How much more complicated does it become beyound TOP?  This is a tradeoff to navigate based on complexity for students building SOP.  We could defer slices to a final project phase that is more challenging, i.e., build out SOP then add slices at the end.

FOLLOWUP: Concrete complexity estimate:

- **SOP v1 without slices:** one new type term `TipSeq(T)` (owned sequence); for-loop codegen over a heap-allocated `{ int length; T* data }` struct; `subseq` can be written as a copying SOP function if needed.  Compiler complexity delta from TOP: **medium**.
- **SOP v2 with slices:** new type term `TipSlice(T)` carrying a lifetime/region parameter; subtyping coercion from `Seq(T)` to `Slice(T)`; slice-origin tracking analysis; single-source returned borrow rule enforcement in the borrow checker.  Compiler complexity delta from SOP v1: **high** — a new type dimension plus a new analysis pass.

Deferring slices to a final project phase is the right call.  SOP v1 without slices supports `fold`, `map`, `sum`, `all`, `any`, `count`, and range loops — a pedagogically rich and self-contained feature set.

**FQ5:** Without slices, `subseq(s, i, j)` must allocate and copy data.  Should `subseq` be **absent from SOP v1** entirely, or present as a **copying operation** with a note that the zero-copy slice version arrives in SOP v2?

ANSWER: 

I think defering slices to a final project is the right choice.  About subseq is that something that we write as library code or is it core to the language?  I think upgrading it for the final project to be zero-copy is fine and that we can include it for convenience.  Should we have a convenience syntax like s[i:j]  and s[i:] and s[:j]

FOLLOWUP: `subseq` as a named TOP global (library code, not a compiler primitive) is the right choice.  The `s[i:j]`, `s[i:]`, `s[:j]` syntax is a clean addition with a natural upgrade path: the same syntax desugars to the copying `subseq` in SOP v1 and to a zero-copy `Slice(T)` in the final project.  Three questions this raises:

**FFQ5a:** Should element indexing `s[i]` (returning a single element — type `T` for Copy elements, `&T` for Own elements) be added to the grammar alongside `s[i:j]`?  Both fit naturally into a single subscript grammar rule.

ANSWER: Yes indexing s[i] is idiomatic so let's extend the subscript grammar rule

FOLLOWUP: Confirmed.  `s[i]`, `s[i:j]`, `s[i:]`, and `s[:j]` are all handled by a unified subscript grammar rule.  Locked.

**FFQ5b:** For the upgrade path: in SOP v1, `s[i:j]` is sugar for `subseq(s, i, j)` and returns a new `Seq(T)` (allocates and copies); in the final project, `s[i:j]` returns a `Slice(T)` (zero-copy borrow).  Should the same syntax silently change semantics across phases, or should the borrow/slice form require an explicit `&s[i:j]` in the final project to distinguish it from the copying form?

ANSWER: How does rust distinguish these?  let's use that syntactic form

FOLLOWUP: In Rust there is only one range-index syntax: `&s[i..j]` always borrows (produces `&[T]`).  There is no dedicated "copy this subarray" syntax — you write `s[i..j].to_vec()` to obtain an owned copy.  The Rust-inspired form for TOP is therefore: **`s[i:j]` is the uniform syntax** — in SOP v1 it desugars to the copying `subseq(s, i, j)` (because `Slice(T)` does not yet exist); in the final project phase it returns a zero-copy `Slice(T)`.  The surface syntax never changes; only the type it produces evolves across phases.  No second "borrow-form" needed.  Decision locked.

**FFQ5c:** With `s[i:j]` subscript syntax in the language, is `#s` still the preferred length operator, or would `s.len` be more consistent with the indexing notation?

ANSWER: I was using "." in TIP as the field access operator so I chose "#".  I guess we have "." for both sum/product type variants in TOP, so perhaps not overload with attributes like len.  Thoughts?

FOLLOWUP: Keep `#s`.  Your reasoning is correct: `.` in TOP is already used for record field access and sum-type variant testing; adding `.len` as a method-like attribute would require the parser and type checker to distinguish field access from attribute lookup — unnecessary complexity.  `#s` is established, unambiguous, and requires no new dispatch mechanism.  A named function `len(s)` is also viable if `#s` ever reads awkwardly, but there is no compelling reason to change.  Decision: **keep `#s`**.

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

ANSWER: If this simplifies the system without significantly reducing expressiveness, then I'd go with one borrowed input.

FOLLOWUP: The main expressiveness cost of the single-source rule is the `pick(b, s1, s2)` pattern — conditionally returning one of two borrowed views.  Under the single-source rule this cannot be expressed without copying a slice.  Rust handles `pick` via lifetime polymorphism (`fn pick<'a>(b: bool, s1: &'a T, s2: &'a T) -> &'a T`); TOP's no-explicit-lifetimes principle would require inferring that both inputs share a lifetime at call sites — precisely the additional region-solver machinery that makes SOP v2 more complex.  For the initial slice phase the `pick` limitation is acceptable; implicit single-lifetime unification for multi-source returned borrows could be an optional SOP v2 extension if the worked examples motivate it.

**FQ6:** Is the `pick` limitation (conditional slice selection requires copying under the single-source rule) acceptable for SOP v2, or should SOP v2 also add **implicit single-lifetime unification** so that `pick` works without explicit annotations?

ANSWER: If we go with the simpler approach what are the ergonomic/expressiveness implications?  What standard algorithmic fragments become awkward or impossible.

FOLLOWUP: Under the single-source rule, the following common patterns work as zero-copy slices: `head(s)` → `s[0:1]`, `tail(s)` → `s[1:]`, `take(s,n)` → `s[0:n]`, `drop(s,n)` → `s[n:]`, any fixed-width window `s[i:i+k]` — all derived from a single sequence input.  Patterns that **require copying**: (1) `pick(b, s1, s2)` — conditional selection between two distinct sequences; (2) `longer(s1, s2)` — returning a view of whichever input is longer; (3) any pattern where the returned view might originate from more than one allocation.  For numeric and sequence-processing examples, `pick`-style patterns are uncommon.  The `head`, `tail`, `take`, `drop`, `subseq`, and windowing patterns — which cover the vast majority of typical SOP algorithms — all work without restriction.

**FFQ6:** Given this analysis, is the single-source rule confirmed as the borrow rule for the SOP v2 / final project slice phase, with the understanding that `pick` across two sequences must copy in that phase?

ANSWER: Yes let's lock this in.

FOLLOWUP: Single-source rule locked in as the borrow rule for the SOP v2 / final project slice phase.  `pick`-style patterns across two sequences must copy; all linear single-derivation patterns (`head`, `tail`, `take`, `drop`, windowing) work zero-copy.

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

ANSWER: I am planning on providing the borrow checker and we already have CFA analysis with control flow graphs, so I thin kwe can go with use based.  Let me know if that is too complicated.  Note also that we may have to refactor the CFG to be a standalone representation in the compiler.

FOLLOWUP: Use-based lifetimes are feasible and the right choice — not too complicated.  The current tipc compiler has a `CallGraph` for inter-procedural CFA but no per-function control-flow graph for intra-procedural dataflow, which both move analysis (Pass D) and the borrow checker (Pass E) need.  The good news: TOP's control flow is **fully structured** (only `if/else` and `while`, no `goto`, `break`, `continue`, or exceptions), so the per-function CFG is deterministic from the AST and can be built with a simple recursive traversal.  No SSA or dominator trees are needed; a basic-block list with successor edges suffices for liveness and ownership dataflow.  This is a well-scoped Phase 9a milestone.  With the CFG in place, use-based liveness is straightforward.

**FQ7a:** Should Phase 9a (per-function CFG construction as a standalone data structure) be **explicitly added** as a prerequisite milestone in the dev-plan before move analysis?  (It is currently implicit inside Phase 9.)

ANSWER: Yes let's make it an explicit component/subsystem and include tests.

FOLLOWUP: Confirmed.  One storage design question:

**FFQ7a:** Should the per-function CFG be stored inside `SemanticAnalysis` (as an additional field, e.g., `std::map<ASTFunction*, CFG>` alongside the existing `CallGraph`), or as a separate named result object (e.g., `IntraproceduralCFGs`) that `SemanticAnalysis` owns?  The separate-object approach keeps `SemanticAnalysis` a clean aggregate of named, independently-testable results.

ANSWER: Let's use a separate object approach.

FOLLOWUP: Confirmed.  `IntraproceduralCFGs` will be a separate named result object stored in `SemanticAnalysis` alongside `CallGraph`, following the same pattern: constructed by a dedicated builder class, independently testable, and named clearly.  Locked.

**FQ7b:** Since you plan to provide the borrow checker: should Phase 10 (Borrow/Lifetime Validity) be designated as **instructor-provided infrastructure**, with student deliverables focused on testing and integration rather than building the checker from scratch?

ANSWER: I meant we will build the borrow checker as part of the topc compiler.  The student's work will focus on extending topc to support SOP v1 from syntax through code generation.  Then in a final project phase they will be able to add some additional functionality of their choosing, e.g., slices.

FOLLOWUP: Course structure confirmed:
- **Instructor builds (Phases 0–11):** full TOP compiler — grammar, AST, type system, ownership classification, per-function CFG, move analysis, borrow checker, and destruction insertion.
- **Student project (Phase 12):** SOP v1 — sequence type, for-loops, range expressions, `fold`, `map`, and codegen.
- **Final project (student choice):** read-only slices, `&mut`, closures, or open-ended extension.

**FFQ7b:** Should the dev-plan add an explicit **student handoff marker** between Phase 11 and Phase 12, and a **final project section** describing the three extension tracks (slices, `&mut`, open-ended)?

ANSWER: No.  I will release all of topc to students at the beginning for them to build out the core SOP support on top of.  We will build out sopc for SOP to validate that this is all "doable" and then work on some of the extensions so that we have a clear idea of the challenges and complexity involved. So our internal docs will flesh all of this out and we should make sure that we move these planning and development docs to the, to be created, sopc repo.

FOLLOWUP: Important structural clarification — the course uses a **two-repo architecture**:

- **`topc`** (this repo): instructor builds Phases 0–11, producing a complete TOP compiler.  This is released in full to students as their starting point.
- **`sopc`** (to be created): instructor's internal validation repo.  Builds SOP v1 on top of `topc` to confirm the student project is achievable, then prototypes extension tracks to calibrate final project difficulty.  The current planning docs (`dev-plan.md`, this design doc) ultimately live here.

Implication for `dev-plan.md`: it currently covers Phases 0–12 in a single plan.  It should be restructured to reflect the `topc`/`sopc` boundary.

**FFQ7c:** Should `dev-plan.md` be restructured so that Phases 0–11 are the `topc` deliverable (this repo) and Phase 12+ material (SOP v1 validation, extension track prototypes) is marked as belonging to the `sopc` repo plan?

ANSWER: Yes.  We can have an topc-dev-plan.md and an sopc-dev-plan.md The sopc repo will never be public.

FOLLOWUP: Confirmed.  The plan splits into two files across two repos:

- **`docs/topc-dev-plan.md`** (this repo, `topc`, visible to students): Phases 0–11, the TOP compiler deliverable.
- **`sopc-dev-plan.md`** (`sopc` repo, private, never public): Phase 12+ — SOP v1 validation build, extension track prototypes, and difficulty calibration notes.

This design consolidation doc (`TOP_SOP_design_consolidation.md`) also belongs in the `sopc` repo, since it contains instructor-only design rationale.

Resulting action items:
1. Rename `docs/dev-plan.md` → `docs/topc-dev-plan.md` and trim to Phases 0–11.
2. Create the `sopc` private repo and seed it with `sopc-dev-plan.md` (Phase 12+ content) and this design doc.

No further questions on this item.

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
Should SOP initially force move-in / move-out style for mutating algorithms, or should it introduce exclusive mutable sequence borrows early?

ANSWER: What are the tradeoffs?  I like &mut as long as it is not too complex.

FOLLOWUP: Tradeoffs:

- **Move-in / move-out (no `&mut`):** reuses the ownership rules already established in TOP; `a = sort(a)` works cleanly; no new type terms or borrow-checker extensions; slightly verbose for partial updates.
- **`&mut` style:** natural for in-place algorithms (`sort(&mut a)`); enables update without transferring ownership.  Requires: (1) new type term `TipMutBorrowRef(T)`; (2) an **exclusivity invariant** — at most one `&mut` borrow live at a time, and no `&` borrow may coexist with a `&mut` borrow of the same variable; (3) the borrow checker must track two borrow modes and enforce mutual exclusion; (4) error messages become significantly harder to phrase ("cannot borrow `a` as immutable because it is also borrowed as mutable").  Complexity increment ≈ the full read-only borrow checker added again.

Move-in / move-out is clean and sufficient for SOP v1 and v2.  `&mut` is the right scope for SOP v3, matching the staged plan in the design doc.

**FQ8:** Is `&mut` confirmed as **SOP v3 scope only**, with SOP v1 and v2 using move-in / move-out for mutating algorithms (e.g., `a = sort(a)` rather than `sort(&mut a)`)?

ANSWER: Is &mut orthogonal to the complexity for slices?  If so I can see it as another option for the final phase of the project extending TOP to SOP.

FOLLOWUP: Slices and `&mut` are largely **orthogonal** final project tracks.  Slices require: `TipSlice(T)` with a lifetime parameter, subtyping coercion from `Seq(T)`, and slice-origin tracking.  `&mut` requires: `TipMutBorrowRef(T)` and an exclusivity invariant in the borrow checker (no simultaneous `&` and `&mut` on the same variable).  A student can implement either without the other; both build on the shared CFG and borrow checker infrastructure but touch different parts of the type system.  Combining both (mutable slices) is the most complex option and a suitable stretch goal.

**FFQ8:** Should the final project offer three explicit tracks: (1) **read-only slices** (`TipSlice(T)`, zero-copy subsequence access); (2) **`&mut` on sequences** (in-place mutation, exclusivity invariant); (3) **open-ended** (closures, mutable slices, additional combinators, or student proposal)?

ANSWER: Yes that's the idea.  We don't have to lock any of this into the TOP and topc development except as far as it shapes the development of TOP and topc as a basis upon which which the base SOP support can eb built and then the extensions.

FOLLOWUP: Confirmed.  Three final project tracks established: read-only slices, `&mut`, open-ended.  The only constraints these place on `topc` development are design constraints, not additional implementation tasks: (1) CFG design must be general enough to underpin `&mut` analysis without rework; (2) the borrow checker pass architecture must be modular so adding a second borrow mode is incremental; (3) type term representation must accommodate `TipSlice(T)` and `TipMutBorrowRef(T)` without restructuring.  These inform the Phase 9–11 architecture.  Locked.

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

ANSWER: What does Rust do?  Borrowed elements seem to make sense to me.  Am I missing something?

FOLLOWUP: In Rust, `for e in &vec` borrows the vector and yields `&T` elements — the vector remains live after the loop.  `for e in vec` consumes the vector and yields `T` elements — the vector is invalid after the loop.  Your intuition is correct: borrowed elements are the right default.  For **Copy element types** (e.g., `int`), `&int` is transparently dereferenced and `e` is effectively just `int` — no visible difference.  For **Own element types**, `e : &T` gives read-only access without moving the element out of the sequence, which is exactly what `fold` and `map` need.  Nothing is missing from the intuition.  The remaining surface design questions are: (1) whether `for (e : s)` implicitly borrows `s` (cleaner syntax, consistent with C++ range-for) or requires an explicit `for (e : &s)` (more consistent with the `&` syntax used elsewhere in the language); and (2) for `fold(f, z, s)` written as a named TOP global, whether `s` should be a borrowed parameter (caller retains `s`) or an owned parameter (caller loses `s` after the call).

**FQ9a:** Does `for (e : s)` **implicitly borrow** `s` (leaving `s` live after the loop), or must the caller write `for (e : &s)` explicitly?

ANSWER: There is a tension between syntactic simplicity and clarity and consistency.  If we went with the explicit borrow, would the lack of & be an error? I'm not sure which is preferable.  What would be more idiomatic in a Rust-like world?  

FOLLOWUP: In Rust, `for e in &vec` is idiomatic for non-consuming iteration — the `&` is explicit and consistent with its meaning everywhere else (borrow).  For TOP, where `&` already carries explicit semantic weight as the sole borrow marker, requiring `for (e : &s)` is the more consistent and self-documenting choice.  Without `&`, `for (e : s)` on an owned sequence would be a consuming iteration (elements moved out, `s` invalid after the loop), which is almost never intended and has no useful SOP v1 semantics.  A clear type error for `for (e : s)` — with a diagnostic "did you mean `for (e : &s)`?" — is better than silent implicit borrowing.  Recommendation: **require `for (e : &s)` explicitly**.

**FFQ9a:** Confirmed: `for (e : &s)` is the required form for non-consuming iteration; `for (e : s)` on a `Seq(T)` is a type error in SOP v1?

ANSWER: Lock in this choice

FOLLOWUP: Locked.  `for (e : &s)` is the required SOP iteration form; `for (e : s)` on a `Seq(T)` is a type error with a helpful diagnostic ("did you mean `for (e : &s)`?").  No further question.

**FQ9b:** For `fold(f, z, s)` and `map(f, s)` written as named TOP globals, should `s` be a **borrowed** parameter (caller retains `s`) or an **owned** parameter (caller moves `s` into the function)?  The borrowed form is far more useful, but it means `&` appears in these functions' parameter declarations — is that acceptable in SOP v1?

ANSWER: I think it is acceptable.  Lifetime/borrowing attributes are appropriate denotations on parameters in my book.

FOLLOWUP: Confirmed.  The standard SOP library functions will declare borrowed sequence parameters explicitly, for example:

```
fold(&s, init, f) {
  var acc, e;
  acc = init;
  for (e : &s) { acc = f(acc, e); }
  return acc;
}
```

This is consistent with the FFQ4 decision (borrow declarations at the definition site).  Grammar implication: `&IDENTIFIER` as a parameter declaration and `for (e : &s)` as a loop head are both new grammar forms that should be added together in **Phase 1**, since they share the same `&` borrow semantics.

**FFQ9b:** Should `&IDENTIFIER` in function parameter declarations and `for (e : &s)` in for-loop heads both be added to the grammar in **Phase 1** alongside sum types and `case`, so borrow parameter syntax is available from the start of the TOP grammar work?

ANSWER: We will need borrow parameter syntax for the base TOP support so we have to have it in the grammar.  Correct me if I am wrong.

FOLLOWUP: You are correct.  TOP already includes borrows (`inspect(&p)` is a valid TOP v1 function), so `&IDENTIFIER` parameter declarations must be in the TOP grammar from Phase 1 — not deferred to SOP.  This is the same grammar extension as FFQ4.  Confirmed and locked: `&IDENTIFIER` parameter declarations are a **Phase 1** grammar addition.

---

### Q10. What is the smallest useful SOP core?

#### Example candidate feature set
- owned sequence type,
- `#s` length,
- indexing,
- sequence literals / computed constructors,
- iterator-style `for (e : s)`,
- `fold` and `map`,
- no slices yet.

**Issue raised:**
Is this already enough to demonstrate the desired combinator-style power, or do slices need to be in the first SOP design to make the language feel convincing?

ANSWER:  I would like to see come interesting examples written in this subset with and without slices to answer.

FOLLOWUP: This is a deliverable, not a further design decision.  A `docs/examples/` directory with representative programs can serve as both a design validation and the primary specification for the Phase 12 system test corpus.  Proposed program set:

**SOP v1 (no slices):**
- `sum.top` — sum a sequence of integers via `fold`
- `double.top` — `map` a sequence to double each element, returning a new owned sequence
- `count-pos.top` — count positive elements via `fold` with an integer accumulator
- `range-sum.top` — sum integers `1 .. 100` via range loop
- `compose.top` — demonstrate `apply` and function composition using named globals

**SOP v2 (with slices):**
- `first-half.top` — `subseq(s, 0, #s/2)` as a zero-copy read-only slice
- `sliding-max.top` — maximum over each sliding window of width 3 using slice views
- `prefix-sum.top` — prefix sums using indexed read access into a slice

**FQ10:** Should `docs/examples/` be created as an **explicit milestone before Phase 12 design is finalized**, with those programs driving the Phase 12 system test corpus?

ANSWER: Yes create them and I'll take a look.

FOLLOWUP: Action item confirmed.  The `docs/examples/` directory will be created with the SOP v1 and SOP v2 program sets listed above.  These programs serve as both a concrete specification for what SOP v1 must compile and run correctly, and as the primary Phase 12 system test corpus.  The v2 programs (with slices) additionally define the target for the final project slice track.

---

## 11. Suggested next steps

A practical next step would be to turn this memo into:

1. a **design decision record** with explicit choices and rationale,
2. a **surface syntax sketch** for TOP and SOP,
3. a **pass-by-pass compiler architecture note**, and
4. a small set of **worked examples** showing TIP → TOP → SOP evolution.

---

## 12. Closing thought

The design space now looks encouraging.

The key opportunity is that much of the desired expressive power appears to come from a sweet spot where:

- polymorphism is implicit for eligible globals,
- sequences are owned values,
- iteration borrows read-only views,
- and combinators mostly consume borrowed input and return owned output.

That sweet spot appears rich enough to make SOP genuinely expressive *before* taking on the full complexity of general borrowed views and mutable slice borrowing.

That is likely the best path to a language that feels both modern and teachable.
