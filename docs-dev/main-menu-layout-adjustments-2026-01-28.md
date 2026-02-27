# Main menu layout scaling + centering (2026-01-28)

## Overview
- Add bitmap/button draw-size overrides so large background images can be scaled to the Q2R-style virtual size.
- Center plaque/logo vertically against the bitmap button stack when alignToBitmaps is enabled.

## UI JSON changes
- main menu button entries now specify `imageWidth`/`imageHeight` (virtual units) to scale
  `/gfx/menu_bg_off` and `/gfx/menu_bg_on` down to the reference size.

## Code changes
- BitmapWidget now supports custom draw sizes and draws scaled bitmaps with R_DrawStretchPic when needed.
- ui_json applies imageWidth/imageHeight to bitmap/button widgets.
- Menu layout aligns plaque/logo to the bitmap stack center when alignToBitmaps is active.

## Notes
- The draw-size override keeps hit-testing and cursor placement aligned with the scaled images.
