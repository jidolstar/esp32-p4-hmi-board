---
name: board-boundary-guard
description: Preserve the Board hardware facade and its boundary with platform adapters, app UI, and LVGL runtime. Use when changing or reviewing Board, its public services, LvglPort, or their callers.
---

# Board boundary

Use [the current Board header](../../../boards/jc1060p470c/board.hpp) as the public API reference. The [bootstrap source map](../../../_doc/migration/hmi_bootstrap_source_map.md) records the original migration and its display, touch, and LVGL invariants.

- `boards/jc1060p470c/` owns hardware bring-up, LCD, touch, backlight and board-owned services such as SD storage, audio, USB MSC and CDC asset reception where enabled. Expose bounded operations or status values through `hmi::boards::jc1060p470c::Board` and its public service facades.
- `boards/jc1060p470c/internal/` owns private drivers. `internal::LvglPort` handles LVGL display/input attachment and runtime pumping. `Board::tick()` delegates LVGL work to it and coordinates board-owned services.
- `hosts/hmi/main.cpp` owns startup, profile composition and the event loop. `platform/hmi/` adapts Board or ESP APIs to shared contracts, as in `platform/hmi/storage/board_storage_access.cpp`.
- `apps/`, `samples/` and `modules/` own shared UI and policy. Pass hardware capabilities through profile contracts and adapters. Keep screens, widgets, wording and product decisions out of Board and LvglPort.
- `diagnostics/` contains standalone bring-up projects with their own build entry points. Keep probe-specific code out of the shared app and Board API.

## Public API review

Prefer operations such as `set_backlight_level()`, `last_touch()` and `storage()` that describe a capability and keep the driver's lifetime within Board. Expose hardware handles only for a concrete caller requirement and document their lifetime.

Check for direct caller access to private drivers separately from compile-time coupling. The current Board header includes private member types and platform headers. Review that coupling when public dependencies change and account for object size, allocations and startup lifetime before introducing indirection.

From the repository root, use targeted searches such as:

```powershell
rg -n 'boards/jc1060p470c|board\.hpp|internal/' apps samples modules
rg -n '#include.*internal/' hosts/hmi platform/hmi
rg -n 'lv_(obj|label|btn)_create' boards/jc1060p470c
rg -n 'lv_tick_inc|lv_timer_handler' hosts/hmi boards/jc1060p470c
```

Inspect hits in context. Board-owned tests can cover private helpers. HMI host code and adapters use public Board operations, and the LVGL pump stays in the port. Select build and device checks using [change-scoped-verification](../change-scoped-verification/SKILL.md).
