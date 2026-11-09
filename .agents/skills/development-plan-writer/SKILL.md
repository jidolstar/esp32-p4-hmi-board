---
name: development-plan-writer
description: Create concrete repository planning documents when the user asks to make, write, save, or register a development plan, especially plans that should live under a `_plan` directory, follow existing plan filename conventions, and include completion checklists split into automated checks Codex can run and manual checks the user must verify.
---

# Development Plan Writer

Use this skill when the user asks for a development plan that should be persisted in the repository, not just described in chat.

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
   - Never overwrite an existing plan unless the user explicitly asks to update that file. When updating a past plan, keep its original month and filename.

3. Write a concrete implementation plan.
   - State the purpose and decision summary first.
   - Include current-state observations grounded in repository files.
   - Define target architecture or target behavior.
   - Break work into ordered phases with clear completion criteria.
   - Include affected files and expected API or module boundaries.
   - Include risks and mitigations when structure, hardware, deployment, data, or user-facing behavior could regress.

4. End with checklists.
   - Add an "automated verification checklist" section for checks Codex can perform directly.
   - Add a "manual verification checklist" section for checks requiring hardware, product judgment, visual QA, credentials, external systems, or user confirmation.
   - Use markdown checkboxes.
   - Make each checklist item objectively verifiable.
   - Include build, test, lint, compile, or static search commands under automated checks when appropriate.
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
- [ ] The user confirms structure, naming, and responsibility boundaries
```

## Quality Bar

- Prefer specific plans over broad roadmaps.
- Keep implementation phases small enough that Codex can execute them in later turns.
- Separate "what to build" from "how to verify it."
- Preserve existing repository style, naming, and language.
- Do not perform implementation work unless the user also asks to execute the plan.
