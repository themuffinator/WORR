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

#include "q2proto/q2proto.h"

// Dummy q2protoio_inflate_* function definitions to make linking work

q2proto_error_t q2protoio_inflate_begin(uintptr_t io_arg, q2proto_inflate_deflate_header_mode_t header_mode,
                                        uintptr_t *inflate_io_arg)
{
    *inflate_io_arg = io_arg;
    return Q2P_ERR_NOT_IMPLEMENTED;
}

q2proto_error_t q2protoio_inflate_data(uintptr_t io_arg, uintptr_t inflate_io_arg, size_t compressed_size)
{
    return Q2P_ERR_NOT_IMPLEMENTED;
}

q2proto_error_t q2protoio_inflate_stream_ended(uintptr_t inflate_io_arg, bool *stream_end)
{
    return Q2P_ERR_NOT_IMPLEMENTED;
}

q2proto_error_t q2protoio_inflate_end(uintptr_t inflate_io_arg) { return Q2P_ERR_NOT_IMPLEMENTED; }
