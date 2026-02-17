/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "renderer/view_setup.h"

#include <math.h>

void R_BuildViewPushEx(const refdef_t *fd,
                       float fov_x, float fov_y, float reflect_x,
                       float znear, float zfar,
                       renderer_view_push_t *out_push)
{
    if (!fd || !out_push) {
        return;
    }

    vec3_t view_axis[3];
    AnglesToAxis(fd->viewangles, view_axis);

    float xmax = znear * tanf(fov_x * (M_PIf / 360.0f));
    float xmin = -xmax;
    float ymax = znear * tanf(fov_y * (M_PIf / 360.0f));
    float ymin = -ymax;
    float width = xmax - xmin;
    float height = ymax - ymin;
    float depth = zfar - znear;

    out_push->proj[0] = reflect_x * 2.0f * znear / width;
    out_push->proj[4] = 0.0f;
    out_push->proj[8] = (xmax + xmin) / width;
    out_push->proj[12] = 0.0f;

    out_push->proj[1] = 0.0f;
    out_push->proj[5] = 2.0f * znear / height;
    out_push->proj[9] = (ymax + ymin) / height;
    out_push->proj[13] = 0.0f;

    out_push->proj[2] = 0.0f;
    out_push->proj[6] = 0.0f;
    out_push->proj[10] = -(zfar + znear) / depth;
    out_push->proj[14] = -2.0f * zfar * znear / depth;

    out_push->proj[3] = 0.0f;
    out_push->proj[7] = 0.0f;
    out_push->proj[11] = -1.0f;
    out_push->proj[15] = 0.0f;

    out_push->view[0] = -view_axis[1][0];
    out_push->view[4] = -view_axis[1][1];
    out_push->view[8] = -view_axis[1][2];
    out_push->view[12] = DotProduct(view_axis[1], fd->vieworg);

    out_push->view[1] = view_axis[2][0];
    out_push->view[5] = view_axis[2][1];
    out_push->view[9] = view_axis[2][2];
    out_push->view[13] = -DotProduct(view_axis[2], fd->vieworg);

    out_push->view[2] = -view_axis[0][0];
    out_push->view[6] = -view_axis[0][1];
    out_push->view[10] = -view_axis[0][2];
    out_push->view[14] = DotProduct(view_axis[0], fd->vieworg);

    out_push->view[3] = 0.0f;
    out_push->view[7] = 0.0f;
    out_push->view[11] = 0.0f;
    out_push->view[15] = 1.0f;
}

void R_BuildViewPush(const refdef_t *fd, float znear, float zfar,
                     renderer_view_push_t *out_push)
{
    if (!fd || !out_push) {
        return;
    }

    R_BuildViewPushEx(fd, fd->fov_x, fd->fov_y, 1.0f, znear, zfar, out_push);
}
