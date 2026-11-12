---
name: font-management-guide
description: Plan, generate, apply, and verify fixed and dynamic font assets in this profile-based HMI starter. Use for typography roles, glyph coverage, font generators, user text rendering, runtime loading, or asset manifests.
---

# Font asset management

Keep font ownership aligned with the catalog and runtime that use it. Discover the selected profile's manifest and catalog paths before choosing inputs or outputs; do not assume a root-level product asset directory.

## Ownership

- Sample-wide fonts and other shared sample assets belong under `samples/<suite>/assets/`; sample-wide catalog and typography inputs belong under `samples/<suite>/i18n/`.
- Assets exclusive to one independent page may remain under `samples/<suite>/pages/<id>/assets/` with matching page-local i18n inputs.
- Application-specific assets belong under `apps/<app>/assets/` with their app-owned catalog/config.
- `modules/ui/` owns reusable loaders and rendering contracts, not a profile's font files or wording.
- A recovery path that must render before external assets are available uses a bounded built-in font and must not depend on an SD-card catalog.

## Fixed and dynamic text

Fixed UI text is generated from locale catalogs plus typography roles. Dynamic user text uses separately declared glyph coverage and may use split runtime-loaded assets. Do not solve a missing fixed catalog entry by expanding only the dynamic glyph set.

Generated headers, C font sources, manifests, and binary font files are outputs. Edit their catalog, typography, font-source, or glyph-range inputs instead.

## Reusable tooling

Font tools under `tools/` must accept or derive explicit input, output, and owner/profile information. Reusable code must not hardcode a sample, app, page, product, host, or SD destination. Thin profile-prefixed wrappers may select a profile; generation and validation logic stays neutral.

Read `tools/assets/typography/README.md` for the currently supported commands and inputs. If the documented tool cannot target the intended owner without source edits, update or plan the tool interface before moving catalog/assets.

## Fresh checkout

The sample profile declares `samples/suite/assets/fonts/`, which is populated by the user font generator. Registry commands, including `project.py list` and isolation checks, validate that directory even when the selected profile is `main`. If validation reports `missing-asset`, run from the repository root:

```powershell
npm ci --prefix tools/assets/typography
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --ensure
```

Run the npm step when the local font converter is missing. After generation, retry the original project command.

## Verification

- Validate catalog keys, placeholders, typography roles, source hashes, glyph coverage, range overlap, output size, and stale generated files.
- Run dry-run or validation before generation, then generate once the source wording and roles are settled, followed by output checks.
- Confirm the owning profile declares all runtime asset directories and relevant config/manifests.
- Build the affected Windows profile and inspect representative Korean, English, numeric, symbol, multiline, and dynamic text.
- Build the affected HMI profile when runtime loading, generated C sources, manifest format, memory ownership, or firmware integration changes.
- Record real-device clipping, baseline, glyph quality, SD failure behavior, and loading latency as manual checks. Never treat compilation as device rendering proof.
- Do not upload or flash hardware from Codex.
