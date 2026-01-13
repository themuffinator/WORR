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
 * Solid packing/unpacking functions
 */
#ifndef Q2PROTO_SOLID_H_
#define Q2PROTO_SOLID_H_

#include "q2proto_coords.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**\name Solid packing/unpacking functions
 * @{ */
/// Pack a bounding box into a 16-bit integer (vanilla encoding)
Q2PROTO_PUBLIC_API uint16_t q2proto_pack_solid_16(const q2proto_vec3_t mins, const q2proto_vec3_t maxs);
/// Unpack a bounding box into from a 16-bit integer (vanilla encoding)
Q2PROTO_PUBLIC_API void q2proto_unpack_solid_16(uint16_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs);

/// Pack a bounding box into a 32-bit integer (R1Q2 encoding)
Q2PROTO_PUBLIC_API uint32_t q2proto_pack_solid_32_r1q2(const q2proto_vec3_t mins, const q2proto_vec3_t maxs);
/// Unpack a bounding box from a 32-bit integer (R1Q2 encoding)
Q2PROTO_PUBLIC_API void q2proto_unpack_solid_32_r1q2(uint32_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs);

/// Pack a bounding box into a 32-bit integer (Q2PROv2 encoding)
Q2PROTO_PUBLIC_API uint32_t q2proto_pack_solid_32_q2pro_v2(const q2proto_vec3_t mins, const q2proto_vec3_t maxs);
/// Unpack a bounding box from a 32-bit integer (Q2PROv2 encoding)
Q2PROTO_PUBLIC_API void q2proto_unpack_solid_32_q2pro_v2(uint32_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs);
/** @} */

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_SOLID_H_
