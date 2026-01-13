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
 * Connection arguments
 */
#ifndef Q2PROTO_CONNECT_H_
#define Q2PROTO_CONNECT_H_

#include "q2proto_protocol.h"
#include "q2proto_string.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Connection information
typedef struct q2proto_connect_s {
    /// Protocol
    q2proto_protocol_t protocol;
    /// Protocol version
    int version;
    /// Port
    int qport;
    /// Challenge
    int32_t challenge;

    /// Initial user info
    q2proto_string_t userinfo;

    /// Maximum packet length
    int packet_length;
    /// zlib compression available?
    bool has_zlib;

    /// Q2PRO netchan type
    int q2pro_nctype;
} q2proto_connect_t;

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_CONNECT_H_
