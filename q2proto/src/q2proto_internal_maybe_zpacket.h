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
 * Write a zpacket, maybe (if supported by protocol & configuration)
 */
#ifndef Q2PROTO_INTERNAL_MAYBE_ZPACKET_H_
#define Q2PROTO_INTERNAL_MAYBE_ZPACKET_H_

#include "q2proto/q2proto.h"

/// State for zpacket writing
typedef struct q2proto_maybe_zpacket_s {
#if Q2PROTO_COMPRESSION_DEFLATE
    uintptr_t original_io_arg;
    bool deflate_enabled;
    uint8_t zpacket_cmd;
#else
    char dummy;
#endif
} q2proto_maybe_zpacket_t;

/**
 * Start zpacket writing (maybe - if supported by protocol & configuration).
 * \param context Server context.
 * \param deflate_args Deflate arguments, passed through to q2protoio_deflate_begin().
 * \param io_arg "I/O argument" to write zpacket to.
 * \param state zpacket writing state.
 * \param new_io_arg Output "I/O argument" that should be used for subsequent writing.
 * \returns Error code. May return Q2P_ERR_NOT_ENOUGH_PACKET_SPACE to indicate not enough
 * packet space is available for the zpacket header. (It may still be possible to write
 * an uncompressed packet, though.)
 */
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_maybe_zpacket_begin(q2proto_servercontext_t *context,
                                                                q2protoio_deflate_args_t *deflate_args,
                                                                uintptr_t io_arg, q2proto_maybe_zpacket_t *state,
                                                                uintptr_t *new_io_arg);
/**
 * End zpacket (maybe) writing.
 * Writes out the compressed data to the original "I/O" argument.
 * \param state zpacket writing state.
 * \param new_io_arg New "I/O argument" returned by q2proto_maybe_zpacket_begin().
 * \returns Error code.
 */
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_maybe_zpacket_end(q2proto_maybe_zpacket_t *state, uintptr_t new_io_arg);

#endif // Q2PROTO_INTERNAL_MAYBE_ZPACKET_H_
