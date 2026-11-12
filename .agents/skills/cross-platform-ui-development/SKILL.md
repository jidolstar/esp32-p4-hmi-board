---
name: cross-platform-ui-development
description: Develop or review LVGL UI and app behavior shared by the Windows simulator and ESP32-P4 HMI in this profile-based starter repository. Use for shared screens, profile composition, platform adapters, or build wiring.
---

# Cross-platform UI development

Use the profile manifest and generated build inputs as the source of truth for which app, sample, modules, assets, and tests belong to each target.

## Boundaries

- `apps/<app>/` and `samples/<suite>/` own profile-specific UI, navigation, state, catalogs, and assets.
- `modules/` owns reusable, domain-neutral contracts and implementations. Do not move one profile's wording, page policy, or asset paths into a shared module.
- `profiles/` owns registry, schema, and common app contracts.
- `hosts/windows/` and `hosts/hmi/` assemble the selected profile and run the event loop; they must not branch on concrete app or page IDs.
- `platform/windows/` and `platform/hmi/` own host-specific LVGL/platform services.
- `boards/` owns hardware facade and board implementation, not UI or product/sample policy.
- Common UI sources must not include Arduino, ESP-IDF, SDL, Board internals, or platform-specific conditionals.

New sample pages should normally change their own page directory and the page registry. Shared sample-shell behavior may change `samples/<suite>/` composition, but must continue to depend on page descriptors rather than concrete page types or IDs.

## Design checks

1. Classify a change as profile UI/state, reusable module behavior, host composition, platform service, or physical hardware.
2. Keep UI/state shared between Windows and HMI in one common source set. Put only real I/O differences behind narrow ports.
3. Declare new sources, include paths, config, assets, and tests in the owning profile/page manifest path rather than adding ad hoc host lists.
4. Keep locale and theme state in the profile composition root when multiple views must observe them. Pass small values or views; do not copy complete catalogs into app state.
5. Review object size, allocation lifetime, callback cleanup, font ownership, and startup order for embedded targets.

## Verification

- Follow [change-scoped-verification](../change-scoped-verification/SKILL.md). After editing Windows sources or inputs, run `python tools/project.py build --host windows --profile <id>` and then `python tools/project.py check --host windows --profile <id>`.
- After profile, page-registry, source declaration, asset declaration, or build-wiring changes, refresh affected host/profile inputs with `prepare` or `build` before `python tools/project.py check --scope isolation`.
- Build only the affected HMI profile when common API/build wiring, generated C assets, platform integration, memory lifetime, or a required integration gate changes.
- Inspect translated layouts, touch/click behavior, navigation, theme/locale rebuilds, and dynamic asset failure in the Windows simulator.
- Record actual LCD, touch, SD, performance, and boot behavior separately as device checks. Codex does not upload firmware.
