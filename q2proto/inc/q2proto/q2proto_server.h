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
 * Server-side functions
 */
#ifndef Q2PROTO_SERVER_H_
#define Q2PROTO_SERVER_H_

#include "q2proto_connect.h"
#include "q2proto_defs.h"
#include "q2proto_entity_bits.h"
#include "q2proto_error.h"
#include "q2proto_game_api.h"
#include "q2proto_io.h"
#include "q2proto_packing.h"
#include "q2proto_protocol.h"
#include "q2proto_string.h"
#include "q2proto_struct_clc.h"
#include "q2proto_struct_svc.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**\name Server-side communications
 * @{ */
/**
 * Produce extra arguments for server "challenge".
 * (Currently the list of supported protocols, per the \a accepted_protocols argument.)
 * \param buf Buffer to receive extra arguments
 * \param buf_size Size of extra arguments buffer
 * \param accepted_protocols Array of accepted protocols, for inclusion in extra arguments
 * \param num_accepted_protocols Size of \a accepted_protocols
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_get_challenge_extras(char *buf, size_t buf_size,
                                                                const q2proto_protocol_t *accepted_protocols,
                                                                size_t num_accepted_protocols);

/// Server information
typedef struct q2proto_server_info_s {
    /// Type of game run by server
    q2proto_game_api_t game_api;
    /// Default packet length value, used for \c packet_length member of q2proto_connect_t.
    int default_packet_length;
} q2proto_server_info_t;

/**
 * Parse the arguments to a "connect" command, sent by a connecting client to the server.
 * \param connect_args "connect" arguments string.
 * \param accepted_protocols Accepted protocols. Checks requested protocol against this list.
 * \param num_accepted_protocols Number of accepted protocols.
 * \param server_info Server info. Currently used to get default packet length.
 * \param parsed_connect Parsed connect info.
 * \returns Error code.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_parse_connect(const char *connect_args,
                                                         const q2proto_protocol_t *accepted_protocols,
                                                         size_t num_accepted_protocols,
                                                         const q2proto_server_info_t *server_info,
                                                         q2proto_connect_t *parsed_connect);

typedef struct q2proto_servercontext_s q2proto_servercontext_t;
typedef struct q2proto_gamestate_s q2proto_gamestate_t;

/**
 * "Server" context. Used for server communications with a single client.
 */
struct q2proto_servercontext_s {
    /// Protocol & connection features
    struct {
        /// Protocol identifies client which should supports "RF_BEAM old_origin fix"
        bool has_beam_old_origin_fix;
        /// The q2proto_svc_playerstate_t::clientnum field is supported
        bool playerstate_clientnum;
        /// Enable deflate compression (for R1Q2/Q2PRO)
        bool enable_deflate;
        /// Q2PROTO_DOWNLOAD_COMPRESS_RAW is supported for downloads
        bool download_compress_raw;
        /// Protocol sends fog data as part of playerstate
        bool has_playerfog;
    } features;

    /// Server information
    const q2proto_server_info_t *Q2PROTO_PRIVATE_API_MEMBER(server_info);
    /// Actual protocol version
    q2proto_protocol_t Q2PROTO_PRIVATE_API_MEMBER(protocol);
    /// Protocol version (for R1Q2/Q2PRO)
    int Q2PROTO_PRIVATE_API_MEMBER(protocol_version);
    /// zpacket command (differs between R1Q2/Q2PRO and Q2rePRO protocol)
    uint8_t Q2PROTO_PRIVATE_API_MEMBER(zpacket_cmd);

    /// For Q2P_PROTOCOL_KEX_DEMOS. Bits indicating whether a baseline entity has a solid value != 0
    q2proto_entity_bits Q2PROTO_PRIVATE_API_MEMBER(kex_demo_baseline_nonzero_solid);
    /// For Q2P_PROTOCOL_KEX_DEMOS. Bits indicating whether an entity was previously seen to have a solid value != 0
    q2proto_entity_bits Q2PROTO_PRIVATE_API_MEMBER(kex_demo_edict_nonzero_solid);

    /// serverdata filling
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, fill_serverdata, q2proto_servercontext_t *context,
                                 q2proto_svc_serverdata_t *serverdata);

    /// Protocol-specific entity state delta message creation
    Q2PROTO_PRIVATE_API_FUNC_PTR(void, make_entity_state_delta, q2proto_servercontext_t *context,
                                 const q2proto_packed_entity_state_t *from, const q2proto_packed_entity_state_t *to,
                                 bool write_old_origin, q2proto_entity_state_delta_t *delta);
    /// Protocol-specific player state delta message creation
    Q2PROTO_PRIVATE_API_FUNC_PTR(void, make_player_state_delta, q2proto_servercontext_t *context,
                                 const q2proto_packed_player_state_t *from, const q2proto_packed_player_state_t *to,
                                 q2proto_svc_playerstate_t *delta);

    /// write message
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, server_write, q2proto_servercontext_t *context, uintptr_t io_arg,
                                 const q2proto_svc_message_t *svc_message);
    /// write gamestate
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, server_write_gamestate, q2proto_servercontext_t *context,
                                 q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                 const q2proto_gamestate_t *gamestate);
    /// read message
    Q2PROTO_PRIVATE_API_FUNC_PTR(q2proto_error_t, server_read, q2proto_servercontext_t *context, uintptr_t io_arg,
                                 q2proto_clc_message_t *clc_message);

    /// Pointer to download function implementations
    const struct q2proto_download_funcs_s *Q2PROTO_PRIVATE_API_MEMBER(download_funcs);

    /// Current element when writing gamestate
    size_t Q2PROTO_PRIVATE_API_MEMBER(gamestate_pos);
};

/**
 * Set up a context for server communications with a single client.
 * \param context Context structure, filled with context-specific data.
 * \param server_info Server info. Pointer will be stored in the server context for use by protocols.
 * \param connect_info Connection info. Usually produced by q2proto_parse_connect().
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_init_servercontext(q2proto_servercontext_t *context,
                                                              const q2proto_server_info_t *server_info,
                                                              const q2proto_connect_t *connect_info);

/**
 * Set up a context for writing messages for a demo.
 * \param context Context structure, filled with context-specific data.
 * \param server_info Server info, configures demo recording protocol. Pointer will be stored in the server context for use by protocols.
 *   The \c default_packet_length member controls the packet length that should be written.
 *   Be aware that different clients support different maximum demo packet lengths.
 *   A default packet length of 0 will choose a conservative default.
 * \param max_msg_len Actual packet length limit. This may differ from the requested default packet length due to protocol-specific
 *   reasons.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_init_servercontext_demo(q2proto_servercontext_t *context,
                                                                   const q2proto_server_info_t *server_info,
                                                                   size_t *max_msg_len);

/**
 * Fill a serverdata structure with protocol-dependent values.
 * Also sets the correct flags for the game type specified in the server info.
 * \param context Server communications context.
 * \param serverdata Serverdata structure to fill.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_fill_serverdata(q2proto_servercontext_t *context,
                                                                  q2proto_svc_serverdata_t *serverdata);

/**
 * Fill an "entity state delta" message based on difference between packed entities.
 * The "from", or "old" entity state plus the "delta" allow to recreate the "to", or "new", entity state.
 * \param context Server communications context.
 * \param from The "from", or "old", packed entity state. Can be \c NULL (which is the same as passing a zero-initialized packed entity state).
 * \param to The "to", or "new", packed entity state.
 * \param write_old_origin Whether the "old origin" field should be included in the delta. Due to complicated rules this isn't determined automatically.
 * \param delta Structure receiving delta between packed entity states.
 */
Q2PROTO_PUBLIC_API void q2proto_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                               const q2proto_packed_entity_state_t *from,
                                                               const q2proto_packed_entity_state_t *to,
                                                               bool write_old_origin,
                                                               q2proto_entity_state_delta_t *delta);
/**
 * Fill a "player state delta" message based on difference between packed player states.
 * The "from", or "old" player state plus the "delta" allow to recreate the "to", or "new", player state.
 * \param context Server communications context.
 * \param from The "from", or "old", packed player state. Can be \c NULL (which is the same as passing a zero-initialized packed player state).
 * \param to The "to", or "new", player entity state.
 * \param delta Structure receiving delta between packed player states.
 */
Q2PROTO_PUBLIC_API void q2proto_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                               const q2proto_packed_player_state_t *from,
                                                               const q2proto_packed_player_state_t *to,
                                                               q2proto_svc_playerstate_t *delta);

/**
 * Write a message for "multicast" server communications, ie send the same binary message to multiple clients.
 * Doesn't need a context, but supports only a restricted set of messages.
 * \param protocol Server protocol.
 * \param server_info Server information.
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param svc_message Message data.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_multicast_write(q2proto_multicast_protocol_t multicast_proto,
                                                                  uintptr_t io_arg,
                                                                  const q2proto_svc_message_t *svc_message);

/**
 * Write a position appropriate for the server protocol and server info's game type.
 * \param protocol Server protocol.
 * \param server_info Server information.
 * \param io_arg "I/O argument", passed to externally provided I/O functions, used to write compressed data.
 * \param pos Position to write.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_write_pos(q2proto_multicast_protocol_t multicast_proto,
                                                            uintptr_t io_arg, const q2proto_vec3_t pos);

/**
 * Write a message for sending from the server to a client.
 * \param context Server communications context.
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param svc_message Message data.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                        const q2proto_svc_message_t *svc_message);

/// Gamestate to write
struct q2proto_gamestate_s {
    /// Number of configstrings to write
    size_t num_configstrings;
    /// Configstrings to write
    const q2proto_svc_configstring_t *configstrings;
    /// Number of spawn baselines to write
    size_t num_spawnbaselines;
    /// Spawn baselines to write
    const q2proto_svc_spawnbaseline_t *spawnbaselines;
};

/**
 * Write a full gamestate from the server to the client, in the most efficient way the protocol allows.
 * \param context Server communications context.
 * \param deflate_args Deflate arguments to compress gamestate data (if supported by protocol).
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param gamestate Gamestate to write.
 * \returns Q2P_ERR_SUCCESS if the gamestate was successfully written, or Q2P_ERR_NOT_ENOUGH_PACKET_SPACE
 * if only a part could be written. In that case, the accumulated data must be "flushed" and
 * q2proto_server_write_gamestate() called again with the same gamestate to continue writing.
 * Error code otherwise.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_write_gamestate(q2proto_servercontext_t *context,
                                                                  q2protoio_deflate_args_t *deflate_args,
                                                                  uintptr_t io_arg,
                                                                  const q2proto_gamestate_t *gamestate);

/**
 * Compress packet data into a "zpacket".
 * \param context Server communications context.
 * \param deflate_args Deflate arguments to compress packet data.
 * \param io_arg "I/O argument", passed to externally provided I/O functions, used to write compressed data.
 * \param packet_data Pointer to uncompressed packet data.
 * \param packet_len Length of uncompressed packet data.
 * \returns May return Q2P_ERR_ALREADY_COMPRESSED, which indicates that either zpacket data was passed in,
 * or the data didn't compress very well. Either way, the original data should be sent.
 * Error code in case of error.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_write_zpacket(q2proto_servercontext_t *context,
                                                                q2protoio_deflate_args_t *deflate_args,
                                                                uintptr_t io_arg, const void *packet_data,
                                                                size_t packet_len);

/// State for download handling
typedef struct q2proto_server_download_state_s {
    // Server communications context.
    q2proto_servercontext_t *Q2PROTO_PRIVATE_API_MEMBER(context);
    // Amount transferred
    size_t Q2PROTO_PRIVATE_API_MEMBER(transferred);
    // Total size
    size_t Q2PROTO_PRIVATE_API_MEMBER(total_size);
    // Whether download data should be compressed (deflated)
    int Q2PROTO_PRIVATE_API_MEMBER(compress);
    // Deflate arguments, passed through to q2protoio_deflate_begin
    q2protoio_deflate_args_t *Q2PROTO_PRIVATE_API_MEMBER(deflate_args);
    // Currently/last used deflate I/O arg
    uintptr_t Q2PROTO_PRIVATE_API_MEMBER(deflate_io);
    // Whether deflate I/O arg is valid
    bool Q2PROTO_PRIVATE_API_MEMBER(deflate_io_valid);
} q2proto_server_download_state_t;

/// Stateful download compression mode
typedef enum {
    /// Never compress
    Q2PROTO_DOWNLOAD_COMPRESS_NEVER,
    /// Compress, if supported
    Q2PROTO_DOWNLOAD_COMPRESS_AUTO,
    /// Pre-compressed data. Only supported if q2proto_servercontext_t::features.download_compress_raw is \c true.
    Q2PROTO_DOWNLOAD_COMPRESS_RAW,
} q2proto_download_compress_t;

/**
 * Initialize stateful download.
 * \param context Server communications context.
 * \param total_size Total size of data to download.
 * \param compress Compression mode.
 * \param deflate_args Deflate arguments. Passed through to deflate initialization if compression is enabled.
 * \param state Download state object.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_download_begin(q2proto_servercontext_t *context, size_t total_size,
                                                                 q2proto_download_compress_t compress,
                                                                 q2protoio_deflate_args_t *deflate_args,
                                                                 q2proto_server_download_state_t *state);
/**
 * Finalize stateful download.
 * \param state Download state object.
 */
Q2PROTO_PUBLIC_API void q2proto_server_download_end(q2proto_server_download_state_t *state);
/**
 * Fill an q2proto_svc_download_t structure with a chunk of data.
 * \param state Download state object.
 * \param data Pointer to start of remaining data. Updated based of the amount of consumed data.
 * \param remaining Size of remaining data. Updated based of the amount of consumed data.
 * \param packet_remaining Maximum size useable for download packet.
 * \param svc_download Output q2proto_svc_download_t structure.
 * \returns Q2P_ERR_NOT_ENOUGH_PACKET_SPACE is returned if useable space is insufficient for download package;
 * download message must \em not be written. Q2P_ERR_DOWNLOAD_COMPLETE is returned if the download is now
 * completed. Q2P_ERR_SUCCESS if download message was filled but download continues.
 * For Q2P_ERR_SUCCESS and Q2P_ERR_DOWNLOAD_COMPLETE download message \em must be written.
 * Error code otherwise.
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_download_data(q2proto_server_download_state_t *state,
                                                                const uint8_t **data, size_t *remaining,
                                                                size_t packet_remaining,
                                                                q2proto_svc_download_t *svc_download);
/**
 * Fill an q2proto_svc_download_t indicating the download was finished (typically if nothing needs to be transferred).
 * \param context Server communications context.
 * \param svc_download Output q2proto_svc_download_t structure.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_download_finish(q2proto_server_download_state_t *state,
                                                                  q2proto_svc_download_t *svc_download);
/**
 * Fill an q2proto_svc_download_t indicating the download was aborted.
 * \param context Server communications context.
 * \param svc_download Output q2proto_svc_download_t structure.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_download_abort(q2proto_server_download_state_t *state,
                                                                 q2proto_svc_download_t *svc_download);
/**
 * Request progress from a stateful download.
 * \param state Download state
 * \param completed Receives number of bytes transferred
 * \param total Receives total number of bytes to transfer
 */
Q2PROTO_PUBLIC_API void q2proto_server_download_get_progress(const q2proto_server_download_state_t *state,
                                                             size_t *completed, size_t *total);

/**
 * Read a message sent from the client to the server.
 * \param context Server communications context.
 * \param io_arg "I/O argument", passed to externally provided I/O functions.
 * \param clc_message Will be filled with message data.
 * \returns Error code
 */
Q2PROTO_PUBLIC_API q2proto_error_t q2proto_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                       q2proto_clc_message_t *clc_message);
/** @} */

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_SERVER_H_
