---
name: change-scoped-verification
description: Select the smallest meaningful build and test set for changes in this profile-based Windows simulator and ESP32-P4 HMI starter. Use when planning verification, running tests, or deciding whether an HMI build is needed.
---

# Change-scoped verification

Choose checks from the files changed, the owning profile, the executed code path, and the remaining concrete risk. Do not run every profile and host after every edit.

## Default scope

| Change | Default verification | Expand when |
| --- | --- | --- |
| Documentation, plan, or skill | Re-read links, paths, format, and referenced commands | Executable code or generator also changed |
| Batch wrapper rename | Search old/new names and inspect wrapper command/profile | Wrapper behavior changed |
| Catalog, wording, typography, or generated fixed assets | Generator validation/check, affected Windows profile tests, representative layout | Runtime loading or firmware-generated sources changed |
| Shared UI or state | Affected Windows profile build/check and direct tests | Common API/build wiring or HMI-only risk changed |
| Profile/page registry, source, config, or asset declaration | Affected profile check plus isolation check | Host generation or firmware inputs changed |
| HMI adapter, board code, runtime asset loader, embedded lifetime | Affected HMI profile build and direct tests | Shared contracts affect another profile |

## Commands

- Inspect profiles: `python tools/project.py list`
- Diagnose: `python tools/project.py doctor --host <windows|hmi> --profile <id>`
- Build: `python tools/project.py build --host <windows|hmi> --profile <id>`
- Run Windows: `python tools/project.py run --host windows --profile <id>`
- Profile checks: `python tools/project.py check --host windows --profile <id>`
- Isolation: `python tools/project.py check --scope isolation`
- Generator tests: `python -m unittest discover -s tools/tests -v`

Use the commands documented by the owning asset generator for dry-run, generate, check, and bundle operations. Do not invent a profile-specific path inside reusable tool code.

## Decision rules

1. Inspect the selected profile manifest and generated inputs to confirm the modified file is compiled or bundled by the intended host.
2. Start with the fastest direct test. Add profiles or hosts only when a shared contract, source selection, failure, or explicit integration gate justifies it.
3. Verify visual layout and input in the Windows simulator when UI changes. A passing assertion suite does not prove clipping or font quality.
4. Build the affected HMI profile for HMI-only code, generated firmware assets, runtime loading, platform adapters, significant object/lifetime changes, or final integration gates.
5. Keep real LCD, touch, SD, boot logs, and perceived latency as explicit manual checks. A Windows pass or firmware compile is not device proof.
6. Never upload or flash hardware from Codex.

Report checks actually run, their results, and only material remaining risks. For phase-gated plans, assign each command to the phase whose exit criteria it proves.
