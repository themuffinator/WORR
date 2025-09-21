/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2011 Richard Stanway
Copyright (C) 2003-2024 Andrey Nazarov
Copyright (C) 2024 Frank Richter

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

#define Q2PROTO_BUILD
#include "q2proto/q2proto_solid.h"

#include "q2proto_internal_common.h"
#include "q2proto_internal_defs.h"

// Assumes x/y are equal and symmetric. Z does not have to be symmetric, and z maxs can be negative.
uint16_t q2proto_pack_solid_16(const q2proto_vec3_t mins, const q2proto_vec3_t maxs)
{
    int x = (int)(maxs[0] / 8);
    int zd = (int)(-mins[2] / 8);
    int zu = (int)((maxs[2] + 32) / 8);

    x = CLAMP(x, 1, 31);
    zd = CLAMP(zd, 1, 31);
    zu = CLAMP(zu, 1, 63);

    return (zu << 10) | (zd << 5) | x;
}

void q2proto_unpack_solid_16(uint16_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs)
{
    int x = 8 * (solid & 31);
    int zd = 8 * ((solid >> 5) & 31);
    int zu = 8 * ((solid >> 10) & 63) - 32;

    mins[0] = (float)-x;
    mins[1] = (float)-x;
    mins[2] = (float)-zd;
    maxs[0] = (float)x;
    maxs[1] = (float)x;
    maxs[2] = (float)zu;
}

// Assumes x/y are equal and symmetric. Z does not have to be symmetric, and z maxs can be negative.
uint32_t q2proto_pack_solid_32_r1q2(const q2proto_vec3_t mins, const q2proto_vec3_t maxs)
{
    int x = (int)(maxs[0]);
    int zd = (int)(-mins[2]);
    int zu = (int)(maxs[2] + 32768);

    x = CLAMP(x, 1, 255);
    zd = CLAMP(zd, 0, 0xff);
    zu = CLAMP(zu, 0, 0xffff);

    return ((uint32_t)zu << 16) | (zd << 8) | x;
}

void q2proto_unpack_solid_32_r1q2(uint32_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs)
{
    int x = solid & 255;
    int zd = (solid >> 8) & 255;
    int zu = ((solid >> 16) & 65535) - 32768;

    mins[0] = (float)-x;
    mins[1] = (float)-x;
    mins[2] = (float)-zd;
    maxs[0] = (float)x;
    maxs[1] = (float)x;
    maxs[2] = (float)zu;
}

// Assumes x/y are equal. Z does not have to be symmetric, and z maxs can be negative.
uint32_t q2proto_pack_solid_32_q2pro_v2(const q2proto_vec3_t mins, const q2proto_vec3_t maxs)
{
    int x = (int)(maxs[0]);
    int y = (int)(maxs[1]);
    int zd = (int)(-mins[2]);
    int zu = (int)(maxs[2] + 32);

    x = CLAMP(x, 1, 255);
    y = CLAMP(y, 1, 255);
    zd = CLAMP(zd, 0, 255);
    zu = CLAMP(zu, 0, 255);

    return (zu << 24) | (zd << 16) | (y << 8) | x;
}

void q2proto_unpack_solid_32_q2pro_v2(uint32_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs)
{
    int x = solid & 255;
    int y = (solid >> 8) & 255;
    int zd = (solid >> 16) & 255;
    int zu = ((solid >> 24) & 255) - 32;

    mins[0] = (float)-x;
    mins[1] = (float)-y;
    mins[2] = (float)-zd;
    maxs[0] = (float)x;
    maxs[1] = (float)y;
    maxs[2] = (float)zu;
}
