---
name: board-boundary-guard
description: Preserve the hardware facade and UI responsibility boundary in this cooker HMI repository. Use when changing, reviewing, or planning Board, public hardware services, hardware/internal, LvglPort, or their callers.
---

# Board Boundary Guard

Keep `hmi::hardware::Board` and its public hardware services as facades over board resources. The original contract is recorded in `../dev_202607_hmi/_plan/2026-07/20260716_001_BoardFacadeRefactor.md`: callers use meaningful facade operations and do not include `hardware/internal` directly. Later plans explicitly allow Board-owned `Gpio`, `Pwm`, `Storage`, `Clock`, and `Audio` service facades.

## Core Boundary

`Board` may own board bring-up and board-owned runtime services:

- Power, IO expander, LCD panel, framebuffer, touch controller, backlight.
- `lv_init()`.
- LVGL display/input driver attachment.
- The LVGL runtime pump through `Board::tick()` delegating its LVGL work to `internal::LvglPort::tick()`. `Board::tick()` may also coordinate board-owned services such as storage polling.
- Narrow hardware-facing public methods such as `set_backlight()` and `last_touch()`.
- Domain-neutral service facades for GPIO, PWM, storage, clock, and audio. The Board owns their lifetime; callers may use their public operations without reaching into `internal`.

`Board` must not own product UI:

- No screen creation with `lv_obj_create()` or screen-specific helpers.
- No custom widgets, styles, themes, screen transitions, or product state binding.
- No cooker domain workflows, recipes, modes, timers, alarms, or business rules.
- No direct knowledge of diagnostics screens except through generic LVGL port readiness.

## Layer Rules

- `src/hardware/board.*` and public service headers: caller-facing hardware contracts, ownership, and initialization/runtime orchestration.
- `src/hardware/internal/*`: private board implementation details. UI and app code must not include this directory.
- `src/hardware/internal/lvgl_port.*`: LVGL display/input port glue and LVGL runtime pump details.
- `src/ui/*`: product UI ownership, screen creation, state binding, screen transitions, and custom LVGL components.
- `src/diagnostics/*`: bring-up and debug UI such as perf overlays and test screens.
- `src/main.cpp`: object composition, setup flow, loop calls only.

## Two Distinct Boundaries

1. **Behavioral/API boundary (required):** App, UI, profile, and control code must not include or call `hardware/internal` classes. Public operations must express hardware capabilities without exposing internal object references, low-level device handles, or product policy. A public service facade is allowed when it provides a meaningful, bounded contract.
2. **Compile-time encapsulation (design quality):** A public header that includes `hardware/internal`, Arduino, FreeRTOS, or chip headers couples callers to implementation at compile time. Report this separately from a behavioral violation. The existing `Board` header has this coupling, and `../dev_202607_hmi/_doc/20260912_src_architecture_ui_build_audit.md` already records it. Improve it when the task warrants the cost; do not assume every internal include violates the original facade rule or add PImpl mechanically. Evaluate object size, allocation failure, lifetime, and startup memory first.

## Placement Decisions

When adding a feature, place it by asking:

1. Does it configure or operate physical board resources? Put it in `hardware` or `hardware/internal`, exposing a meaningful `Board` method or Board-owned service facade when needed.
2. Does it adapt display/touch hardware to LVGL driver APIs or run LVGL timers? Put it in `internal::LvglPort`.
3. Does it create LVGL objects, screens, styles, or widgets? Put it in `ui` or `diagnostics`, not `Board`.
4. Does it show temporary bring-up information? Put it in `diagnostics`.
5. Does it only compose objects and call `begin()`/`tick()`? Keep it in `main.cpp`.

## Public API Discipline

Before adding a `Board` public method:

- Prefer a domain-neutral hardware verb: `set_backlight`, `last_touch`, `tick`.
- Avoid leaking low-level handles unless there is a concrete need.
- Do not expose `hardware/internal` objects.
- Keep public signatures independent of internal drivers and transport primitives where practical. A private constructor used only by `Board` is different from a caller-facing constructor.
- Do not add methods named after screens, widgets, product flows, or UI events.
- If the method returns an LVGL handle, treat it as exceptional and document why the UI cannot use LVGL defaults instead.

## Review Checklist

Run these checks when relevant:

```powershell
rg -n "hardware/internal" src/ui src/diagnostics src/main.cpp
rg -n "hardware/internal|internal::|Wire.h|freertos/|driver/" src/hardware --glob "*.hpp" --glob "!src/hardware/internal/**"
rg -n "lv_obj_|lv_label_|lv_btn_|lv_tabview_|lv_style_|create_.*screen|screen|widget" src/hardware
rg -n "lv_tick_inc|lv_timer_handler" src/main.cpp
rg -n "class Board|void tick|begin_lvgl|attach_lvgl|display_handle" src/hardware/board.hpp src/hardware/board.cpp
```

Expected results:

- `src/ui` and `src/main.cpp` should not include `hardware/internal`.
- `src/hardware/board.*` should not create LVGL screens or widgets.
- `src/main.cpp` should not call `lv_tick_inc()` or `lv_timer_handler()` directly.
- `Board::tick()` should delegate LVGL pump details to the LVGL port; any other work must be board-owned runtime coordination.
- Review public-header search hits by visibility and effect. Record direct caller access to internal objects as a boundary violation; record private member/include coupling as a separate encapsulation concern.

## When Reviewing

Lead with boundary violations first:

- UI logic moved into `Board`.
- Internal hardware objects exposed to UI/app code.
- Product concepts entering `hardware`.
- `main.cpp` regaining low-level LVGL runtime calls.
- `LvglPort` taking over screen/widget responsibilities.

Do not call a public header's private member or transitive internal include a facade violation solely because the type appears in that header. If the user asks for stricter compile-time isolation, plan it as a separate change with embedded-memory and build verification.

If no violations exist, say that explicitly and mention any remaining manual hardware checks.
