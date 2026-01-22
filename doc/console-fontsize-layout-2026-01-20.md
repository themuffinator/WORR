# Console Font Size Cvar + Layout Spacing

**Date:** 2026-01-20  
**Scope:** `src/client/console.cpp`

## Summary
- Added a dedicated console font size cvar (`con_fontsize`) to control
  the virtual line height and fixed advance for the console font.
- Reworked bottom console layout to enforce a one‑line gap between the
  last output line and the input line, and to align the version text
  with the last output line.
- Ensured console media changes trigger resize updates so wrapping and
  line feed behavior updates with font size changes.

## Details
### Font size cvar
- `con_fontsize` defaults to `4` and is clamped to `[1, 64]`.
- `Con_RegisterMedia` uses `con_fontsize` for both the console font
  line height and fixed advance, keeping TTF/kfont/legacy sizes aligned.

### Layout spacing rules
Computed positions are now derived from:
- `bottom_line_y = vislines - 1`
- `separator_y = bottom_line_y - char_height - 1`
- `input_y = separator_y + 1`
- `output_bottom_y = input_y - 2 * char_height`

This yields:
- One empty line between console output and input line.
- Input line centered between the separator and bottom border line.
- Version/clock text aligned with the last output line (`output_bottom_y`).

### Resize refresh
`con_media_changed` now calls `Con_CheckResize()` after reloading media
to update wrapping and visible character limits whenever the font size
changes.

## Verification
1. `con_fontsize 4` shows TTF and legacy at matching sizes.
2. Bottom output line sits one line above the input line.
3. Version text aligns with the bottom output line.
