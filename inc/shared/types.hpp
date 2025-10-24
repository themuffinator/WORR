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

/* Minimal shared type definitions used by both client and server headers. */
#pragma once

#include <stdint.h>

#ifndef BIT
#define BIT(n)          (1U << (n))
#endif

#ifndef BIT_ULL
#define BIT_ULL(n)      (1ULL << (n))
#endif

typedef unsigned char byte;

typedef int qhandle_t;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

typedef float mat4_t[16];

typedef struct vrect_s {
    int x;
    int y;
    int width;
    int height;
} vrect_t;

struct cvar_s;
typedef struct cvar_s cvar_t;
