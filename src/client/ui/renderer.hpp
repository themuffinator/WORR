#pragma once

#include "ui.hpp"
#include "client/ui_theme.hpp"

/*
=============
uiRendererContext_t

Holds scaling parameters for renderer helper calls.
=============
*/
typedef struct uiRendererContext_s {
	uiScaleRules_t	scale;
} uiRendererContext_t;

/*
=============
uiRendererTextStyle_t

Describes typography, color, and alignment for themed text.
=============
*/
typedef struct uiRendererTextStyle_s {
	uiTypographyRole_t	typography;
	uiColorRole_t		color;
	uiControlState_t	state;
	int				flags;
} uiRendererTextStyle_t;

/*
=============
uiRendererRectStyle_t

Specifies palette role and UI state for rectangle fills and strokes.
=============
*/
typedef struct uiRendererRectStyle_s {
	uiColorRole_t		color;
	uiControlState_t	state;
} uiRendererRectStyle_t;

/*
=============
uiRendererSize_t

Represents measured width and height in pixels.
=============
*/
typedef struct uiRendererSize_s {
	int	width;
	int	height;
} uiRendererSize_t;

void	UI_RendererInitContext(uiRendererContext_t *ctx);
uiRendererSize_t	UI_RendererMeasureText(const uiRendererContext_t *ctx, const uiRendererTextStyle_t *style, const char *text);
void	UI_RendererDrawText(const uiRendererContext_t *ctx, const uiRendererTextStyle_t *style, float x, float y, const char *text);
void	UI_RendererFillRect(const uiRendererContext_t *ctx, const uiRendererRectStyle_t *style, float x, float y, float width, float height);
void	UI_RendererStrokeRect(const uiRendererContext_t *ctx, const uiRendererRectStyle_t *style, float x, float y, float width, float height, float borderWidth);
