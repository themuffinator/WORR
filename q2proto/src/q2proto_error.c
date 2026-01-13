/*
Copyright (C) 2025 Frank Richter

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
#include "q2proto_internal.h"

const char *q2proto_error_string(q2proto_error_t err)
{
    switch(err)
    {
#define E(X)          \
    case Q2P_ERR_##X: \
        return #X;

    E(SUCCESS)
    E(NO_MORE_INPUT)
    E(NOT_ENOUGH_PACKET_SPACE)
    E(DOWNLOAD_COMPLETE)
    E(ALREADY_COMPRESSED)
    E(NOT_IMPLEMENTED)
    E(INVALID_ARGUMENT)
    E(BAD_DATA)
    E(BAD_COMMAND)
    E(GAMETYPE_UNSUPPORTED)
    E(BUFFER_TOO_SMALL)
    E(NO_ACCEPTABLE_PROTOCOL)
    E(EXPECTED_SERVERDATA)
    E(PROTOCOL_NOT_SUPPORTED)
    E(DEFLATE_NOT_SUPPORTED)
    E(MORE_DATA_DEFLATED)
    E(INFLATE_FAILED)
    E(DEFLATE_FAILED)
    E(RAW_COMPRESS_NOT_SUPPORTED)

#undef E
    }

    return q2proto_va("%d", err);
}
