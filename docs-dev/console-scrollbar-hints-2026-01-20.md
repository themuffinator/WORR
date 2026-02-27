# Console Scrollbar + Menu Hint Layout

**Date:** 2026-01-20  
**Scope:** `src/client/console.cpp`, `src/game/cgame/ui/ui_hints.cpp`

## Summary
- Added a thin, modern scrollbar to the console output region.
- Introduced `con_fontscale` (default `5`) as the console font sizing cvar
  and kept `con_fontsize` as a legacy fallback.
- Moved menu hint bar upward by half a line height and doubled key icon
  size while keeping text aligned.

## Console font scale
- New cvar: `con_fontscale` default `5`.
- Legacy `con_fontsize` is still read if `con_fontscale` remains at its
  default value, preserving older configs.

## Console scrollbar
- The scrollbar is drawn at the right edge of the output region.
- Thumb height scales to the visible line count vs. total scrollback.
- Subtle track and brighter thumb colors keep it lightweight but readable.

## Menu hint layout
- Hint bar y-position is offset by `-0.5 * CONCHAR_HEIGHT`.
- Key icons are drawn at `2 * CONCHAR_HEIGHT` and vertically centered
  on the text line for consistent alignment.

## Verification
1. Scroll the console and confirm the thumb moves with scrollback.
2. Check the hint bar position and key icon size on any menu screen.
3. Verify `con_fontscale` overrides old `con_fontsize` values.
