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
#include "q2proto_internal_maybe_zpacket.h"

#include "q2proto_internal_io.h"
#include "q2proto_internal_protocol.h"

// Minimum size of a zpacket
#define SVC_ZPACKET_SIZE  5
/* Minimal space in packet need to enable compression
 * (minimal zpacket size + some slack for compressed data,
 * to avoid generating an empty zpacket) */
#define MIN_COMPRESS_SIZE SVC_ZPACKET_SIZE + 16

q2proto_error_t q2proto_maybe_zpacket_begin(q2proto_servercontext_t *context, q2protoio_deflate_args_t *deflate_args,
                                            uintptr_t io_arg, q2proto_maybe_zpacket_t *state, uintptr_t *new_io_arg)
{
    *new_io_arg = io_arg; // safe default
#if Q2PROTO_COMPRESSION_DEFLATE
    memset(state, 0, sizeof(*state));
    state->original_io_arg = io_arg;
    state->zpacket_cmd = context->zpacket_cmd;
    if (deflate_args && context->features.enable_deflate) {
        size_t max_deflated = q2protoio_write_available(io_arg);
        if (max_deflated < MIN_COMPRESS_SIZE)
            return Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;
        max_deflated -= SVC_ZPACKET_SIZE;
        q2proto_error_t deflate_err =
            q2protoio_deflate_begin(deflate_args, max_deflated, Q2P_INFL_DEFL_RAW, new_io_arg);
        if (deflate_err == Q2P_ERR_SUCCESS) {
            state->deflate_enabled = true;
            return Q2P_ERR_SUCCESS;
        }
    }
#endif
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_maybe_zpacket_end(q2proto_maybe_zpacket_t *state, uintptr_t new_io_arg)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    if (!state->deflate_enabled)
        return Q2P_ERR_SUCCESS;

    const void *data;
    size_t uncompressed_len = 0, compressed_len = 0;
    q2proto_error_t err = q2protoio_deflate_get_data(new_io_arg, &uncompressed_len, &data, &compressed_len);
    if (err != Q2P_ERR_SUCCESS)
        goto error;

    WRITE_CHECKED(server_write, state->original_io_arg, u8, state->zpacket_cmd);
    WRITE_CHECKED(server_write, state->original_io_arg, u16, compressed_len);
    WRITE_CHECKED(server_write, state->original_io_arg, u16, uncompressed_len);
    WRITE_CHECKED(server_write, state->original_io_arg, raw, data, compressed_len, NULL);

    return q2protoio_deflate_end(new_io_arg);

error:
    q2protoio_deflate_end(new_io_arg);
    return err;

#else
    return Q2P_ERR_SUCCESS;
#endif
}
