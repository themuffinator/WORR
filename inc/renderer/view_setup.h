/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include "shared/shared.h"
#include "renderer/renderer.h"

typedef struct {
    mat4_t proj;
    mat4_t view;
} renderer_view_push_t;

void R_BuildViewPushEx(const refdef_t *fd,
                       float fov_x, float fov_y, float reflect_x,
                       float znear, float zfar,
                       renderer_view_push_t *out_push);

void R_BuildViewPush(const refdef_t *fd, float znear, float zfar,
                     renderer_view_push_t *out_push);
