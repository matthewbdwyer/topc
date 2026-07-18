# Git Branch Status Analysis

Date: 2026-07-17
Repository: topc

## Executive Summary

The current working branch is product-types.

The branch product-types contains the recent CFG and return-lifting work, but it is not a small feature branch on top of main. It is ahead of main by 10 commits, including broader product-type and SOPC-related changes.

The branch otip-language-bootstrap is a separate historical migration branch. It is ahead of main by 32 commits and behind main by 1 commit. It appears to contain an older TIP to TOP rename and bootstrap migration stream.

## Snapshot Of Branches

Local branches:
- main at eefdcb4, tracking origin/main
- product-types at 4107ba8, tracking origin/product-types

Remote branches:
- origin/main at eefdcb4
- origin/product-types at 4107ba8
- origin/otip-language-bootstrap at fcc5b67

Current branch:
- product-types

## Divergence Metrics

Compared to origin/main:
- origin/product-types: 0 behind, 10 ahead
- origin/otip-language-bootstrap: 1 behind, 32 ahead

Interpretation:
- product-types contains a stack of 10 commits not on main.
- otip-language-bootstrap is heavily diverged from main and appears to be an older migration history.

## Commits Unique To product-types Relative To main

1. 4107ba8 Implement source CFG phases 1-5.5 and return-lifting
2. 3bcada9 Refine SOPC migration docs and trim demo artifacts
3. 0d4d62c Enrich diagnostic options to show analysis results and analysis constraints
4. 57936ea feat(codegen): nested pattern destructuring, ownership in bindings, wildcard destruction (Phase B4)
5. 2dbdbd1 feat(semantic): exhaustiveness and redundancy checking for nested patterns (Phase B3)
6. f148364 feat(semantic): nested pattern type checking (Phase B2)
7. 4a49b94 feat(frontend): pattern grammar, AST hierarchy, and pretty-printer (Phase B1)
8. 4d453fa Phase A: add product/record types end-to-end
9. 59a24b2 docs: add SOP language guide, implementation notes, and phased upgrade plans
10. 2d2b535 Add SIPC planning doc for ADT-complete TOP and SOP scope

## Characterization Of otip-language-bootstrap

Recent history indicates this branch is a broad TIP to TOP migration stream, including:
- large-scale renaming from TIP identifiers to TOP identifiers
- grammar and test file renames
- runtime and ABI rename work
- documentation rewrites
- ownership and borrow related evolution

This branch does not look like a focused continuation of current CFG work.

## Current Working Tree Notes

Unstaged local deletions are present:
- docs/new-sipc-plan.md
- docs/topc-upgrade-phaseA-plan.md
- docs/topc-upgrade-phaseB-plan.md

These are not included in the CFG commit and should be triaged intentionally before any branch surgery.

## Risk Assessment

If product-types is merged directly into main, all 10 commits above will come in together, not only CFG and plan updates.

If otip-language-bootstrap is merged directly into main, a very large historical migration stack will be introduced, with high conflict and regression risk.

## Suggested Repair Paths

If only CFG and return-lifting should land on main:
1. Create a fresh branch from main.
2. Cherry-pick 4107ba8.
3. Resolve conflicts and run full test gates.
4. Open PR from that clean branch.

If product-types is intended as the canonical integration stream:
1. Keep building on product-types.
2. Confirm all 10 ahead commits are desired for main.
3. Merge via PR with explicit scope review.

If otip-language-bootstrap has useful remnants:
1. Do not merge wholesale.
2. Isolate required commits by topic and cherry-pick selectively.

## Bottom Line

As of this snapshot, the CFG and plan work is safely present on product-types and remote origin/product-types. The key triage decision is whether main should receive only the CFG commit or the full product-types stack.