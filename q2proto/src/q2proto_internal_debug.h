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
 * Debug helpers
 */
#ifndef Q2PROTO_INTERNAL_DEBUG_H_
#define Q2PROTO_INTERNAL_DEBUG_H_

#include "q2proto/q2proto.h"
#include "q2proto_internal_common.h"

/**\def SHOWNET
 * Output 'shownet' feedback.
 */
#if Q2PROTO_SHOWNET
    #if defined(_MSC_VER)
        #define SHOWNET(IO_ARG, LEVEL, OFFSET, MSG, ...) \
            q2protodbg_shownet((IO_ARG), (LEVEL), (OFFSET), (MSG), ##__VA_ARGS__)
    #else
        #define SHOWNET(IO_ARG, LEVEL, OFFSET, MSG, ...) \
            q2protodbg_shownet((IO_ARG), (LEVEL), (OFFSET), (MSG)__VA_OPT__(, ) __VA_ARGS__)
    #endif
#else
    #define SHOWNET(IO_ARG, LEVEL, OFFSET, MSG, ...) (void)0
#endif

/**\name Debug output helpers
 * @{ */
/// Return string describing the (common) server command \a command, or \c NULL.
Q2PROTO_PRIVATE_API const char *q2proto_debug_common_svc_string(int command);

/// Stringify entity delta bits (U_xxx) to buffer
Q2PROTO_PRIVATE_API void q2proto_debug_common_entity_delta_bits_to_str(char *buf, size_t size, uint64_t bits);
/// Print default "shownet" feedback for entity delta bits (U_xxx)
static inline void q2proto_debug_shownet_entity_delta_bits(uintptr_t io_arg, const char *prefix, uint16_t entnum,
                                                           uint64_t bits)
{
#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && bits) {
        char buf[1024];
        q2proto_debug_common_entity_delta_bits_to_str(buf, sizeof(buf), bits);
        SHOWNET(io_arg, 2, -q2proto_common_entity_bits_size(bits), "%s%i %s", prefix, entnum, buf);
    }
#endif
}

/// Stringify player delta bits (PS_xxx) to buffer
Q2PROTO_PRIVATE_API void q2proto_debug_common_player_delta_bits_to_str(char *buf, size_t size, uint32_t bits);

/// Stringify extra player delta bits (EPS_xxx) to buffer
Q2PROTO_PRIVATE_API void q2proto_debug_common_player_delta_extrabits_to_str(char *buf, size_t size, uint32_t bits);
/** @} */


#endif // Q2PROTO_INTERNAL_DEBUG_H_
