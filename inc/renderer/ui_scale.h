/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include "renderer/renderer.h"

typedef struct {
    float base_scale;
    float virtual_width;
    float virtual_height;
} renderer_ui_scale_t;

int R_UIScaleBaseInt(int width, int height);
int R_UIScaleIntForCvar(int base_scale_int, cvar_t *var);
float R_UIScaleClamp(int width, int height, cvar_t *var);
renderer_ui_scale_t R_UIScaleCompute(int width, int height);
void R_UIScalePixelRectToVirtual(int x, int y, int w, int h, float base_scale,
                                 int *out_x, int *out_y, int *out_w, int *out_h);
bool R_UIScaleClipToPixels(const clipRect_t *clip, float base_scale, float draw_scale,
                           int framebuffer_width, int framebuffer_height,
                           clipRect_t *out_clip);
