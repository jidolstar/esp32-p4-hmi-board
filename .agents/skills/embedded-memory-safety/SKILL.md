---
name: embedded-memory-safety
description: Prevent stack overflow, oversized embedded models, excessive snapshot copying, and eager allocation in this cooker HMI firmware. Use when creating or changing C/C++ structs, classes, nested arrays or containers, recipe/catalog/session models, JSON parsing or serialization, startup paths, callbacks, task stack settings, large buffers, or code that copies model objects.
---

# Embedded Memory Safety

Review memory layout before implementing the change, then verify the resulting firmware footprint.

## Review workflow

1. Identify every changed type containing strings, vectors, arrays, buffers, or nested model objects.
2. Calculate `sizeof(element) × dimensions × simultaneously live copies`.
3. Search for function-local instances, pass-by-value parameters, value-return temporaries, original/edit/projected snapshots, and callback captures.
4. Keep large objects out of task stacks. Prefer references, move semantics, persistent ownership, or checked heap allocation.
5. Use sparse containers when capacity is a maximum and most entries are empty. Do not eagerly construct expensive elements solely to provide indexed access.
6. Preserve bounded validation even when storage is dynamic. Reject data beyond the product limit before reserving or appending it.
7. Clear the entire nested value when deleting or reusing a slot so stale heap-owned detail data is released.
8. Add or update `static_assert` size guards for core models when their layout changes.
9. Use [change-scoped-verification](../change-scoped-verification/SKILL.md) to select checks. Build the affected firmware environment and report RAM/flash when embedded allocation, stack pressure, large models, or lifetime/startup behavior needs target verification. Small scalar fields or UI callbacks alone do not require a firmware build; inspect size/lifetime and use the relevant host checks first.

## Stack rules

- Treat task stack increases as diagnostic headroom, not the primary correction.
- Do not create catalogs, full batch sessions, framebuffers, JSON payload buffers, or image data as automatic local variables.
- Small views, indices, handles, and bounded scalar arrays may remain on the stack.
- When heap allocation can fail, use checked allocation and leave the previous persistent state unchanged on failure.

## Model rules

- Keep maximum counts as named constants and enforce them at parsing and UI boundaries.
- Prefer an empty vector for optional repeated detail data; resize only when editing or parsing that data.
- Avoid duplicate ownership when a stable reference or identity is sufficient.
- Account for all snapshots explicitly. A model that is acceptable once may be unsafe when copied into source, projection, tombstone, and session collections.

## Verification checklist

Use targeted searches where the changed types are used. Select builds through `change-scoped-verification`; the examples below are searches, not a mandatory whole-repository checklist:

```powershell
rg -n "RecipeCatalog|RecipeBatchEditSession|std::array|std::vector|std::string" src test
rg -n "RecipeCatalog\s+\w+|RecipeBatchEditSession\s+\w+" src
```

Before completion, confirm:

- No newly introduced large automatic local exists on startup or UI callback paths.
- Nested capacity multiplication and live-copy counts were reviewed.
- Compile-time size guards still pass without raising limits casually.
- Report RAM/flash when a firmware build was needed; otherwise state any material unverified memory risk. No direct upload was performed.
