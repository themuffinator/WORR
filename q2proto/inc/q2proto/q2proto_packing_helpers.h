/*
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

/**\file
 * Packing helper definitions (functions & macros)
 */
#ifndef Q2PROTO_PACKING_HELPERS_H_
#define Q2PROTO_PACKING_HELPERS_H_

#include "q2proto_valenc.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define _Q2PROTO_PACKING_CONCAT2(X, Y)      X##Y
#define _Q2PROTO_PACKING_CONCAT(X, Y)       _Q2PROTO_PACKING_CONCAT2(X, Y)
#define _Q2PROTO_PACKING_NAME(BASE, SUFFIX) _Q2PROTO_PACKING_CONCAT(_q2proto_, _Q2PROTO_PACKING_CONCAT(BASE, SUFFIX))

static inline void _q2p_packing_coord_short_to_int(int32_t *packed_coord, const int16_t *src)
{
    packed_coord[0] = src[0];
    packed_coord[1] = src[1];
    packed_coord[2] = src[2];
}

static inline void _q2p_packing_coord_int_to_int(int32_t *packed_coord, const int32_t *src)
{
    memcpy(packed_coord, src, sizeof(int32_t) * 3);
}

static inline void _q2p_packing_coord_float_to_int(int32_t *packed_coord, const float *src)
{
    packed_coord[0] = _q2proto_valenc_coord2int(src[0]);
    packed_coord[1] = _q2proto_valenc_coord2int(src[1]);
    packed_coord[2] = _q2proto_valenc_coord2int(src[2]);
}

static inline void _q2p_packing_coord_short_to_floatbits(int32_t *packed_coord, const int16_t *src)
{
    packed_coord[0] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[0]));
    packed_coord[1] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[1]));
    packed_coord[2] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[2]));
}

static inline void _q2p_packing_coord_int_to_floatbits(int32_t *packed_coord, const int32_t *src)
{
    packed_coord[0] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[0]));
    packed_coord[1] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[1]));
    packed_coord[2] = _q2proto_valenc_float2bits(_q2proto_valenc_int2coord(src[2]));
}

static inline void _q2p_packing_coord_float_to_floatbits(int32_t *packed_coord, const float *src)
{
    packed_coord[0] = _q2proto_valenc_float2bits(src[0]);
    packed_coord[1] = _q2proto_valenc_float2bits(src[1]);
    packed_coord[2] = _q2proto_valenc_float2bits(src[2]);
}

static inline void _q2p_packing_angle_short_to_int(int16_t *dest, const int16_t *src)
{
    memcpy(dest, src, sizeof(int16_t) * 3);
}

static inline void _q2p_packing_angle_float_to_int(int16_t *dest, const float *src)
{
    dest[0] = _q2proto_valenc_angle2short(src[0]);
    dest[1] = _q2proto_valenc_angle2short(src[1]);
    dest[2] = _q2proto_valenc_angle2short(src[2]);
}

// Helper macro: pack an input coordinate, based on returned data type, to encoded integer
#define _Q2P_PACKING_PACK_COORD_VEC_TO_INT(DEST, SOURCE)                                   \
    _Generic((SOURCE),                                                                     \
        const int16_t *: _q2p_packing_coord_short_to_int(DEST, (const int16_t *)(SOURCE)), \
        const int32_t *: _q2p_packing_coord_int_to_int(DEST, (const int32_t *)(SOURCE)),   \
        const float *: _q2p_packing_coord_float_to_int(DEST, (const float *)(SOURCE)))
// Helper macro: pack an input coordinate, based on returned data type, to raw float
#define _Q2P_PACKING_PACK_COORD_VEC_TO_FLOATBITS(DEST, SOURCE)                                   \
    _Generic((SOURCE),                                                                           \
        const int16_t *: _q2p_packing_coord_short_to_floatbits(DEST, (const int16_t *)(SOURCE)), \
        const int32_t *: _q2p_packing_coord_int_to_floatbits(DEST, (const int32_t *)(SOURCE)),   \
        const float *: _q2p_packing_coord_float_to_floatbits(DEST, (const float *)(SOURCE)))
// Helper macro: pack an input angle, based on returned data type, to encoded integer
#define _Q2P_PACKING_PACK_ANGLE_VEC_TO_INT(DEST, SOURCE)                                   \
    _Generic((SOURCE),                                                                     \
        const int16_t *: _q2p_packing_angle_short_to_int(DEST, (const int16_t *)(SOURCE)), \
        const float *: _q2p_packing_angle_float_to_int(DEST, (const float *)(SOURCE)))

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_PACKING_HELPERS_H_
