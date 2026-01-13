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
 * Entity bits, internal functions
 */
#ifndef Q2PROTO_INTERNAL_ENTITY_BITS_H_
#define Q2PROTO_INTERNAL_ENTITY_BITS_H_

#include "q2proto/q2proto_entity_bits.h"
#include "q2proto_internal_defs.h"

static inline bool q2proto_get_entity_bit(const q2proto_entity_bits bits, unsigned entnum)
{
    // FIXME assert entnum range?
    return bits[entnum >> 5] & BIT(entnum & 0x1f);
}

static inline void q2proto_set_entity_bit(q2proto_entity_bits bits, unsigned entnum, bool flag)
{
    // FIXME assert entnum range?
    if (flag)
        bits[entnum >> 5] |= BIT(entnum & 0x1f);
    else
        bits[entnum >> 5] &= ~BIT(entnum & 0x1f);
}

#endif // Q2PROTO_INTERNAL_ENTITY_BITS_H_
