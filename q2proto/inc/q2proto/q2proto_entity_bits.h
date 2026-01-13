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
 * Entity bits type
 */
#ifndef Q2PROTO_ENTITY_H_
#define Q2PROTO_ENTITY_H_

#include "q2proto_limits.h"

#include <stdint.h>

/// \internal Bit array to store a single bit for each entity
typedef uint32_t q2proto_entity_bits[Q2PROTO_MAX_ENTITIES / 32];

#endif // Q2PROTO_ENTITY_H_
