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
 * Client-side functions
 */
#ifndef Q2PROTO_CLIENT_H_
#define Q2PROTO_CLIENT_H_

#include "q2proto_connect.h"
#include "q2proto_coords.h"
#include "q2proto_defs.h"
#include "q2proto_entity_bits.h"
#include "q2proto_error.h"
#include "q2proto_game_api.h"
#include "q2proto_protocol.h"
#include "q2proto_struct_clc.h"
#include "q2proto_struct_svc.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**\name Client-side communications
 * @{ */
/// Parsed challenge string
typedef struct q2proto_challenge_s {
    /// Challenge value
    int32_t challenge;
    /// Protocol version
    q2proto_protocol_t server_protocol;
} q2proto_challenge_t;

/**
 * Parse args of "challenge" from server.
 * \param challenge_args "challenge" arguments string.
 * \param accepted_protocols Accepted protocols. Picks a protocol from this list. Sorts by priority - protocols appearing earlier are preferred.
 * \param num_accepted_protocols Number of accepted protocols.
 * \param parsed_challenge Filled with parsed challenge data
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_parse_challenge(const char *challenge_args,
                                                           const q2proto_protocol_t *accepted_protocols,
                                                           size_t num_accepted_protocols,
                                                           q2proto_challenge_t *parsed_challenge);

/**
 * Complete values of a q2proto_connect_t structure, based on contained protocol.
 * Usually sets the following fields:
 * - \c version (if zero)
 * - \c has_zlib
 * All other fields should contain sensible values.
 * Additionally, may change the following fields:
 * - \c qport
 * \param connect Structure to change.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_complete_connect(q2proto_connect_t *connect);

/**
 * Create a string with arguments to "connect" command sent by client.
 * \param args_str Buffer to receive arguments string.
 * \param size Size of arguments string buffer.
 * \param need_size If not \c NULL, returns the required buffer size.
 * \param connect Structure with connect arguments.
 * \returns Error code. If \c Q2P_ERR_BUFFER_TOO_SMALL is returned, use \c need_size
 *   to provide an adequately sized larger buffer.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_get_connect_arguments(char *args_str, size_t size, size_t *need_size,
                                                                 const q2proto_connect_t *connect);

typedef struct q2proto_clientcontext_s q2proto_clientcontext_t;

/**
 * "Client" context. Used for client communications with server.
 */
struct q2proto_clientcontext_s {
    /// Protocol & connection features
    struct {
        /// Protocol supports batch moves (Q2P_CLC_BATCH_MOVE)
        bool batch_move;
        /// Protocol supports userinfo delta (Q2P_CLC_USERINFO_DELTA)
        bool userinfo_delta;
        /// Protocol supports usercmd upmove
        bool has_upmove;
        /// Protocol supports playerstate clientnum
        bool has_clientnum;
        /// Protocol supports 32-bit 'solid' values
        bool has_solid32;
        /// Protocol sends fog data as part of playerstate
        bool has_playerfog;
        /// Type of game run by the server
        q2proto_game_api_t server_game_api;
    } features;

    /// Server protocol number
    q2proto_protocol_t Q2PROTO_PRIVATE_API_MEMBER(server_protocol);
    /// Protocol version (for R1Q2/Q2PRO)
    int Q2PROTO_PRIVATE_API_MEMBER(protocol_version);

    /// Whether a "inflate" ioarg should be used to read packets
    bool Q2PROTO_PRIVATE_API_MEMBER(has_inflate_io_arg);
    /// inflate ioarg
    uintptr_t Q2PROTO_PRIVATE_API_MEMBER(inflate_io_arg);

    /// Whether we have a zdownload inflate io_arg
    bool Q2PROTO_PRIVATE_API_MEMBER(has_zdownload_inflate_io_arg);
    /// zdownload inflate ioarg
    uintptr_t Q2PROTO_PRIVATE_API_MEMBER(zdownload_inflate_io_arg);

    /// For Q2P_PROTOCOL_KEX_DEMOS. Bits indicating whether a baseline entity has a solid value != 0
    q2proto_entity_bits Q2PROTO_PRIVATE_API_MEMBER(kex_demo_baseline_nonzero_solid);
    /// For Q2P_PROTOCOL_KEX_DEMOS. Bits indicating whether an entity was previously seen to have a solid value != 0
    q2proto_entity_bits Q2PROTO_PRIVATE_API_MEMBER(kex_demo_edict_nonzero_solid);

    /// "Pack solid" function
    Q2PROTO_PRIVATE_API_FUNC_PTR(uint32_t, pack_solid, q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                 const q2proto_vec3_t maxs);
    /// "Unpack solid" function
    Q2PROTO_PRIVATE_API_FUNC_PTR(void, unpack_solid, q2proto_clientcontext_t *context, uint32_t solid,
                                 q2proto_vec3_t mins, q2proto_vec3_t maxs);
    /// Packet parsing function
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, client_read, q2proto_clientcontext_t *context, uintptr_t io_arg,
                                 q2proto_svc_message_t *svc_message);
    /// "Send client command" function
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, client_write, q2proto_clientcontext_t *context, uintptr_t io_arg,
                                 const q2proto_clc_message_t *clc_message);
};

/**
 * Set up a context for client communications with server.
 * \param context Context structure, filled with context-specific data.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_init_clientcontext(q2proto_clientcontext_t *context);

/**
 * Read next message from server.
 * \param context Client communications context.
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param svc_message Will be filled with message data.
 * \returns Error code.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                       q2proto_svc_message_t *svc_message);

/**
 * Reset/clean up client download state.
 * The client context may hold some download-related state (currently, when the server sends compressed download
 * packages). After a download was completed or aborted, the state should be reset to free up resources that may have
 * been used for handling download packets.
 * \param context Client communications context.
 * \returns Error code.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_client_download_reset(q2proto_clientcontext_t *context);

/**
 * Write a message for sending from the client to the server.
 * \param context Client communications context.
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param clc_message Message data.
 * \returns Error code.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                        const q2proto_clc_message_t *clc_message);

/**
 * Pack a bounding box into a q2proto_entity_state_delta_t::solid value
 * \param context Client context for solid packing
 * \param mins Bounding box minimum point
 * \param maxs Bounding box maximum point
 * \returns q2proto_entity_state_delta_t::solid value
 */
Q2PROTO_PUBLIC_API uint32_t q2proto_client_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                                      const q2proto_vec3_t maxs);
/**
 * Unpack a q2proto_entity_state_delta_t::solid value into a bounding box
 * \param context Client context for solid unpacking
 * \param solid q2proto_entity_state_delta_t::solid value
 * \param mins Bounding box minimum point
 * \param maxs Bounding box maximum point
 */
Q2PROTO_PUBLIC_API void q2proto_client_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid,
                                                    q2proto_vec3_t mins, q2proto_vec3_t maxs);
/** @} */

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_CLIENT_H_
