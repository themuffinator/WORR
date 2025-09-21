/*
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

#define Q2PROTO_BUILD
#include "q2proto_internal_debug.h"

#include "q2proto_internal_fmt.h"
#include "q2proto_internal_protocol.h"

const char *q2proto_debug_common_svc_string(int command)
{
#define S(X) \
    case X:  \
        return #X;

    switch (command) {
        S(svc_muzzleflash)
        S(svc_muzzleflash2)
        S(svc_temp_entity)
        S(svc_layout)
        S(svc_inventory)
        S(svc_nop)
        S(svc_disconnect)
        S(svc_reconnect)
        S(svc_sound)
        S(svc_print)
        S(svc_stufftext)
        S(svc_serverdata)
        S(svc_configstring)
        S(svc_spawnbaseline)
        S(svc_centerprint)
        S(svc_download)
        S(svc_playerinfo)
        S(svc_packetentities)
        S(svc_frame)
    }

#undef S

    return NULL;
}

#define SHOWBITS(DESCR)                                         \
    do {                                                        \
        if (!first)                                             \
            q2proto_snprintf_update(&buf, &size, " %s", DESCR); \
        else                                                    \
            q2proto_snprintf_update(&buf, &size, "%s", DESCR);  \
        first = false;                                          \
    } while (0)

void q2proto_debug_common_entity_delta_bits_to_str(char *buf, size_t size, uint64_t bits)
{
    bool first = true;

#define S(b, s)         \
    if (bits & U_##b) { \
        SHOWBITS(s);    \
        bits &= ~U_##b; \
    }
    S(REMOVE, "remove")

    S(MODEL16, "model16")
    S(MODEL, "modelindex");
    S(MODEL2, "modelindex2");
    S(MODEL3, "modelindex3");
    S(MODEL4, "modelindex4");

    if (bits & U_FRAME8)
        SHOWBITS("frame8");
    if (bits & U_FRAME16)
        SHOWBITS("frame16");
    bits &= ~(U_FRAME8 | U_FRAME16);

    if ((bits & U_SKIN32) == U_SKIN32)
        SHOWBITS("skinnum32");
    else if (bits & U_SKIN8)
        SHOWBITS("skinnum8");
    else if (bits & U_SKIN16)
        SHOWBITS("skinnum16");
    bits &= ~U_SKIN32;

    if ((bits & U_EFFECTS32) == U_EFFECTS32)
        SHOWBITS("effects32");
    else if (bits & U_EFFECTS8)
        SHOWBITS("effects8");
    else if (bits & U_EFFECTS16)
        SHOWBITS("effects16");
    bits &= ~U_EFFECTS32;

    if ((bits & U_RENDERFX32) == U_RENDERFX32)
        SHOWBITS("renderfx32");
    else if (bits & U_RENDERFX8)
        SHOWBITS("renderfx8");
    else if (bits & U_RENDERFX16)
        SHOWBITS("renderfx16");
    bits &= ~U_RENDERFX32;

    if ((bits & U_MOREFX32) == U_MOREFX32)
        SHOWBITS("more32");
    else if (bits & U_MOREFX8)
        SHOWBITS("more8");
    else if (bits & U_MOREFX16)
        SHOWBITS("more16");
    bits &= ~U_MOREFX32;

    S(ORIGIN1, "origin[0]");
    S(ORIGIN2, "origin[1]");
    S(ORIGIN3, "origin[2]");
    S(ANGLE1, "angles[0]");
    S(ANGLE2, "angles[1]");
    S(ANGLE3, "angles[2]");
    S(OLDORIGIN, "old_origin");
    S(SOUND, "sound");
    S(EVENT, "event");
    S(SOLID, "solid");
    S(ALPHA, "alpha");
    S(SCALE, "scale");
#undef S

    // Bits to ignore
    bits &= ~(U_NUMBER16 | U_MOREBITS1 | U_MOREBITS2 | U_MOREBITS3 | U_MOREBITS4 | U_ANGLE16);

    if (bits != 0 || first) {
        if (!first)
            q2proto_snprintf_update(&buf, &size, " 0x%llx", bits);
        else
            q2proto_snprintf_update(&buf, &size, "0x%llx", bits);
    }
}

void q2proto_debug_common_player_delta_bits_to_str(char *buf, size_t size, uint32_t bits)
{
    bool first = true;

#define S(b, s)          \
    if (bits & PS_##b) { \
        SHOWBITS(s);     \
        bits &= ~PS_##b; \
    }

    S(M_TYPE, "pm_type");
    S(M_ORIGIN, "pm_origin");
    S(M_VELOCITY, "pm_velocity");
    S(M_TIME, "pm_time");
    S(M_FLAGS, "pm_flags");
    S(M_GRAVITY, "pm_gravity");
    S(M_DELTA_ANGLES, "pm_delta_angles");
    S(VIEWOFFSET, "viewoffset");
    S(VIEWANGLES, "viewangles");
    S(KICKANGLES, "kick_angles");
    S(WEAPONINDEX, "gunindex");
    S(WEAPONFRAME, "gunframe");
    S(BLEND, "blend");
    S(FOV, "fov");
    S(RDFLAGS, "rdflags");
#undef S

    if (bits != 0 || first) {
        if (!first)
            q2proto_snprintf_update(&buf, &size, " 0x%x", bits);
        else
            q2proto_snprintf_update(&buf, &size, "0x%x", bits);
    }
}

void q2proto_debug_common_player_delta_extrabits_to_str(char *buf, size_t size, uint32_t bits)
{
    bool first = true;

#define S(b, s)           \
    if (bits & EPS_##b) { \
        SHOWBITS(s);      \
        bits &= ~EPS_##b; \
    }

    S(GUNOFFSET, "gunoffset");
    S(GUNANGLES, "gunangles");
    S(GUNRATE, "gunrate")
    S(M_VELOCITY2, "pm_velocity2");
    S(M_ORIGIN2, "pm_origin2");
    S(VIEWANGLE2, "viewangle2");
    S(STATS, "stats");
    S(CLIENTNUM, "clientnum");
#undef S

    if (bits != 0 || first) {
        if (!first)
            q2proto_snprintf_update(&buf, &size, " 0x%x", bits);
        else
            q2proto_snprintf_update(&buf, &size, "0x%x", bits);
    }
}

#undef SHOWBITS
