# UI Drawing Interface Proposal

## Goals
- Provide a unified interface for drawing text, icons, fills, and borders that accommodates varying UI states (normal, hover, active, disabled).
- Allow palette-driven styling and typography tokens to keep visuals consistent and themeable.
- Support DPI-aware and layout scaling so widgets remain crisp and proportionate.

## Core Concepts
- **UI state**: `UI_STATE_NORMAL`, `UI_STATE_HOVER`, `UI_STATE_ACTIVE`, `UI_STATE_DISABLED`.
- **Palette keys**: string or enum identifiers (e.g., `ui.text.primary`, `ui.fill.surface`, `ui.border.focus`) used to look up colors and stroke thickness from the current theme.
- **Typography tokens**: identifiers for font family, size, weight, letter spacing, and line height (e.g., `ui.label.sm`, `ui.button.md`, `ui.caption.xs`).
- **Scaling**: separate factors for device DPI (`dpi_scale`) and layout zoom (`layout_scale`). Text metrics and stroke widths should multiply both; icon/vector sizes may snap to integral device pixels.

## Data Structures
```c
/*
=============
UIState

UI interaction state for styling.
=============
*/
typedef enum UIState {
	UI_STATE_NORMAL = 0,
	UI_STATE_HOVER,
	UI_STATE_ACTIVE,
	UI_STATE_DISABLED,
} UIState;

/*
=============
UITypographyToken

Identifies a typography recipe resolved by the theme.
=============
*/
typedef struct UITypographyToken {
	const char *name; /* e.g., "ui.button.md" */
} UITypographyToken;

/*
=============
UIPaletteKey

Identifies a palette entry for fills, strokes, text, or icon tint.
=============
*/
typedef struct UIPaletteKey {
	const char *name; /* e.g., "ui.fill.surface" */
} UIPaletteKey;

/*
=============
UIDrawContext

Holds scaling factors and theme lookup hooks for draw calls.
=============
*/
typedef struct UIDrawContext {
        float dpi_scale;     /* device pixel ratio */
        float layout_scale;  /* user or viewport zoom */

	/* Theme resolvers supplied by the embedding UI system */
	const UIColor *(*resolve_color)(const UIPaletteKey *key, UIState state);
	float (*resolve_stroke_width)(const UIPaletteKey *key, UIState state);
        const UIFont *(*resolve_font)(const UITypographyToken *token, UIState state, float dpi_scale, float layout_scale);
} UIDrawContext;
```

### Palette roles with state variants
- Palette entries are defined as **roles** so fill, stroke, text, and icon lookups can share names such as `ui.surface`, `ui.text.primary`, and `ui.accent`.
- Each role carries a **normal** value and optional overrides for `hover`, `active`, and `disabled` states. Roles omit overrides they do not need; lookups then fall back to the normal entry.
- Color values are stored as floats to keep interpolation and alpha modulation lossless before converting to renderer-native formats.

### Typography tokens and font recipes
- Typography tokens provide a base pixel size plus ratios for line height and letter spacing. State overrides can swap font family/weight and adjust the scales, enabling focused or disabled variants without duplicating the token catalog.
- Tokens are keyed (e.g., `ui.label.sm`, `ui.button.md`) and map to recipes that are expanded using the current DPI/layout scales. A typography-specific `type_scale` multiplies the combined DPI/layout factor, allowing readable type ramping without inflating icon sizes.

### DPI and layout scaling rules
- `dpi_scale` tracks the device pixel ratio; `layout_scale` captures user zoom or viewport scaling. Both feed into size calculations, while a `min_dpi_scale` guard clamps tiny ratios during window downscaling.
- Layout helpers use `dpi_scale * layout_scale` to size borders, padding, and iconography. Typography helpers use the same product plus `type_scale` to bias font sizes separately from UI chrome.

## State-aware lookup resolution
- Color lookups request a palette role plus the current `UIState`. If that role supplies an override for the requested state, it is used; otherwise the `normal` entry is returned. This preserves predictable fallback behavior when some states share the same appearance.
- Typography resolution follows the same pattern: use the requested state's override when present, else fall back to the normal variant. The resolved recipe multiplies `size_px`, `line_height_px`, and `letter_spacing_px` by the combined DPI/layout scale and `type_scale` so per-state tweaks stay proportional at any zoom level.

## Drawing Operations
```c
/*
=============
ui_draw_text

Renders styled text with palette-driven color and typography tokens.
=============
*/
void ui_draw_text(const UIDrawContext *ctx,
	UIState state,
	const char *utf8_text,
	const UITypographyToken *type,
	const UIPaletteKey *color_key,
	float x, float y);

/*
=============
ui_draw_icon

Draws an icon glyph or vector at the given position and size.
=============
*/
void ui_draw_icon(const UIDrawContext *ctx,
	UIState state,
	const UIIcon *icon,
	const UIPaletteKey *tint_key,
	float x, float y,
	float size_px);

/*
=============
ui_fill_rect

Fills a rectangle with a themed color.
=============
*/
void ui_fill_rect(const UIDrawContext *ctx,
	UIState state,
	const UIPaletteKey *fill_key,
	float x, float y,
	float width, float height,
	float corner_radius_px);

/*
=============
ui_stroke_rect

Draws a rectangle border using themed stroke width and color.
=============
*/
void ui_stroke_rect(const UIDrawContext *ctx,
	UIState state,
	const UIPaletteKey *stroke_key,
	float x, float y,
	float width, float height,
	float corner_radius_px);
```

## Measurement Operations
```c
/*
=============
ui_measure_text

Returns pixel size of text with the provided typography token and scaling.
=============
*/
UISize ui_measure_text(const UIDrawContext *ctx,
	UIState state,
	const char *utf8_text,
	const UITypographyToken *type);

/*
=============
ui_icon_metrics

Returns layout metrics for an icon at a logical size before DPI/layout scaling.
=============
*/
UISize ui_icon_metrics(const UIDrawContext *ctx,
	UIState state,
	const UIIcon *icon,
	float logical_size);
```

## Usage Notes
- Callers supply `UIState` per draw/measure to ensure hover/active/disabled variants resolve to correct colors, strokes, and fonts.
- Theme resolvers should incorporate both `dpi_scale` and `layout_scale` when computing font sizes, stroke widths, and spacing. For example, a 1px stroke at 200% zoom becomes `1 * dpi_scale * layout_scale`.
- Palette keys can map to multiple layers (e.g., base color plus alpha) to differentiate focus/hover/disabled styling while keeping API surface small.
- To keep icons crisp, allow renderers to snap to integer device pixels after applying scale factors.
- Future extensions: 9-patch image fills, gradients in `UIPaletteKey`, text truncation/ellipsis rules, bidirectional text shaping hooks.
