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
 * Q2PRO "extended demo" protocol
 */
#ifndef Q2PROTO_PROTO_Q2PRO_EXTDEMO_H_
#define Q2PROTO_PROTO_Q2PRO_EXTDEMO_H_

#include "q2proto/q2proto.h"

/**\name Q2PRO "extended demo" protocol
 * @{ */
/// Client context setup (Note: Only supports reading server messages)
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2pro_extdemo_continue_serverdata(q2proto_clientcontext_t *context,
                                                                              uintptr_t io_arg,
                                                                              q2proto_svc_serverdata_t *serverdata);

/// Server context setup (Note: Only supports writing server messages)
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2pro_extdemo_init_servercontext(q2proto_servercontext_t *context,
                                                                             const q2proto_connect_t *connect_info);
/** @} */

#endif // Q2PROTO_PROTO_Q2PRO_EXTDEMO_H_
