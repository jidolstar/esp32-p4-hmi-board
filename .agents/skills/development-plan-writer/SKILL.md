---
name: development-plan-writer
description: Write or update development plans the user wants saved in this repository. Use for the _plan naming convention and for defining automated and manual verification.
---

# Development Plan Writer

Apply the file workflow below when the user requests a repository plan or asks to save or update one. Keep a chat-only plan in chat.

## Workflow

1. Inspect existing planning conventions before writing.
   - List `_plan/` and the relevant month subdirectory (`YYYY-MM/`).
   - Read nearby or similarly named plan files to match title style, section order, language, and filename pattern.
   - Use the user's language for the new plan unless existing project plans strongly indicate otherwise.

2. Choose the next filename from the existing convention.
   - Use `_plan/YYYY-MM/YYYYMMDD_NNN_Title.md`; create the current local month directory if it does not exist.
   - Use the current local date for both `YYYY-MM` and `YYYYMMDD`; the folder month must match the filename date.
   - Increment `NNN` from the highest existing sequence for that date in the month directory (also check legacy root files during migration). If there are no files for the date, use `001`.
   - Use a concise PascalCase or existing local title style for `Title`.
   - Create a new numbered file for a new plan, including a second plan for the same feature on the same day. When the user requests an update to an existing plan, keep its original month and filename.

3. Write a concrete implementation plan.
   - State the purpose and decision summary first.
   - Include current-state observations grounded in repository files.
   - Define target architecture or target behavior.
   - Break multi-phase work into verifiable phases and record dependencies using [phase-gated-development](../phase-gated-development/SKILL.md).
   - Include affected files and expected API or module boundaries.
   - Include risks and mitigations when structure, hardware, deployment, data, or user-facing behavior could regress.

4. End with checklists.
   - Add an "automated verification checklist" section for checks Codex can perform directly.
   - Add a "manual verification checklist" section for checks requiring hardware, product judgment, visual QA, credentials, external systems, or user confirmation.
   - Use markdown checkboxes.
   - Make each checklist item objectively verifiable.
   - Use [change-scoped-verification](../change-scoped-verification/SKILL.md) to choose build, test, generator, or static checks appropriate to the change.
   - Include hardware behavior, screen appearance, touch input, device logs, and acceptance criteria under manual checks when appropriate.

5. Validate the artifact.
   - Re-open the written plan and confirm filename, sections, and checklists are present.
   - If the plan references repo files, prefer accurate paths and avoid speculative details.
   - Report the created or updated file path and a brief summary.

## Checklist Pattern

Use this ending pattern unless the repository already has a stronger convention:

```markdown
## Automated Verification Checklist

- [ ] `command` succeeds
- [ ] `rg` confirms forbidden includes or APIs are absent
- [ ] New public APIs are reflected in both declarations and implementations

## Manual Verification Checklist

- [ ] Boot logs are checked on real hardware
- [ ] Display output and touch input match the expected behavior
```

## Quality Bar

- Prefer specific plans over broad roadmaps.
- Keep implementation phases small enough that Codex can execute them in later turns.
- Separate "what to build" from "how to verify it."
- Preserve existing repository style, naming, and language.
- Do not perform implementation work unless the user also asks to execute the plan.
