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

#include "server.h"

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

// some protocol optimizations are disabled when recording a demo
#define Q2PRO_OPTIMIZE(c) \
    (!(c)->settings[CLS_RECORDING])

/*
=============
SV_TruncPacketEntities

Truncates remainder of entity_packed_t list, patching current frame to make
delta compression happy.
=============
*/
static bool SV_TruncPacketEntities(client_t *client, const client_frame_t *from,
                                   client_frame_t *to, int oldindex, int newindex)
{
    server_entity_packed_t *newent;
    const server_entity_packed_t *oldent;
    int i, oldnum, newnum, entities_mask, from_num_entities, to_num_entities;
    bool ret = true;

    if (!sv_trunc_packet_entities->integer || client->netchan.type == NETCHAN_NEW)
        return false;

    SV_DPrintf(1, "Truncating frame %d at %u bytes for %s\n",
               client->framenum, msg_write.cursize, client->name);

    if (!from)
        from_num_entities = 0;
    else
        from_num_entities = from->num_entities;
    to_num_entities = to->num_entities;

    entities_mask = client->num_entities - 1;
    oldent = newent = NULL;
    while (newindex < to->num_entities || oldindex < from_num_entities) {
        if (newindex >= to->num_entities) {
            newnum = MAX_EDICTS;
        } else {
            i = (to->first_entity + newindex) & entities_mask;
            newent = &client->entities[i];
            newnum = newent->number;
        }

        if (oldindex >= from_num_entities) {
            oldnum = MAX_EDICTS;
        } else {
            i = (from->first_entity + oldindex) & entities_mask;
            oldent = &client->entities[i];
            oldnum = oldent->number;
        }

        if (newnum == oldnum) {
            // skip delta update
            *newent = *oldent;
            oldindex++;
            newindex++;
            continue;
        }

        if (newnum < oldnum) {
            // remove new entity from frame
            to->num_entities--;
            for (i = newindex; i < to->num_entities; i++) {
                client->entities[(to->first_entity + i    ) & entities_mask] =
                client->entities[(to->first_entity + i + 1) & entities_mask];
            }
            continue;
        }

        if (newnum > oldnum) {
            // drop the frame if entity list got too big.
            // should not normally happen.
            if (to->num_entities >= MAX_PACKET_ENTITIES) {
                ret = false;
                break;
            }

            // insert old entity into frame
            for (i = to->num_entities - 1; i >= newindex; i--) {
                client->entities[(to->first_entity + i + 1) & entities_mask] =
                client->entities[(to->first_entity + i    ) & entities_mask];
            }

            client->entities[(to->first_entity + newindex) & entities_mask] = *oldent;
            to->num_entities++;

            // should never go backwards
            to_num_entities = max(to_num_entities, to->num_entities);

            oldindex++;
            newindex++;
            continue;
        }
    }

    client->next_entity = to->first_entity + to_num_entities;
    return ret;
}

static client_frame_t *get_last_frame(client_t *client)
{
    client_frame_t *frame;

    if (client->lastframe <= 0) {
        // client is asking for a retransmit
        client->frames_nodelta++;
        return NULL;
    }

    client->frames_nodelta = 0;

    if (client->framenum - client->lastframe >= UPDATE_BACKUP) {
        // client hasn't gotten a good message through in a long time
        Com_DPrintf("%s: delta request from out-of-date packet.\n", client->name);
        return NULL;
    }

    // we have a valid message to delta from
    frame = &client->frames[client->lastframe & UPDATE_MASK];
    if (frame->number != client->lastframe) {
        // but it got never sent
        Com_DPrintf("%s: delta request from dropped frame.\n", client->name);
        return NULL;
    }

    if (client->next_entity - frame->first_entity > client->num_entities) {
        // but entities are too old
        Com_DPrintf("%s: delta request from out-of-date entities.\n", client->name);
        return NULL;
    }

    return frame;
}

static void make_playerstate_delta(client_t *client, const q2proto_packed_player_state_t *from, q2proto_packed_player_state_t *to, q2proto_svc_playerstate_t *playerstate, msgPsFlags_t flags)
{
    q2proto_server_make_player_state_delta(&client->q2proto_ctx, from, to, playerstate);

    if (flags & MSG_PS_IGNORE_PREDICTION) {
        memcpy(&playerstate->pm_velocity.write.current, &playerstate->pm_velocity.write.prev, sizeof(playerstate->pm_velocity.write.current));
        playerstate->delta_bits &= ~(Q2P_PSD_PM_TIME | Q2P_PSD_PM_FLAGS | Q2P_PSD_PM_GRAVITY);

        /* MSG_PS_IGNORE_xxx don't just indicate something should be omitted from the delta, but also
         * that the "new" state should retain the "from" value.
         * (Presumably that, once a MSG_PS_IGNORE_xxx isn't given, the correct value is emitted.) */
        if(from)
        {
            memcpy(&to->pm_velocity, &from->pm_velocity, sizeof(to->pm_velocity));
            to->pm_time = from->pm_time;
            to->pm_flags = from->pm_flags;
            to->pm_gravity = from->pm_gravity;
        }
        else
        {
            memset(&to->pm_velocity, 0, sizeof(to->pm_velocity));
            to->pm_time = 0;
            to->pm_flags = 0;
            to->pm_gravity = 0;
        }
    }

    if (flags & MSG_PS_IGNORE_DELTAANGLES) {
        playerstate->delta_bits &= ~Q2P_PSD_PM_DELTA_ANGLES;

        if(from)
            memcpy(&to->pm_delta_angles, &from->pm_delta_angles, sizeof(to->pm_delta_angles));
        else
            memset(&to->pm_delta_angles, 0, sizeof(to->pm_delta_angles));
    }

    if (flags & MSG_PS_IGNORE_VIEWANGLES)
    {
        playerstate->viewangles.delta_bits = 0;

        if(from)
            memcpy(&to->viewangles, &from->viewangles, sizeof(to->viewangles));
        else
            memset(&to->viewangles, 0, sizeof(to->viewangles));
    }

    if (flags & MSG_PS_IGNORE_BLEND) {
        playerstate->blend.delta_bits = 0;
        playerstate->damage_blend.delta_bits = 0;

        if(from)
        {
            memcpy(&to->blend, &from->blend, sizeof(to->blend));
            memcpy(&to->damage_blend, &from->damage_blend, sizeof(to->damage_blend));
        }
        else
        {
            memset(&to->blend, 0, sizeof(to->blend));
            memset(&to->damage_blend, 0, sizeof(to->damage_blend));
        }
    }

    if (flags & MSG_PS_IGNORE_GUNFRAMES) {
        playerstate->delta_bits &= ~Q2P_PSD_GUNFRAME;
        playerstate->gunangles.delta_bits = 0;
        playerstate->gunoffset.delta_bits = 0;
    }

    if (flags & MSG_PS_IGNORE_GUNINDEX) {
        playerstate->delta_bits &= ~Q2P_PSD_GUNINDEX;

        if(from)
        {
            to->gunindex = from->gunindex;
            to->gunskin = from->gunskin;
        }
        else
        {
            to->gunindex = 0;
            to->gunskin = 0;
        }
    }
}

static void write_entity_delta(client_t *client, const server_entity_packed_t *from, const server_entity_packed_t *to, msgEsFlags_t flags)
{
    q2proto_svc_message_t message = {.type = Q2P_SVC_FRAME_ENTITY_DELTA, .frame_entity_delta = {0}};

    if (!to) {
        Q_assert(from);
        Q_assert(from->number > 0 && from->number < MAX_EDICTS);

        message.frame_entity_delta.remove = true;
        message.frame_entity_delta.newnum = from->number;
        q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);

        return; // remove entity
    }

    Q_assert(to->number > 0 && to->number < MAX_EDICTS);
    message.frame_entity_delta.newnum = to->number;

    if (client->q2proto_ctx.features.has_beam_old_origin_fix)
        flags |= MSG_ES_BEAMORIGIN;
    bool entity_differs = Q2PROTO_MakeEntityDelta(&client->q2proto_ctx, &message.frame_entity_delta.entity_delta, from ? &from->e : NULL, &to->e, flags);
    if (!(flags & MSG_ES_FORCE) && !entity_differs)
        return;
    q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);
}

static bool emit_packet_entities(client_t               *client,
                                 const client_frame_t   *from,
                                 client_frame_t         *to,
                                 int                    clientEntityNum,
                                 unsigned               maxsize)
{
    server_entity_packed_t *newent;
    const server_entity_packed_t *oldent;
    int i, oldnum, newnum, oldindex, newindex, from_num_entities;
    bool ret = true;

    if (msg_write.cursize + 2 > maxsize)
        return false;

    if (!from)
        from_num_entities = 0;
    else
        from_num_entities = from->num_entities;

    newindex = 0;
    oldindex = 0;
    oldent = newent = NULL;
    while (newindex < to->num_entities || oldindex < from_num_entities) {
        if (msg_write.cursize + MAX_PACKETENTITY_BYTES > maxsize) {
            ret = SV_TruncPacketEntities(client, from, to, oldindex, newindex);
            break;
        }

        if (newindex >= to->num_entities) {
            newnum = MAX_EDICTS;
        } else {
            i = (to->first_entity + newindex) & (client->num_entities - 1);
            newent = &client->entities[i];
            newnum = newent->number;
        }

        if (oldindex >= from_num_entities) {
            oldnum = MAX_EDICTS;
        } else {
            i = (from->first_entity + oldindex) & (client->num_entities - 1);
            oldent = &client->entities[i];
            oldnum = oldent->number;
        }

        if (newnum == oldnum) {
            // Delta update from old position. Because the force param is false,
            // this will not result in any bytes being emitted if the entity has
            // not changed at all. Note that players are always 'newentities',
            // this updates their old_origin always and prevents warping in case
            // of packet loss.
            int flags = 0;
            if (newnum <= client->maxclients) {
                flags |= MSG_ES_NEWENTITY;
            }
            if (newnum == clientEntityNum) {
                flags |= MSG_ES_FIRSTPERSON;
                VectorCopy(oldent->e.origin, newent->e.origin);
                VectorCopy(oldent->e.angles, newent->e.angles);
            }
            write_entity_delta(client, oldent, newent, flags);
            oldindex++;
            newindex++;
            continue;
        }

        if (newnum < oldnum) {
            // this is a new entity, send it from the baseline
            oldent = client->baselines[newnum >> SV_BASELINES_SHIFT];
            if (oldent) {
                oldent += (newnum & SV_BASELINES_MASK);
            }
            write_entity_delta(client, oldent, newent, MSG_ES_NEWENTITY | MSG_ES_FORCE);
            newindex++;
            continue;
        }

        if (newnum > oldnum) {
            // the old entity isn't present in the new message
            write_entity_delta(client, oldent, NULL, 0);
            oldindex++;
            continue;
        }
    }

    // end of packetentities
    q2proto_svc_message_t message = {.type = Q2P_SVC_FRAME_ENTITY_DELTA, .frame_entity_delta = {0}};
    q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);
    return ret;
}


/*
==================
SV_WriteFrameToClient_Enhanced
==================
*/
bool SV_WriteFrameToClient_Enhanced(client_t *client, unsigned maxsize)
{
    client_frame_t  *frame, *oldframe;
    q2proto_packed_player_state_t *oldstate;
    msgPsFlags_t    psFlags;
    int             clientEntityNum;

    // this is the frame we are creating
    frame = &client->frames[client->framenum & UPDATE_MASK];

    // this is the frame we are delta'ing from
    oldframe = get_last_frame(client);
    int lastframe;
    if (oldframe) {
        oldstate = &oldframe->ps;
        lastframe = client->lastframe;
    } else {
        oldstate = NULL;
        lastframe = -1;
    }

    q2proto_svc_message_t message = {.type = Q2P_SVC_FRAME, .frame = {0}};
    message.frame.serverframe = client->framenum;
    message.frame.deltaframe = lastframe;
    message.frame.suppress_count = client->suppress_count;
    message.frame.q2pro_frame_flags = client->frameflags;

    message.frame.areabits_len = frame->areabytes;
    message.frame.areabits = frame->areabits;

    // ignore some parts of playerstate if not recording demo
    psFlags = client->psFlags;
    if (!client->settings[CLS_RECORDING]) {
        if (client->settings[CLS_NOGUN]) {
            psFlags |= MSG_PS_IGNORE_GUNFRAMES;
            if (client->settings[CLS_NOGUN] != 2) {
                psFlags |= MSG_PS_IGNORE_GUNINDEX;
            }
        }
        if (client->settings[CLS_NOBLEND]) {
            psFlags |= MSG_PS_IGNORE_BLEND;
        }
        if (frame->ps.pm_type < PM_DEAD) {
            if (!(frame->ps.pm_flags & PMF_NO_PREDICTION)) {
                psFlags |= MSG_PS_IGNORE_VIEWANGLES;
            }
        } else {
            // lying dead on a rotating platform?
            psFlags |= MSG_PS_IGNORE_DELTAANGLES;
        }
    }

    clientEntityNum = 0;
    if (frame->ps.pm_type < PM_DEAD && !client->settings[CLS_RECORDING]) {
        clientEntityNum = frame->clientNum + 1;
    }
    if (client->settings[CLS_NOPREDICT]) {
        psFlags |= MSG_PS_IGNORE_PREDICTION;
    }
    psFlags |= MSG_PS_EXTENSIONS;
    psFlags |= MSG_PS_RERELEASE;

    // delta encode the playerstate
    make_playerstate_delta(client, oldstate, &frame->ps, &message.frame.playerstate, psFlags);

    if ((oldframe ? oldframe->clientNum : 0) != frame->clientNum) {
        message.frame.playerstate.clientnum = frame->clientNum;
        message.frame.playerstate.delta_bits |= Q2P_PSD_CLIENTNUM;
    }

    client->suppress_count = 0;
    client->frameflags = 0;

    q2proto_server_write(&client->q2proto_ctx, (uintptr_t)&client->io_data, &message);

    // delta encode the entities
    return emit_packet_entities(client, oldframe, frame, clientEntityNum, maxsize);
}

/*
=============================================================================

Build a client frame structure

=============================================================================
*/

#if USE_FPS
static void
fix_old_origin(const client_t *client, entity_packed_t *state, const edict_t *ent, int e)
{
    server_entity_t *sent = &sv.entities[e];
    int i, j, k;

    if (ent->s.renderfx & RF_BEAM)
        return;

    if (!ent->linkcount)
        return; // not linked in anywhere

    if (sent->create_framenum >= sv.framenum) {
        // created this frame. unfortunate for projectiles: they will move only
        // with 1/client->framediv fraction of their normal speed on the client
        return;
    }

    if (state->event == EV_PLAYER_TELEPORT && !Q2PRO_OPTIMIZE(client)) {
        // other clients will lerp from old_origin on EV_PLAYER_TELEPORT...
        VectorCopy(state->origin, state->old_origin);
        return;
    }

    if (sent->create_framenum > sv.framenum - client->framediv) {
        // created between client frames
        VectorCopy(sent->create_origin, state->old_origin);
        return;
    }

    // find the oldest valid origin
    for (i = 0; i < client->framediv - 1; i++) {
        j = sv.framenum - (client->framediv - i);
        k = j & ENT_HISTORY_MASK;
        if (sent->history[k].framenum == j) {
            VectorCopy(sent->history[k].origin, state->old_origin);
            return;
        }
    }

    // no valid old_origin, just use what game provided
}
#endif

static bool SV_EntityVisible(const client_t *client, const server_entity_t *svent, const visrow_t *mask)
{
    if (svent->num_clusters == -1)
        // too many leafs for individual check, go by headnode
        return CM_HeadnodeVisible(CM_NodeNum(client->cm, svent->headnode), mask->b);

    // check individual leafs
    for (int i = 0; i < svent->num_clusters; i++)
        if (Q_IsBitSet(mask, svent->clusternums[i]))
            return true;

    return false;
}

static bool SV_EntityAttenuatedAway(const vec3_t org, const edict_t *ent)
{
    float mult = Com_GetEntityLoopDistMult(ent->s.loop_attenuation);
    float dist = Distance(org, ent->s.origin) - SOUND_FULLVOLUME;

    return dist * mult > 1.0f;
}

#define IS_MONSTER(ent) \
    ((ent->svflags & (SVF_MONSTER | SVF_DEADMONSTER)) == SVF_MONSTER || (ent->s.renderfx & RF_FRAMELERP))

#define IS_HI_PRIO(ent) \
    (ent->s.number <= sv_client->maxclients || IS_MONSTER(ent) || ent->solid == SOLID_BSP)

#define IS_GIB(ent) \
    (sv_client->csr->extended ? (ent->s.renderfx & RF_LOW_PRIORITY) : (ent->s.effects & (EF_GIB | EF_GREENGIB)))

#define IS_LO_PRIO(ent) \
    (IS_GIB(ent) || (!ent->s.modelindex && !ent->s.effects))

static vec3_t clientorg;

static int entpriocmp(const void *p1, const void *p2)
{
    const edict_t *a = *(const edict_t **)p1;
    const edict_t *b = *(const edict_t **)p2;

    bool hi_a = IS_HI_PRIO(a);
    bool hi_b = IS_HI_PRIO(b);
    if (hi_a != hi_b)
        return hi_b - hi_a;

    bool lo_a = IS_LO_PRIO(a);
    bool lo_b = IS_LO_PRIO(b);
    if (lo_a != lo_b)
        return lo_a - lo_b;

    float dist_a = DistanceSquared(a->s.origin, clientorg);
    float dist_b = DistanceSquared(b->s.origin, clientorg);
    if (dist_a > dist_b)
        return 1;
    return -1;
}

static int entnumcmp(const void *p1, const void *p2)
{
    const edict_t *a = *(const edict_t **)p1;
    const edict_t *b = *(const edict_t **)p2;
    return a->s.number - b->s.number;
}

/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame(client_t *client)
{
    int         i, e;
    vec3_t      org;
    edict_t     *ent;
    server_entity_t *svent;
    edict_t     *clent;
    client_frame_t  *frame;
    server_entity_packed_t *state;
    const mleaf_t   *leaf;
    int         clientarea, clientcluster;
    visrow_t    clientphs;
    visrow_t    clientpvs;
    int         max_packet_entities;
    edict_t     *edicts[MAX_EDICTS];
    int         num_edicts;
    qboolean (*visible)(edict_t *, edict_t *) = NULL;
    qboolean (*customize)(edict_t *, edict_t *, customize_entity_t *) = NULL;
    customize_entity_t temp;

    clent = client->edict;
    if (!clent->client)
        return;        // not in game yet

    Q_assert(client->entities);

    // this is the frame we are creating
    frame = &client->frames[client->framenum & UPDATE_MASK];
    frame->number = client->framenum;
    frame->sentTime = com_eventTime; // save it for ping calc later
    frame->latency = -1; // not yet acked

    client->frames_sent++;

    // find the client's PVS
    SV_GetClient_ViewOrg(client, org);
    // Rerelease game doesn't include viewheight in viewoffset, vanilla does
    if (svs.game_api == Q2PROTO_GAME_RERELEASE)
        org[2] += clent->client->ps.pmove.viewheight;

    leaf = CM_PointLeaf(client->cm, org);
    clientarea = leaf->area;
    clientcluster = leaf->cluster;

    // calculate the visible areas
    frame->areabytes = CM_WriteAreaBits(client->cm, frame->areabits, clientarea);
    if (!frame->areabytes) {
        frame->areabits[0] = 255;
        frame->areabytes = 1;
    }

    // grab the current player_state_t
    PackPlayerstate(&client->q2proto_ctx, (const player_state_t*)clent->client, &frame->ps);

    // grab the current clientNum
    if (g_features->integer & GMF_CLIENTNUM) {
        frame->clientNum = SV_GetClient_ClientNum(client);
        if (!VALIDATE_CLIENTNUM(client->csr, frame->clientNum)) {
            Com_DWPrintf("%s: bad clientNum %d for client %d\n",
                         __func__, frame->clientNum, client->number);
            frame->clientNum = client->number;
        }
    } else {
        frame->clientNum = client->number;
    }

    // limit maximum number of entities in client frame
    max_packet_entities =
        sv_max_packet_entities->integer > 0 ? sv_max_packet_entities->integer :
        MAX_PACKET_ENTITIES;

    if (g_customize_entity) {
        visible = g_customize_entity->EntityVisibleToClient;
        customize = g_customize_entity->CustomizeEntityToClient;
    }

    CM_FatPVS(client->cm, &clientpvs, org);
    BSP_ClusterVis(client->cm->cache, &clientphs, clientcluster, DVIS_PHS);

    bool client_is_spectator = clent->client->ps.pmove.pm_type == PM_SPECTATOR
        || clent->client->ps.pmove.pm_type == PM_FREEZE;
    bool client_is_dead = clent->client->ps.pmove.pm_type == PM_DEAD
        || clent->client->ps.pmove.pm_type == PM_GIB;

    // build up the list of visible entities
    frame->num_entities = 0;
    frame->first_entity = client->next_entity;

    num_edicts = 0;
    for (e = 1; e < client->ge->num_edicts; e++) {
        ent = EDICT_NUM2(client->ge, e);
        svent = &sv.entities[e];

        // ignore entities not in use
        if (!ent->inuse && (g_features->integer & GMF_PROPERINUSE))
            continue;

        // ignore ents without visible models
        if (ent->svflags & SVF_NOCLIENT)
            continue;

        // ignore ents without visible models unless they have an effect
        if (!HAS_EFFECTS(ent))
            continue;

        // ignore gibs if client says so
        if (client->settings[CLS_NOGIBS]) {
            if (ent->s.effects & EF_GIB && !(client->csr->extended && ent->s.effects & EF_ROCKET))
                continue;
            if (ent->s.effects & EF_GREENGIB)
                continue;
        }

        bool force_visible = false;
        if (ent != clent && ent->client && clent->client) {
            uint8_t my_team = clent->client->ps.team_id;
            if (my_team && ent->client->ps.team_id == my_team)
                force_visible = true;
            if (client_is_spectator || client_is_dead)
                force_visible = true;
        }

        // ignore flares if client says so
        if (client->csr->extended && ent->s.renderfx & RF_FLARE && client->settings[CLS_NOFLARES])
            continue;

        // ignore if not touching a PV leaf
        if (ent != clent && !sv_novis->integer && !(ent->svflags & SVF_NOCULL) && !force_visible) {
            // check area
            if (!CM_AreasConnected(client->cm, clientarea, ent->areanum)) {
                // doors can legally straddle two areas, so
                // we may need to check another one
                if (!CM_AreasConnected(client->cm, clientarea, ent->areanum2)) {
                    continue;        // blocked by a door
                }
            }

            // beams just check one point for PHS
            bool beam_cull = ent->s.renderfx & RF_BEAM;
            // remaster uses different sound culling rules
            bool sound_cull = ent->s.sound;

            if (!SV_EntityVisible(client, svent, (beam_cull || sound_cull || (ent->s.renderfx & RF_CASTSHADOW)) ? &clientphs : &clientpvs))
                continue;

            // don't send sounds if they will be attenuated away
            if (sound_cull) {
                if (SV_EntityAttenuatedAway(org, ent)) {
                    if (!ent->s.modelindex)
                        continue;
                    if (!beam_cull && !SV_EntityVisible(client, svent, &clientpvs))
                        continue;
                }
            } else if (!ent->s.modelindex && !(ent->s.renderfx & RF_CASTSHADOW)) {
                // Paril TODO: is this a good idea? seems weird to remove
                // visual effects based on distance if there's no model and
                // no sound...
                if (DistanceSquared(org, ent->s.origin) > 400 * 400)
                    continue;
            }
        }

        SV_CheckEntityNumber(ent, e);

        // optionally skip it
        if (visible && !visible(clent, ent))
            continue;

        edicts[num_edicts++] = ent;

        if (num_edicts == max_packet_entities && !sv_prioritize_entities->integer)
            break;
    }

    // prioritize entities on overflow
    if (num_edicts > max_packet_entities) {
        VectorCopy(org, clientorg);
        sv_client = client;
        sv_player = client->edict;
        qsort(edicts, num_edicts, sizeof(edicts[0]), entpriocmp);
        sv_client = NULL;
        sv_player = NULL;
        num_edicts = max_packet_entities;
        qsort(edicts, num_edicts, sizeof(edicts[0]), entnumcmp);
    }

    for (i = 0; i < num_edicts; i++) {
        ent = edicts[i];
        e = ent->s.number;

        // add it to the circular client_entities array
        state = &client->entities[client->next_entity & (client->num_entities - 1)];

        // optionally customize it
        if (customize && customize(clent, ent, &temp)) {
            Q_assert(temp.s.number == e);
            PackEntity(&client->q2proto_ctx, &temp.s, &state->e);
        } else {
            PackEntity(&client->q2proto_ctx, &ent->s, &state->e);
        }
        state->number = e;

#if USE_FPS
        // fix old entity origins for clients not running at
        // full server frame rate
        if (client->framediv != 1)
            fix_old_origin(client, state, ent, e);
#endif

        // clear footsteps
        if (client->settings[CLS_NOFOOTSTEPS] && (state->e.event == EV_FOOTSTEP
            || (state->e.event == EV_OTHER_FOOTSTEP || state->e.event == EV_LADDER_STEP))) {
            state->e.event = 0;
        }

        // hide POV entity from renderer, unless this is player's own entity
        if (e == frame->clientNum + 1 && ent != clent &&
            (!Q2PRO_OPTIMIZE(client))) {
            state->e.modelindex = 0;
        }

        if ((!USE_MVD_CLIENT || sv.state != ss_broadcast) && (ent->owner == clent)) {
            // don't mark players missiles as solid
            state->e.solid = 0;
        } else if (client->esFlags & MSG_ES_LONGSOLID && !client->csr->extended) {
            state->e.solid = sv.entities[e].solid32;
        }

        frame->num_entities++;
        client->next_entity++;
    }
}
