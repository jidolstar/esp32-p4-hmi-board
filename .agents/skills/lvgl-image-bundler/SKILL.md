---
name: lvgl-image-bundler
description: Convert uploaded images or image URLs into verified LVGL RGB565+A8 C-array firmware assets in this cooker HMI repository. Use when the user asks to bundle, embed, resource, convert, or display a PNG, logo, icon, or certification mark as a firmware image; when deciding between firmware bundle and SD-card image storage; or when adding an image entry to the LVGL asset generator and wiring it into an LVGL widget.
---

# LVGL Image Bundler

Use the deterministic pipeline under `tools/lvgl_image_assets/`. Do not hand-write pixel arrays or add a second converter.

## Decide storage

- Prefer a firmware C-array for small, fixed product UI assets required at boot, such as logos, certification marks, status art, and fixed icons.
- Prefer SD/runtime resources for large, frequently replaced, user-provided, localized, or numerous images when missing-SD behavior is acceptable.
- State the choice and expected payload before converting. For RGB565+A8, estimate `width × height × 3` bytes.
- Do not enable an image decoder or add Board/storage responsibilities merely to display one fixed asset.

## Acquire and inspect the source

1. Read `tools/lvgl_image_assets/README.md`, `tools/lvgl_image_assets/ui_image_assets.json`, and the relevant UI part before changing files.
2. For an uploaded image, preserve the original under `tools/lvgl_image_assets/sources/`.
3. For an image URL, obtain the exact file, retain the source URL, and research author/license information. Prefer official or primary sources.
4. Record source dimensions, mode, alpha extrema, corner pixels, SHA-256, author, license, and license URL.
5. Inspect the image visually. A viewer showing white does not prove alpha exists; inspect the alpha channel programmatically.
6. Use a lowercase `snake_case` asset ID that is also a valid C symbol.

If licensing or authorization is unclear, stop before committing a distributable asset and ask the user. Label a development-only interim source in `_doc/THIRD_PARTY_IMAGE_ASSETS.md`.

## Configure conversion

Add one entry to `tools/lvgl_image_assets/ui_image_assets.json`:

- `source`: repository-relative path below `tools/lvgl_image_assets/sources/`.
- `target_box`: intended descriptor size selected from the actual UI layout.
- `resize_mode`: `contain`.
- `trim_transparent`: normally `true`.
- `format`: `true_color_alpha`.
- `source_url`, `author`, `license`, `license_url`: provide all four for external sources.
- `white_matte_to_alpha`: use only for grayscale marks with an opaque white matte. Colored input intentionally fails.

Preserve aspect ratio. Do not enlarge a low-resolution source, distort it to fill a box, or edit generated C files manually.

## Generate

Prepare the workspace-local environment when missing:

```powershell
py -3 -m venv tools\lvgl_image_assets\.venv
tools\lvgl_image_assets\.venv\Scripts\python.exe -m pip install -r tools\lvgl_image_assets\requirements.txt
```

Run in this order:

```powershell
tools\lvgl_image_assets\.venv\Scripts\python.exe tools\lvgl_image_assets\generate.py --self-test --dry-run
tools\lvgl_image_assets\.venv\Scripts\python.exe tools\lvgl_image_assets\generate.py
tools\lvgl_image_assets\.venv\Scripts\python.exe tools\lvgl_image_assets\generate.py --check
```

If `py` is unavailable, use an installed Python 3 executable to create the same `.venv`. Do not use another tool workspace's venv.

## Verify the asset

- Confirm manifest source hash, source/trim/content/output sizes, format, and payload bytes.
- Confirm `payload_bytes == width × height × 3`.
- Confirm preview alpha includes the expected transparent and opaque values.
- View `tmp/ui_image_assets/<id>_preview.png` for aspect ratio, crop, antialiasing, halo, and orientation.
- For migration or regeneration, compare pixel payload hashes.
- Update `_doc/THIRD_PARTY_IMAGE_ASSETS.md` for third-party sources and transformations.

## Integrate with LVGL

1. Include `ui/assets/generated/ui_image_assets.hpp` in the owning UI part.
2. Create an `lv_img` and call `lv_img_set_src(image, &<asset_id>)`.
3. Keep layout and image ownership in `src/ui`; do not move it into `hardware::Board` or `hardware/internal`.
4. Clear click/scroll flags for decorative images.
5. Prevent overlap with labels and card boundaries.
6. Use the generated static descriptor directly; do not allocate or copy its pixel buffer at runtime.

## Completion gates

- Run `generate.py --check` and `git diff --check`.
- Search for accidental SD paths, decoder changes, old text placeholders, and direct hardware dependencies.
- Run `pio run -e jc1060p470c-p4-main` as a compile-only check.
- Never upload or flash firmware from Codex.
- Treat real-device size, transparency, halo, clipping, and visual balance as a manual gate. Ask for a device photo or explicit confirmation before claiming visual completion.
