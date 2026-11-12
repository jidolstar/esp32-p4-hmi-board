---
name: embedded-memory-safety
description: Review ESP32-P4 memory use when a change affects stack pressure, allocation size or placement, model copies, buffer ownership, or lifetime.
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
- Keep framebuffers, camera frames, font caches and other large buffers off task stacks. Check the required memory capabilities before moving buffers between internal RAM and PSRAM.
- Small views, indices, handles, and bounded scalar arrays may remain on the stack.
- When heap allocation can fail, use checked allocation and leave the previous persistent state unchanged on failure.

## Model rules

- Keep maximum counts as named constants and enforce them at parsing and UI boundaries.
- Prefer an empty vector for optional repeated detail data; resize only when editing or parsing that data.
- Avoid duplicate ownership when a stable reference or identity is sufficient.
- Account for all snapshots explicitly. A model that is acceptable once may be unsafe when copied into source, projection, tombstone, and session collections.

## Verification checklist

Search for the affected allocation and its owner. Use [change-scoped-verification](../change-scoped-verification/SKILL.md) to choose builds. Examples for current code paths:

```powershell
rg -n 'framebuffer|CameraFrameLeaseTracker|MALLOC_CAP_' boards/jc1060p470c platform/hmi
rg -n 'FontRepository|BinaryFontPort' modules/ui profiles/runtime hosts
```

Before completion, confirm:

- No newly introduced large automatic local exists on startup or UI callback paths.
- Nested capacity multiplication and live-copy counts were reviewed.
- Compile-time size guards still pass without raising limits casually.
- Report firmware RAM/flash when a firmware build was needed and identify any remaining runtime memory checks.
