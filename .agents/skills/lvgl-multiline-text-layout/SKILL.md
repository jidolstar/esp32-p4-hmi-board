---
name: lvgl-multiline-text-layout
description: Keep multiline LVGL text readable and visually balanced in this cooker HMI. Use whenever creating, changing, reviewing, or debugging labels, buttons, footers, notices, dialogs, wrapped recipe text, or any user-visible string that contains a newline or can wrap to two or more lines.
---

# LVGL Multiline Text Layout

Apply deliberate line spacing, alignment, and bounds to every user-visible multiline text element.

## Workflow

1. Inspect the changed labels and their line spacing first. Use `scripts/audit_multiline_text.ps1` from the repository root for a cross-screen audit or when relevant call sites are unclear; a small label edit does not require a whole-repository audit.
2. Inspect text that wraps dynamically as well as strings containing `\n`; an audit cannot infer runtime wrapping.
3. Set `lv_obj_set_style_text_line_space(label, value, 0)` explicitly for multiline `lv_label` objects.
4. Use 5 px as the normal minimum for compact 16-24 px Korean UI text. Use more space for large cards or dense explanatory lists. Use less only when measured widget height makes 5 px impossible and preserve readability.
5. Use `LV_TEXT_ALIGN_CENTER` for centered buttons, footer guidance, notices, and dialogs. Preserve intentional left alignment for paragraphs and recipe content, but balance each rendered line within its content box when required.
6. Check label width, height, long mode, parent padding, and vertical centering together. Increased spacing must not clip the first or last line.
7. Keep title and body as separate labels when hierarchy differs. Differentiate font role, color, and vertical separation.
8. Use [change-scoped-verification](../change-scoped-verification/SKILL.md): small shared layout/text edits normally need the affected Windows screen, not a firmware build. Build firmware only for a target-specific issue or required HMI integration gate. Never upload; Windows verification does not prove real-device clipping or visual balance.

## Review checklist

- Every explicit or dynamically wrapped multiline label has intentional line spacing.
- Buttons and footer messages remain centered within their actual bounds.
- Paragraphs and recipe text use intentional alignment rather than accidental left bias.
- Title and body hierarchy is visually distinct.
- No line is clipped, overlapped, or outside its parent.
- Single-line labels and compact symbols are unchanged unless necessary.
