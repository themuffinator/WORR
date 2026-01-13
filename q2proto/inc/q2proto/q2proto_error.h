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
 * Error codes
 */
#ifndef Q2PROTO_ERROR_H_
#define Q2PROTO_ERROR_H_

#include "q2proto_defs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/// q2proto error codes
typedef enum q2proto_error_e {
    /// Successfuly
    Q2P_ERR_SUCCESS = 0,

    /// When reading packets, indicates end of input was reached
    Q2P_ERR_NO_MORE_INPUT = 1,
    /// Not enough space in remaining packet
    Q2P_ERR_NOT_ENOUGH_PACKET_SPACE = 2,
    /// Download is completed
    Q2P_ERR_DOWNLOAD_COMPLETE = 3,
    /// Message is already compressed
    Q2P_ERR_ALREADY_COMPRESSED = 4,

    /// Function not implemented
    Q2P_ERR_NOT_IMPLEMENTED = -1,
    /// Invalid argument
    Q2P_ERR_INVALID_ARGUMENT = -2,
    /// Bad data from client or server
    Q2P_ERR_BAD_DATA = -3,
    /// Unsupported command
    Q2P_ERR_BAD_COMMAND = -4,
    /// Unsupported gametype
    Q2P_ERR_GAMETYPE_UNSUPPORTED = -5,
    /// Destination buffer too small
    Q2P_ERR_BUFFER_TOO_SMALL = -6,

    /// Challenge parsing: no acceptable protocol found
    Q2P_ERR_NO_ACCEPTABLE_PROTOCOL = -100,

    /// Client packet handling: Expected serverdata, received something else
    Q2P_ERR_EXPECTED_SERVERDATA = -200,
    /// Client packet handling: Unsupported protocol
    Q2P_ERR_PROTOCOL_NOT_SUPPORTED = -201,

    /// Deflate-compressed data is not supported
    Q2P_ERR_DEFLATE_NOT_SUPPORTED = -300,
    /// Not all deflated data was consumed
    Q2P_ERR_MORE_DATA_DEFLATED = -301,
    /// Failed to inflate data
    Q2P_ERR_INFLATE_FAILED = -302,
    /// Failed to deflate data
    Q2P_ERR_DEFLATE_FAILED = -303,
    /// Raw compressed download data not supported
    Q2P_ERR_RAW_COMPRESS_NOT_SUPPORTED = -304,
} q2proto_error_t;

/// Return a string representation of the given error code. Intended for error reporting.
Q2PROTO_PUBLIC_API const char *q2proto_error_string(q2proto_error_t err);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_ERROR_H_
