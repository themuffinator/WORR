/*
Copyright (C) 1997-2001 Id Software, Inc.
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
 * Quake 2 protocol constants
 */
#ifndef Q2PROTO_INTERNAL_PROTOCOL_H_
#define Q2PROTO_INTERNAL_PROTOCOL_H_

#include "q2proto/q2proto_game_api.h"
#include "q2proto_internal_defs.h"

// Protocol major version numbers
#define PROTOCOL_OLD_DEMO                 26
#define PROTOCOL_VANILLA                  34
#define PROTOCOL_R1Q2                     35
#define PROTOCOL_Q2PRO                    36
#define PROTOCOL_Q2PRO_DEMO_EXT           3434
#define PROTOCOL_Q2PRO_DEMO_EXT_LIMITS_2  3435
#define PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG 3436
#define PROTOCOL_Q2PRO_DEMO_EXT_CURRENT   3436
#define PROTOCOL_Q2REPRO                  1038
#define PROTOCOL_KEX_DEMOS                2022
#define PROTOCOL_KEX                      2023

// Protocol revision numbers used by R1Q2 and Q2PRO
#define PROTOCOL_VERSION_R1Q2_MINIMUM    1903 // b6377
#define PROTOCOL_VERSION_R1Q2_UCMD       1904 // b7387
#define PROTOCOL_VERSION_R1Q2_LONG_SOLID 1905 // b7759
#define PROTOCOL_VERSION_R1Q2_CURRENT    1905 // b7759

#define PROTOCOL_VERSION_Q2PRO_MINIMUM           1015 // r335
#define PROTOCOL_VERSION_Q2PRO_RESERVED          1016 // r364
#define PROTOCOL_VERSION_Q2PRO_BEAM_ORIGIN       1017 // r1037-8
#define PROTOCOL_VERSION_Q2PRO_SHORT_ANGLES      1018 // r1037-44
#define PROTOCOL_VERSION_Q2PRO_SERVER_STATE      1019 // r1302
#define PROTOCOL_VERSION_Q2PRO_EXTENDED_LAYOUT   1020 // r1354
#define PROTOCOL_VERSION_Q2PRO_ZLIB_DOWNLOADS    1021 // r1358
#define PROTOCOL_VERSION_Q2PRO_CLIENTNUM_SHORT   1022 // r2161
#define PROTOCOL_VERSION_Q2PRO_CINEMATICS        1023 // r2263
#define PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS   1024 // r2894
#define PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS_2 1025 // r3300
#define PROTOCOL_VERSION_Q2PRO_PLAYERFOG         1026 // r3579
#define PROTOCOL_VERSION_Q2PRO_CURRENT           1026 // r3579

/**\name Common protocol constants
 * @{ */
/// Server command IDs
enum common_svc_cmds {
    svc_muzzleflash = 1,
    svc_muzzleflash2,
    svc_temp_entity,
    svc_layout,
    svc_inventory,
    svc_nop,
    svc_disconnect,
    svc_reconnect,
    svc_sound,
    svc_print,
    svc_stufftext,
    svc_serverdata,
    svc_configstring,
    svc_spawnbaseline,
    svc_centerprint,
    svc_download,
    svc_playerinfo,
    svc_packetentities,
    svc_frame = 20,
    svc_r1q2_zpacket,
    svc_r1q2_zdownload,
    svc_r1q2_setting = 24,
    svc_q2pro_gamestate = 23,
    svc_q2pro_configstringstream = 25,
    svc_q2pro_baselinestream,
    svc_rr_splitclient = 21,
    svc_rr_configblast,
    svc_rr_spawnbaselineblast,
    svc_rr_damage = 25,
    svc_rr_locprint,
    svc_rr_fog,
    svc_rr_poi = 30,
    svc_rr_help_path,
    svc_rr_muzzleflash3,
    svc_rr_achievement,
    svc_q2repro_zpacket = 34,
    svc_q2repro_zdownload,
    svc_q2repro_gamestate,
    svc_q2repro_setting,
    svc_q2repro_configstringstream,
    svc_q2repro_baselinestream,
};

#define SND_VOLUME        BIT(0)
#define SND_ATTENUATION   BIT(1)
#define SND_POS           BIT(2)
#define SND_ENT           BIT(3)
#define SND_OFFSET        BIT(4)
#define SND_Q2PRO_INDEX16 BIT(5) // Q2PRO extended
#define SND_KEX_LARGE_ENT BIT(6) // KEX

#define DEFAULT_SOUND_PACKET_VOLUME      1.0f
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0f

#define U_ORIGIN1   BIT_ULL(0)
#define U_ORIGIN2   BIT_ULL(1)
#define U_ANGLE2    BIT_ULL(2)
#define U_ANGLE3    BIT_ULL(3)
#define U_FRAME8    BIT_ULL(4) // frame is a byte
#define U_EVENT     BIT_ULL(5)
#define U_REMOVE    BIT_ULL(6) // REMOVE this entity, don't add it
#define U_MOREBITS1 BIT_ULL(7) // read one additional byte

#define U_NUMBER16  BIT_ULL(8) // NUMBER8 is implicit if not set
#define U_ORIGIN3   BIT_ULL(9)
#define U_ANGLE1    BIT_ULL(10)
#define U_MODEL     BIT_ULL(11)
#define U_RENDERFX8 BIT_ULL(12) // fullbright, etc
#define U_ANGLE16   BIT_ULL(13) // Q2PRO: 'short' angles
#define U_EFFECTS8  BIT_ULL(14) // autorotate, trails, etc
#define U_MOREBITS2 BIT_ULL(15) // read one additional byte

#define U_SKIN8      BIT_ULL(16)
#define U_FRAME16    BIT_ULL(17) // frame is a short
#define U_RENDERFX16 BIT_ULL(18) // 8 + 16 = 32
#define U_EFFECTS16  BIT_ULL(19) // 8 + 16 = 32
#define U_MODEL2     BIT_ULL(20) // weapons, flags, etc
#define U_MODEL3     BIT_ULL(21)
#define U_MODEL4     BIT_ULL(22)
#define U_MOREBITS3  BIT_ULL(23) // read one additional byte

#define U_OLDORIGIN     BIT_ULL(24) // FIXME: get rid of this
#define U_SKIN16        BIT_ULL(25)
#define U_SOUND         BIT_ULL(26)
#define U_SOLID         BIT_ULL(27)
#define U_MODEL16       BIT_ULL(28)
#define U_MOREFX8       BIT_ULL(29) // Q2PRO, Q2rePRO
#define U_KEX_EFFECTS64 BIT_ULL(29) // KEX
#define U_ALPHA         BIT_ULL(30)
#define U_MOREBITS4     BIT_ULL(31) // read one additional byte

#define U_SCALE        BIT_ULL(32)
#define U_MOREFX16     BIT_ULL(33) // Q2PRO, Q2rePRO
#define U_KEX_INSTANCE BIT_ULL(33)
#define U_KEX_OWNER    BIT_ULL(34)
#define U_KEX_OLDFRAME BIT_ULL(35)

#define U_SKIN32     (U_SKIN8 | U_SKIN16) // used for laser colors
#define U_EFFECTS32  (U_EFFECTS8 | U_EFFECTS16)
#define U_RENDERFX32 (U_RENDERFX8 | U_RENDERFX16)
#define U_MOREFX32   (U_MOREFX8 | U_MOREFX16)

// duplicating TE values here as we need them to parse svc_temp_entity
typedef enum {
    TE_GUNSHOT,
    TE_BLOOD,
    TE_BLASTER,
    TE_RAILTRAIL,
    TE_SHOTGUN,
    TE_EXPLOSION1,
    TE_EXPLOSION2,
    TE_ROCKET_EXPLOSION,
    TE_GRENADE_EXPLOSION,
    TE_SPARKS,
    TE_SPLASH,
    TE_BUBBLETRAIL,
    TE_SCREEN_SPARKS,
    TE_SHIELD_SPARKS,
    TE_BULLET_SPARKS,
    TE_LASER_SPARKS,
    TE_PARASITE_ATTACK,
    TE_ROCKET_EXPLOSION_WATER,
    TE_GRENADE_EXPLOSION_WATER,
    TE_MEDIC_CABLE_ATTACK,
    TE_BFG_EXPLOSION,
    TE_BFG_BIGEXPLOSION,
    TE_BOSSTPORT, // used as '22' in a map, so DON'T RENUMBER!!!
    TE_BFG_LASER,
    TE_GRAPPLE_CABLE,
    TE_WELDING_SPARKS,
    TE_GREENBLOOD,
    TE_BLUEHYPERBLASTER,
    TE_PLASMA_EXPLOSION,
    TE_TUNNEL_SPARKS,

    // ROGUE
    TE_BLASTER2,
    TE_RAILTRAIL2,
    TE_FLAME,
    TE_LIGHTNING,
    TE_DEBUGTRAIL,
    TE_PLAIN_EXPLOSION,
    TE_FLASHLIGHT,
    TE_FORCEWALL,
    TE_HEATBEAM,
    TE_MONSTER_HEATBEAM,
    TE_STEAM,
    TE_BUBBLETRAIL2,
    TE_MOREBLOOD,
    TE_HEATBEAM_SPARKS,
    TE_HEATBEAM_STEAM,
    TE_CHAINFIST_SMOKE,
    TE_ELECTRIC_SPARKS,
    TE_TRACKER_EXPLOSION,
    TE_TELEPORT_EFFECT,
    TE_DBALL_GOAL,
    TE_WIDOWBEAMOUT,
    TE_NUKEBLAST,
    TE_WIDOWSPLASH,
    TE_EXPLOSION1_BIG,
    TE_EXPLOSION1_NP,
    TE_FLECHETTE,
    // ROGUE

    //[Paril-KEX]
    TE_BLUEHYPERBLASTER_2,
    TE_BFG_ZAP,
    TE_BERSERK_SLAM,
    TE_GRAPPLE_CABLE_2,
    TE_POWER_SPLASH,
    TE_LIGHTNING_BEAM,
    TE_EXPLOSION1_NL,
    TE_EXPLOSION2_NL,
    //[Paril-KEX]

    TE_Q2PRO_DAMAGE_DEALT = 128,

    TE_NUM_ENTITIES
} temp_event_t;

#define MZ_SILENCED BIT(7) // bit flag ORed with MZ_ value

#define PS_M_TYPE           BIT(0)
#define PS_M_ORIGIN         BIT(1)
#define PS_M_VELOCITY       BIT(2)
#define PS_M_TIME           BIT(3)
#define PS_M_FLAGS          BIT(4)
#define PS_M_GRAVITY        BIT(5)
#define PS_M_DELTA_ANGLES   BIT(6)
#define PS_VIEWOFFSET       BIT(7)
#define PS_VIEWANGLES       BIT(8)
#define PS_KICKANGLES       BIT(9)
#define PS_BLEND            BIT(10)
#define PS_FOV              BIT(11)
#define PS_WEAPONINDEX      BIT(12)
#define PS_WEAPONFRAME      BIT(13)
#define PS_RDFLAGS          BIT(14)
#define PS_RR_VIEWHEIGHT    BIT(15) // Q2rePRO
#define PS_MOREBITS         BIT(15) // Q2PRO extended, KEX
#define PS_Q2PRO_PLAYERFOG  BIT(16) // Q2PRO extended
#define PS_KEX_DAMAGE_BLEND BIT(16) // KEX
#define PS_KEX_TEAM_ID      BIT(17) // KEX

// r1q2 protocol specific extra flags
#define EPS_GUNOFFSET   BIT(0)
#define EPS_GUNANGLES   BIT(1)
#define EPS_M_VELOCITY2 BIT(2)
#define EPS_M_ORIGIN2   BIT(3)
#define EPS_VIEWANGLE2  BIT(4)
#define EPS_STATS       BIT(5)

// Q2PRO protocol specific extra flags
#define EPS_CLIENTNUM BIT(6)
// KEX
#define EPS_GUNRATE   BIT(7)

/// Client command IDs
enum common_clc_cmds {
    clc_bad,
    clc_nop,
    clc_move,
    clc_userinfo,
    clc_stringcmd,
    clc_r1q2_setting,
    clc_q2pro_move_nodelta = 10,
    clc_q2pro_move_batched,
    clc_q2pro_userinfo_delta,
};

#define CM_ANGLE1  BIT(0)
#define CM_ANGLE2  BIT(1)
#define CM_ANGLE3  BIT(2)
#define CM_FORWARD BIT(3)
#define CM_SIDE    BIT(4)
#define CM_UP      BIT(5)
#define CM_BUTTONS BIT(6)
#define CM_IMPULSE BIT(7)

// Flag for extended sound value: loop volume is present
#define SOUND_FLAG_VOLUME      BIT(14)
// Flag for extended sound value: loop attenuation is present
#define SOUND_FLAG_ATTENUATION BIT(15)

// Max size of a Q2PRO (possibly extended) entity baseline
#define Q2PRO_WRITE_GAMESTATE_BASELINE_SIZE \
    (1   /* command byte */                 \
     + 7 /* bits & number */                \
     + 8 /* model indices */                \
     + 2 /* frame */                        \
     + 4 /* skin */                         \
     + 8 /* effects  + morefx */            \
     + 4 /* renderfx */                     \
     + 9 /* origin */                       \
     + 6 /* angles */                       \
     + 9 /* old_origin */                   \
     + 2 /* sound */                        \
     + 1 /* loop volume */                  \
     + 1 /* loop attenuation */             \
     + 1 /* event */                        \
     + 4 /* solid */                        \
     + 1 /* alpha */                        \
     + 1 /* scale */                        \
    )

// Q2PRO/Q2rePRO protocol flags
#define Q2PRO_PF_STRAFEJUMP_HACK BIT(0)
#define Q2PRO_PF_QW_MODE         BIT(1)
#define Q2PRO_PF_WATERJUMP_HACK  BIT(2)
#define Q2PRO_PF_EXTENSIONS      BIT(3)
#define Q2PRO_PF_EXTENSIONS_2    BIT(4)
#define Q2REPRO_PF_GAME3_COMPAT  BIT(15) // This indicates the server game library is a version 3 game

/// Max. configstrings for "version 3" games/servers
#define MAX_CONFIGSTRINGS_V3        2080
/// Max. configstrings for "q2pro extended" games/servers
#define MAX_CONFIGSTRINGS_EXTENDED  13630
/// Max. configstrings for "rerelease" games/servers
#define MAX_CONFIGSTRINGS_RERELEASE 12448

static inline unsigned int max_configstrings_for_game(q2proto_game_api_t game)
{
    switch (game) {
    case Q2PROTO_GAME_VANILLA:
        return MAX_CONFIGSTRINGS_V3;
    case Q2PROTO_GAME_Q2PRO_EXTENDED:
    case Q2PROTO_GAME_Q2PRO_EXTENDED_V2:
        return MAX_CONFIGSTRINGS_EXTENDED;
    case Q2PROTO_GAME_RERELEASE:
        return MAX_CONFIGSTRINGS_RERELEASE;
    }
    // shouldn't happen...
    return MAX_CONFIGSTRINGS_V3;
}

/** @} */

/// Number of bits used by Q2PRO, Q2rePRO, KEX protocols for gunindex (remainder is gunskin)
#define Q2PRO_GUNINDEX_BITS 13
/// Mask to extract gunindex
#define Q2PRO_GUNINDEX_MASK (BIT(13) - 1)

#endif // Q2PROTO_INTERNAL_PROTOCOL_H_
