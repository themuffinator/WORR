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

#define Q2PROTO_BUILD
#include "q2proto_internal.h"

#define GUNBIT_OFFSET_X BIT(0)
#define GUNBIT_OFFSET_Y BIT(1)
#define GUNBIT_OFFSET_Z BIT(2)
#define GUNBIT_ANGLES_X BIT(3)
#define GUNBIT_ANGLES_Y BIT(4)
#define GUNBIT_ANGLES_Z BIT(5)
#define GUNBIT_GUNRATE  BIT(6)

//
// CLIENT: PARSE MESSAGES FROM SERVER
//

static q2proto_error_t kex_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                       q2proto_svc_message_t *svc_message);
static q2proto_error_t kex_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static uint32_t kex_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins, const q2proto_vec3_t maxs);
static void kex_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                             q2proto_vec3_t maxs);

q2proto_error_t q2proto_kex_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                q2proto_svc_serverdata_t *serverdata)
{
    context->pack_solid = kex_pack_solid;
    context->unpack_solid = kex_unpack_solid;

    READ_CHECKED(client_read, io_arg, serverdata->servercount, i32);
    READ_CHECKED(client_read, io_arg, serverdata->attractloop, bool);
    READ_CHECKED(client_read, io_arg, serverdata->kex.server_fps, u8);
    READ_CHECKED(client_read, io_arg, serverdata->gamedir, string);
    READ_CHECKED(client_read, io_arg, serverdata->clientnum, i16);
    if (serverdata->clientnum == -2)
        // FIXME: -2 indicates split screen - don't support this at all
        return Q2P_ERR_BAD_DATA;
    READ_CHECKED(client_read, io_arg, serverdata->levelname, string);

    context->client_read = kex_client_read;
    context->server_protocol = q2proto_protocol_from_netver(serverdata->protocol);
    context->protocol_version = serverdata->protocol_version;
    context->features.has_solid32 = true;
    context->features.server_game_api = Q2PROTO_GAME_RERELEASE;

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                  q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t kex_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg, uint64_t bits,
                                                    unsigned entnum, q2proto_entity_state_delta_t *entity_state,
                                                    bool default_solid_nonzero);
static q2proto_error_t kex_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t kex_client_read_sound(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                             q2proto_svc_sound_t *sound);
static q2proto_error_t kex_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                             q2proto_svc_frame_t *frame);
static q2proto_error_t kex_client_read_splitclient(q2proto_clientcontext_t *context, uintptr_t io_arg);
static q2proto_error_t kex_client_read_begin_configblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                         q2proto_svc_message_t *svc_message);
static q2proto_error_t kex_client_read_begin_spawnbaselineblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                                q2proto_svc_message_t *svc_message);
static q2proto_error_t kex_client_read_locprint(uintptr_t io_arg, q2proto_svc_locprint_t *locprint);

static MAYBE_UNUSED const char *kex_server_cmd_string(int command)
{
#define S(X) \
    case X:  \
        return #X;

    switch (command) {
        S(svc_rr_splitclient)
        S(svc_rr_configblast)
        S(svc_rr_spawnbaselineblast)
        S(svc_rr_damage)
        S(svc_rr_locprint)
        S(svc_rr_fog)
        S(svc_rr_poi)
        S(svc_rr_help_path)
        S(svc_rr_muzzleflash3)
    }

#undef S

    const char *str = q2proto_debug_common_svc_string(command);
    return str ? str : q2proto_va("%d", command);
}

static MAYBE_UNUSED void kex_debug_player_delta_bits_to_str(char *buf, size_t size, uint32_t bits)
{
    q2proto_debug_common_player_delta_bits_to_str(buf, size,
                                                  bits & ~(PS_MOREBITS | PS_KEX_DAMAGE_BLEND | PS_KEX_TEAM_ID));
    size -= strlen(buf);
    buf += strlen(buf);

#define S(b, s)                                         \
    if (bits & PS_##b) {                                \
        q2proto_snprintf_update(&buf, &size, " %s", s); \
        bits &= ~PS_##b;                                \
    }

    S(KEX_DAMAGE_BLEND, "damage_blend");
    S(KEX_TEAM_ID, "team_id");
#undef S
}

static MAYBE_UNUSED void kex_debug_shownet_entity_delta_bits(uintptr_t io_arg, const char *prefix, uint16_t entnum,
                                                             uint64_t bits)
{
#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && bits) {
        char buf[1024];
        q2proto_debug_common_entity_delta_bits_to_str(
            buf, sizeof(buf), bits & ~(U_KEX_EFFECTS64 | U_KEX_INSTANCE | U_KEX_OLDFRAME | U_KEX_OWNER));

        char *buf_p = buf + strlen(buf);
        size_t buf_remaining = sizeof(buf) - strlen(buf);
    #define S(b, s)                                                    \
        if (bits & U_##b) {                                            \
            q2proto_snprintf_update(&buf_p, &buf_remaining, " %s", s); \
            bits &= ~U_##b;                                            \
        }

        S(KEX_EFFECTS64, "effects64");
        S(KEX_INSTANCE, "instance");
        S(KEX_OLDFRAME, "oldframe");
        S(KEX_OWNER, "owner");
    #undef S

        SHOWNET(io_arg, 2, -q2proto_common_entity_bits_size(bits), "%s%i %s", prefix, entnum, buf);
    }
#endif
}

static q2proto_error_t kex_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                       q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    size_t command_read = 0;
    const void *command_ptr = NULL;
    READ_CHECKED(client_read, io_arg, command_ptr, raw, 1, &command_read);
    if (command_read == 0)
        return Q2P_ERR_NO_MORE_INPUT;

    uint8_t command = *(const uint8_t *)command_ptr;
    SHOWNET(io_arg, 1, -1, "%s", kex_server_cmd_string(command));

    switch (command) {
    case svc_nop:
        svc_message->type = Q2P_SVC_NOP;
        return Q2P_ERR_SUCCESS;

    case svc_disconnect:
        svc_message->type = Q2P_SVC_DISCONNECT;
        return Q2P_ERR_SUCCESS;

    case svc_reconnect:
        svc_message->type = Q2P_SVC_RECONNECT;
        return Q2P_ERR_SUCCESS;

    case svc_print:
        svc_message->type = Q2P_SVC_PRINT;
        return q2proto_common_client_read_print(io_arg, &svc_message->print);

    case svc_centerprint:
        svc_message->type = Q2P_SVC_CENTERPRINT;
        return q2proto_common_client_read_centerprint(io_arg, &svc_message->centerprint);

    case svc_stufftext:
        svc_message->type = Q2P_SVC_STUFFTEXT;
        return q2proto_common_client_read_stufftext(io_arg, &svc_message->stufftext);

    case svc_serverdata:
        svc_message->type = Q2P_SVC_SERVERDATA;
        return kex_client_read_serverdata(context, io_arg, &svc_message->serverdata);

    case svc_configstring:
        svc_message->type = Q2P_SVC_CONFIGSTRING;
        return q2proto_common_client_read_configstring(io_arg, &svc_message->configstring);

    case svc_sound:
        svc_message->type = Q2P_SVC_SOUND;
        return kex_client_read_sound(context, io_arg, &svc_message->sound);

    case svc_spawnbaseline:
        svc_message->type = Q2P_SVC_SPAWNBASELINE;
        return kex_client_read_baseline(context, io_arg, &svc_message->spawnbaseline);

    case svc_temp_entity:
        svc_message->type = Q2P_SVC_TEMP_ENTITY;
        if (context->server_protocol == Q2P_PROTOCOL_KEX_DEMOS)
            return q2proto_common_client_read_temp_entity_short(io_arg, Q2PROTO_GAME_RERELEASE,
                                                                &svc_message->temp_entity);
        else
            return q2proto_common_client_read_temp_entity_float(io_arg, Q2PROTO_GAME_RERELEASE,
                                                                &svc_message->temp_entity);

    case svc_muzzleflash:
        svc_message->type = Q2P_SVC_MUZZLEFLASH;
        return q2proto_common_client_read_muzzleflash(io_arg, &svc_message->muzzleflash, MZ_SILENCED);

    case svc_muzzleflash2:
        svc_message->type = Q2P_SVC_MUZZLEFLASH2;
        return q2proto_common_client_read_muzzleflash(io_arg, &svc_message->muzzleflash, 0);

    case svc_rr_muzzleflash3:
        svc_message->type = Q2P_SVC_MUZZLEFLASH2;
        return q2proto_q2repro_client_read_muzzleflash3(io_arg, &svc_message->muzzleflash);

    case svc_frame:
        svc_message->type = Q2P_SVC_FRAME;
        return kex_client_read_frame(context, io_arg, &svc_message->frame);

    case svc_inventory:
        svc_message->type = Q2P_SVC_INVENTORY;
        return q2proto_common_client_read_inventory(io_arg, &svc_message->inventory);

    case svc_layout:
        svc_message->type = Q2P_SVC_LAYOUT;
        return q2proto_common_client_read_layout(io_arg, &svc_message->layout);

    case svc_rr_splitclient:
        // Split screen messages are currently not supported...
        svc_message->type = Q2P_SVC_NOP;
        return kex_client_read_splitclient(context, io_arg);

    case svc_rr_configblast:
        return kex_client_read_begin_configblast(context, io_arg, svc_message);

    case svc_rr_spawnbaselineblast:
        return kex_client_read_begin_spawnbaselineblast(context, io_arg, svc_message);

    case svc_rr_damage:
        svc_message->type = Q2P_SVC_DAMAGE;
        return q2proto_q2repro_client_read_damage(io_arg, &svc_message->damage);

    case svc_rr_locprint:
        svc_message->type = Q2P_SVC_LOCPRINT;
        return kex_client_read_locprint(io_arg, &svc_message->locprint);

    case svc_rr_fog:
        svc_message->type = Q2P_SVC_FOG;
        return q2proto_q2repro_client_read_fog(io_arg, &svc_message->fog);

    case svc_rr_poi:
        svc_message->type = Q2P_SVC_POI;
        return q2proto_q2repro_client_read_poi(io_arg, &svc_message->poi);

    case svc_rr_help_path:
        svc_message->type = Q2P_SVC_HELP_PATH;
        return q2proto_q2repro_client_read_help_path(io_arg, &svc_message->help_path);

    case svc_rr_achievement:
        svc_message->type = Q2P_SVC_ACHIEVEMENT;
        return q2proto_q2repro_client_read_achievement(io_arg, &svc_message->achievement);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t kex_client_read_delta_entities(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                      q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    // zpacket might contain multiple packets, so try to read from inflated message repeatedly
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    svc_message->type = Q2P_SVC_FRAME_ENTITY_DELTA;
    q2proto_error_t err = kex_client_next_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);
    if (err != Q2P_ERR_SUCCESS) {
        // FIXME: May be insufficient, might need some explicit way to reset parsing...
        context->client_read = kex_client_read;
        return err;
    }

    if (svc_message->frame_entity_delta.newnum == 0) {
        context->client_read = kex_client_read;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_frame_entity_delta_t *frame_entity_delta)
{
    memset(frame_entity_delta, 0, sizeof(*frame_entity_delta));

    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &frame_entity_delta->newnum));

    kex_debug_shownet_entity_delta_bits(io_arg, "   delta:", frame_entity_delta->newnum, bits);

    if (frame_entity_delta->newnum == 0) {
        return Q2P_ERR_SUCCESS;
    }

    if (bits & U_REMOVE) {
        frame_entity_delta->remove = true;
        // non-zero solid tracking: reset to baseline
        q2proto_set_entity_bit(
            context->kex_demo_edict_nonzero_solid, frame_entity_delta->newnum,
            q2proto_get_entity_bit(context->kex_demo_baseline_nonzero_solid, frame_entity_delta->newnum));
        return Q2P_ERR_SUCCESS;
    }

    return kex_client_read_entity_delta(
        context, io_arg, bits, frame_entity_delta->newnum, &frame_entity_delta->entity_delta,
        q2proto_get_entity_bit(context->kex_demo_edict_nonzero_solid, frame_entity_delta->newnum));
}

static q2proto_error_t kex_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                  q2proto_svc_serverdata_t *serverdata)
{
    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    if (protocol < PROTOCOL_KEX_DEMOS || protocol > PROTOCOL_KEX)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "unexpected protocol %d", protocol);

    serverdata->protocol = protocol;

    return q2proto_kex_continue_serverdata(context, io_arg, serverdata);
}

static q2proto_error_t kex_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg, uint64_t bits,
                                                    unsigned entnum, q2proto_entity_state_delta_t *entity_state,
                                                    bool default_solid_nonzero)
{
    bool model16 = bits & U_MODEL16;
    if (delta_bits_check(bits, U_MODEL, &entity_state->delta_bits, Q2P_ESD_MODELINDEX)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex, u8);
    }
    if (delta_bits_check(bits, U_MODEL2, &entity_state->delta_bits, Q2P_ESD_MODELINDEX2)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex2, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex2, u8);
    }
    if (delta_bits_check(bits, U_MODEL3, &entity_state->delta_bits, Q2P_ESD_MODELINDEX3)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex3, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex3, u8);
    }
    if (delta_bits_check(bits, U_MODEL4, &entity_state->delta_bits, Q2P_ESD_MODELINDEX4)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex4, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex4, u8);
    }

    if (delta_bits_check(bits, U_FRAME8, &entity_state->delta_bits, Q2P_ESD_FRAME))
        READ_CHECKED(client_read, io_arg, entity_state->frame, u8);
    else if (delta_bits_check(bits, U_FRAME16, &entity_state->delta_bits, Q2P_ESD_FRAME))
        READ_CHECKED(client_read, io_arg, entity_state->frame, u16);

    if (delta_bits_check(bits, U_SKIN32, &entity_state->delta_bits, Q2P_ESD_SKINNUM)) {
        if ((bits & U_SKIN32) == U_SKIN32) // used for laser colors
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u32);
        else if (bits & U_SKIN16)
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u16);
        else if (bits & U_SKIN8)
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u8);
    }

    // if 64-bit effects are sent, the low bits are sent first
    // and the high bits come after.
    uint64_t effects = 0;
    uint32_t low_effects = 0;
    if (bits & U_KEX_EFFECTS64) {
        READ_CHECKED(client_read, io_arg, low_effects, u32);
    }

    if (bits & U_EFFECTS32) {
        if ((bits & U_EFFECTS32) == U_EFFECTS32)
            READ_CHECKED(client_read, io_arg, effects, u32);
        else if (bits & U_EFFECTS16)
            READ_CHECKED(client_read, io_arg, effects, u16);
        else if (bits & U_EFFECTS8)
            READ_CHECKED(client_read, io_arg, effects, u8);
    }

    if (bits & U_KEX_EFFECTS64)
        effects = effects << 32 | low_effects;

    // All 64 effects bits are always replaced
    if (bits & (U_KEX_EFFECTS64 | U_EFFECTS32)) {
        entity_state->delta_bits |= Q2P_ESD_EFFECTS | Q2P_ESD_EFFECTS_MORE;
        entity_state->effects = (uint32_t)effects;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->effects_more = effects >> 32;
#endif
    }

    if (delta_bits_check(bits, U_RENDERFX32, &entity_state->delta_bits, Q2P_ESD_RENDERFX)) {
        if ((bits & U_RENDERFX32) == U_RENDERFX32)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u32);
        else if (bits & U_RENDERFX16)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u16);
        else if (bits & U_RENDERFX8)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u8);
    }

    /* note: for the protocol in the demos (2022), if `solid` is zero, origin reads are lower precision
     * (12.3 fixed point); so track whether solid is != 0 */
    bool nonzero_solid;
    if (delta_bits_check(bits, U_SOLID, &entity_state->delta_bits, Q2P_ESD_SOLID)) {
        READ_CHECKED(client_read, io_arg, entity_state->solid, u32);
        nonzero_solid = entity_state->solid != 0;
        q2proto_set_entity_bit(context->kex_demo_edict_nonzero_solid, entnum, nonzero_solid);
    } else
        nonzero_solid = default_solid_nonzero;

    bool high_precision_origin = context->server_protocol != Q2P_PROTOCOL_KEX_DEMOS || nonzero_solid;
    entity_state->origin.read.value.delta_bits = 0;
    if (high_precision_origin) {
        if (bits & U_ORIGIN1) {
            READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 0);
            entity_state->origin.read.value.delta_bits |= BIT(0);
        }
        if (bits & U_ORIGIN2) {
            READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 1);
            entity_state->origin.read.value.delta_bits |= BIT(1);
        }
        if (bits & U_ORIGIN3) {
            READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 2);
            entity_state->origin.read.value.delta_bits |= BIT(2);
        }
        if (delta_bits_check(bits, U_OLDORIGIN, &entity_state->delta_bits, Q2P_ESD_OLD_ORIGIN)) {
            CHECKED(client_read, io_arg, read_var_coords_float(io_arg, &entity_state->old_origin));
        }
    } else {
        if (bits & U_ORIGIN1) {
            READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 0);
            entity_state->origin.read.value.delta_bits |= BIT(0);
        }
        if (bits & U_ORIGIN2) {
            READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 1);
            entity_state->origin.read.value.delta_bits |= BIT(1);
        }
        if (bits & U_ORIGIN3) {
            READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 2);
            entity_state->origin.read.value.delta_bits |= BIT(2);
        }
        if (delta_bits_check(bits, U_OLDORIGIN, &entity_state->delta_bits, Q2P_ESD_OLD_ORIGIN)) {
            CHECKED(client_read, io_arg, read_var_coords_short(io_arg, &entity_state->old_origin));
        }
    }

    entity_state->angle.delta_bits = 0;
    if (bits & U_ANGLE1) {
        READ_CHECKED_VAR_ANGLES_COMP_FLOAT(client_read, io_arg, &entity_state->angle.values, 0);
        entity_state->angle.delta_bits |= BIT(0);
    }
    if (bits & U_ANGLE2) {
        READ_CHECKED_VAR_ANGLES_COMP_FLOAT(client_read, io_arg, &entity_state->angle.values, 1);
        entity_state->angle.delta_bits |= BIT(1);
    }
    if (bits & U_ANGLE3) {
        READ_CHECKED_VAR_ANGLES_COMP_FLOAT(client_read, io_arg, &entity_state->angle.values, 2);
        entity_state->angle.delta_bits |= BIT(2);
    }

    if (delta_bits_check(bits, U_SOUND, &entity_state->delta_bits, Q2P_ESD_SOUND)) {
        uint16_t sound_word;
        READ_CHECKED(client_read, io_arg, sound_word, u16);
        entity_state->sound = sound_word & 0x3fff;
        if (delta_bits_check(sound_word, SOUND_FLAG_VOLUME, &entity_state->delta_bits, Q2P_ESD_LOOP_VOLUME)) {
            MAYBE_UNUSED uint8_t loop_volume;
            READ_CHECKED(client_read, io_arg, loop_volume, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
            entity_state->loop_volume = loop_volume;
#endif
        }
        if (delta_bits_check(sound_word, SOUND_FLAG_ATTENUATION, &entity_state->delta_bits, Q2P_ESD_LOOP_ATTENUATION)) {
            MAYBE_UNUSED uint8_t loop_attenuation;
            READ_CHECKED(client_read, io_arg, loop_attenuation, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
            entity_state->loop_attenuation = loop_attenuation;
#endif
        }
    }

    if (delta_bits_check(bits, U_EVENT, &entity_state->delta_bits, Q2P_ESD_EVENT))
        READ_CHECKED(client_read, io_arg, entity_state->event, u8);

    if (delta_bits_check(bits, U_ALPHA, &entity_state->delta_bits, Q2P_ESD_ALPHA)) {
        MAYBE_UNUSED uint8_t alpha;
        READ_CHECKED(client_read, io_arg, alpha, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->alpha = alpha;
#endif
    }

    if (delta_bits_check(bits, U_SCALE, &entity_state->delta_bits, Q2P_ESD_SCALE)) {
        MAYBE_UNUSED uint8_t scale;
        READ_CHECKED(client_read, io_arg, scale, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->scale = scale;
#endif
    }

    if (bits & U_KEX_INSTANCE) {
        uint8_t instance_bits;
        READ_CHECKED(client_read, io_arg, instance_bits, u8);
        (void)instance_bits;
    }

    if (bits & U_KEX_OWNER) {
        uint16_t owner;
        READ_CHECKED(client_read, io_arg, owner, u16);
        (void)owner;
    }

    if (bits & U_KEX_OLDFRAME) {
        uint16_t oldframe;
        READ_CHECKED(client_read, io_arg, oldframe, u16);
        (void)oldframe;
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &spawnbaseline->entnum));

    kex_debug_shownet_entity_delta_bits(io_arg, "   baseline:", spawnbaseline->entnum, bits);

    q2proto_error_t result =
        kex_client_read_entity_delta(context, io_arg, bits, spawnbaseline->entnum, &spawnbaseline->delta_state, false);
    bool nonzero_solid = q2proto_get_entity_bit(context->kex_demo_edict_nonzero_solid, spawnbaseline->entnum);
    q2proto_set_entity_bit(context->kex_demo_baseline_nonzero_solid, spawnbaseline->entnum, nonzero_solid);
    return result;
}

static q2proto_error_t kex_client_read_sound(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                             q2proto_svc_sound_t *sound)
{
    READ_CHECKED(client_read, io_arg, sound->flags, u8);
    READ_CHECKED(client_read, io_arg, sound->index, u16);

    if (sound->flags & SND_VOLUME)
        READ_CHECKED(client_read, io_arg, sound->volume, u8);
    else
        sound->volume = SOUND_DEFAULT_VOLUME;

    if (sound->flags & SND_ATTENUATION)
        READ_CHECKED(client_read, io_arg, sound->attenuation, u8);
    else
        sound->attenuation = SOUND_DEFAULT_ATTENUATION;

    if (sound->flags & SND_OFFSET)
        READ_CHECKED(client_read, io_arg, sound->timeofs, u8);

    if (sound->flags & SND_ENT) {
        // entity relative
        uint32_t entchan;
        if (sound->flags & SND_KEX_LARGE_ENT)
            READ_CHECKED(client_read, io_arg, entchan, u32);
        else
            READ_CHECKED(client_read, io_arg, entchan, u16);
        sound->entity = entchan >> 3;
        sound->channel = entchan & 7;
    }

    // positioned in space
    if (sound->flags & SND_POS) {
        if (context->server_protocol == Q2P_PROTOCOL_KEX_DEMOS)
            CHECKED(client_read, io_arg, read_var_coords_short(io_arg, &sound->pos));
        else
            CHECKED(client_read, io_arg, read_var_coords_float(io_arg, &sound->pos));
    }

    return Q2P_ERR_SUCCESS;
}

/// Read a single component of a gunoffset
#define READ_CHECKED_GUNOFFSET_COMP_FLOAT(SOURCE, IO_ARG, TARGET, COMP)      \
    do {                                                                     \
        float o;                                                             \
        READ_CHECKED(SOURCE, (IO_ARG), o, float);                            \
        q2proto_var_small_offsets_set_float_comp(&(TARGET).values, COMP, o); \
        (TARGET).delta_bits |= BIT(COMP);                                    \
    } while (0)

/// Read a single component of a gunangle
#define READ_CHECKED_GUNANGLES_COMP_FLOAT(SOURCE, IO_ARG, TARGET, COMP)     \
    do {                                                                    \
        float a;                                                            \
        READ_CHECKED(SOURCE, (IO_ARG), a, float);                           \
        q2proto_var_small_angles_set_float_comp(&(TARGET).values, COMP, a); \
        (TARGET).delta_bits |= BIT(COMP);                                   \
    } while (0)

static q2proto_error_t kex_client_read_playerstate(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                   q2proto_svc_playerstate_t *playerstate)
{
    uint32_t flags;
    READ_CHECKED(client_read, io_arg, flags, u16);
    if (flags & PS_MOREBITS) {
        uint16_t more_flags;
        READ_CHECKED(client_read, io_arg, more_flags, u16);
        flags |= more_flags << 16;
    }

#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && flags) {
        char buf[1024];
        kex_debug_player_delta_bits_to_str(buf, sizeof(buf), flags);
        SHOWNET(io_arg, 2, -2, "   %s", buf);
    }
#endif

    //
    // parse the pmove_state_t
    //
    if (delta_bits_check(flags, PS_M_TYPE, &playerstate->delta_bits, Q2P_PSD_PM_TYPE))
        READ_CHECKED(client_read, io_arg, playerstate->pm_type, u8);

    if (flags & PS_M_ORIGIN) {
        CHECKED(client_read, io_arg, read_var_coords_float(io_arg, &playerstate->pm_origin.read.value.values));
        playerstate->pm_origin.read.value.delta_bits = 0x7;
    }

    if (flags & PS_M_VELOCITY) {
        CHECKED(client_read, io_arg, read_var_coords_float(io_arg, &playerstate->pm_velocity.read.value.values));
        playerstate->pm_velocity.read.value.delta_bits = 0x7;
    }

    if (delta_bits_check(flags, PS_M_TIME, &playerstate->delta_bits, Q2P_PSD_PM_TIME))
        READ_CHECKED(client_read, io_arg, playerstate->pm_time, u16);

    if (delta_bits_check(flags, PS_M_FLAGS, &playerstate->delta_bits, Q2P_PSD_PM_FLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->pm_flags, u16);

    if (delta_bits_check(flags, PS_M_GRAVITY, &playerstate->delta_bits, Q2P_PSD_PM_GRAVITY))
        READ_CHECKED(client_read, io_arg, playerstate->pm_gravity, i16);

    if (delta_bits_check(flags, PS_M_DELTA_ANGLES, &playerstate->delta_bits, Q2P_PSD_PM_DELTA_ANGLES))
        CHECKED(client_read, io_arg, read_var_angles_float(io_arg, &playerstate->pm_delta_angles));

    //
    // parse the rest of the player_state_t
    //
    if (delta_bits_check(flags, PS_VIEWOFFSET, &playerstate->delta_bits, Q2P_PSD_VIEWOFFSET | Q2P_PSD_PM_VIEWHEIGHT)) {
        CHECKED(client_read, io_arg, read_viewoffsets_q2repro(io_arg, &playerstate->viewoffset));
        // Documented as not in protocol 2022. But in reality it seems to be there.
        MAYBE_UNUSED int8_t pm_viewheight;
        READ_CHECKED(client_read, io_arg, pm_viewheight, i8);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
        playerstate->pm_viewheight = pm_viewheight;
#endif
    }

    if (flags & PS_VIEWANGLES) {
        CHECKED(client_read, io_arg, read_var_angles_float(io_arg, &playerstate->viewangles.values));
        playerstate->viewangles.delta_bits = 0x7;
    }

    if (delta_bits_check(flags, PS_KICKANGLES, &playerstate->delta_bits, Q2P_PSD_KICKANGLES))
        CHECKED(client_read, io_arg, read_kickangles_q2repro(io_arg, &playerstate->kick_angles));

    if (delta_bits_check(flags, PS_WEAPONINDEX, &playerstate->delta_bits, Q2P_PSD_GUNINDEX | Q2P_PSD_GUNSKIN)) {
        uint16_t gun_index_and_skin;
        READ_CHECKED(client_read, io_arg, gun_index_and_skin, u16);
        playerstate->gunindex = gun_index_and_skin & Q2PRO_GUNINDEX_MASK;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        playerstate->gunskin = gun_index_and_skin >> Q2PRO_GUNINDEX_BITS;
#endif
    }

    if (flags & PS_WEAPONFRAME) {
        uint16_t gunbits;
        READ_CHECKED(client_read, io_arg, gunbits, u16);
        playerstate->gunframe = gunbits & 0x1ff;
        playerstate->delta_bits |= Q2P_PSD_GUNFRAME;
        gunbits >>= 9;

        if (gunbits & GUNBIT_OFFSET_X)
            READ_CHECKED_GUNOFFSET_COMP_FLOAT(client_read, io_arg, playerstate->gunoffset, 0);
        if (gunbits & GUNBIT_OFFSET_Y)
            READ_CHECKED_GUNOFFSET_COMP_FLOAT(client_read, io_arg, playerstate->gunoffset, 1);
        if (gunbits & GUNBIT_OFFSET_Z)
            READ_CHECKED_GUNOFFSET_COMP_FLOAT(client_read, io_arg, playerstate->gunoffset, 2);
        if (gunbits & GUNBIT_ANGLES_X)
            READ_CHECKED_GUNANGLES_COMP_FLOAT(client_read, io_arg, playerstate->gunangles, 0);
        if (gunbits & GUNBIT_ANGLES_Y)
            READ_CHECKED_GUNANGLES_COMP_FLOAT(client_read, io_arg, playerstate->gunangles, 1);
        if (gunbits & GUNBIT_ANGLES_Z)
            READ_CHECKED_GUNANGLES_COMP_FLOAT(client_read, io_arg, playerstate->gunangles, 2);
        if (gunbits & GUNBIT_GUNRATE) {
            MAYBE_UNUSED uint8_t gunrate;
            READ_CHECKED(client_read, io_arg, gunrate, u8);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
            playerstate->gunrate = gunrate;
            playerstate->delta_bits |= Q2P_PSD_GUNRATE;
#endif
        }
    }

    if (flags & PS_BLEND) {
        CHECKED(client_read, io_arg, read_var_color(io_arg, &playerstate->blend.values));
        playerstate->blend.delta_bits = BIT(0) | BIT(1) | BIT(2) | BIT(3);
    }

    if (delta_bits_check(flags, PS_FOV, &playerstate->delta_bits, Q2P_PSD_FOV))
        READ_CHECKED(client_read, io_arg, playerstate->fov, u8);

    if (delta_bits_check(flags, PS_RDFLAGS, &playerstate->delta_bits, Q2P_PSD_RDFLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->rdflags, u8);

    uint32_t statbits1, statbits2;
    READ_CHECKED(client_read, io_arg, statbits1, u32);
    for (int i = 0; i < 32; i++)
        if (statbits1 & BIT(i))
            READ_CHECKED(client_read, io_arg, playerstate->stats[i], i16);
    READ_CHECKED(client_read, io_arg, statbits2, u32);
    for (int i = 0; i < 32; i++)
        if (statbits2 & BIT(i))
            READ_CHECKED(client_read, io_arg, playerstate->stats[32 + i], i16);
    playerstate->statbits = statbits1 | ((uint64_t)statbits2) << 32;

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (flags & PS_KEX_DAMAGE_BLEND) {
        CHECKED(client_read, io_arg, read_var_color(io_arg, &playerstate->damage_blend.values));
        playerstate->damage_blend.delta_bits = BIT(0) | BIT(1) | BIT(2) | BIT(3);
    }
#endif

    if (flags & PS_KEX_TEAM_ID) {
        uint8_t team_id; // FIXME unused
        READ_CHECKED(client_read, io_arg, team_id, u8);
        (void)team_id;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                             q2proto_svc_frame_t *frame)
{
    READ_CHECKED(client_read, io_arg, frame->serverframe, i32);
    READ_CHECKED(client_read, io_arg, frame->deltaframe, i32);
    READ_CHECKED(client_read, io_arg, frame->suppress_count, u8);

    // read areabits
    READ_CHECKED(client_read, io_arg, frame->areabits_len, u8);
    READ_CHECKED(client_read, io_arg, frame->areabits, raw, frame->areabits_len, NULL);

    uint8_t cmd;
    // read playerinfo
    READ_CHECKED(client_read, io_arg, cmd, u8);
    if (cmd != svc_playerinfo)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: expected playerinfo, got %d", __func__, cmd);
    SHOWNET(io_arg, 2, -1, "playerinfo");
    CHECKED(client_read, io_arg, kex_client_read_playerstate(context, io_arg, &frame->playerstate));

    // read packet entities
    READ_CHECKED(client_read, io_arg, cmd, u8);
    if (cmd != svc_packetentities)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: expected packetentities, got %d", __func__,
                            cmd);
    context->client_read = kex_client_read_delta_entities;
    SHOWNET(io_arg, 2, -1, "packetentities");

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_client_read_splitclient(q2proto_clientcontext_t *context, uintptr_t io_arg)
{
    uint8_t isplit;
    READ_CHECKED(client_read, io_arg, isplit, u8);
    (void)isplit;

    return Q2P_ERR_SUCCESS;
}

#if Q2PROTO_COMPRESSION_DEFLATE
static q2proto_error_t kex_client_read_continue_spawnbaselineblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                                   q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    if (q2protoio_read_available(context->inflate_io_arg) == 0) {
        // No more configblast data, tear down
        CHECKED_IO(client_read, io_arg, q2protoio_inflate_end(context->inflate_io_arg), "finishing inflate");

        context->client_read = kex_client_read;
        // Call recursively to pick up next message from raw message
        return kex_client_read(context, io_arg, svc_message);
    }

    svc_message->type = Q2P_SVC_SPAWNBASELINE;
    return kex_client_read_baseline(context, context->inflate_io_arg, &svc_message->spawnbaseline);
}
#endif

static q2proto_error_t kex_client_read_begin_spawnbaselineblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                                q2proto_svc_message_t *svc_message)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    uint16_t compressed_len, uncompressed_len;
    READ_CHECKED(client_read, io_arg, compressed_len, u16);
    READ_CHECKED(client_read, io_arg, uncompressed_len, u16);
    (void)uncompressed_len;

    uintptr_t inflate_io_arg;
    CHECKED(client_read, io_arg, q2protoio_inflate_begin(io_arg, Q2P_INFL_DEFL_HEADER, &inflate_io_arg));
    CHECKED(client_read, io_arg, q2protoio_inflate_data(io_arg, inflate_io_arg, compressed_len));
    context->inflate_io_arg = inflate_io_arg;

    context->client_read = kex_client_read_continue_spawnbaselineblast;
    return kex_client_read_continue_spawnbaselineblast(context, io_arg, svc_message);
#else
    return Q2P_ERR_DEFLATE_NOT_SUPPORTED;
#endif
}

#if Q2PROTO_COMPRESSION_DEFLATE
static q2proto_error_t kex_client_read_continue_configblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                            q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    if (q2protoio_read_available(context->inflate_io_arg) == 0) {
        // No more configblast data, tear down
        CHECKED_IO(client_read, io_arg, q2protoio_inflate_end(context->inflate_io_arg), "finishing inflate");

        context->client_read = kex_client_read;
        // Call recursively to pick up next message from raw message
        return kex_client_read(context, io_arg, svc_message);
    }

    svc_message->type = Q2P_SVC_CONFIGSTRING;
    READ_CHECKED(client_read, context->inflate_io_arg, svc_message->configstring.index, u16);
    READ_CHECKED(client_read, context->inflate_io_arg, svc_message->configstring.value, string);

    return Q2P_ERR_SUCCESS;
}
#endif

static q2proto_error_t kex_client_read_begin_configblast(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                         q2proto_svc_message_t *svc_message)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    uint16_t compressed_len, uncompressed_len;
    READ_CHECKED(client_read, io_arg, compressed_len, u16);
    READ_CHECKED(client_read, io_arg, uncompressed_len, u16);
    (void)uncompressed_len;

    uintptr_t inflate_io_arg;
    CHECKED(client_read, io_arg, q2protoio_inflate_begin(io_arg, Q2P_INFL_DEFL_HEADER, &inflate_io_arg));
    CHECKED(client_read, io_arg, q2protoio_inflate_data(io_arg, inflate_io_arg, compressed_len));
    context->inflate_io_arg = inflate_io_arg;

    context->client_read = kex_client_read_continue_configblast;
    return kex_client_read_continue_configblast(context, io_arg, svc_message);
#else
    return Q2P_ERR_DEFLATE_NOT_SUPPORTED;
#endif
}

static q2proto_error_t kex_client_read_locprint(uintptr_t io_arg, q2proto_svc_locprint_t *locprint)
{
    READ_CHECKED(client_read, io_arg, locprint->flags, u8);
    READ_CHECKED(client_read, io_arg, locprint->base, string);
    READ_CHECKED(client_read, io_arg, locprint->num_args, u8);
    if (locprint->num_args > Q2PROTO_MAX_LOCALIZATION_ARGS)
        return Q2P_ERR_BAD_DATA;
    for (int i = 0; i < locprint->num_args; i++)
        READ_CHECKED(client_read, io_arg, locprint->args[i], string);
    return Q2P_ERR_SUCCESS;
}

static uint32_t kex_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins, const q2proto_vec3_t maxs)
{
    return q2proto_pack_solid_32_q2pro_v2(mins, maxs);
}

static void kex_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins, q2proto_vec3_t maxs)
{
    q2proto_unpack_solid_32_q2pro_v2(solid, mins, maxs);
}

//
// SERVER: INITIALIZATION
//

static q2proto_error_t kex_server_fill_serverdata(q2proto_servercontext_t *context,
                                                  q2proto_svc_serverdata_t *serverdata);
static void kex_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                               const q2proto_packed_entity_state_t *from,
                                               const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                               q2proto_entity_state_delta_t *delta);
static void kex_server_make_player_state_delta(q2proto_servercontext_t *context,
                                               const q2proto_packed_player_state_t *from,
                                               const q2proto_packed_player_state_t *to,
                                               q2proto_svc_playerstate_t *delta);
static q2proto_error_t kex_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                        const q2proto_svc_message_t *svc_message);
static q2proto_error_t kex_server_write_sound(q2proto_servercontext_t *context, uintptr_t io_arg,
                                              const q2proto_svc_sound_t *sound);
static q2proto_error_t kex_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                              const q2proto_svc_frame_t *frame);
static q2proto_error_t kex_server_write_frame_entity_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                           const q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static q2proto_error_t kex_server_write_gamestate(q2proto_servercontext_t *context,
                                                  q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                  const q2proto_gamestate_t *gamestate);

q2proto_error_t q2proto_kex_init_servercontext(q2proto_servercontext_t *context, const q2proto_connect_t *connect_info)
{
    context->features.enable_deflate = false;
    context->features.download_compress_raw = false;
    context->features.has_beam_old_origin_fix = true; // I guess?
    context->features.playerstate_clientnum = false;
    context->features.has_playerfog = false;

    context->fill_serverdata = kex_server_fill_serverdata;
    context->make_entity_state_delta = kex_server_make_entity_state_delta;
    context->make_player_state_delta = kex_server_make_player_state_delta;
    context->server_write = kex_server_write;
    context->server_write_gamestate = kex_server_write_gamestate;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_fill_serverdata(q2proto_servercontext_t *context,
                                                  q2proto_svc_serverdata_t *serverdata)
{
    serverdata->protocol = PROTOCOL_KEX;
    return Q2P_ERR_SUCCESS;
}

static void kex_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                               const q2proto_packed_entity_state_t *from,
                                               const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                               q2proto_entity_state_delta_t *delta)
{
    q2proto_packing_make_entity_state_delta(from, to, write_old_origin,
                                            context->server_info->game_api != Q2PROTO_GAME_VANILLA, delta);
}

static void kex_server_make_player_state_delta(q2proto_servercontext_t *context,
                                               const q2proto_packed_player_state_t *from,
                                               const q2proto_packed_player_state_t *to,
                                               q2proto_svc_playerstate_t *delta)
{
    q2proto_packing_make_player_state_delta(from, to, delta);
}

static q2proto_error_t kex_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                   const q2proto_svc_serverdata_t *serverdata);

static q2proto_error_t kex_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                        const q2proto_svc_message_t *svc_message)
{
    switch (svc_message->type) {
    case Q2P_SVC_NOP:
        return q2proto_common_server_write_nop(io_arg);

    case Q2P_SVC_DISCONNECT:
        return q2proto_common_server_write_disconnect(io_arg);

    case Q2P_SVC_RECONNECT:
        return q2proto_common_server_write_reconnect(io_arg);

    case Q2P_SVC_SOUND:
        return kex_server_write_sound(context, io_arg, &svc_message->sound);

    case Q2P_SVC_PRINT:
        return q2proto_common_server_write_print(io_arg, &svc_message->print);

    case Q2P_SVC_STUFFTEXT:
        return q2proto_common_server_write_stufftext(io_arg, &svc_message->stufftext);

    case Q2P_SVC_SERVERDATA:
        return kex_server_write_serverdata(context, io_arg, &svc_message->serverdata);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    case Q2P_SVC_CENTERPRINT:
        return q2proto_common_server_write_centerprint(io_arg, &svc_message->centerprint);

    case Q2P_SVC_FRAME:
        return kex_server_write_frame(context, io_arg, &svc_message->frame);

    case Q2P_SVC_FRAME_ENTITY_DELTA:
        return kex_server_write_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);

    case Q2P_SVC_LAYOUT:
        return q2proto_common_server_write_layout(io_arg, &svc_message->layout);

    case Q2P_SVC_FOG:
        // Although typically written by the game, this is useful when writing demos
        return q2proto_q2repro_server_write_fog(io_arg, &svc_message->fog);

    default:
        break;
    }

    /* The following messages are currently not covered,
     * as they're actually sent by game code:
     *  muzzleflash
     *  muzzleflash2
     *  temp_entity
     *  inventory
     * 'layout' is needed for demo writing, so handle it here as well.
     */

    return Q2P_ERR_NOT_IMPLEMENTED;
}

static q2proto_error_t kex_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                   const q2proto_svc_serverdata_t *serverdata)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_serverdata);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->protocol);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->servercount);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->attractloop);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->gamedir);
    WRITE_CHECKED(server_write, io_arg, i16, serverdata->clientnum);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->levelname);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_entity_state_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                           uint16_t entnum,
                                                           const q2proto_entity_state_delta_t *entity_state_delta,
                                                           bool default_solid_nonzero)
{
    uint64_t bits = 0;

    unsigned origin_changes = q2proto_maybe_diff_coords_write_differs_int(&entity_state_delta->origin);
    if (origin_changes & BIT(0))
        bits |= U_ORIGIN1;
    if (origin_changes & BIT(1))
        bits |= U_ORIGIN2;
    if (origin_changes & BIT(2))
        bits |= U_ORIGIN3;

    if (entity_state_delta->angle.delta_bits & BIT(0))
        bits |= U_ANGLE1;
    if (entity_state_delta->angle.delta_bits & BIT(1))
        bits |= U_ANGLE2;
    if (entity_state_delta->angle.delta_bits & BIT(2))
        bits |= U_ANGLE3;

    if (entity_state_delta->delta_bits & Q2P_ESD_SKINNUM)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->skinnum, U_SKIN8, U_SKIN16, true);

    if (entity_state_delta->delta_bits & Q2P_ESD_FRAME) {
        if (entity_state_delta->frame >= 256)
            bits |= U_FRAME16;
        else
            bits |= U_FRAME8;
    }

    if (entity_state_delta->delta_bits & (Q2P_ESD_EFFECTS | Q2P_ESD_EFFECTS_MORE)) {
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        if (entity_state_delta->effects_more != 0) {
            bits |= U_KEX_EFFECTS64;
            bits |= q2proto_common_choose_width_flags(entity_state_delta->effects_more, U_EFFECTS8, U_EFFECTS16, true);
        } else
#endif
            bits |= q2proto_common_choose_width_flags(entity_state_delta->effects, U_EFFECTS8, U_EFFECTS16, true);
    }

    if (entity_state_delta->delta_bits & Q2P_ESD_RENDERFX)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->renderfx, U_RENDERFX8, U_RENDERFX16, true);

    if (entity_state_delta->delta_bits & Q2P_ESD_SOLID)
        bits |= U_SOLID;

    if (entity_state_delta->delta_bits & Q2P_ESD_EVENT)
        bits |= U_EVENT;

    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX)
        bits |= U_MODEL;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX2)
        bits |= U_MODEL2;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX3)
        bits |= U_MODEL3;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX4)
        bits |= U_MODEL4;
    if (((bits & U_MODEL) && (entity_state_delta->modelindex > 255))
        || ((bits & U_MODEL2) && (entity_state_delta->modelindex2 > 255))
        || ((bits & U_MODEL3) && (entity_state_delta->modelindex3 > 255))
        || ((bits & U_MODEL4) && (entity_state_delta->modelindex4 > 255)))
        bits |= U_MODEL16;

    if (entity_state_delta->delta_bits & Q2P_ESD_SOUND)
        bits |= U_SOUND;

    if (entity_state_delta->delta_bits & Q2P_ESD_OLD_ORIGIN)
        bits |= U_OLDORIGIN;

    if (entity_state_delta->delta_bits & Q2P_ESD_ALPHA)
        bits |= U_ALPHA;

    if (entity_state_delta->delta_bits & Q2P_ESD_SCALE)
        bits |= U_SCALE;

    //----------

    q2proto_common_server_write_entity_bits(io_arg, bits, entnum);

    if (bits & U_MODEL16) {
        if (bits & U_MODEL)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex);
        if (bits & U_MODEL2)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex2);
        if (bits & U_MODEL3)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex3);
        if (bits & U_MODEL4)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex4);
    } else {
        if (bits & U_MODEL)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex);
        if (bits & U_MODEL2)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex2);
        if (bits & U_MODEL3)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex3);
        if (bits & U_MODEL4)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex4);
    }

    if (bits & U_FRAME16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->frame);
    else if (bits & U_FRAME8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->frame);

    if ((bits & U_SKIN32) == U_SKIN32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->skinnum);
    else if (bits & U_SKIN16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->skinnum);
    else if (bits & U_SKIN8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->skinnum);

#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    if (bits & U_KEX_EFFECTS64) {
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->effects);
        if ((bits & U_EFFECTS32) == U_EFFECTS32)
            WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->effects_more);
        else if (bits & U_EFFECTS16)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->effects_more);
        else if (bits & U_EFFECTS8)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->effects_more);
    } else
#endif
    {
        if ((bits & U_EFFECTS32) == U_EFFECTS32)
            WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->effects);
        else if (bits & U_EFFECTS16)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->effects);
        else if (bits & U_EFFECTS8)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->effects);
    }

    if ((bits & U_RENDERFX32) == U_RENDERFX32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->renderfx);
    else if (bits & U_RENDERFX16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->renderfx);
    else if (bits & U_RENDERFX8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->renderfx);

    /* note: for the protocol in the demos (2022), if `solid` is zero, origin reads are lower precision
     * (12.3 fixed point); so track whether solid is != 0 */
    bool nonzero_solid;
    if (bits & U_SOLID) {
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->solid);
        nonzero_solid = entity_state_delta->solid != 0;
        q2proto_set_entity_bit(context->kex_demo_edict_nonzero_solid, entnum, nonzero_solid);
    } else
        nonzero_solid = default_solid_nonzero;

    bool high_precision_origin = context->protocol != Q2P_PROTOCOL_KEX_DEMOS || nonzero_solid;
    if (high_precision_origin) {
        if (bits & U_ORIGIN1)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 0));
        if (bits & U_ORIGIN2)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 1));
        if (bits & U_ORIGIN3)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 2));
        if (bits & U_OLDORIGIN)
            CHECKED_IO(server_write, io_arg, q2protoio_write_var_coords_float(io_arg, &entity_state_delta->old_origin),
                       "write old_origin");
    } else {
        if (bits & U_ORIGIN1)
            WRITE_CHECKED(server_write, io_arg, u16,
                          q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 0));
        if (bits & U_ORIGIN2)
            WRITE_CHECKED(server_write, io_arg, u16,
                          q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 1));
        if (bits & U_ORIGIN3)
            WRITE_CHECKED(server_write, io_arg, u16,
                          q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 2));
        if (bits & U_OLDORIGIN)
            CHECKED_IO(server_write, io_arg, q2protoio_write_var_coords_short(io_arg, &entity_state_delta->old_origin),
                       "write old_origin");
    }

    if (bits & U_ANGLE1)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&entity_state_delta->angle.values, 0));
    if (bits & U_ANGLE2)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&entity_state_delta->angle.values, 1));
    if (bits & U_ANGLE3)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&entity_state_delta->angle.values, 2));

    if (bits & U_SOUND) {
        uint16_t sound_word = entity_state_delta->sound;
        if (entity_state_delta->delta_bits & Q2P_ESD_LOOP_ATTENUATION)
            sound_word |= SOUND_FLAG_ATTENUATION;
        if (entity_state_delta->delta_bits & Q2P_ESD_LOOP_VOLUME)
            sound_word |= SOUND_FLAG_VOLUME;
        WRITE_CHECKED(server_write, io_arg, u16, sound_word);

        uint8_t loop_volume = 0, loop_attenuation = 0;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        loop_volume = entity_state_delta->loop_volume;
        loop_attenuation = entity_state_delta->loop_attenuation;
#endif
        if (sound_word & SOUND_FLAG_VOLUME)
            WRITE_CHECKED(server_write, io_arg, u8, loop_volume);
        if (sound_word & SOUND_FLAG_ATTENUATION)
            WRITE_CHECKED(server_write, io_arg, u8, loop_attenuation);
    }
    if (bits & U_EVENT)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->event);

    uint8_t alpha = 0, scale = 0;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    alpha = entity_state_delta->alpha;
    scale = entity_state_delta->scale;
#endif
    if (bits & U_ALPHA)
        WRITE_CHECKED(server_write, io_arg, u8, alpha);

    if (bits & U_SCALE)
        WRITE_CHECKED(server_write, io_arg, u8, scale);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                      const q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_spawnbaseline);
    CHECKED(server_write, io_arg,
            kex_server_write_entity_state_delta(context, io_arg, spawnbaseline->entnum, &spawnbaseline->delta_state,
                                                false));
    bool nonzero_solid = q2proto_get_entity_bit(context->kex_demo_edict_nonzero_solid, spawnbaseline->entnum);
    q2proto_set_entity_bit(context->kex_demo_baseline_nonzero_solid, spawnbaseline->entnum, nonzero_solid);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_playerstate(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                    const q2proto_svc_playerstate_t *playerstate)
{
    uint32_t flags = 0;

    if (playerstate->delta_bits & Q2P_PSD_PM_TYPE)
        flags |= PS_M_TYPE;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin) != 0)
        flags |= PS_M_ORIGIN;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_velocity) != 0)
        flags |= PS_M_VELOCITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_TIME)
        flags |= PS_M_TIME;
    if (playerstate->delta_bits & Q2P_PSD_PM_FLAGS)
        flags |= PS_M_FLAGS;
    if (playerstate->delta_bits & Q2P_PSD_PM_GRAVITY)
        flags |= PS_M_GRAVITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_DELTA_ANGLES)
        flags |= PS_M_DELTA_ANGLES;
    if (playerstate->delta_bits & Q2P_PSD_VIEWOFFSET)
        flags |= PS_VIEWOFFSET;
    if (playerstate->viewangles.delta_bits != 0)
        flags |= PS_VIEWANGLES;
    if (playerstate->delta_bits & Q2P_PSD_KICKANGLES)
        flags |= PS_KICKANGLES;
    if (playerstate->blend.delta_bits != 0)
        flags |= PS_BLEND;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->damage_blend.delta_bits != 0)
        flags |= PS_KEX_DAMAGE_BLEND;
#endif
    if (playerstate->delta_bits & Q2P_PSD_FOV)
        flags |= PS_FOV;
    if (playerstate->delta_bits & Q2P_PSD_RDFLAGS)
        flags |= PS_RDFLAGS;
    if (playerstate->delta_bits & (Q2P_PSD_GUNINDEX | Q2P_PSD_GUNSKIN))
        flags |= PS_WEAPONINDEX;
    if ((playerstate->delta_bits & (Q2P_PSD_GUNFRAME | Q2P_PSD_GUNRATE)) || (playerstate->gunoffset.delta_bits != 0)
        || (playerstate->gunangles.delta_bits != 0))
        flags |= PS_WEAPONFRAME;
    if (playerstate->delta_bits & Q2P_PSD_CLIENTNUM)
        return Q2P_ERR_BAD_DATA;
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->fog.flags != 0 || playerstate->fog.global.color.delta_bits != 0
        || playerstate->fog.height.start_color.delta_bits != 0 || playerstate->fog.height.end_color.delta_bits != 0)
        return Q2P_ERR_BAD_DATA;
#endif

    if (flags > UINT16_MAX)
        flags |= PS_MOREBITS;

    //
    // write it
    //
    WRITE_CHECKED(server_write, io_arg, u8, svc_playerinfo);
    WRITE_CHECKED(server_write, io_arg, u16, flags);
    if (flags & PS_MOREBITS)
        WRITE_CHECKED(server_write, io_arg, u16, flags >> 16);

    if (flags & PS_M_TYPE)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_type);

    if (flags & PS_M_ORIGIN)
        WRITE_CHECKED(server_write, io_arg, var_coords_float, &playerstate->pm_origin.write.current);

    if (flags & PS_M_VELOCITY)
        WRITE_CHECKED(server_write, io_arg, var_coords_float, &playerstate->pm_velocity.write.current);

    if (flags & PS_M_TIME)
        WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_time);

    if (flags & PS_M_FLAGS)
        WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_flags);

    if (flags & PS_M_GRAVITY)
        WRITE_CHECKED(server_write, io_arg, i16, playerstate->pm_gravity);

    if (flags & PS_M_DELTA_ANGLES) {
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_angles_get_float_comp(&playerstate->pm_delta_angles, 0));
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_angles_get_float_comp(&playerstate->pm_delta_angles, 1));
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_angles_get_float_comp(&playerstate->pm_delta_angles, 2));
    }

    if (flags & PS_VIEWOFFSET) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 2));
        int8_t pm_viewheight = 0;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
        pm_viewheight = playerstate->pm_viewheight;
#endif
        WRITE_CHECKED(server_write, io_arg, i8, pm_viewheight);
    }

    if (flags & PS_VIEWANGLES) {
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&playerstate->viewangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&playerstate->viewangles.values, 1));
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_angles_get_float_comp(&playerstate->viewangles.values, 2));
    }

    if (flags & PS_KICKANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 2));
    }

    if (flags & PS_WEAPONINDEX) {
        uint16_t gun_index_and_skin = playerstate->gunindex;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        gun_index_and_skin |= (playerstate->gunskin << Q2PRO_GUNINDEX_BITS);
#endif
        WRITE_CHECKED(server_write, io_arg, u16, gun_index_and_skin);
    }

    if (flags & PS_WEAPONFRAME) {
        uint16_t gunbits = 0;
        if (playerstate->gunoffset.delta_bits & BIT(0))
            gunbits |= GUNBIT_OFFSET_X;
        if (playerstate->gunoffset.delta_bits & BIT(1))
            gunbits |= GUNBIT_OFFSET_Y;
        if (playerstate->gunoffset.delta_bits & BIT(2))
            gunbits |= GUNBIT_OFFSET_Z;
        if (playerstate->gunangles.delta_bits & BIT(0))
            gunbits |= GUNBIT_ANGLES_X;
        if (playerstate->gunangles.delta_bits & BIT(1))
            gunbits |= GUNBIT_ANGLES_Y;
        if (playerstate->gunangles.delta_bits & BIT(2))
            gunbits |= GUNBIT_ANGLES_Z;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
        if (playerstate->delta_bits & Q2P_PSD_GUNRATE)
            gunbits |= GUNBIT_GUNRATE;
#endif

        uint16_t gunbits_and_frame = gunbits << 9 | playerstate->gunframe;
        WRITE_CHECKED(server_write, io_arg, u16, gunbits_and_frame);

        if (gunbits & GUNBIT_OFFSET_X)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_offsets_get_float_comp(&playerstate->gunoffset.values, 0));
        if (gunbits & GUNBIT_OFFSET_Y)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_offsets_get_float_comp(&playerstate->gunoffset.values, 1));
        if (gunbits & GUNBIT_OFFSET_Z)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_offsets_get_float_comp(&playerstate->gunoffset.values, 2));
        if (gunbits & GUNBIT_ANGLES_X)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_angles_get_float_comp(&playerstate->gunangles.values, 0));
        if (gunbits & GUNBIT_ANGLES_Y)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_angles_get_float_comp(&playerstate->gunangles.values, 1));
        if (gunbits & GUNBIT_ANGLES_Z)
            WRITE_CHECKED(server_write, io_arg, float,
                          q2proto_var_small_angles_get_float_comp(&playerstate->gunangles.values, 2));
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
        if (gunbits & GUNBIT_GUNRATE)
            WRITE_CHECKED(server_write, io_arg, u8, playerstate->gunrate);
#endif
    }

    if (flags & PS_BLEND) {
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 0));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 1));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 2));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 3));
    }

    if (flags & PS_FOV)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->fov);
    if (flags & PS_RDFLAGS)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->rdflags);

    // send stats
    uint32_t statbits1 = playerstate->statbits & 0xffffffff;
    WRITE_CHECKED(server_write, io_arg, u32, statbits1);
    for (int i = 0; i < 32; i++)
        if (statbits1 & BIT(i))
            WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i]);
    uint32_t statbits2 = playerstate->statbits >> 32;
    WRITE_CHECKED(server_write, io_arg, u32, statbits2);
    for (int i = 0; i < 32; i++)
        if (statbits2 & BIT(i))
            WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i + 32]);

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (flags & PS_KEX_DAMAGE_BLEND) {
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->damage_blend.values, 0));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->damage_blend.values, 1));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->damage_blend.values, 2));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->damage_blend.values, 3));
    }
#endif

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_sound(q2proto_servercontext_t *context, uintptr_t io_arg,
                                              const q2proto_svc_sound_t *sound)
{
    uint16_t flags = sound->flags;
    uint32_t entchan = (sound->entity << 3) | (sound->channel & 0x7);
    if (entchan > UINT16_MAX)
        flags |= SND_KEX_LARGE_ENT;

    WRITE_CHECKED(server_write, io_arg, u8, svc_sound);
    WRITE_CHECKED(server_write, io_arg, u8, sound->flags);
    WRITE_CHECKED(server_write, io_arg, u16, sound->index);

    if (sound->flags & SND_VOLUME)
        WRITE_CHECKED(server_write, io_arg, u8, sound->volume);
    if (sound->flags & SND_ATTENUATION)
        WRITE_CHECKED(server_write, io_arg, u8, sound->attenuation);
    if (sound->flags & SND_OFFSET)
        WRITE_CHECKED(server_write, io_arg, u8, sound->timeofs);

    if (sound->flags & SND_ENT) {
        if (flags & SND_KEX_LARGE_ENT)
            WRITE_CHECKED(server_write, io_arg, u32, entchan);
        else
            WRITE_CHECKED(server_write, io_arg, u16, entchan);
    }

    if (sound->flags & SND_POS) {
        if (context->protocol == Q2P_PROTOCOL_KEX_DEMOS) {
            WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&sound->pos, 0));
            WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&sound->pos, 1));
            WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&sound->pos, 2));
        } else {
            WRITE_CHECKED(server_write, io_arg, float, q2proto_var_coords_get_float_comp(&sound->pos, 0));
            WRITE_CHECKED(server_write, io_arg, float, q2proto_var_coords_get_float_comp(&sound->pos, 1));
            WRITE_CHECKED(server_write, io_arg, float, q2proto_var_coords_get_float_comp(&sound->pos, 2));
        }
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                              const q2proto_svc_frame_t *frame)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_frame);
    WRITE_CHECKED(server_write, io_arg, i32, frame->serverframe);
    WRITE_CHECKED(server_write, io_arg, i32, frame->deltaframe);
    WRITE_CHECKED(server_write, io_arg, u8, frame->suppress_count);

    // write areabits
    WRITE_CHECKED(server_write, io_arg, u8, frame->areabits_len);
    void *areabits;
    CHECKED_IO(server_write, io_arg, areabits = q2protoio_write_reserve_raw(io_arg, frame->areabits_len),
               "reserve areabits");
    memcpy(areabits, frame->areabits, frame->areabits_len);

    CHECKED(server_write, io_arg, kex_server_write_playerstate(context, io_arg, &frame->playerstate));

    WRITE_CHECKED(server_write, io_arg, u8, svc_packetentities);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t kex_server_write_frame_entity_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                           const q2proto_svc_frame_entity_delta_t *frame_entity_delta)
{
    if (frame_entity_delta->remove) {
        q2proto_common_server_write_entity_bits(io_arg, U_REMOVE, frame_entity_delta->newnum);
        // non-zero solid tracking: reset to baseline
        q2proto_set_entity_bit(
            context->kex_demo_edict_nonzero_solid, frame_entity_delta->newnum,
            q2proto_get_entity_bit(context->kex_demo_baseline_nonzero_solid, frame_entity_delta->newnum));
        return Q2P_ERR_SUCCESS;
    }

    if (frame_entity_delta->newnum == 0) {
        // special case: packetentities "terminator"
        WRITE_CHECKED(server_write, io_arg, u8, 0); // bits
        WRITE_CHECKED(server_write, io_arg, u8, 0); // entnum
        return Q2P_ERR_SUCCESS;
    }

    return kex_server_write_entity_state_delta(
        context, io_arg, frame_entity_delta->newnum, &frame_entity_delta->entity_delta,
        q2proto_get_entity_bit(context->kex_demo_edict_nonzero_solid, frame_entity_delta->newnum));
}

#define WRITE_GAMESTATE_FUNCTION_NAME kex_server_write_gamestate_mono
#define WRITE_GAMESTATE_BASELINE_SIZE \
    (1    /* command byte */          \
     + 7  /* bits & number */         \
     + 8  /* model indices */         \
     + 2  /* frame */                 \
     + 4  /* skin */                  \
     + 8  /* effects  + morefx */     \
     + 4  /* renderfx */              \
     + 12 /* origin */                \
     + 12 /* angles */                \
     + 12 /* old_origin */            \
     + 2  /* sound */                 \
     + 1  /* loop volume */           \
     + 1  /* loop attenuation */      \
     + 1  /* event */                 \
     + 4  /* solid */                 \
     + 1  /* alpha */                 \
     + 1  /* scale */                 \
    )
#define WRITE_GAMESTATE_BASELINE(C, I, S) kex_server_write_spawnbaseline(C, I, S)

#include "q2proto_write_gamestate.inc"

#undef WRITE_GAMESTATE_FUNCTION_NAME
#undef WRITE_GAMESTATE_BASELINE

#if Q2PROTO_COMPRESSION_DEFLATE
    #define BLAST_PACKET_SIZE     (1 /* command */ + 4 /* sizes */)
    /* Minimal space in packet needed for blast packet
     * (minimal packet size + some slack for compressed data) */
    #define MIN_BLAST_PACKET_SIZE (BLAST_PACKET_SIZE + 16 /* slack */)

static q2proto_error_t kex_blast_begin(q2proto_servercontext_t *context, q2protoio_deflate_args_t *deflate_args,
                                       uintptr_t io_arg, uintptr_t *new_io_arg)
{
    size_t max_deflated = q2protoio_write_available(io_arg);
    if (max_deflated < MIN_BLAST_PACKET_SIZE)
        return Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;
    max_deflated -= BLAST_PACKET_SIZE;
    q2proto_error_t deflate_err = q2protoio_deflate_begin(deflate_args, max_deflated, Q2P_INFL_DEFL_HEADER, new_io_arg);
    return deflate_err;
}

static q2proto_error_t kex_blast_end(uintptr_t io_arg, uintptr_t new_io_arg, uint8_t command)
{
    const void *data;
    size_t uncompressed_len = 0, compressed_len = 0;
    q2proto_error_t err = q2protoio_deflate_get_data(new_io_arg, &uncompressed_len, &data, &compressed_len);
    if (err != Q2P_ERR_SUCCESS)
        goto error;

    WRITE_CHECKED(server_write, io_arg, u8, command);
    WRITE_CHECKED(server_write, io_arg, u16, compressed_len);
    WRITE_CHECKED(server_write, io_arg, u16, uncompressed_len);
    WRITE_CHECKED(server_write, io_arg, raw, data, compressed_len, NULL);

    return q2protoio_deflate_end(new_io_arg);

error:
    q2protoio_deflate_end(new_io_arg);
    return err;
}

static q2proto_error_t kex_server_write_gamestate_blast(q2proto_servercontext_t *context,
                                                        q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                        const q2proto_gamestate_t *gamestate)
{
    uintptr_t deflate_io_arg;
    if (context->gamestate_pos < gamestate->num_configstrings) {
        q2proto_error_t compress_err = kex_blast_begin(context, deflate_args, io_arg, &deflate_io_arg);
        if (compress_err != Q2P_ERR_SUCCESS)
            return compress_err;

        // Write configstrings
        while (context->gamestate_pos < gamestate->num_configstrings) {
            const q2proto_svc_configstring_t *cfgstr = gamestate->configstrings + context->gamestate_pos;
            size_t configstring_size = 2 /* index */ + cfgstr->value.len + 1 /* string */;
            if (q2protoio_write_available(deflate_io_arg) < configstring_size) {
                q2proto_error_t result = kex_blast_end(io_arg, deflate_io_arg, svc_rr_configblast);
                if (result == Q2P_ERR_SUCCESS)
                    result = Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;
                return result;
            }

            WRITE_CHECKED(server_write, deflate_io_arg, u16, cfgstr->index);
            WRITE_CHECKED(server_write, deflate_io_arg, string, &cfgstr->value);
            context->gamestate_pos++;
        }
        q2proto_error_t result = kex_blast_end(io_arg, deflate_io_arg, svc_rr_configblast);
        if (result != Q2P_ERR_SUCCESS)
            return result;
    }

    if (context->gamestate_pos - gamestate->num_configstrings < gamestate->num_spawnbaselines) {
        q2proto_error_t compress_err = kex_blast_begin(context, deflate_args, io_arg, &deflate_io_arg);
        if (compress_err != Q2P_ERR_SUCCESS)
            return compress_err;

        // Write spawn baselines
        size_t baseline_num;
        while ((baseline_num = context->gamestate_pos - gamestate->num_configstrings) < gamestate->num_spawnbaselines) {
            const q2proto_svc_spawnbaseline_t *baseline = gamestate->spawnbaselines + baseline_num;
            if (q2protoio_write_available(deflate_io_arg) < WRITE_GAMESTATE_BASELINE_SIZE) {
                q2proto_error_t result = kex_blast_end(io_arg, deflate_io_arg, svc_rr_spawnbaselineblast);
                if (result == Q2P_ERR_SUCCESS)
                    result = Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;
                return result;
            }

            CHECKED(server_write, deflate_io_arg,
                    kex_server_write_entity_state_delta(context, deflate_io_arg, baseline->entnum,
                                                        &baseline->delta_state, false));
            context->gamestate_pos++;
        }
        q2proto_error_t result = kex_blast_end(io_arg, deflate_io_arg, svc_rr_spawnbaselineblast);
        if (result != Q2P_ERR_SUCCESS)
            return result;
    }

    // Game state written successfully, reset state
    context->gamestate_pos = 0;

    return Q2P_ERR_SUCCESS;
}
#endif

static q2proto_error_t kex_server_write_gamestate(q2proto_servercontext_t *context,
                                                  q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                  const q2proto_gamestate_t *gamestate)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    if (deflate_args)
        return kex_server_write_gamestate_blast(context, deflate_args, io_arg, gamestate);
#endif
    return kex_server_write_gamestate_mono(context, deflate_args, io_arg, gamestate);
}

#undef WRITE_GAMESTATE_BASELINE_SIZE
