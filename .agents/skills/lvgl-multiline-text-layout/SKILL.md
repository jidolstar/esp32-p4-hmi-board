---
name: lvgl-multiline-text-layout
description: Set and verify line spacing, alignment, and bounds for LVGL text that wraps or spans multiple lines in HMI apps and samples.
---

# LVGL Multiline Text Layout

Apply deliberate line spacing, alignment, and bounds to every user-visible multiline text element.

## Workflow

1. Inspect the changed labels and their line spacing first. For a broader audit, run `./.agents/skills/lvgl-multiline-text-layout/scripts/audit_multiline_text.ps1` from the repository root. It searches app and sample code, shared UI and YAML catalogs. Use `-SourcePath samples/suite/pages/date_time,samples/suite/i18n` to focus the search.
2. Inspect text that wraps dynamically as well as strings containing `\n`; an audit cannot infer runtime wrapping.
3. Use `hmi::ui::configure_multiline()` where its fixed bounds and wrapping behavior fit the widget. For other multiline labels, set line spacing explicitly using `lv_obj_set_style_text_line_space()`.
4. Use 5 px as the normal minimum for compact 16-24 px Korean UI text. Use more space for large cards or dense explanatory lists. Use less only when measured widget height makes 5 px impossible and preserve readability.
5. Use `LV_TEXT_ALIGN_CENTER` for centered buttons, footer guidance, notices, and dialogs. Use intentional left alignment for paragraphs where it aids reading.
6. Check label width, height, long mode, parent padding, and vertical centering together. Increased spacing must not clip the first or last line.
7. Keep title and body as separate labels when hierarchy differs. Differentiate font role, color, and vertical separation.
8. Use [change-scoped-verification](../change-scoped-verification/SKILL.md): small shared layout/text edits normally need the affected Windows screen, not a firmware build. Build firmware only for a target-specific issue or required HMI integration gate. Never upload; Windows verification does not prove real-device clipping or visual balance.

## Review checklist

- Every explicit or dynamically wrapped multiline label has intentional line spacing.
- Buttons and footer messages remain centered within their actual bounds.
- Paragraph alignment matches the content and its container.
- Title and body hierarchy is visually distinct.
- No line is clipped, overlapped, or outside its parent.
- Single-line labels and compact symbols are unchanged unless necessary.
