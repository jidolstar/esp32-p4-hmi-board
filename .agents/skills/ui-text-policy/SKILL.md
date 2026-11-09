---
name: ui-text-policy
description: Design, audit, and maintain visible UI text and i18n ownership in this profile-based HMI starter. Use for labels, placeholders, action captions, status/error messages, units, formats, locale controls, and translation catalog changes.
---

# UI text and i18n policy

Keep fixed UI text in the catalog owned by the app or sample that presents it. Do not assume a repository-root catalog or a particular product domain. Before editing, inspect the selected profile manifest, its config inputs, and the screen's runtime role.

## Choose the owner

- Text shared by a sample suite, its Dashboard, and multiple sample pages belongs under that sample root, such as `samples/<suite>/i18n/`.
- Text used by only one independent page may remain under `samples/<suite>/pages/<id>/i18n/` when keeping it local avoids coupling.
- Text owned by an application belongs under that app root, such as `apps/<app>/i18n/`.
- `modules/i18n/` owns domain-neutral locale, catalog-view, and formatting contracts. It does not own one profile's wording or generated catalog.
- Startup, recovery, and asset-update UI that must work without external assets may use a small built-in ASCII catalog owned by that recovery path.

If a fixed string appears in a shared shell, Dashboard, navigation descriptor, button, status, error, placeholder, unit, date/time format, or keyboard action key, treat it as catalog text. Keyboard character keys, user input, discovered device names, measurements, file names, and other runtime values are dynamic data; translate their labels and formats, not the values themselves.

## Implement catalog text

- Reuse a key only when meaning and context match. Prefer `common.*` for genuinely shared actions and a screen/page namespace for local wording.
- Keep every supported locale's key and placeholder contract identical. Missing text must fail generation or validation rather than silently falling back to another language.
- Register typography roles with fixed text. Do not add glyphs to a font config as a substitute for catalog ownership.
- Use typed generated IDs or catalog lookup at call sites. Do not edit generated headers, generated font sources, manifests, or binary assets directly.
- Keep dynamic values as format arguments so locale-specific order stays in the catalog.
- A locale selector changes screen language; a keyboard language/mode selector changes input layout. Keep these states independent unless the product explicitly defines otherwise.

## Keep tools reusable

Generators under `tools/` must accept or derive explicit profile/app input and output paths. Do not hardcode `samples/`, `apps/main/`, a page ID, or a product namespace into reusable generator logic. Profile-specific defaults or convenience commands belong in profile configuration or a thin profile-prefixed wrapper.

Generated outputs should remain next to their owning catalog or in an owner-declared output directory. The selected profile manifest must declare source, config, and runtime asset inputs so Windows and HMI builds resolve the same catalog deterministically.

## Audit and verify

1. Search UI helpers and direct calls such as `lv_label_set_text`, `lv_label_set_text_fmt`, `lv_textarea_set_placeholder_text`, `snprintf`, registry titles, and theme captions.
2. Exclude logs, test-only comparisons, empty clearing strings, keyboard character keys, and confirmed dynamic data.
3. Check locale parity, placeholder types, typography roles, generated manifests, and stale outputs with the owning generator.
4. Build and test the affected Windows profile first. Verify locale changes update the current view and remain consistent across navigation and theme rebuilds.
5. Treat clipping, wrapping, glyph appearance, touch behavior, asset-free recovery, and real-device rendering as manual gates where applicable.

Use `font-management-guide` when font roles, glyph coverage, generated fonts, or runtime font loading change. Use `lvgl-multiline-text-layout` when translated text can wrap or span multiple lines.
