/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// sv_game.c -- interface to the game dll

#include "server.h"
#include "game3_proxy/game3_proxy.h"
#include "common/loc.h"
#include "common/gamedll.h"

#if USE_CLIENT
#include "client/video.h"
#endif

#include "server/nav.h"

const game_export_t     *ge;
const game_q2pro_restart_filesystem_t *g_restart_fs;
const game_q2pro_customize_entity_t   *g_customize_entity;

static void PF_configstring(int index, const char *val);

/*
================
PF_FindIndex

================
*/
static int PF_FindIndex(const char *name, int start, int max, int skip, const char *func)
{
    char *string;
    int i;

    if (!name || !name[0])
        return 0;

    for (i = 1; i < max; i++) {
        if (i == skip) {
            continue;
        }
        string = sv.configstrings[start + i];
        if (!string[0]) {
            break;
        }
        if (!strcmp(string, name)) {
            return i;
        }
    }

    if (i == max) {
        if (g_features->integer & GMF_ALLOW_INDEX_OVERFLOW) {
            Com_DPrintf("%s(%s): overflow\n", func, name);
            return 0;
        }
        Com_Error(ERR_DROP, "%s(%s): overflow", func, name);
    }

    PF_configstring(i + start, name);

    return i;
}

static int PF_ModelIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.models, svs.csr.max_models, MODELINDEX_PLAYER, __func__);
}

static int PF_SoundIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.sounds, svs.csr.max_sounds, 0, __func__);
}

static int PF_ImageIndex(const char *name)
{
    return PF_FindIndex(name, svs.csr.images, svs.csr.max_images, 0, __func__);
}

/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client.
Archived in MVD stream.
===============
*/
static void PF_Unicast(edict_t *ent, bool reliable, uint32_t dupe_key)
{
    client_t    *client;
    int         cmd, flags, clientNum;

    if (!ent) {
        goto clear;
    }

    if (msg_write.overflowed)
        Com_Error(ERR_DROP, "%s: message buffer overflowed", __func__);

    clientNum = NUM_FOR_EDICT(ent) - 1;
    if (clientNum < 0 || clientNum >= svs.maxclients) {
        Com_DWPrintf("%s to a non-client %d\n", __func__, clientNum);
        goto clear;
    }

    client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_DWPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        goto clear;
    }

    if (!msg_write.cursize) {
        Com_DPrintf("%s with empty data\n", __func__);
        goto clear;
    }

    cmd = msg_write.data[0];

    flags = 0;
    if (reliable) {
        flags |= MSG_RELIABLE;
    }

    if (cmd == svc_layout || (cmd == svc_configstring && RL16(&msg_write.data[1]) == CS_STATUSBAR)) {
        flags |= MSG_COMPRESS_AUTO;
    }

    SV_ClientAddMessage(client, flags);

    // fix anti-kicking exploit for broken mods
    if (cmd == svc_disconnect) {
        client->drop_hack = true;
        goto clear;
    }

    SV_MvdUnicast(ent, clientNum, reliable);

clear:
    SZ_Clear(&msg_write);
}

/*
=================
PF_bprintf

Sends text to all active clients.
Archived in MVD stream.
=================
*/
void PF_Broadcast_Print(int level, const char *msg)
{
    char        string[MAX_STRING_CHARS];
    client_t    *client;
    size_t      len;
    int         i;

    len = Q_strlcpy(string, msg, sizeof(string));
    if (len >= sizeof(string)) {
        Com_DWPrintf("%s: overflow\n", __func__);
        return;
    }

    SV_MvdBroadcastPrint(level, string);

    q2proto_svc_message_t message{};
    message.type = Q2P_SVC_PRINT;
    message.print.level = level;
    message.print.string.str = string;
    message.print.string.len = len;
    q2proto_server_multicast_write(Q2P_PROTOCOL_MULTICAST_FLOAT, Q2PROTO_IOARG_SERVER_WRITE_MULTICAST, &message);

    // echo to console
    if (COM_DEDICATED) {
        // mask off high bits
        for (i = 0; i < len; i++)
            string[i] &= 127;
        Com_Printf("%s", string);
    }

    FOR_EACH_CLIENT(client) {
        if (client->state != cs_spawned)
            continue;
        if (level >= client->messagelevel) {
            SV_ClientAddMessage(client, MSG_RELIABLE);
        }
    }

    SZ_Clear(&msg_write);
}

/*
===============
PF_dprintf

Debug print to server console.
===============
*/
static void PF_Com_Print(const char *msg)
{
    Con_SkipNotify(true);
    Com_Printf("%s", msg);
    Con_SkipNotify(false);
}

/*
===============
PF_cprintf

Print to a single client if the level passes.
Archived in MVD stream.
===============
*/
static void PF_Client_Print(edict_t *ent, int level, const char *msg)
{
    int         clientNum;
    client_t    *client;

    if (!ent) {
        Com_LPrintf(level == PRINT_CHAT ? PRINT_TALK : PRINT_ALL, "%s", msg);
        return;
    }

    clientNum = NUM_FOR_EDICT(ent) - 1;
    if (clientNum < 0 || clientNum >= svs.maxclients) {
        Com_DWPrintf("%s to a non-client %d\n", __func__, clientNum);
        return;
    }

    client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_DWPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        return;
    }

    q2proto_svc_message_t message{};
    message.type = Q2P_SVC_PRINT;
    message.print.level = level;
    message.print.string = q2proto_make_string(msg);
    q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);

    if (level >= client->messagelevel) {
        SV_ClientAddMessage(client, MSG_RELIABLE);
    }

    SV_MvdUnicast(ent, clientNum, true);

    SZ_Clear(&msg_write);
}

/*
===============
PF_centerprintf

Centerprint to a single client.
Archived in MVD stream.
===============
*/
static void PF_Center_Print(edict_t *ent, const char *msg)
{
    int         n;

    if (!ent) {
        return;
    }

    n = NUM_FOR_EDICT(ent);
    if (n < 1 || n > svs.maxclients) {
        Com_DWPrintf("%s to a non-client %d\n", __func__, n - 1);
        return;
    }

    client_t* client = svs.client_pool + n - 1;
    if (client->state <= cs_zombie) {
        Com_DWPrintf("%s to a free/zombie client %d\n", __func__, n - 1);
        return;
    }

    q2proto_svc_message_t message{};
    message.type = Q2P_SVC_CENTERPRINT;
    message.centerprint.message = q2proto_make_string(msg);
    q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);

    PF_Unicast(ent, true, 0);
}

/*
===============
PF_error

Abort the server with a game error
===============
*/
static q_noreturn void PF_error(const char *msg)
{
    Com_Error(ERR_DROP, "Game Error: %s", msg);
}

static trace_t PF_Clip(edict_t *entity, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, contents_t contentmask)
{
    return SV_Clip(start, mins, maxs, end, entity, contentmask);
}

/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
static void PF_setmodel(edict_t *ent, const char *name)
{
    if (!ent || !name)
        Com_Error(ERR_DROP, "PF_setmodel: NULL");

    ent->s.modelindex = PF_ModelIndex(name);

// if it is an inline model, get the size information for it
    if (name[0] == '*') {
        const mmodel_t *mod = CM_InlineModel(&sv.cm, name);
        VectorCopy(mod->mins, ent->mins);
        VectorCopy(mod->maxs, ent->maxs);
        PF_LinkEdict(ent);
    }
}

/*
===============
PF_configstring

If game is actively running, broadcasts configstring change.
Archived in MVD stream.
===============
*/
static void PF_configstring(int index, const char *val)
{
    size_t len, maxlen;
    client_t *client;
    char *dst;

    if (index < 0 || index >= svs.csr.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    if (sv.state == ss_dead) {
        Com_DWPrintf("%s: not yet initialized\n", __func__);
        return;
    }

    if (!val)
        val = "";

    // error out entirely if it exceedes array bounds
    len = strlen(val);
    maxlen = (svs.csr.end - index) * CS_MAX_STRING_LENGTH;
    if (len >= maxlen) {
        Com_Error(ERR_DROP,
                  "%s: index %d overflowed: %zu > %zu",
                  __func__, index, len, maxlen - 1);
    }

    // print a warning and truncate everything else
    maxlen = Com_ConfigstringSize(&svs.csr, index);
    if (len >= maxlen) {
        Com_DWPrintf(
            "%s: index %d overflowed: %zu > %zu\n",
            __func__, index, len, maxlen - 1);
        len = maxlen - 1;
    }

    dst = sv.configstrings[index];
    if (!strncmp(dst, val, maxlen)) {
        return;
    }

    // change the string in sv
    memcpy(dst, val, len);
    dst[len] = 0;

    if (sv.state == ss_loading) {
        return;
    }

    SV_MvdConfigstring(index, val, len);

    // send the update to everyone
    q2proto_svc_message_t message{};
    message.type = Q2P_SVC_CONFIGSTRING;
    message.configstring.index = index;
    message.configstring.value.str = val;
    message.configstring.value.len = len;
    q2proto_server_multicast_write(Q2P_PROTOCOL_MULTICAST_FLOAT, Q2PROTO_IOARG_SERVER_WRITE_MULTICAST, &message);

    FOR_EACH_CLIENT(client) {
        if (client->state < cs_primed) {
            continue;
        }
        SV_ClientAddMessage(client, MSG_RELIABLE);
    }

    SZ_Clear(&msg_write);
}

static const char *PF_GetConfigstring(int index)
{
    if (index < 0 || index >= svs.csr.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    return sv.configstrings[index];
}

static void PF_WriteFloat(float f)
{
    MSG_WriteFloat(f);
}

typedef enum {
    VIS_PVS     = 0,
    VIS_PHS     = 1,
    VIS_NOAREAS = 2     // can be OR'ed with one of above
} vis_t;

static void PF_WritePos(const vec3_t pos)
{
    q2proto_server_write_pos(Q2P_PROTOCOL_MULTICAST_FLOAT, Q2PROTO_IOARG_SERVER_WRITE_MULTICAST, pos);
}

static qboolean PF_inVIS(const vec3_t p1, const vec3_t p2, vis_t vis)
{
    const mleaf_t *leaf1, *leaf2;
    visrow_t mask;

    leaf1 = CM_PointLeaf(&sv.cm, p1);
    BSP_ClusterVis(sv.cm.cache, &mask, leaf1->cluster, vis & VIS_PHS);

    leaf2 = CM_PointLeaf(&sv.cm, p2);
    if (leaf2->cluster == -1)
        return qfalse;
    if (!Q_IsBitSet(mask.b, leaf2->cluster))
        return qfalse;
    if (vis & VIS_NOAREAS)
        return qtrue;
    if (!CM_AreasConnected(&sv.cm, leaf1->area, leaf2->area))
        return qfalse;      // a door blocks it
    return qtrue;
}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
static qboolean PF_inPVS(const vec3_t p1, const vec3_t p2, bool portals)
{
    const vis_t vis = static_cast<vis_t>(VIS_PVS | (portals ? 0 : VIS_NOAREAS));
    return PF_inVIS(p1, p2, vis);
}

/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
static qboolean PF_inPHS(const vec3_t p1, const vec3_t p2, bool portals)
{
    const vis_t vis = static_cast<vis_t>(VIS_PHS | (portals ? 0 : VIS_NOAREAS));
    return PF_inVIS(p1, p2, vis);
}

/*
==================
SV_StartSound

Each entity can have eight independent sound sources, like voice,
weapon, feet, etc.

If channel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/
static void SV_StartSound(const vec3_t origin, edict_t *edict,
                          soundchan_t channel, int soundindex, float volume,
                          float attenuation, float timeofs)
{
    vec3_t      origin_v;
    client_t    *client;
    visrow_t    mask;
    const mleaf_t       *leaf1, *leaf2;
    q2proto_sound_t snd{};
    message_packet_t    *msg;
    bool        force_pos;

    if (!edict)
        Com_Error(ERR_DROP, "%s: edict = NULL", __func__);
    if (volume < 0 || volume > 1)
        Com_Error(ERR_DROP, "%s: volume = %f", __func__, volume);
    if (attenuation < 0 || attenuation > 4)
        Com_Error(ERR_DROP, "%s: attenuation = %f", __func__, attenuation);
    if (timeofs < 0 || timeofs > 0.255f)
        Com_Error(ERR_DROP, "%s: timeofs = %f", __func__, timeofs);
    if (soundindex < 0 || soundindex >= svs.csr.max_sounds)
        Com_Error(ERR_DROP, "%s: soundindex = %d", __func__, soundindex);

    // send origin for invisible entities
    // the origin can also be explicitly set
    force_pos = (edict->svflags & SVF_NOCLIENT) || origin;

    // use the entity origin unless it is a bmodel or explicitly specified
    if (!origin) {
        if (edict->solid == SOLID_BSP) {
            VectorAvg(edict->mins, edict->maxs, origin_v);
            VectorAdd(origin_v, edict->s.origin, origin_v);
            origin = origin_v;
        } else {
            origin = edict->s.origin;
        }
    }

    snd.index = soundindex;
    // always send the entity number for channel overrides
    snd.has_entity_channel = true;
    snd.entity = NUM_FOR_EDICT(edict);
    snd.channel = channel;
    snd.has_position = true;
    VectorCopy(origin, snd.pos);
    snd.volume = volume;
    snd.attenuation = attenuation;
    snd.timeofs = timeofs;

    // prepare multicast message
    q2proto_svc_message_t sound_msg{};
    sound_msg.type = Q2P_SVC_SOUND;
    sound_msg.sound = {};
    q2proto_sound_encode_message(&snd, &sound_msg.sound);

    q2proto_server_multicast_write(Q2P_PROTOCOL_MULTICAST_FLOAT, Q2PROTO_IOARG_SERVER_WRITE_MULTICAST, &sound_msg);

    // if the sound doesn't attenuate, send it to everyone
    // (global radio chatter, voiceovers, etc)
    if (attenuation == ATTN_NONE)
        channel |= CHAN_NO_PHS_ADD;

    // multicast if force sending origin
    if (force_pos) {
        multicast_t to = MULTICAST_PHS;
        if (channel & CHAN_NO_PHS_ADD)
            to = MULTICAST_ALL;
        SV_Multicast(origin, to, channel & CHAN_RELIABLE);
        return;
    }

    leaf1 = NULL;
    if (!(channel & CHAN_NO_PHS_ADD)) {
        leaf1 = CM_PointLeaf(&sv.cm, origin);
        BSP_ClusterVis(sv.cm.cache, &mask, leaf1->cluster, DVIS_PHS);
    }

    // decide per client if origin needs to be sent
    FOR_EACH_CLIENT(client) {
        // do not send sounds to connecting clients
        if (!CLIENT_ACTIVE(client)) {
            continue;
        }

        // PHS cull this sound
        if (!(channel & CHAN_NO_PHS_ADD)) {
            leaf2 = CM_PointLeaf(&sv.cm, client->edict->s.origin);
            if (!CM_AreasConnected(&sv.cm, leaf1->area, leaf2->area))
                continue;
            if (leaf2->cluster == -1)
                continue;
            if (!Q_IsBitSet(mask.b, leaf2->cluster))
                continue;
        }

        // reliable sounds will always have position explicitly set,
        // as no one guarantees reliables to be delivered in time
        if (channel & CHAN_RELIABLE) {
            SV_ClientAddMessage(client, MSG_RELIABLE);
            continue;
        }

        if (LIST_EMPTY(&client->msg_free_list)) {
            Com_DWPrintf("%s to %s: out of message slots\n",
                         __func__, client->name);
            continue;
        }

        msg = LIST_FIRST(message_packet_t, &client->msg_free_list, entry);

        msg->cursize = SOUND_PACKET;
        msg->sound = sound_msg.sound;
        msg->sound.flags &= ~SND_POS; // SND_POS, will be set, if necessary, by emit_snd()

        List_Remove(&msg->entry);
        List_Append(&client->msg_unreliable_list, &msg->entry);
        client->msg_unreliable_bytes += msg_write.cursize;
    }

    // clear multicast buffer
    SZ_Clear(&msg_write);

    SV_MvdStartSound(snd.entity, channel, sound_msg.sound.flags, soundindex, sound_msg.sound.volume, sound_msg.sound.attenuation, sound_msg.sound.timeofs);
}

static void PF_StartSound(edict_t *entity, soundchan_t channel,
                          int soundindex, float volume,
                          float attenuation, float timeofs)
{
    if (!entity)
        return;
    SV_StartSound(NULL, entity, channel, soundindex, volume, attenuation, timeofs);
}

// TODO: support origin; add range checks?
static void PF_LocalSound(edict_t *target, const vec3_t origin,
                          edict_t *entity, soundchan_t channel,
                          int soundindex, float volume,
                          float attenuation, float timeofs,
                          uint32_t dupe_key)
{
    int entnum = NUM_FOR_EDICT(target);

    q2proto_sound_t snd{};
    snd.index = soundindex;
    // always send the entity number for channel overrides
    snd.has_entity_channel = true;
    snd.entity = entnum;
    snd.channel = channel;
    snd.volume = volume;
    snd.attenuation = attenuation;
    snd.timeofs = timeofs;

    q2proto_svc_message_t message{};
    message.type = Q2P_SVC_SOUND;
    message.sound = {};
    q2proto_sound_encode_message(&snd, &message.sound);

    int clientNum = entnum - 1;
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
        return;
    }

    client_t *client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_WPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        return;
    }

    q2proto_server_write(&client->q2proto_ctx, Q2PROTO_IOARG_SERVER_WRITE_MULTICAST, &message);

    PF_Unicast(target, !!(channel & CHAN_RELIABLE), dupe_key);
}

void PF_Pmove(void *pm)
{
    const pmoveParams_t *pmp = sv_client ? &sv_client->pmp : &svs.pmp;

    Pmove(static_cast<pmove_t *>(pm), pmp);
}

static void PF_WriteEntity(const edict_t *entity)
{
    MSG_WriteShort(NUM_FOR_EDICT(entity));
}

static cvar_t *PF_cvar(const char *name, const char *value, int flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("Game attempted to set extended flags on '%s', masked out.\n", name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(name, value, flags | CVAR_GAME);
}

static const char* PF_Argv(int idx)
{
    return Cmd_Argv(idx);
}

static const char* PF_RawArgs(void)
{
    return Cmd_RawArgs();
}

static void PF_AddCommandString(const char *string)
{
#if USE_CLIENT
    if (!strcmp(string, "menu_loadgame\n"))
        string = "pushmenu loadgame\n";
#endif
    Cbuf_AddText(&cmd_buffer, string);
}

static void PF_SetAreaPortalState(int portalnum, bool open)
{
    CM_SetAreaPortalState(&sv.cm, portalnum, open);
}

static qboolean PF_AreasConnected(int area1, int area2)
{
    return CM_AreasConnected(&sv.cm, area1, area2) ? qtrue : qfalse;
}

static void *PF_TagMalloc(size_t size, int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    const memtag_t memtag = static_cast<memtag_t>(tag + TAG_MAX);
    return Z_TagMallocz(size, memtag);
}

static void PF_FreeTags(int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    const memtag_t memtag = static_cast<memtag_t>(tag + TAG_MAX);
    Z_FreeTags(memtag);
}

static int PF_LoadFile(const char *path, void **buffer, unsigned flags, unsigned tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    const memtag_t memtag = static_cast<memtag_t>(tag + TAG_MAX);
    return FS_LoadFileEx(path, buffer, flags, memtag);
}

static void *PF_GetExtension(const char *name);

static void PF_Bot_RegisterEdict(const edict_t * edict)
{
    Nav_RegisterEdict(edict);
}

static void PF_Bot_UnRegisterEdict(const edict_t * edict)
{
    Nav_UnRegisterEdict(edict);
}

static GoalReturnCode PF_Bot_MoveToPoint(const edict_t * bot, const vec3_t point, const float moveTolerance)
{
    return GoalReturnCode_Error;
}

static GoalReturnCode PF_Bot_FollowActor(const edict_t * bot, const edict_t * actor)
{
    return GoalReturnCode_Error;
}

static bool PF_GetPathToGoal(const PathRequest* request, PathInfo* info)
{
    nav_path_t path = { 0 };
    path.request = request;

    PathInfo result = Nav_Path(&path);

    if (info)
        *info = result;

    return result.returnCode < PathReturnCode_StartPathErrors;
}

#define SAY_CMD_RESULT      "invalid game command \"say\"\n"
#define SAY_CMD_HASH        558
#define SAY_TEAM_CMD_RESULT "invalid game command \"say_team\"\n"
#define SAY_TEAM_CMD_HASH   909

static void PF_Loc_Print(edict_t* ent, int level, const char* base, const char** args, size_t num_args)
{
    /* FIXME - actually support localization & perform formatting.
     * Also, the rerelease game docs call this "The new primary entry point for printing." and
     * "This function replaces all of the others (except Com_Print).",
     * suggesting all other print functions should be wrappers of this one.
     * Other things that need fixing:
       - PRINT_TTS is just PRINT_HIGH with text to speech
       - BROADCAST can support any type, but it doesn't support NO_NOTIFY
       - NO_NOTIFY is just sent as a bit which is used to not print it on notify.
         the code is there in the client now, just needs hooked up here.
       - the client is supposed to translate `##P<n>` to player names. This
         isn't required to be done on the client side here, since we aren't
         supporting censoring. ##P0 translates to player 0's configstring name,
         etc etc. It only occurs for BROADCAST prints.
     */

    char string[MAX_STRING_CHARS];
    Loc_Localize(base, true, args, num_args, string, sizeof(string));
    
    // HACK: check for missing "say/say_team"
    if (ent && svs.scan_for_say_cmd) {
        size_t h = Com_HashString(string, MAX_STRING_CHARS);

        if ((h == SAY_CMD_HASH && !Q_strcasecmp(string, SAY_CMD_RESULT)) ||
            (h == SAY_TEAM_CMD_HASH && !Q_strcasecmp(string, SAY_TEAM_CMD_RESULT))) {
            svs.server_supplied_say = true;
            Com_DPrintf("Server-supplied `say`/`say_team` enabled\n");
            return;
        }
    }

    if (level & PRINT_BROADCAST) {
        int broadcast_level = level & ~(PRINT_BROADCAST | PRINT_NO_NOTIFY);
        // restrict to print levels support by by svc_print
        if (broadcast_level > PRINT_CHAT && broadcast_level != PRINT_TTS)
            broadcast_level = PRINT_CHAT;
        PF_Broadcast_Print(broadcast_level, string);
    } else {
        level &= ~PRINT_NO_NOTIFY; // TODO implement
        PF_Client_Print(ent, level, string);
    }
}

#if USE_REF
#include "refresh/refresh.h"

static void PF_Draw_Line(const vec3_t start, const vec3_t end, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugLine(start, end, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Point(const vec3_t point, const float size, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugPoint(point, size, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Circle(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugCircle(origin, radius, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Bounds(const vec3_t mins, const vec3_t maxs, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugBounds(mins, maxs, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Sphere(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugSphere(origin, radius, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_OrientedWorldText(const vec3_t origin, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugText(origin, NULL, text, size, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_StaticWorldText(const vec3_t origin, const vec3_t angles, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugText(origin, angles, text, size, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Cylinder(const vec3_t origin, const float halfHeight, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugCylinder(origin, halfHeight, radius, *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Ray(const vec3_t origin, const vec3_t direction, const float length, const float size, const rgba_t* color, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    vec3_t end;
    VectorMA(origin, length, direction, end);
    R_AddDebugArrow(origin, end, size, *((color_t *) color), *((color_t *) color), lifeTime * 1000, depth);
}
static void PF_Draw_Arrow(const vec3_t start, const vec3_t end, const float size, const rgba_t* lineColor, const rgba_t* arrowColor, const float lifeTime, const bool depthTest)
{
    const qboolean depth = depthTest ? qtrue : qfalse;
    R_AddDebugArrow(start, end, size, *((color_t *) lineColor), *((color_t *) arrowColor), lifeTime * 1000, depth);
}
#else
static void PF_Draw_Line(const vec3_t start, const vec3_t end, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Point(const vec3_t point, const float size, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Circle(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Bounds(const vec3_t mins, const vec3_t maxs, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Sphere(const vec3_t origin, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_OrientedWorldText(const vec3_t origin, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest) {}
static void PF_Draw_StaticWorldText(const vec3_t origin, const vec3_t angles, const char * text, const rgba_t* color, const float size, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Cylinder(const vec3_t origin, const float halfHeight, const float radius, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Ray(const vec3_t origin, const vec3_t direction, const float length, const float size, const rgba_t* color, const float lifeTime, const bool depthTest) {}
static void PF_Draw_Arrow(const vec3_t start, const vec3_t end, const float size, const rgba_t* lineColor, const rgba_t* arrowColor, const float lifeTime, const bool depthTest) {}
#endif

static void PF_ReportMatchDetails_Multicast(bool is_end)
{
    /* "This function is solely for platforms that need match result data." -
     * somewhat unclear...
     * Anyhow, rerelease game writes some message data prior to calling this,
     * at least discard that data ... */
    SZ_Clear(&msg_write);
}

static uint32_t PF_ServerFrame(void)
{
    return sv.framenum;
}

static void PF_SendToClipboard(const char* text)
{
#if USE_CLIENT
    if(vid->set_clipboard_data)
        vid->set_clipboard_data(text);
#endif
}

static size_t PF_Info_ValueForKey (const char *s, const char *key, char *buffer, size_t buffer_len)
{
    char *infostr = Info_ValueForKey(s, key);
    return Q_strlcpy(buffer, infostr, buffer_len);
}

//==============================================

static const game_import_t game_import = {
    .multicast = SV_Multicast,
    .unicast = PF_Unicast,
    .Broadcast_Print = PF_Broadcast_Print,
    .Com_Print = PF_Com_Print,
    .Client_Print = PF_Client_Print,
    .Center_Print = PF_Center_Print,
    .Com_Error = PF_error,

    .linkentity = PF_LinkEdict,
    .unlinkentity = PF_UnlinkEdict,
    .BoxEdicts = SV_AreaEdicts,
    .trace = SV_Trace,
    .clip = PF_Clip,
    .pointcontents = SV_PointContents,
    .setmodel = PF_setmodel,
    .inPVS = PF_inPVS,
    .inPHS = PF_inPHS,

    .modelindex = PF_ModelIndex,
    .soundindex = PF_SoundIndex,
    .imageindex = PF_ImageIndex,

    .configstring = PF_configstring,
    .get_configstring = PF_GetConfigstring,
    .sound = PF_StartSound,
    .positioned_sound = SV_StartSound,
    .local_sound = PF_LocalSound,

    .WriteChar = MSG_WriteChar,
    .WriteByte = MSG_WriteByte,
    .WriteShort = MSG_WriteShort,
    .WriteLong = MSG_WriteLong,
    .WriteFloat = PF_WriteFloat,
    .WriteString = MSG_WriteString,
    .WritePosition = PF_WritePos,
    .WriteDir = MSG_WriteDir,
    .WriteAngle = MSG_WriteAngle,
    .WriteEntity = PF_WriteEntity,

    .TagMalloc = PF_TagMalloc,
    .TagFree = Z_Free,
    .FreeTags = PF_FreeTags,

    .cvar = PF_cvar,
    .cvar_set = Cvar_UserSet,
    .cvar_forceset = Cvar_Set,

    .argc = Cmd_Argc,
    .argv = PF_Argv,
    .args = PF_RawArgs,
    .AddCommandString = PF_AddCommandString,

    .DebugGraph = SCR_DebugGraph,
    .SetAreaPortalState = PF_SetAreaPortalState,
    .AreasConnected = PF_AreasConnected,
    .GetExtension = PF_GetExtension,

    .Bot_RegisterEdict = PF_Bot_RegisterEdict,
    .Bot_UnRegisterEdict = PF_Bot_UnRegisterEdict,
    .Bot_MoveToPoint = PF_Bot_MoveToPoint,
    .Bot_FollowActor = PF_Bot_FollowActor,
    .GetPathToGoal = PF_GetPathToGoal,

    .Loc_Print = PF_Loc_Print,
    .Draw_Line = PF_Draw_Line,
    .Draw_Point = PF_Draw_Point,
    .Draw_Circle = PF_Draw_Circle,
    .Draw_Bounds = PF_Draw_Bounds,
    .Draw_Sphere = PF_Draw_Sphere,
    .Draw_OrientedWorldText = PF_Draw_OrientedWorldText,
    .Draw_StaticWorldText = PF_Draw_StaticWorldText,
    .Draw_Cylinder = PF_Draw_Cylinder,
    .Draw_Ray = PF_Draw_Ray,
    .Draw_Arrow = PF_Draw_Arrow,
    .ReportMatchDetails_Multicast = PF_ReportMatchDetails_Multicast,
    .ServerFrame = PF_ServerFrame,
    .SendToClipBoard = PF_SendToClipboard,

    .Info_ValueForKey = PF_Info_ValueForKey,
    .Info_RemoveKey = Info_RemoveKey,
    .Info_SetValueForKey = Info_SetValueForKey,
};

static const filesystem_api_v1_t filesystem_api_v1 = {
    .OpenFile = FS_OpenFile,
    .CloseFile = FS_CloseFile,
    .LoadFile = PF_LoadFile,

    .ReadFile = FS_Read,
    .WriteFile = FS_Write,
    .FlushFile = FS_Flush,
    .TellFile = FS_Tell,
    .SeekFile = FS_Seek,
    .ReadLine = FS_ReadLine,

    .ListFiles = FS_ListFiles,
    .FreeFileList = FS_FreeList,

    .ErrorString = Q_ErrorString,
};

#if USE_REF && USE_DEBUG
static const debug_draw_api_v1_t debug_draw_api_v1 = {
    .ClearDebugLines = R_ClearDebugLines,
    .AddDebugLine = R_AddDebugLine,
    .AddDebugPoint = R_AddDebugPoint,
    .AddDebugAxis = R_AddDebugAxis,
    .AddDebugBounds = R_AddDebugBounds,
    .AddDebugSphere = R_AddDebugSphere,
    .AddDebugCircle = R_AddDebugCircle,
    .AddDebugCylinder = R_AddDebugCylinder,
    .AddDebugArrow = R_AddDebugArrow,
    .AddDebugCurveArrow = R_AddDebugCurveArrow,
    .AddDebugText = R_AddDebugText,
};
#endif

static void *PF_GetExtension(const char *name)
{
    if (!name)
        return NULL;

    if (!strcmp(name, FILESYSTEM_API_V1))
        return (void *)&filesystem_api_v1;

#if USE_REF && USE_DEBUG
    if (!strcmp(name, DEBUG_DRAW_API_V1) && !dedicated->integer)
        return (void *)&debug_draw_api_v1;
#endif

    return NULL;
}

static void *game_library;

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs(void)
{
    g_restart_fs = NULL;
    if (ge) {
        ge->Shutdown();
        ge = NULL;
    }
    if (game_library) {
        Sys_FreeLibrary(game_library);
        game_library = NULL;
    }
    Cvar_Set("g_features", "0");

    Z_LeakTest(TAG_FREE);
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitGameProgs(void)
{
    game_import_t   import;
    game_entry_t    entry = NULL;

    // unload anything we have now
    SV_ShutdownGameProgs();

    game_library = GameDll_Load();
    if (game_library)
        entry = reinterpret_cast<game_entry_t>(Sys_GetProcAddress(game_library, "GetGameAPI"));
    if (!entry)
        Com_Error(ERR_DROP, "Failed to load game library");

    // load a new game dll
    import = game_import;
    import.tick_rate = SV_FRAMERATE;
    import.frame_time_s = SV_FRAMETIME * 0.001f;
    import.frame_time_ms = SV_FRAMETIME;

    ge = entry(&import);
    if (!ge) {
        Com_Error(ERR_DROP, "Game library returned NULL exports");
    }

    Com_DPrintf("Game API version: %d\n", ge->apiversion);

    // get extended api if present
    void* entry_ex = Sys_GetProcAddress(game_library, "GetGameAPIEx");

    if (ge->apiversion != GAME_API_VERSION) {
        if (ge->apiversion == GAME3_API_VERSION_OLD || ge->apiversion == GAME3_API_VERSION_NEW) {
            Com_DPrintf("... using proxy game\n");
            if (ge->apiversion == GAME3_API_VERSION_NEW)
                svs.game_api = Q2PROTO_GAME_Q2PRO_EXTENDED_V2;
            else
                svs.game_api = Q2PROTO_GAME_VANILLA; // may also be Q2PROTO_GAME_Q2PRO_EXTENDED, we'll know after checking g_features
            ge = GetGame3Proxy(&import, entry, entry_ex);
        } else {
            Com_Error(ERR_DROP, "...but expected %d\n", GAME_API_VERSION);
        }
    } else {
        svs.game_api = Q2PROTO_GAME_RERELEASE;
        Cvar_SetInteger(g_features, GMF_PROTOCOL_EXTENSIONS | GMF_ENHANCED_SAVEGAMES | GMF_PROPERINUSE | GMF_WANT_ALL_DISCONNECTS, FROM_CODE);
    }

    // initialize
    /* Note: Those functions may already call configstring(). They also decide the features...
     * So start with an extended csr, and possible choose a smaller one later. */
    if (svs.game_api == Q2PROTO_GAME_RERELEASE)
        svs.csr = cs_remap_rerelease;
    else
        svs.csr = cs_remap_q2pro_new;
    ge->PreInit(); // FIXME: When to call PreInit(), when Init()?
    ge->Init();

    if (ge->GetExtension) {
        g_restart_fs = (game_q2pro_restart_filesystem_t *)ge->GetExtension(game_q2pro_restart_filesystem_ext);
        g_customize_entity = (game_q2pro_customize_entity_t *)ge->GetExtension(game_q2pro_customize_entity_ext);
    } else {
        g_restart_fs = NULL;
        g_customize_entity = NULL;
    }

    if (svs.game_api == Q2PROTO_GAME_VANILLA) {
        if ((g_features->integer & GMF_PROTOCOL_EXTENSIONS) != 0)
            svs.game_api = Q2PROTO_GAME_Q2PRO_EXTENDED;
        else
            svs.csr = cs_remap_old;
    }

    // sanitize maxclients
    if (sv_maxclients->integer != svs.maxclients || sv_maxclients->value != svs.maxclients) {
        Com_Error(ERR_DROP, "Game library corrupted maxclients value");
    }

    // sanitize edict_size
    unsigned min_size = sizeof(edict_t);
    unsigned max_size = INT_MAX / svs.csr.max_edicts;

    if (ge->edict_size < min_size || ge->edict_size > max_size || ge->edict_size % q_alignof(edict_t)) {
        Com_Error(ERR_DROP, "Game library returned bad size of edict_t");
    }

    // sanitize max_edicts
    if (ge->max_edicts <= svs.maxclients || ge->max_edicts > svs.csr.max_edicts) {
        Com_Error(ERR_DROP, "Game library returned bad number of max_edicts");
    }

    svs.server_info.game_api = svs.game_api;
    svs.server_info.default_packet_length = MAX_PACKETLEN_WRITABLE_DEFAULT;
}
