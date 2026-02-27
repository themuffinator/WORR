# Main menu absolute positioning + footer (2026-01-29)

## Overview
- Enable absolute positioning for bitmap/button widgets in UI JSON.
- Allow fixed rects for main menu plaque/logo.
- Add footer text support for tiny disclaimer/copyright.

## UI JSON changes
- main menu now uses plaqueRect/logoRect, fixedLayout, per-button x/y positions,
  button text colors, and a footer block for disclaimer/copyright.

## Code changes
- BitmapWidget supports fixed x/y positions and optional draw-size overrides.
- MenuButtonWidget supports per-button text colors for normal/selected states.
- Menu JSON parser supports x/y, textColor/textSelectedColor, fixedLayout,
  plaqueRect/logoRect, and footer blocks.
- Menu layout skips centering overrides for fixed plaque/logo and avoids clipping
  when fixedLayout is enabled. Footer renders at the bottom in UI font.
