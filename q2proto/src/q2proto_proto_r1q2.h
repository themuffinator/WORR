/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2011 Richard Stanway
Copyright (C) 2003-2024 Andrey Nazarov
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
 * R1Q2 protocol
 */
#ifndef Q2PROTO_PROTO_R1Q2_H_
#define Q2PROTO_PROTO_R1Q2_H_

#include "q2proto/q2proto.h"

/**\name R1Q2 protocol
 * @{ */
/// "connect" token parsing
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_r1q2_parse_connect(q2proto_string_t *connect_str,
                                                               q2proto_connect_t *parsed_connect);
/// connect struct completion
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_r1q2_complete_connect(q2proto_connect_t *connect);
/// Get "connect" string tail
Q2PROTO_PRIVATE_API const char *q2proto_r1q2_connect_tail(const q2proto_connect_t *connect);

/// Client context setup
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_r1q2_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                                     q2proto_svc_serverdata_t *serverdata);

/// Server context setup
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_r1q2_init_servercontext(q2proto_servercontext_t *context,
                                                                    const q2proto_connect_t *connect_info);
/** @} */

/**\name R1Q2 protocol parts reused by Q2PRO
 * @{ */
/// Parse svc_r1q2_zpacket
Q2PROTO_PRIVATE_API q2proto_error_t r1q2_client_read_zpacket(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                             q2proto_svc_message_t *svc_message);
/// Parse svc_r1q2_setting
Q2PROTO_PRIVATE_API q2proto_error_t r1q2_client_read_setting(uintptr_t io_arg, q2proto_svc_setting_t *setting);

/// Write clc_r1q2_setting
Q2PROTO_PRIVATE_API q2proto_error_t r1q2_client_write_setting(uintptr_t io_arg, const q2proto_clc_setting_t *setting);
/** @} */

#endif // Q2PROTO_PROTO_R1Q2_H_
