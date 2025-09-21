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
 * Q2rePRO protocol
 */
#ifndef Q2PROTO_PROTO_Q2REPRO_H_
#define Q2PROTO_PROTO_Q2REPRO_H_

#include "q2proto/q2proto.h"

/**\name Q2rePRO protocol
 * @{ */
/// "connect" token parsing
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_parse_connect(q2proto_string_t *connect_str,
                                                                  q2proto_connect_t *parsed_connect);
/// connect struct completion
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_complete_connect(q2proto_connect_t *connect);
/// Get "connect" string tail
Q2PROTO_PRIVATE_API const char *q2proto_q2repro_connect_tail(const q2proto_connect_t *connect);

/// Client context setup
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_continue_serverdata(q2proto_clientcontext_t *context,
                                                                        uintptr_t io_arg,
                                                                        q2proto_svc_serverdata_t *serverdata);

// Functions to read messages written by rerelease game DLL
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_damage(uintptr_t io_arg, q2proto_svc_damage_t *damage);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_fog(uintptr_t io_arg, q2proto_svc_fog_t *fog);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_poi(uintptr_t io_arg, q2proto_svc_poi_t *poi);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_help_path(uintptr_t io_arg,
                                                                          q2proto_svc_help_path_t *help_path);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_muzzleflash3(uintptr_t io_arg,
                                                                             q2proto_svc_muzzleflash_t *muzzleflash);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_client_read_achievement(uintptr_t io_arg,
                                                                            q2proto_svc_achievement_t *achievement);

/// Server context setup
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_init_servercontext(q2proto_servercontext_t *context,
                                                                       const q2proto_connect_t *connect_info);

// Functions to write messages like the rerelease game DLL
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_q2repro_server_write_fog(uintptr_t io_arg, const q2proto_svc_fog_t *fog);
/** @} */

#endif // Q2PROTO_PROTO_Q2REPRO_H_
