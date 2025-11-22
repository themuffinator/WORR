# UI Renderer Interface and Migration Guide

## Renderer interface overview
- The renderer is asked to perform **state-aware** operations for text, icons, fills, strokes, and borders. Callers supply a `UIDrawContext` containing DPI and layout scale plus theme lookup callbacks; the renderer resolves palette and typography for each request rather than caching raw colors or font handles.
- Renderers should expose thin entry points for **drawing** (`ui_draw_text`, `ui_draw_icon`, `ui_fill_rect`, `ui_stroke_rect`) and **measurement** (`ui_measure_text`, `ui_icon_metrics`). Each entry takes a `UIState` value so hover/active/disabled overrides flow through naturally.
- Implementations are free to convert resolved theme data into backend-specific types (GPU color structs, font atlases, vector caches) but must preserve **float precision** long enough to avoid rounding artifacts when multiple scales are multiplied.
- Snap-to-pixel behavior is controlled by the backend: after applying `dpi_scale * layout_scale`, round icon quads and stroke positions to integer device pixels to keep edges crisp, while leaving text positioning untouched so kerning remains accurate.

## Theme data structures
- **Palette roles**: Palette keys represent roles (for example, `ui.surface`, `ui.text.primary`, `ui.accent`). Each role stores a normal entry and optional overrides for `hover`, `active`, and `disabled` states. Lookups fall back to the normal value when the requested state is missing.
- **Typography tokens**: Typography entries define `family`, `size_px`, `weight`, `line_height_ratio`, and `letter_spacing_px`. State overrides can swap the family/weight or tweak the ratios without duplicating the base token catalog.
- **Stroke widths**: Stroke thickness is stored alongside palette roles so borders can inherit the same state-aware override behavior as colors. Store stroke widths in logical pixels (pre-scale) and multiply by `dpi_scale * layout_scale` at resolve time.
- **Resolver hooks**: `UIDrawContext` supplies callbacks (`resolve_color`, `resolve_stroke_width`, `resolve_font`) that the renderer calls per operation. This keeps renderer code stateless and lets hosts implement caching or skinning policies externally.

## Scaling behavior
- Two scales drive layout: **`dpi_scale`** (device pixel ratio) and **`layout_scale`** (user zoom or viewport adjustment). Multiply them together for layout math. Clamp `dpi_scale` to a sane minimum (for example, `1.0f`) so downscaled windows do not collapse strokes and spacing.
- **Text**: Apply `dpi_scale * layout_scale` and the typography recipe’s `type_scale` when computing font sizes, line heights, and letter spacing. Avoid rounding these values until glyph shaping is done to maintain kerning precision.
- **Icons and vectors**: Apply the combined scale, then round the final quad size and origin to integral device pixels. This keeps vector edges aligned without forcing text to snap.
- **Borders and padding**: Use scaled stroke widths and layout spacing derived from theme tokens or helper structs (for example, `uis.layout`) so controls feel proportionate at any zoom level. Do not mix raw pixel constants with scaled values inside the same control.

## Migration guidance for controls
1. **Adopt `UIDrawContext`**: Change control render functions to accept a `const UIDrawContext *ctx` and forward it to every draw/measure helper. Remove cached colors or fonts that bypass the resolver callbacks.
2. **Replace hardcoded colors**: Swap direct RGBA usage for palette keys. Choose roles that match intent (`ui.text.primary`, `ui.fill.surface`, `ui.border.focus`). Ensure controls request the correct `UIState` so hover/active/disabled colors resolve.
3. **Use typography tokens**: Map existing font sizes to tokens (e.g., `ui.label.sm`, `ui.button.md`). Remove manual size multipliers in controls; instead rely on the token recipe plus the context’s scale factors.
4. **Scale-aware layout**: Audit measurements and padding. Multiply logical dimensions by `ctx->dpi_scale * ctx->layout_scale` (or use existing helpers that already do this) and let the renderer snap icons/borders as needed. Avoid mixing scaled and unscaled values.
5. **Opacity and layering**: When controls render inside composited layers, pass colors through the theme/resolver path instead of applying per-primitive alpha. Layer opacity modulation should happen before draw calls (for example, via a color stack in the UI system) so the renderer receives fully composed values.
6. **Testing checklist**:
   - Verify hover, active, and disabled states pick up the correct palette overrides.
   - Check DPI changes (window resize, high-DPI monitors) to confirm border thickness and icon edges remain crisp.
   - Confirm zoom/layout scaling keeps padding and text size in proportion without double-scaling any dimension.
   - Ensure fallback typography and palette resolution works when a token or role is missing an override.

## Reference relationships
- The renderer API complements the proposal in `doc/ui_drawing_interface.md` and the legacy helpers documented in `doc/ui_rendering_helpers.md`. When migrating, keep the state-aware theme resolution from the proposal while continuing to respect compositor opacity and layout metrics from the helper guide.
