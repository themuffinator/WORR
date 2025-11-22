#include "renderer.hpp"

/*
=============
UI_RendererScaledDpi

Returns the effective DPI scale after enforcing the minimum threshold.
=============
*/
static float UI_RendererScaledDpi(const uiRendererContext_t *ctx)
{
	if (!ctx) {
		return 1.0f;
	}

	const float dpi = ctx->scale.dpi_scale < ctx->scale.min_dpi_scale ? ctx->scale.min_dpi_scale : ctx->scale.dpi_scale;
	return dpi;
}

/*
=============
UI_RendererScaledLayout

Returns the layout scale with a safe fallback for invalid values.
=============
*/
static float UI_RendererScaledLayout(const uiRendererContext_t *ctx)
{
	if (!ctx) {
		return 1.0f;
	}

	return ctx->scale.layout_scale > 0.0f ? ctx->scale.layout_scale : 1.0f;
}

/*
=============
UI_RendererApplyScale

Applies DPI and layout scaling to a logical coordinate or size.
=============
*/
static float UI_RendererApplyScale(const uiRendererContext_t *ctx, float value)
{
	if (!ctx) {
		return value;
	}

	return UI_ApplyScale(&ctx->scale, value);
}

/*
=============
UI_RendererLogicalFontPixels

Recovers the unscaled logical font height for a typography role.
=============
*/
static float UI_RendererLogicalFontPixels(const uiRendererContext_t *ctx, uiTypographyRole_t role)
{
	const int baseHeight = UI_FontPixelHeightForRole(role);
	if (baseHeight <= 0) {
		return 0.0f;
	}

	const float dpi = UI_RendererScaledDpi(ctx);
	const float layout = UI_RendererScaledLayout(ctx);

	return static_cast<float>(baseHeight) / (dpi * layout);
}

/*
=============
UI_RendererTextScale

Calculates the renderer scale factor for themed text.
=============
*/
static int UI_RendererTextScale(const uiRendererContext_t *ctx, uiTypographyRole_t role)
{
	const float logical = UI_RendererLogicalFontPixels(ctx, role);
	if (logical <= 0.0f) {
		return 1;
	}

	const float target = UI_ApplyTypeScale(&ctx->scale, logical);
	int scaled = Q_rint(target / logical);

	if (scaled < 1) {
		scaled = 1;
	}

	return scaled;
}

/*
=============
UI_RendererResolveColor

Derives a color for the provided style using the active palette.
=============
*/
static color_t UI_RendererResolveColor(const uiRendererRectStyle_t *style)
{
	if (!style) {
		return ColorRGBA(0, 0, 0, 0);
	}

	return UI_ColorForRole(style->color, style->state);
}

/*
=============
UI_RendererResolveTextColor

Derives the text color using the active palette.
=============
*/
static color_t UI_RendererResolveTextColor(const uiRendererTextStyle_t *style)
{
	if (!style) {
		return ColorRGBA(0, 0, 0, 0);
	}

	return UI_ColorForRole(style->color, style->state);
}

/*
=============
UI_RendererInitContext

Initializes the renderer context with current DPI and layout scaling values.
=============
*/
void UI_RendererInitContext(uiRendererContext_t *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->scale.dpi_scale = uis.layout.dpiScale;
	ctx->scale.layout_scale = uis.scale > 0.0f ? uis.scale : 1.0f;
	ctx->scale.min_dpi_scale = 1.0f;
	ctx->scale.type_scale = 1.0f;
}

/*
=============
UI_RendererMeasureText

Measures themed text after applying DPI and layout scaling.
=============
*/
uiRendererSize_t UI_RendererMeasureText(const uiRendererContext_t *ctx, const uiRendererTextStyle_t *style, const char *text)
{
	uiRendererSize_t size{ 0, 0 };

	if (!ctx || !style || !text) {
		return size;
	}

	const int scale = UI_RendererTextScale(ctx, style->typography);
	const qhandle_t handle = UI_FontForRole(style->typography);

	size.width = SCR_MeasureString(scale, style->flags & ~UI_MULTILINE, MAX_STRING_CHARS, text, handle);
	size.height = SCR_FontLineHeight(scale, handle);

	return size;
}

/*
=============
UI_RendererDrawText

Draws themed text using palette colors and scaled typography metrics.
=============
*/
void UI_RendererDrawText(const uiRendererContext_t *ctx, const uiRendererTextStyle_t *style, float x, float y, const char *text)
{
	if (!ctx || !style || !text) {
		return;
	}

	const int scale = UI_RendererTextScale(ctx, style->typography);
	const qhandle_t handle = UI_FontForRole(style->typography);
	const color_t color = UI_RendererResolveTextColor(style);
	const int drawX = Q_rint(UI_RendererApplyScale(ctx, x));
	const int drawY = Q_rint(UI_RendererApplyScale(ctx, y));

	SCR_DrawStringStretch(drawX, drawY, scale, style->flags, MAX_STRING_CHARS, text, color, handle);
}

/*
=============
UI_RendererFillRect

Fills a rectangle using a themed palette role and scaled layout values.
=============
*/
void UI_RendererFillRect(const uiRendererContext_t *ctx, const uiRendererRectStyle_t *style, float x, float y, float width, float height)
{
	if (!ctx || !style) {
		return;
	}

	const color_t color = UI_RendererResolveColor(style);
	const int drawX = Q_rint(UI_RendererApplyScale(ctx, x));
	const int drawY = Q_rint(UI_RendererApplyScale(ctx, y));
	const int drawWidth = Q_rint(UI_RendererApplyScale(ctx, width));
	const int drawHeight = Q_rint(UI_RendererApplyScale(ctx, height));

	if (drawWidth <= 0 || drawHeight <= 0) {
		return;
	}

	R_DrawFill32(drawX, drawY, drawWidth, drawHeight, color);
}

/*
=============
UI_RendererStrokeRect

Draws a rectangle border using themed colors and scaled stroke width.
=============
*/
void UI_RendererStrokeRect(const uiRendererContext_t *ctx, const uiRendererRectStyle_t *style, float x, float y, float width, float height, float borderWidth)
{
	if (!ctx || !style) {
		return;
	}

	const color_t color = UI_RendererResolveColor(style);
	const int drawX = Q_rint(UI_RendererApplyScale(ctx, x));
	const int drawY = Q_rint(UI_RendererApplyScale(ctx, y));
	const int drawWidth = Q_rint(UI_RendererApplyScale(ctx, width));
	const int drawHeight = Q_rint(UI_RendererApplyScale(ctx, height));
	int border = Q_rint(UI_RendererApplyScale(ctx, borderWidth));

	if (drawWidth <= 0 || drawHeight <= 0) {
		return;
	}

	if (border < 1) {
		border = 1;
	}

	const int right = drawX + drawWidth - border;
	const int bottom = drawY + drawHeight - border;

	R_DrawFill32(drawX, drawY, border, drawHeight, color);
	R_DrawFill32(right, drawY, border, drawHeight, color);
	R_DrawFill32(drawX + border, drawY, drawWidth - border * 2, border, color);
	R_DrawFill32(drawX + border, bottom, drawWidth - border * 2, border);
}
