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
 * Client command (CLC) structures and definitions
 */
#ifndef Q2PROTO_STRUCT_CLC_H_
#define Q2PROTO_STRUCT_CLC_H_

#include "q2proto_coords.h"
#include "q2proto_string.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Types of message from client to server
typedef enum q2proto_clc_message_type_e {
    /// Invalid
    Q2P_CLC_INVALID,
    /// No-op
    Q2P_CLC_NOP,
    /// Player move
    Q2P_CLC_MOVE,
    /// Player batch move (multiple frames, Q2PRO)
    Q2P_CLC_BATCH_MOVE,
    /// Userinfo
    Q2P_CLC_USERINFO,
    /// Command
    Q2P_CLC_STRINGCMD,
    /// Setting (R1Q2/Q2PRO)
    Q2P_CLC_SETTING,
    /// Userinfo delta (Q2PRO)
    Q2P_CLC_USERINFO_DELTA,
} q2proto_clc_message_type_t;

/// Flag bits for fields set in q2proto_clc_move_delta_t structure
enum q2proto_client_move_delta_flags {
    /// 'angles[0]' is set
    Q2P_CMD_ANGLE0 = 0x1,
    /// 'angles[1]' is set
    Q2P_CMD_ANGLE1 = 0x2,
    /// 'angles[2]' is set
    Q2P_CMD_ANGLE2 = 0x4,
    /// forward move/'move[0]' is set
    Q2P_CMD_MOVE_FORWARD = 0x8,
    /// side move/'move[1]' is set
    Q2P_CMD_MOVE_SIDE = 0x10,
    /// up move/'move[2]' is set
    Q2P_CMD_MOVE_UP = 0x20,
    /// 'buttons' is set
    Q2P_CMD_BUTTONS = 0x40,
    /// 'impulse' is set
    Q2P_CMD_IMPULSE = 0x80,
};

/// Player move delta message
typedef struct q2proto_clc_move_delta_s {
    /// Combination of q2proto_client_move_delta_flags, indicating which fields are set
    uint32_t delta_bits;
    /// view angles
    q2proto_var_angles_t angles;
    /// movement vector, [0]: forward, [1]: side, [2]: up
    q2proto_var_coords_t move;
    /// pressed buttons
    uint8_t buttons;
    /// impulsed impulse
    uint8_t impulse;

    // msec and lightlevel are not covered by delta bits
    /// command offset from frame start
    uint8_t msec;
    /// current player light level
    uint8_t lightlevel;
} q2proto_clc_move_delta_t;

/// Player move message
typedef struct q2proto_clc_move_s {
    /// last frame received from server
    int32_t lastframe;
    /// last move commands
    q2proto_clc_move_delta_t moves[3];
    /// packet output sequence value. used for checksum only
    int32_t sequence;
} q2proto_clc_move_t;

#define Q2PROTO_MAX_CLC_BATCH_MOVE_FRAMES 4
#define Q2PROTO_MAX_CLC_BATCH_MOVE_CMDS   32

typedef struct q2proto_clc_batch_move_frame_s {
    /// Number of move commands
    uint8_t num_cmds;
    /// last move commands
    q2proto_clc_move_delta_t moves[Q2PROTO_MAX_CLC_BATCH_MOVE_CMDS];
} q2proto_clc_batch_move_frame_t;

/// Player move message batch
typedef struct q2proto_clc_batch_move_s {
    /// last frame received from server
    int32_t lastframe;
    /// duplicate batch move frames (command contains num_dups+1 move frames)
    uint8_t num_dups;
    /// batched move frame
    q2proto_clc_batch_move_frame_t batch_frames[Q2PROTO_MAX_CLC_BATCH_MOVE_FRAMES];
} q2proto_clc_batch_move_t;

/// Client userinfo message
typedef struct q2proto_clc_userinfo_s {
    /// Userinfo string
    q2proto_string_t str;
} q2proto_clc_userinfo_t;

/// Client string command message
typedef struct q2proto_clc_stringcmd_s {
    /// Command string
    q2proto_string_t cmd;
} q2proto_clc_stringcmd_t;

/// Client setting message
typedef struct q2proto_clc_setting_s {
    /// Setting index
    int16_t index;
    /// Setting value
    int16_t value;
} q2proto_clc_setting_t;

/// Client userinfo delta
typedef struct q2proto_clc_userinfo_delta_s {
    /// Userinfo name
    q2proto_string_t name;
    /// Userinfo value
    q2proto_string_t value;
} q2proto_clc_userinfo_delta_t;

/// A single message sent by the client
typedef struct q2proto_clc_message_s {
    /**
     * Type of message. Determines the contained data.
     * Note: Some messages don't have any additional data beyond the type.
     */
    q2proto_clc_message_type_t type;

    union {
        /// Q2P_CLC_MOVE message
        q2proto_clc_move_t move;
        /// Q2P_CLC_BATCH_MOVE message
        q2proto_clc_batch_move_t batch_move;
        /// Q2P_CLC_USERINFO message
        q2proto_clc_userinfo_t userinfo;
        /// Q2P_CLC_STRING message
        q2proto_clc_stringcmd_t stringcmd;
        /// Q2P_CLC_SETTING message
        q2proto_clc_setting_t setting;
        /// Q2P_CLC_USERINFO_DELTA message
        q2proto_clc_userinfo_delta_t userinfo_delta;
    };
} q2proto_clc_message_t;

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_STRUCT_CLC_H_
