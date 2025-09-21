/*
Copyright (C) 1997-2001 Id Software, Inc.

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

/*
==============================================================================

SERVER API EXTENSIONS

==============================================================================
*/

#define FILESYSTEM_API_V1 "FILESYSTEM_API_V1"

typedef struct {
    int64_t     (*OpenFile)(const char *path, qhandle_t *f, unsigned mode); // returns file length
    int         (*CloseFile)(qhandle_t f);
    int         (*LoadFile)(const char *path, void **buffer, unsigned flags, unsigned tag);

    int         (*ReadFile)(void *buffer, size_t len, qhandle_t f);
    int         (*WriteFile)(const void *buffer, size_t len, qhandle_t f);
    int         (*FlushFile)(qhandle_t f);
    int64_t     (*TellFile)(qhandle_t f);
    int         (*SeekFile)(qhandle_t f, int64_t offset, int whence);
    int         (*ReadLine)(qhandle_t f, char *buffer, size_t size);

    void        **(*ListFiles)(const char *path, const char *filter, unsigned flags, int *count_p);
    void        (*FreeFileList)(void **list);

    const char  *(*ErrorString)(int error);
} filesystem_api_v1_t;

#define DEBUG_DRAW_API_V1 "DEBUG_DRAW_API_V1"

typedef struct {
    void (*ClearDebugLines)(void);
    void (*AddDebugLine)(const vec3_t start, const vec3_t end, color_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugPoint)(const vec3_t point, float size, color_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugAxis)(const vec3_t origin, const vec3_t angles, float size, uint32_t time, qboolean depth_test);
    void (*AddDebugBounds)(const vec3_t mins, const vec3_t maxs, color_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugSphere)(const vec3_t origin, float radius, color_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCircle)(const vec3_t origin, float radius, color_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCylinder)(const vec3_t origin, float half_height, float radius, color_t color, uint32_t time,
                             qboolean depth_test);
    void (*AddDebugArrow)(const vec3_t start, const vec3_t end, float size, color_t line_color,
                          color_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugCurveArrow)(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                               color_t line_color, color_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugText)(const vec3_t origin, const vec3_t angles, const char *text,
                         float size, color_t color, uint32_t time, qboolean depth_test);
} debug_draw_api_v1_t;
