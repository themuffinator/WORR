#pragma once

#include <stdbool.h>

/*
=============
uiState_t

Enumerates UI interaction states used for theming.
=============
*/
typedef enum uiState_e {
	UI_STATE_NORMAL = 0,
	UI_STATE_HOVER,
	UI_STATE_ACTIVE,
	UI_STATE_DISABLED,
	UI_STATE_COUNT
} uiState_t;

/*
=============
uiColorRGBA_t

Represents an RGBA color stored as floats.
=============
*/
typedef struct uiColorRGBA_s {
	float	r;
	float	g;
	float	b;
	float	a;
} uiColorRGBA_t;

/*
=============
uiPaletteVariant_t

State-aware color variant for a palette role.
=============
*/
typedef struct uiPaletteVariant_s {
	uiColorRGBA_t	color;
	bool	overrides;
} uiPaletteVariant_t;

/*
=============
uiPaletteRole_t

Palette role with optional overrides for each UI state.
=============
*/
typedef struct uiPaletteRole_s {
	const char		*key;
	uiPaletteVariant_t	variants[UI_STATE_COUNT];
} uiPaletteRole_t;

/*
=============
uiFontFace_t

Defines a font family and weight pairing.
=============
*/
typedef struct uiFontFace_s {
	const char	*family;
	float		weight;
} uiFontFace_t;

/*
=============
uiTypographyVariant_t

State-aware typography overrides for a token.
=============
*/
typedef struct uiTypographyVariant_s {
	uiFontFace_t	face;
	float		size_scale;
	float		line_height_scale;
	float		letter_spacing_scale;
	bool		has_override;
} uiTypographyVariant_t;

/*
=============
uiTypographyToken_t

Typography token with per-state overrides and base metrics.
=============
*/
typedef struct uiTypographyToken_s {
	const char		*name;
	float			size_px;
	float			line_height_px;
	float			letter_spacing_px;
	uiTypographyVariant_t	variants[UI_STATE_COUNT];
} uiTypographyToken_t;

/*
=============
uiScaleRules_t

Combines DPI and layout scaling parameters.
=============
*/
typedef struct uiScaleRules_s {
	float	dpi_scale;
	float	layout_scale;
	float	min_dpi_scale;
	float	type_scale;
} uiScaleRules_t;

/*
=============
uiResolvedTypography_t

Concrete typography metrics after scaling and state resolution.
=============
*/
typedef struct uiResolvedTypography_s {
	uiFontFace_t	face;
	float		size_px;
	float		line_height_px;
	float		letter_spacing_px;
} uiResolvedTypography_t;

/*
=============
UI_ApplyScale

Combines DPI and layout scale with a minimum DPI guard.
=============
*/
static inline float UI_ApplyScale(const uiScaleRules_t *scale, float logical_px)
{
	float applied_dpi = scale->dpi_scale < scale->min_dpi_scale ? scale->min_dpi_scale : scale->dpi_scale;
	return logical_px * applied_dpi * scale->layout_scale;
}

/*
=============
UI_ApplyTypeScale

Adds typography-specific scale on top of general DPI/layout scaling.
=============
*/
static inline float UI_ApplyTypeScale(const uiScaleRules_t *scale, float logical_px)
{
	return UI_ApplyScale(scale, logical_px) * scale->type_scale;
}

/*
=============
UI_SelectPaletteVariant

Returns the variant for a given state, falling back to normal if absent.
=============
*/
static inline const uiPaletteVariant_t *UI_SelectPaletteVariant(const uiPaletteRole_t *role, uiState_t state)
{
	const uiPaletteVariant_t *variant = &role->variants[state];
	if (variant->overrides || state == UI_STATE_NORMAL) {
		return variant;
	}
	return &role->variants[UI_STATE_NORMAL];
}

/*
=============
UI_ResolveTypography

Expands a typography token to pixel metrics for the requested state.
=============
*/
static inline uiResolvedTypography_t UI_ResolveTypography(const uiTypographyToken_t *token, const uiScaleRules_t *scale, uiState_t state)
{
	const uiTypographyVariant_t *variant = &token->variants[state];
	if (!variant->has_override && state != UI_STATE_NORMAL) {
		variant = &token->variants[UI_STATE_NORMAL];
	}

	uiResolvedTypography_t resolved;
	resolved.face = variant->face;
	resolved.size_px = UI_ApplyTypeScale(scale, token->size_px * variant->size_scale);
	resolved.line_height_px = UI_ApplyTypeScale(scale, token->line_height_px * variant->line_height_scale);
	resolved.letter_spacing_px = UI_ApplyTypeScale(scale, token->letter_spacing_px * variant->letter_spacing_scale);
	return resolved;
}

