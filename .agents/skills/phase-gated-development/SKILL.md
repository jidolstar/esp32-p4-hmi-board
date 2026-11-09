---
name: phase-gated-development
description: Create and execute sequential development plans with verifiable phase gates. Use when writing a multi-phase implementation plan, executing work divided into phases, reporting phase progress, deciding whether to advance to the next phase, or claiming that a planned implementation is complete.
---

# Phase-gated Development

Treat every phase as a deliverable that must be independently verifiable. Finish and verify the current phase before implementing the next phase.

## Write verifiable phases

For every phase, record all of the following in the plan:

1. **Objective**: one observable outcome, not a vague activity.
2. **Scope**: exact files, components, or behavior included and excluded.
3. **Deliverables**: concrete artifacts or code changes that must exist.
4. **Automated verification**: exact commands or deterministic inspections and their expected results.
5. **Manual verification**: visual, device, hardware, or user checks that automation cannot replace.
6. **Exit criteria**: a binary checklist derived from the deliverables and verification results.
7. **Dependencies**: only prerequisites that must already be complete.

Split a phase when its exit criteria cannot be evaluated independently. Do not use a later phase to verify an earlier phase. If hardware or user inspection is required, make it an explicit exit criterion rather than silently deferring it.

## Execute one phase at a time

Use these states only:

- `pending`: no implementation has started.
- `in_progress`: implementation or verification is underway.
- `blocked`: an exit criterion cannot be completed without user input, hardware, credentials, or an external state change.
- `complete`: every required exit criterion has passed with evidence.

Keep at most one phase `in_progress`. While a phase is active:

1. Implement only its declared scope and unavoidable prerequisites.
2. Run every automated verification assigned to that phase.
3. Perform every available manual verification.
4. Record evidence next to each exit criterion: command, result, artifact, screenshot, or explicit user confirmation.
5. Fix failures and rerun the relevant checks.
6. Mark the phase `complete` only after every required criterion passes.

Do not begin implementation for the next phase while the current phase is `in_progress` or `blocked`. Planning or inspecting future work is allowed, but changing future-phase production code is not.

## Handle manual gates and blockers

If a required check needs a physical device or user judgment that Codex cannot perform:

1. Complete all remaining safe work in the current phase.
2. Mark the phase `blocked`, not `complete` or `partial complete`.
3. Give the user a short, exact verification procedure and expected result.
4. Wait for explicit confirmation before closing the phase and advancing.

Never reinterpret "build succeeds" as proof that visual layout, hardware behavior, runtime assets, or user interaction passed.

If the user explicitly changes scope or authorizes deferring a criterion, update the written plan first. Move the deferred criterion into a newly named phase with its own gate; do not erase it or claim the original plan completed unchanged.

## Report progress accurately

At each handoff, report:

- current phase and state;
- passed exit criteria with evidence;
- failed or unverified criteria;
- whether the next phase is permitted to start.

Avoid phrases such as "all phases complete," "development complete," or "finished" unless every phase in the current written plan is `complete`. Distinguish implementation, automated verification, and manual verification explicitly.

When the user asks what was completed, compare the actual evidence against the stored plan instead of summarizing from memory.

## Completion gate

Before declaring the entire plan complete, verify all of the following:

- Every phase is marked `complete`.
- Every exit criterion has recorded evidence.
- No required manual check is pending.
- No legacy cleanup, migration, or follow-up listed in the plan remains.
- The final end-to-end verification passes after the last phase.

If any item is false, report the exact current phase and remaining gate instead of declaring completion.
