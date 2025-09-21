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
 * Protocol enum & helpers
 */
#ifndef Q2PROTO_PROTOCOL_H_
#define Q2PROTO_PROTOCOL_H_

#include "q2proto_defs.h"
#include "q2proto_game_api.h"

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Supported protocols
typedef enum q2proto_protocol_e {
    /// Invalid protocol
    Q2P_PROTOCOL_INVALID = 0,
    /// Protocol 26, used by original release demos
    Q2P_PROTOCOL_OLD_DEMO,
    /// Vanilla 3.20 protocol
    Q2P_PROTOCOL_VANILLA,
    /// R1Q2 protocol
    Q2P_PROTOCOL_R1Q2,
    /// Q2PRO protocol
    Q2P_PROTOCOL_Q2PRO,
    /// Q2PRO extended demo (not used for network communication)
    Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO,
    /// Q2PRO extended v2 demo (not used for network communication)
    Q2P_PROTOCOL_Q2PRO_EXTENDED_V2_DEMO,
    /// Q2PRO extended v2 demo w/ playerfog (not used for network communication)
    Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG,
    /// Q2rePRO (Q2PRO fork w/ rerelease game support) protocol
    Q2P_PROTOCOL_Q2REPRO,
    /// Quake 2 Remastered/KEX, protocol used for demos
    Q2P_PROTOCOL_KEX_DEMOS,
    /// Quake 2 Remastered/KEX protocol
    Q2P_PROTOCOL_KEX,

    /// Number of supported protocols
    Q2P_NUM_PROTOCOLS
} q2proto_protocol_t;

/// Map from q2proto_protocol_t value to protocol version number communicated over network
Q2PROTO_PUBLIC_API int q2proto_get_protocol_netver(q2proto_protocol_t protocol);
/// Map from protocol version number communicated over network to q2proto_protocol_t value
Q2PROTO_PUBLIC_API q2proto_protocol_t q2proto_protocol_from_netver(int version);

/// Get array with protocols, suitable for given game types
Q2PROTO_PUBLIC_API size_t q2proto_get_protocols_for_gametypes(q2proto_protocol_t *protocols, size_t num_protocols,
                                                              const q2proto_game_api_t *games, size_t num_games);

/**
 * Multicast protocol.
 * Multiple protocols actually produce the same multicast-supported messages, thus
 * it's possible to send the same multicast message to clients using different protocols.
 *
 * A "multicast protocol" shouldn't be picked manually, instead use q2proto_get_multicast_protocol()
 * to get a suitable value for the protocol(s) you offer.
 */
typedef enum q2proto_multicast_protocol_e {
    /// Invalid protocol
    Q2P_PROTOCOL_MULTICAST_INVALID = 0,
    /// Multicast uses "short" positions
    Q2P_PROTOCOL_MULTICAST_SHORT,
    /// Multicast uses Q2PRO "int23" positions
    Q2P_PROTOCOL_MULTICAST_Q2PRO_EXT,
    /// Multicast uses "float" positions
    Q2P_PROTOCOL_MULTICAST_FLOAT,
} q2proto_multicast_protocol_t;

/// Get suitable multicast protocol for accepted protocol(s) and game type.
Q2PROTO_PUBLIC_API q2proto_multicast_protocol_t q2proto_get_multicast_protocol(q2proto_protocol_t *protocols,
                                                                               size_t num_protocols,
                                                                               q2proto_game_api_t game_api);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_PROTOCOL_H_
