# SOPC Migration Plan

This document is the working plan for creating the private `sopc` repository from the current `topc` codebase and selectively importing useful material from `~/sipc`.

## Goal

Create a private, instructor-only `sopc` repo that:

1. Starts from the current completed `topc` baseline.
2. Carries over the SOPC-specific internal planning and design docs.
3. Selectively reuses useful implementation ideas from `~/sipc`, especially array/sequence and runtime support.
4. Becomes the working repo for SOP validation and extension-track prototyping.

The public `topc` repo remains the student-facing baseline.

## Recommended Sequence

### 1. Freeze the `topc` baseline

Confirm the current `topc` tree is in the state you want to preserve.

At the time of this update, the intended baseline is commit `0d4d62c`
(`Enrich diagnostic options to show analysis results and analysis constraints`),
which includes the new diagnostic/inspection views and their tests.

```bash
cd /Users/matthewdwyer/topc
git status
git branch --show-current
git rev-parse --short HEAD
```

Optional checkpoint:

```bash
git tag sopc-source-baseline-2026-07-17
git branch backup-before-sopc-split
```

### 2. Create the private `sopc` repo

Create a new private repository and initialize a local working tree for it.

If you use GitHub CLI:

```bash
mkdir -p /Users/matthewdwyer/sopc
cd /Users/matthewdwyer/sopc
git init
gh repo create sopc --private --source=. --remote=origin --push
```

If you prefer to create the repo on GitHub first:

```bash
mkdir -p /Users/matthewdwyer/sopc
cd /Users/matthewdwyer/sopc
git init
git remote add origin git@github.com:YOUR_ORG_OR_USER/sopc.git
```

### 3. Seed `sopc` from `topc`

Copy the current `topc` tree into the new repo as the baseline fork.

```bash
cd /Users/matthewdwyer/sopc
rsync -a --exclude .git /Users/matthewdwyer/topc/ ./
```

After the copy, update any repo identity text so the new repo is clearly marked as private and instructor-only.

### 4. Move SOPC-only docs into `sopc`

The internal SOPC docs should live in the private repo:

- `docs/sopc-dev-plan.md`
- `docs/sopc-implementation-notes.md`
- `docs/design/TOP_SOP_design_consolidation.md`
- `docs/new-sopc-plan.md`

Copy them into the new repo:

```bash
cd /Users/matthewdwyer/sopc
mkdir -p docs/design
cp /Users/matthewdwyer/topc/docs/sopc-dev-plan.md docs/
cp /Users/matthewdwyer/topc/docs/sopc-implementation-notes.md docs/
cp /Users/matthewdwyer/topc/docs/new-sopc-plan.md docs/
cp /Users/matthewdwyer/topc/docs/design/TOP_SOP_design_consolidation.md docs/design/
```

If you want those docs removed from `topc` later, do that only after the private repo is confirmed working.

### 5. Inspect `~/sipc` for reusable pieces

Treat `~/sipc` as a reference source, not a wholesale import.

```bash
cd ~/sipc
git status
find . -maxdepth 2 -type f | sort | head -200
```

Then compare the trees before copying anything over:

```bash
diff -ruN --brief /Users/matthewdwyer/topc ~/sipc > /tmp/topc-vs-sipc.diff || true
```

Likely reusable areas include:

- sequence or array syntax support
- runtime allocation and bounds-check helpers
- destruction or lowering patterns that still fit the current compiler architecture
- tests that exercise sequence semantics or useful extension behavior

Copy only the pieces that still align with the modern `topc` codebase.

### 6. Make the repo identity explicit

Add a short private-repo README or top-level note in `sopc` that states:

- the repo is private and instructor-only
- it is seeded from `topc`
- it exists for SOP validation and prototype work
- the public student baseline remains `topc`

### 7. Commit and push the initial snapshot

Once the tree is in place and builds cleanly enough to serve as a baseline fork, create the first commit.

```bash
cd /Users/matthewdwyer/sopc
git add -A
git commit -m "Seed private sopc repo from topc baseline"
git push -u origin main
```

If the default branch is not `main`, use your actual branch name.

## Suggested Follow-On Work

After the repo exists, use this order for implementation work:

1. Verify the copied `topc` baseline still builds in `sopc`.
2. Confirm the SOPC-only docs are present and internally consistent.
3. Import only the reusable pieces from `~/sipc`.
4. Implement SOP v1 sequence semantics.
5. Add for-loop, iteration, and slice-related work after the core sequence model is stable.

## Practical Notes

1. Do not try to preserve `topc` and `sopc` as the same repo. They have different audiences and different doc scopes.
2. Treat `sopc` as a private fork with a clear baseline commit, not as a long history rewrite.
3. Prefer selective imports from `~/sipc` over bulk copying. The codebase is useful as a reference, but the architecture should stay aligned with the current `topc` implementation.
