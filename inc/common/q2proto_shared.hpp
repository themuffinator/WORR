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

#pragma once

#include "common/msg.hpp"
#include "common/sizebuf.hpp"

#include "q2proto/q2proto.h"

#if USE_ZLIB
#include <zlib.h>

struct q2protoio_deflate_args_s
{
    /// Buffer to store deflated data
    byte *z_buffer;
    /// Size of deflated data buffer
    unsigned z_buffer_size;
    /// Deflate stream (raw)
    z_streamp z_raw;
    /// Deflate stream (headered)
    z_stream z_header;
    /// Currently active stream
    z_streamp z_current;
};

void Q2Proto_IO_Init(void);
void Q2Proto_IO_ResetInflate(void);
void Q2Proto_IO_Shutdown(void);
void Q2Proto_IO_ShutdownDeflate(struct q2protoio_deflate_args_s *deflate_args);
#endif // USE_ZLIB

typedef struct q2protoio_ioarg_s {
    sizebuf_t *sz_read;
    sizebuf_t *sz_write;
    size_t max_msg_len;
    struct q2protoio_deflate_args_s *deflate;
} q2protoio_ioarg_t;

extern q2protoio_ioarg_t default_q2protoio_ioarg;

#define _Q2PROTO_IOARG_DEFAULT      ((uintptr_t)&default_q2protoio_ioarg)

#if USE_CLIENT || USE_SERVER
#define Q2PROTO_IOARG_SERVER_READ               _Q2PROTO_IOARG_DEFAULT
#define Q2PROTO_IOARG_SERVER_WRITE_MULTICAST    _Q2PROTO_IOARG_DEFAULT
#endif

#if USE_CLIENT
#define Q2PROTO_IOARG_CLIENT_READ   _Q2PROTO_IOARG_DEFAULT
#define Q2PROTO_IOARG_CLIENT_WRITE  _Q2PROTO_IOARG_DEFAULT
#endif

extern bool nonfatal_client_read_errors;

typedef struct entity_state_s entity_state_t;

Q2PROTO_DECLARE_ENTITY_PACKING_FUNCTION(PackEntity, entity_state_t *);

Q2PROTO_DECLARE_PLAYER_PACKING_FUNCTION(PackPlayerstate, player_state_t *);

bool Q2PROTO_MakeEntityDelta(q2proto_servercontext_t *context, q2proto_entity_state_delta_t *delta, const q2proto_packed_entity_state_t *from, const q2proto_packed_entity_state_t *to, msgEsFlags_t flags);
