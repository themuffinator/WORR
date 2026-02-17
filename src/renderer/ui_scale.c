/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "shared/shared.h"
#include "renderer/renderer_api.h"
#include "renderer/ui_scale.h"

int R_UIScaleBaseInt(int width, int height)
{
    if (width < 1 || height < 1)
        return 1;

    float scale_x = (float)width / VIRTUAL_SCREEN_WIDTH;
    float scale_y = (float)height / VIRTUAL_SCREEN_HEIGHT;
    float base_scale = max(scale_x, scale_y);
    int base_scale_int = (int)base_scale;

    if (base_scale_int < 1)
        base_scale_int = 1;

    return base_scale_int;
}

int R_UIScaleIntForCvar(int base_scale_int, cvar_t *var)
{
    if (base_scale_int < 1)
        base_scale_int = 1;

    float extra_scale = 1.0f;
    if (var && var->value)
        extra_scale = Cvar_ClampValue(var, 0.25f, 10.0f);

    int ui_scale_int = (int)((float)base_scale_int * extra_scale);
    if (ui_scale_int < 1)
        ui_scale_int = 1;

    return ui_scale_int;
}

float R_UIScaleClamp(int width, int height, cvar_t *var)
{
    if (!var)
        return 1.0f;

    int base_scale_int = R_UIScaleBaseInt(width, height);
    int ui_scale_int = R_UIScaleIntForCvar(base_scale_int, var);

    return (float)base_scale_int / (float)ui_scale_int;
}

renderer_ui_scale_t R_UIScaleCompute(int width, int height)
{
    renderer_ui_scale_t metrics;
    int base_scale_int = R_UIScaleBaseInt(width, height);

    metrics.base_scale = (float)base_scale_int;

    metrics.virtual_width = (float)(width / base_scale_int);
    if (metrics.virtual_width <= 0.0f)
        metrics.virtual_width = 1.0f;

    metrics.virtual_height = (float)(height / base_scale_int);
    if (metrics.virtual_height <= 0.0f)
        metrics.virtual_height = 1.0f;

    return metrics;
}

void R_UIScalePixelRectToVirtual(int x, int y, int w, int h, float base_scale,
                                 int *out_x, int *out_y, int *out_w, int *out_h)
{
    float inv_base = base_scale > 0.0f ? (1.0f / base_scale) : 1.0f;
    int x0 = Q_rint(x * inv_base);
    int y0 = Q_rint(y * inv_base);
    int x1 = Q_rint((x + w) * inv_base);
    int y1 = Q_rint((y + h) * inv_base);

    if (out_x)
        *out_x = x0;
    if (out_y)
        *out_y = y0;
    if (out_w)
        *out_w = max(0, x1 - x0);
    if (out_h)
        *out_h = max(0, y1 - y0);
}

bool R_UIScaleClipToPixels(const clipRect_t *clip, float base_scale, float draw_scale,
                           int framebuffer_width, int framebuffer_height,
                           clipRect_t *out_clip)
{
    if (!clip || !out_clip)
        return false;

    float safe_scale = draw_scale;
    if (safe_scale <= 0.0f)
        safe_scale = 1.0f;

    float pixel_scale = base_scale / safe_scale;

    out_clip->left = Q_rint(clip->left * pixel_scale);
    out_clip->top = Q_rint(clip->top * pixel_scale);
    out_clip->right = Q_rint(clip->right * pixel_scale);
    out_clip->bottom = Q_rint(clip->bottom * pixel_scale);

    if (out_clip->left < 0)
        out_clip->left = 0;
    if (out_clip->top < 0)
        out_clip->top = 0;
    if (out_clip->right > framebuffer_width)
        out_clip->right = framebuffer_width;
    if (out_clip->bottom > framebuffer_height)
        out_clip->bottom = framebuffer_height;

    if (out_clip->right < out_clip->left)
        return false;
    if (out_clip->bottom < out_clip->top)
        return false;

    return true;
}
