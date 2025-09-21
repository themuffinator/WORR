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

#define Q2PROTO_BUILD
#include "q2proto/q2proto_coords.h"

#include "q2proto_internal_defs.h"
#include "q2proto_internal_io.h"

#include <assert.h>
#include <limits.h>

static inline int8_t clip_int8(int a) { return ((a + 0x80U) & ~0xFF) ? (a >> 31) ^ 0x7F : a; }

static inline int16_t clip_int16(int a) { return ((a + 0x8000U) & ~0xFFFF) ? (a >> 31) ^ 0x7FFF : a; }

void q2proto_var_coords_set_float_comp(q2proto_var_coords_t *coord, int comp, float f)
{
    coord->comps[comp].f = f;
    coord->float_bits |= BIT(comp);
}

void q2proto_var_coords_set_int_comp(q2proto_var_coords_t *coord, int comp, int32_t i)
{
    coord->comps[comp].i = i;
    coord->float_bits &= ~(BIT(comp));
}

void q2proto_var_coords_set_short_comp(q2proto_var_coords_t *coord, int comp, int16_t s)
{
    q2proto_var_coords_set_int_comp(coord, comp, s);
}

void q2proto_var_coords_set_int_unscaled_comp(q2proto_var_coords_t *coord, int comp, int32_t i)
{
    if (i > INT32_MAX / 8)
        i = INT32_MAX / 8;
    else if (i < INT32_MIN / 8)
        i = INT32_MIN / 8;
    q2proto_var_coords_set_int_comp(coord, comp, i * 8);
}

void q2proto_var_coords_set_short_unscaled_comp(q2proto_var_coords_t *coord, int comp, int16_t s)
{
    q2proto_var_coords_set_int_unscaled_comp(coord, comp, s);
}

float q2proto_var_coords_get_float_comp(const q2proto_var_coords_t *coord, int comp)
{
    if (coord->float_bits & BIT(comp))
        return coord->comps[comp].f;
    else
        return _q2proto_valenc_int2coord(coord->comps[comp].i);
}

int32_t q2proto_var_coords_get_int_comp(const q2proto_var_coords_t *coord, int comp)
{
    if (coord->float_bits & BIT(comp))
        return _q2proto_valenc_coord2int(coord->comps[comp].f);
    else
        return coord->comps[comp].i;
}

int16_t q2proto_var_coords_get_short_comp(const q2proto_var_coords_t *coord, int comp)
{
    return q2proto_var_coords_get_int_comp(coord, comp);
}

int32_t q2proto_var_coords_get_int_unscaled_comp(const q2proto_var_coords_t *coord, int comp)
{
    return q2proto_var_coords_get_int_comp(coord, comp) / 8;
}

int16_t q2proto_var_coords_get_short_unscaled_comp(const q2proto_var_coords_t *coord, int comp)
{
    return clip_int16(q2proto_var_coords_get_int_unscaled_comp(coord, comp));
}

void q2proto_var_coord_set_float(q2proto_var_coord_t *coord, float f)
{
    coord->val.f = f;
    coord->type = 1;
}

void q2proto_var_coord_set_int(q2proto_var_coord_t *coord, int32_t i)
{
    coord->val.i = i;
    coord->type = 0;
}

void q2proto_var_coord_set_int_unscaled(q2proto_var_coord_t *coord, int32_t i)
{
    if (i > INT32_MAX / 8)
        i = INT32_MAX / 8;
    else if (i < INT32_MIN / 8)
        i = INT32_MIN / 8;
    q2proto_var_coord_set_int(coord, i * 8);
}

float q2proto_var_coord_get_float(const q2proto_var_coord_t *coord)
{
    if (coord->type != 0)
        return coord->val.f;
    else
        return _q2proto_valenc_int2coord(coord->val.i);
}

int32_t q2proto_var_coord_get_int(const q2proto_var_coord_t *coord)
{
    if (coord->type != 0)
        return _q2proto_valenc_coord2int(coord->val.f);
    else
        return coord->val.i;
}

int32_t q2proto_var_coord_get_int_unscaled(const q2proto_var_coord_t *coord)
{
    return q2proto_var_coord_get_int(coord) / 8;
}

typedef enum {
    VAR_ANGLES_TYPE_SHORT = 0,
    VAR_ANGLES_TYPE_CHAR = 1,
    VAR_ANGLES_TYPE_FLOAT = 2,
} var_angles_type_t;

static inline void set_var_angles_comp_type(q2proto_var_angles_t *angle, int comp, var_angles_type_t type)
{
    angle->float_bits &= ~(0x3 << (comp * 2));
    angle->float_bits |= type << (comp * 2);
}

void q2proto_var_angles_set_float_comp(q2proto_var_angles_t *angle, int comp, float f)
{
    angle->comps[comp].f = f;
    set_var_angles_comp_type(angle, comp, VAR_ANGLES_TYPE_FLOAT);
}

void q2proto_var_angles_set_short_comp(q2proto_var_angles_t *angle, int comp, int16_t s)
{
    angle->comps[comp].s = s;
    set_var_angles_comp_type(angle, comp, VAR_ANGLES_TYPE_SHORT);
}

void q2proto_var_angles_set_char_comp(q2proto_var_angles_t *angle, int comp, int8_t c)
{
    angle->comps[comp].c = c;
    set_var_angles_comp_type(angle, comp, VAR_ANGLES_TYPE_CHAR);
}

static inline var_angles_type_t get_var_angles_comp_type(const q2proto_var_angles_t *angle, int comp)
{
    return (angle->float_bits >> (comp * 2)) & 0x3;
}

float q2proto_var_angles_get_float_comp(const q2proto_var_angles_t *angle, int comp)
{
    switch (get_var_angles_comp_type(angle, comp)) {
    case VAR_ANGLES_TYPE_SHORT:
        return _q2proto_valenc_short2angle(angle->comps[comp].s);
    case VAR_ANGLES_TYPE_CHAR:
        return _q2proto_valenc_char2angle(angle->comps[comp].c);
    case VAR_ANGLES_TYPE_FLOAT:
        return angle->comps[comp].f;
    }
    return 0;
}

int16_t q2proto_var_angles_get_short_comp(const q2proto_var_angles_t *angle, int comp)
{
    switch (get_var_angles_comp_type(angle, comp)) {
    case VAR_ANGLES_TYPE_SHORT:
        return angle->comps[comp].s;
    case VAR_ANGLES_TYPE_CHAR:
        return angle->comps[comp].c * 0x101;
    case VAR_ANGLES_TYPE_FLOAT:
        return _q2proto_valenc_angle2short(angle->comps[comp].f);
    }
    return 0;
}

int8_t q2proto_var_angles_get_char_comp(const q2proto_var_angles_t *angle, int comp)
{
    switch (get_var_angles_comp_type(angle, comp)) {
    case VAR_ANGLES_TYPE_SHORT:
        return angle->comps[comp].s >> 8;
    case VAR_ANGLES_TYPE_CHAR:
        return angle->comps[comp].c;
    case VAR_ANGLES_TYPE_FLOAT:
        return _q2proto_valenc_angle2char(angle->comps[comp].f);
    }
    return 0;
}

typedef enum {
    VAR_SMALL_OFFSETS_TYPE_FLOAT = 0,
    VAR_SMALL_OFFSETS_TYPE_CHAR = 1,
    VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET = 2,
    VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET = 3,

    _VAR_SMALL_OFFSETS_TYPE_MAX
} var_small_offsets_type_t;

#define VAR_SMALL_OFFSETS_TYPE_BITS 2
#define VAR_SMALL_OFFSETS_TYPE_MASK (BIT(VAR_SMALL_OFFSETS_TYPE_BITS) - 1)

static inline void set_var_small_offsets_comp_type(q2proto_var_small_offsets_t *coord, int comp,
                                                   var_small_offsets_type_t type)
{
    static_assert(sizeof(coord->type_bits) * CHAR_BIT >= 3 * VAR_SMALL_OFFSETS_TYPE_BITS,
                  "VAR_SMALL_OFFSETS_TYPE_BITS is too large");
    static_assert((_VAR_SMALL_OFFSETS_TYPE_MAX - 1) <= VAR_SMALL_OFFSETS_TYPE_MASK,
                  "VAR_SMALL_OFFSETS_TYPE_MASK is too small");
    coord->type_bits &= ~(VAR_SMALL_OFFSETS_TYPE_MASK << (comp * VAR_SMALL_OFFSETS_TYPE_BITS));
    coord->type_bits |= type << (comp * VAR_SMALL_OFFSETS_TYPE_BITS);
}

void q2proto_var_small_offsets_set_float_comp(q2proto_var_small_offsets_t *coord, int comp, float f)
{
    coord->comps[comp].f = f;
    set_var_small_offsets_comp_type(coord, comp, VAR_SMALL_OFFSETS_TYPE_FLOAT);
}

void q2proto_var_small_offsets_set_char_comp(q2proto_var_small_offsets_t *coord, int comp, int8_t c)
{
    coord->comps[comp].c = c;
    set_var_small_offsets_comp_type(coord, comp, VAR_SMALL_OFFSETS_TYPE_CHAR);
}

void q2proto_var_small_offsets_set_q2repro_viewoffset_comp(q2proto_var_small_offsets_t *coord, int comp, int16_t x)
{
    coord->comps[comp].s = x;
    set_var_small_offsets_comp_type(coord, comp, VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET);
}

void q2proto_var_small_offsets_set_q2repro_gunoffset_comp(q2proto_var_small_offsets_t *coord, int comp, int16_t x)
{
    coord->comps[comp].s = x;
    set_var_small_offsets_comp_type(coord, comp, VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET);
}

static inline var_small_offsets_type_t get_var_small_offsets_comp_type(const q2proto_var_small_offsets_t *coord,
                                                                       int comp)
{
    return (coord->type_bits >> (comp * VAR_SMALL_OFFSETS_TYPE_BITS)) & VAR_SMALL_OFFSETS_TYPE_MASK;
}

float q2proto_var_small_offsets_get_float_comp(const q2proto_var_small_offsets_t *coord, int comp)
{
    switch (get_var_small_offsets_comp_type(coord, comp)) {
    case VAR_SMALL_OFFSETS_TYPE_FLOAT:
        return coord->comps[comp].f;
    case VAR_SMALL_OFFSETS_TYPE_CHAR:
        return _q2proto_valenc_char2smalloffset(coord->comps[comp].c);
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET:
        return _q2proto_valenc_q2repro_short2viewoffset(coord->comps[comp].s);
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET:
        return _q2proto_valenc_q2repro_short2gunoffset(coord->comps[comp].s);
    case _VAR_SMALL_OFFSETS_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int8_t q2proto_var_small_offsets_get_char_comp(const q2proto_var_small_offsets_t *coord, int comp)
{
    switch (get_var_small_offsets_comp_type(coord, comp)) {
    case VAR_SMALL_OFFSETS_TYPE_FLOAT:
        return _q2proto_valenc_smalloffset2char(coord->comps[comp].f);
    case VAR_SMALL_OFFSETS_TYPE_CHAR:
        return coord->comps[comp].c;
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET:
        return clip_int8(coord->comps[comp].s >> 2);
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET:
        return clip_int8(coord->comps[comp].s >> 7);
    case _VAR_SMALL_OFFSETS_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int16_t q2proto_var_small_offsets_get_q2repro_viewoffset_comp(const q2proto_var_small_offsets_t *coord, int comp)
{
    switch (get_var_small_offsets_comp_type(coord, comp)) {
    case VAR_SMALL_OFFSETS_TYPE_FLOAT:
        return _q2proto_valenc_q2repro_viewoffset2short(coord->comps[comp].f);
    case VAR_SMALL_OFFSETS_TYPE_CHAR:
        return coord->comps[comp].c << 2;
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET:
        return coord->comps[comp].s;
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET:
        return clip_int16(coord->comps[comp].s >> 5);
    case _VAR_SMALL_OFFSETS_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int16_t q2proto_var_small_offsets_get_q2repro_gunoffset_comp(const q2proto_var_small_offsets_t *coord, int comp)
{
    switch (get_var_small_offsets_comp_type(coord, comp)) {
    case VAR_SMALL_OFFSETS_TYPE_FLOAT:
        return _q2proto_valenc_q2repro_gunoffset2short(coord->comps[comp].f);
    case VAR_SMALL_OFFSETS_TYPE_CHAR:
        return coord->comps[comp].c << 7;
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_VIEWOFFSET:
        return clip_int16(coord->comps[comp].s << 5);
    case VAR_SMALL_OFFSETS_TYPE_Q2REPRO_GUNOFFSET:
        return coord->comps[comp].s;
    case _VAR_SMALL_OFFSETS_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

typedef enum {
    VAR_SMALL_ANGLES_TYPE_FLOAT = 0,
    VAR_SMALL_ANGLES_TYPE_CHAR = 1,
    VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE = 2,
    VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE = 3,

    _VAR_SMALL_ANGLES_TYPE_MAX
} var_small_angles_type_t;

#define VAR_SMALL_ANGLES_TYPE_BITS 2
#define VAR_SMALL_ANGLES_TYPE_MASK (BIT(VAR_SMALL_ANGLES_TYPE_BITS) - 1)

static inline void set_var_small_angles_comp_type(q2proto_var_small_angles_t *coord, int comp,
                                                  var_small_angles_type_t type)
{
    static_assert(sizeof(coord->type_bits) * CHAR_BIT >= 3 * VAR_SMALL_ANGLES_TYPE_BITS,
                  "VAR_SMALL_ANGLES_TYPE_BITS is too large");
    static_assert((_VAR_SMALL_ANGLES_TYPE_MAX - 1) <= VAR_SMALL_ANGLES_TYPE_MASK,
                  "VAR_SMALL_ANGLES_TYPE_MASK is too small");
    coord->type_bits &= ~(VAR_SMALL_ANGLES_TYPE_MASK << (comp * VAR_SMALL_ANGLES_TYPE_BITS));
    coord->type_bits |= type << (comp * VAR_SMALL_ANGLES_TYPE_BITS);
}

void q2proto_var_small_angles_set_float_comp(q2proto_var_small_angles_t *angle, int comp, float f)
{
    angle->comps[comp].f = f;
    set_var_small_angles_comp_type(angle, comp, VAR_SMALL_ANGLES_TYPE_FLOAT);
}

void q2proto_var_small_angles_set_char_comp(q2proto_var_small_angles_t *angle, int comp, int8_t c)
{
    angle->comps[comp].c = c;
    set_var_small_angles_comp_type(angle, comp, VAR_SMALL_ANGLES_TYPE_CHAR);
}

void q2proto_var_small_angles_set_q2repro_kick_angles_comp(q2proto_var_small_angles_t *angle, int comp, int16_t x)
{
    angle->comps[comp].s = x;
    set_var_small_angles_comp_type(angle, comp, VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE);
}

void q2proto_var_small_angles_set_q2repro_gunangles_comp(q2proto_var_small_angles_t *angle, int comp, int16_t x)
{
    angle->comps[comp].s = x;
    set_var_small_angles_comp_type(angle, comp, VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE);
}

static inline var_small_angles_type_t get_var_small_angles_comp_type(const q2proto_var_small_angles_t *coord, int comp)
{
    return (coord->type_bits >> (comp * VAR_SMALL_ANGLES_TYPE_BITS)) & VAR_SMALL_ANGLES_TYPE_MASK;
}

float q2proto_var_small_angles_get_float_comp(const q2proto_var_small_angles_t *angle, int comp)
{
    switch (get_var_small_angles_comp_type(angle, comp)) {
    case VAR_SMALL_ANGLES_TYPE_FLOAT:
        return angle->comps[comp].f;
    case VAR_SMALL_ANGLES_TYPE_CHAR:
        return _q2proto_valenc_char2smallangle(angle->comps[comp].c);
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE:
        return _q2proto_valenc_q2repro_short2kick_angle(angle->comps[comp].s);
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE:
        return _q2proto_valenc_q2repro_short2gunangle(angle->comps[comp].s);
    case _VAR_SMALL_ANGLES_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int8_t q2proto_var_small_angles_get_char_comp(const q2proto_var_small_angles_t *angle, int comp)
{
    switch (get_var_small_angles_comp_type(angle, comp)) {
    case VAR_SMALL_ANGLES_TYPE_FLOAT:
        return _q2proto_valenc_smallangle2char(angle->comps[comp].f);
    case VAR_SMALL_ANGLES_TYPE_CHAR:
        return angle->comps[comp].c;
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE:
        return clip_int8(angle->comps[comp].s >> 8);
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE:
        return clip_int8(angle->comps[comp].s >> 10);
    case _VAR_SMALL_ANGLES_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int16_t q2proto_var_small_angles_get_q2repro_kick_angles_comp(const q2proto_var_small_angles_t *angle, int comp)
{
    switch (get_var_small_angles_comp_type(angle, comp)) {
    case VAR_SMALL_ANGLES_TYPE_FLOAT:
        return _q2proto_valenc_q2repro_kick_angle2short(angle->comps[comp].f);
    case VAR_SMALL_ANGLES_TYPE_CHAR:
        return angle->comps[comp].c << 8;
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE:
        return angle->comps[comp].s;
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE:
        return angle->comps[comp].s >> 2;
    case _VAR_SMALL_ANGLES_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

int16_t q2proto_var_small_angles_get_q2repro_gunangles_comp(const q2proto_var_small_angles_t *angle, int comp)
{
    switch (get_var_small_angles_comp_type(angle, comp)) {
    case VAR_SMALL_ANGLES_TYPE_FLOAT:
        return _q2proto_valenc_q2repro_gunangle2short(angle->comps[comp].f);
    case VAR_SMALL_ANGLES_TYPE_CHAR:
        return angle->comps[comp].s >> 10;
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_KICK_ANGLE:
        return angle->comps[comp].s << 2;
    case VAR_SMALL_ANGLES_TYPE_Q2REPRO_GUNANGLE:
        return angle->comps[comp].s;
    case _VAR_SMALL_ANGLES_TYPE_MAX:
        break;
    }
    assert(false);
    return 0;
}

void q2proto_var_color_set_float_comp(q2proto_var_color_t *blend, int comp, float f)
{
    blend->comps[comp].f = f;
    blend->float_bits |= BIT(comp);
}

void q2proto_var_color_set_byte_comp(q2proto_var_color_t *blend, int comp, uint8_t b)
{
    blend->comps[comp].c = b;
    blend->float_bits &= ~(BIT(comp));
}

float q2proto_var_color_get_float_comp(const q2proto_var_color_t *blend, int comp)
{
    if (blend->float_bits & BIT(comp))
        return blend->comps[comp].f;
    else
        return _q2proto_valenc_byte2color(blend->comps[comp].c);
}

uint8_t q2proto_var_color_get_byte_comp(const q2proto_var_color_t *blend, int comp)
{
    if (blend->float_bits & BIT(comp))
        return _q2proto_valenc_color2byte(blend->comps[comp].f);
    else
        return blend->comps[comp].c;
}

typedef enum {
    VAR_FRACTION_TYPE_WORD = 0,
    VAR_FRACTION_TYPE_BYTE = 1,
    VAR_FRACTION_TYPE_FLOAT = 2,
} var_fraction_type_t;

void q2proto_var_fraction_set_float(q2proto_var_fraction_t *frac, float f)
{
    frac->val.f = f;
    frac->type = VAR_FRACTION_TYPE_FLOAT;
}

void q2proto_var_fraction_set_word(q2proto_var_fraction_t *frac, uint16_t w)
{
    frac->val.w = w;
    frac->type = VAR_FRACTION_TYPE_WORD;
}

void q2proto_var_fraction_set_byte(q2proto_var_fraction_t *frac, uint8_t b)
{
    frac->val.b = b;
    frac->type = VAR_FRACTION_TYPE_BYTE;
}

float q2proto_var_fraction_get_float(const q2proto_var_fraction_t *frac)
{
    switch (frac->type) {
    case VAR_FRACTION_TYPE_WORD:
        return frac->val.w / 65535.f;
    case VAR_FRACTION_TYPE_BYTE:
        return frac->val.b / 255.f;
    case VAR_FRACTION_TYPE_FLOAT:
        return frac->val.f;
    }
    return 0;
}

uint16_t q2proto_var_fraction_get_word(const q2proto_var_fraction_t *frac)
{
    switch (frac->type) {
    case VAR_FRACTION_TYPE_WORD:
        return frac->val.w;
    case VAR_FRACTION_TYPE_BYTE:
        return frac->val.b * 0x101;
    case VAR_FRACTION_TYPE_FLOAT:
        return _q2proto_valenc_clamped_mul(frac->val.f, UINT16_MAX, 0, UINT16_MAX);
    }
    return 0;
}

uint8_t q2proto_var_fraction_get_byte(const q2proto_var_fraction_t *frac)
{
    switch (frac->type) {
    case VAR_FRACTION_TYPE_WORD:
        return frac->val.w >> 8;
    case VAR_FRACTION_TYPE_BYTE:
        return frac->val.b;
    case VAR_FRACTION_TYPE_FLOAT:
        return _q2proto_valenc_clamped_mul(frac->val.f, UINT8_MAX, 0, UINT8_MAX);
    }
    return 0;
}
