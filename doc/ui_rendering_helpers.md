# UI Rendering Helpers Overview

## Text and Icon Primitives
- **`UI_DrawString`** chooses a font via `UI_SelectFontHandle`, then draws stretched text at full alpha with the supplied color and alignment flags. Font selection prefers the configured `uis.fontHandle`, falls back to `uis.fallbackFontHandle`, and finally the default font if neither can measure the string. Color alpha is forced to 255 so layer opacity must be applied earlier. 
- **`UI_DrawChar`** mirrors the string helper for single glyphs (including icon codepoints used elsewhere), picking the same font path and forcing full alpha before issuing `R_DrawChar`.
- **`UI_StringDimensions`** reuses the font selection logic to measure height and width, then adjusts the rectangle origin when center- or right-alignment flags are present.

## Fill and Border Utilities
- **`UI_DrawRect8`** builds a solid border by issuing four `R_DrawFill8` calls for the left, right, top, and bottom edges of the provided rectangle. Border thickness is defined by the `border` argument; color is passed straight through to the renderer.
- **`UI_DrawBackdropForLayer`** provides modal dimming. It scales the layer opacity to an alpha in `[0, 255]` and draws a full-screen fill via `R_DrawFill32` using black tinted by that alpha.

## Color Palette Handling (`uis.color.*`)
- The theme palette is configured in `UI_ApplyThemeColors`, assigning RGBA values for `background`, `normal`, `active`, `selection`, and `disabled` entries depending on the `ui_color_theme` cvar.
- During layered composition, `UI_CompositorPushOpacity` snapshots `uis.color` and rescales each entry’s alpha to match the layer’s opacity. After the layer draws, `UI_CompositorPopOpacity` restores the cached palette. Because the draw helpers force full alpha on submitted colors, this palette modulation is the mechanism by which layer opacity affects text, icons, borders, and fills.

## DPI and Layout Scaling
- `UI_UpdateLayoutMetrics` samples `get_auto_scale()` into `metrics.dpiScale` (clamped to a minimum of 1.0) and recalculates character sizes using either the active UI font or fallback console dimensions. These metrics drive spacing, column padding, and scrollbar widths, so rendering helpers work with DPI-aware dimensions derived from `uis.layout`.
- `UI_Resize` recomputes `uis.scale` via `R_ClampScale` and updates layout metrics for the current `r_config` resolution; `UI_Draw` later feeds `uis.scale` into `R_SetScale`, making all subsequent draw calls—including the helpers above—resolution-aware.

## Layer Transitions and Opacity Flow
- `UI_CompositorUpdateLayer` processes layer transitions (fade/slide in and out) by interpolating `opacity` and slide offsets over the configured duration. The resulting opacity is consumed by the backdrop renderer and color stack modulation before any items draw.
- `UI_Draw` iterates composited layers: it updates transition state, draws optional backdrops, pushes opacity to the color stack, invokes the menu draw routine (defaulting to `Menu_Draw`), and finally pops the color stack. Cursor and debug text draw afterward using the fully resolved scale and colors.

## Integration Notes
- When introducing new rendering helpers, route colors through `uis.color.*` so they respect theme selection and layer opacity modulation. Avoid applying per-draw alpha; rely on `UI_CompositorPushOpacity` for cross-layer consistency.
- Prefer layout values from `uis.layout` (e.g., `UI_CharWidth`, `UI_CharHeight`, spacing fields) to ensure DPI-aware sizing.
- Any new layer transitions should integrate with `UI_CompositorUpdateLayer` to maintain consistent opacity and slide calculations used by the existing rendering flow.
