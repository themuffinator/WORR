/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "renderer/renderer_api.h"

typedef enum ref_type_e {
    REF_TYPE_NONE = 0,
    REF_TYPE_GL,
    REF_TYPE_VKPT
} ref_type_t;

// Shift amount for storing fake emissive synthesis threshold.
#define IF_FAKE_EMISSIVE_THRESH_SHIFT 24

qhandle_t R_RegisterRawImage(const char *name, int width, int height, byte *pic,
                             imagetype_t type, imageflags_t flags);
void R_UnregisterImage(qhandle_t handle);

void R_ClearColor(void);
void R_SetAlpha(float alpha);
void R_SetAlphaScale(float alpha);
void R_SetColor(uint32_t color);
void R_DiscardRawPic(void);

bool R_SupportsDebugLines(void);
void R_AddDebugText_(const vec3_t origin, const vec3_t angles, const char *text,
                     float size, color_t color, uint32_t time, bool depth_test);

bool R_InterceptKey(unsigned key, bool down);
bool R_IsHDR(void);
