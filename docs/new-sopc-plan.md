# New SOPC Plan: ADT-Complete TOP with Challenging SOP Extension

## Purpose

This document updates project direction for the next release window:

1. TOP should become a more complete algebraic-data-type language by including both sum and product types.
2. SOP should remain a high-value, non-trivial extension that teaches deep static-analysis and compiler-engineering skills.
3. The open-ended "choose your own adventure" work remains a separate concern.

This plan assumes approximately three months before student release, allowing iteration and refinement.

## Strategic Position

Adding product types to TOP is desirable and should proceed.

Why this does **not** trivialize SOP by itself:

1. Product ADTs primarily improve modeling expressiveness and language coherence.
2. SOP difficulty should come from ownership-aware sequence semantics, alias control, slice/view safety, and analysis soundness.
3. Those difficulties remain even with a richer ADT baseline.

## What Must Stay Challenging in SOP

To ensure SOP remains informative and hard (even with AI agents), scope it around semantic invariants, not parser-only feature additions.

### Required difficulty sources

1. **Ownership semantics for collections**
   - `Seq(T)` is owned and move-sensitive.
   - Recursive destruction for nested owning elements is required and tested.

2. **Borrow-aware access and iteration**
   - Distinguish Copy vs Own element behavior under indexing and loop bindings.
   - Reject unsafe iteration/use forms with diagnostics.

3. **Slice/view soundness**
   - Introduce and enforce zero-copy view semantics (`Slice(T, r)` style model).
   - Enforce single-source origin / non-escaping lifetime-style constraints.

4. **Cross-pass invariants**
   - Rules must be reflected consistently in parser/AST, symbol analysis, constraints, ownership/move analysis, and codegen.

5. **Performance-correct lowering decisions**
   - Distinguish copy-based and view-based operations explicitly.
   - Ensure tests include asymptotic/behavioral checks, not only functional output checks.

## TOP Baseline Upgrade Plan (ADT-Complete Direction)

### Phase A: Product Types as First-Class (Nominal)

1. Add product type declarations and value construction syntax.
2. Add projection and basic destructuring bindings.
3. Add `TopProductType` (or equivalent) to type terms and solver/substituter visitors.
4. Extend ownership classifier and destruction recursion over product fields.
5. Add parser/AST/pretty-printer/unit/system tests.

### Phase B: Unified Pattern Story (Sum + Product)

1. Extend pattern language to include product patterns.
2. Support nested patterns in match/case contexts.
3. Integrate product patterns into existing arity/completeness/type consistency checks.
4. Update codegen lowering for nested destructuring.

### Release guidance

If schedule pressure appears, release after Phase A and keep Phase B as internal follow-up.

## SOP Core Scope (Separate from Open-Ended Track)

The student-facing SOP core should be framed as a **semantic systems project**.

### SOP Core 1: Owned sequences

1. Native sequence type term, literals, length, indexing.
2. Owned move semantics and valid destruction behavior.
3. Clear diagnostics for illegal ownership transitions.

### SOP Core 2: Borrowed sequence programming model

1. Borrowed iteration semantics.
2. Typed behavior for copyable vs owning elements.
3. Library-level higher-order helpers (`fold`, `map`, `subseq`) under inferred polymorphism constraints.

### SOP Core 3: Zero-copy slices/views

1. Add slice type term and construction/coercion rules.
2. Add slice-origin analysis and single-source rule.
3. Ensure owner/view interactions remain sound under move/destruction analysis.

## Anti-Trivialization Rules for Assignment Design

To keep work substantial even with agent assistance, require deliverables that are hard to fake:

1. A short invariants document mapping each safety invariant to specific compiler phases/files.
2. Student-authored negative tests demonstrating rejected unsound programs.
3. Hidden tests targeting aliasing, move-after-borrow, and destruction corner cases.
4. A brief implementation note explaining at least one key tradeoff in lowering/analysis.
5. Rubric emphasis on soundness and diagnostics quality, not just feature presence.

## Student Impact Assessment

### Benefits

1. ADT-complete TOP gives cleaner conceptual grounding for later language work.
2. Students can model data naturally before tackling sequence/slice ownership complexity.
3. Project better reflects modern PL/compiler design where data modeling and memory safety interact.

### Risks

1. Larger baseline may increase onboarding time.
2. More code paths can increase debugging burden for weaker teams.
3. Without strict milestones, teams may spend too long on front-end syntax and too little on semantic soundness.

### Mitigations

1. Publish milestone gates with pass/fail criteria tied to analyses.
2. Provide minimal starter tests for each milestone.
3. Keep open-ended project requirements separate from core SOP grading.

## 3-Month Iteration Cadence

### Month 1: Baseline hardening

1. Implement product types (Phase A) in TOP.
2. Run regression and update docs/examples.
3. Freeze syntax and core semantics.

### Month 2: SOP prototype stabilization

1. Validate sequence ownership + borrow iteration.
2. Validate slice-origin analysis and negative tests.
3. Calibrate challenge level with pilot implementations.

### Month 3: Course packaging

1. Finalize assignment specification and rubric.
2. Partition public vs hidden tests.
3. Finalize release branch, tags, and student starter instructions.

## Explicit Scope Boundary

The "choose your own adventure" extension is intentionally separate from the SOP core.

Core grading should focus on required sequence/slice ownership semantics and analyses. Open-ended work should be additive and independently evaluated.

## Acceptance Criteria for This Plan

This plan is considered ready when:

1. ADT-complete TOP direction (sum + product) is accepted.
2. SOP core is defined as analysis-heavy and ownership/slice-soundness-heavy.
3. Milestones and rubric emphasize semantic correctness over surface feature count.
4. Open-ended extension remains outside required scope.
