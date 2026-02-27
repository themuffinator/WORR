# Wheel Bind Icons + Triangle Pointer

## Overview
This change set updates the weapon/inventory wheel UI to use controller/keyboard
bind icons, introduces a gradient triangle pointer rendered via `DrawPolygon()`,
and smooths hover scaling using wheel-local frame timing.

## Bind Icon Tooling
- Added shared bind icon helpers in `src/client/screen.c`:
  - `SCR_GetBindIconForKey()` resolves a keynum to a `gfx/controller/` icon.
  - `SCR_DrawBindIcon()` draws the icon (scaled to a requested size) and returns
    the drawn width for inline text layout.
- Implemented key-to-icon mapping for common keyboard, keypad, and mouse inputs:
  - ASCII keys use their ASCII code (`gfx/controller/keyboard/<code>.png`).
  - Special keys (F1–F12, arrows, insert/delete/home/end, etc) map to the
    rerelease asset codes in the 256–304 range.
  - Mouse buttons and wheel directions map to `gfx/controller/mouse/f000X.png`.
- Fallback behavior:
  - If a binding has no icon, the UI falls back to text like `[K] Drop Weapon`.
  - Unbound actions render `<UNBOUND> ...`.

## Weapon/Inventory Wheel Changes
- Drop hint lines now render controller icons + text using the shared tooling.
- The selection pointer is now a gradient triangle rendered with `DrawPolygon()`,
  using per-vertex colors and a small triangle aligned to the selected slot.
- Hover scaling is advanced using the wheel-local `Sys_Milliseconds()` delta for
  smoother 200ms transitions, instead of `cls.frametime`.

## Center Print Changes
- `CG_SCR_DrawBind()` now draws bind icons alongside text, and keeps the full
  icon+text line centered for centerprint usage.
