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
// cl_ents.c -- entity parsing and management

#include "client.h"

extern qhandle_t cl_mod_powerscreen;
extern qhandle_t cl_mod_laser;
extern qhandle_t cl_mod_dmspot;
extern qhandle_t cl_img_flare;

static cvar_t *cl_brightskins_custom;
static cvar_t *cl_brightskins_enemy_color;
static cvar_t *cl_brightskins_team_color;
static cvar_t *cl_brightskins_dead;
static color_t brightskin_enemy_color = COLOR_RED;
static color_t brightskin_team_color = COLOR_GREEN;

#define RESERVED_ENTITY_GUN 1
#define RESERVED_ENTITY_TESTMODEL 2
#define RESERVED_ENTITY_COUNT 3

static bool CL_ParseBrightskinColor(const char *s, color_t *color)
{
    if (SCR_ParseColor(s, color))
        return true;

    if (s[0] != '#') {
        size_t len = strlen(s);
        if (len == 6 || len == 8) {
            char buffer[10];

            buffer[0] = '#';
            Q_strlcpy(buffer + 1, s, sizeof(buffer) - 1);
            return SCR_ParseColor(buffer, color);
        }
    }

    return false;
}

static void cl_brightskins_enemy_color_changed(cvar_t *self)
{
    if (!CL_ParseBrightskinColor(self->string, &brightskin_enemy_color)) {
        Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
        Cvar_Reset(self);
        brightskin_enemy_color = COLOR_RED;
    }
}

static void cl_brightskins_team_color_changed(cvar_t *self)
{
    if (!CL_ParseBrightskinColor(self->string, &brightskin_team_color)) {
        Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
        Cvar_Reset(self);
        brightskin_team_color = COLOR_GREEN;
    }
}

void CL_InitBrightskins(void)
{
    cl_brightskins_custom = Cvar_Get("cl_brightskins_custom", "0", CVAR_ARCHIVE);
    cl_brightskins_enemy_color = Cvar_Get("cl_brightskins_enemy_color", "#ff0000", CVAR_ARCHIVE);
    cl_brightskins_enemy_color->changed = cl_brightskins_enemy_color_changed;
    cl_brightskins_enemy_color->generator = Com_Color_g;
    cl_brightskins_enemy_color_changed(cl_brightskins_enemy_color);
    cl_brightskins_team_color = Cvar_Get("cl_brightskins_team_color", "#00ff00", CVAR_ARCHIVE);
    cl_brightskins_team_color->changed = cl_brightskins_team_color_changed;
    cl_brightskins_team_color->generator = Com_Color_g;
    cl_brightskins_team_color_changed(cl_brightskins_team_color);
    cl_brightskins_dead = Cvar_Get("cl_brightskins_dead", "1", CVAR_ARCHIVE);
}

typedef struct {
    clientinfo_t info;
    bool active;
} forced_model_t;

static forced_model_t cl_forced_enemy_model;
static forced_model_t cl_forced_team_model;
static int32_t cl_forced_enemy_modcount = -1;
static int32_t cl_forced_team_modcount = -1;

static bool CL_IsSpectatorView(void)
{
    pmtype_t pm_type = cl.frame.ps.pmove.pm_type;

    return pm_type == PM_SPECTATOR || pm_type == PM_FREEZE;
}

void CL_MigratePlayerCvars(void)
{
    float old_enemy_outline = 0.0f;
    float old_self_outline = 0.0f;
    float old_enemy_rim = 0.0f;
    float old_self_rim = 0.0f;

    if (cl_enemy_outline)
        old_enemy_outline = Cvar_ClampValue(cl_enemy_outline, 0.0f, 1.0f);
    if (cl_enemy_outline_self)
        old_self_outline = Cvar_ClampValue(cl_enemy_outline_self, 0.0f, 1.0f);
    if (cl_enemy_rimlight)
        old_enemy_rim = Cvar_ClampValue(cl_enemy_rimlight, 0.0f, 1.0f);
    if (cl_enemy_rimlight_self)
        old_self_rim = Cvar_ClampValue(cl_enemy_rimlight_self, 0.0f, 1.0f);

    if (cl_player_outline_enemy && cl_player_outline_enemy->modified_count == 0 && old_enemy_outline > 0.0f)
        Cvar_SetValue(cl_player_outline_enemy, old_enemy_outline, FROM_CODE);

    if (cl_player_outline_team && cl_player_outline_team->modified_count == 0) {
        float team_outline = old_enemy_outline > old_self_outline ? old_enemy_outline : old_self_outline;
        if (team_outline > 0.0f)
            Cvar_SetValue(cl_player_outline_team, team_outline, FROM_CODE);
    }

    if (cl_player_rimlight_enemy && cl_player_rimlight_enemy->modified_count == 0 && old_enemy_rim > 0.0f)
        Cvar_SetValue(cl_player_rimlight_enemy, old_enemy_rim, FROM_CODE);

    if (cl_player_rimlight_team && cl_player_rimlight_team->modified_count == 0) {
        float team_rim = old_enemy_rim > old_self_rim ? old_enemy_rim : old_self_rim;
        if (team_rim > 0.0f)
            Cvar_SetValue(cl_player_rimlight_team, team_rim, FROM_CODE);
    }
}

static bool CL_ForceModelActive(const cvar_t *var)
{
    if (!var || !var->string[0])
        return false;

    return Q_stricmp(var->string, "0") != 0;
}

static void CL_LoadForcedModel(forced_model_t *forced, const char *label, const cvar_t *var)
{
    memset(forced, 0, sizeof(*forced));

    if (!CL_ForceModelActive(var))
        return;

    char buffer[MAX_QPATH * 2];
    if (Q_concat(buffer, sizeof(buffer), "forced\\", var->string) >= sizeof(buffer)) {
        Com_WPrintf("cl_force_%s_model too long: '%s'\n", label, var->string);
        return;
    }

    CL_LoadClientinfo(&forced->info, buffer);
    if (!forced->info.model || !forced->info.skin) {
        memset(&forced->info, 0, sizeof(forced->info));
        return;
    }

    forced->active = true;
}

static void CL_UpdateForcedModels(void)
{
    if (!cl_force_enemy_model || !cl_force_team_model)
        return;

    if (cl_forced_enemy_modcount != cl_force_enemy_model->modified_count) {
        cl_forced_enemy_modcount = cl_force_enemy_model->modified_count;
        CL_LoadForcedModel(&cl_forced_enemy_model, "enemy", cl_force_enemy_model);
    }

    if (cl_forced_team_modcount != cl_force_team_model->modified_count) {
        cl_forced_team_modcount = cl_force_team_model->modified_count;
        CL_LoadForcedModel(&cl_forced_team_model, "team", cl_force_team_model);
    }
}

void CL_RegisterForcedModels(void)
{
    cl_forced_enemy_modcount = -1;
    cl_forced_team_modcount = -1;
    CL_UpdateForcedModels();
}

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

// returns true if origin/angles update has been optimized out
static inline bool entity_is_optimized(const entity_state_t *state)
{
    return (cls.serverProtocol == PROTOCOL_VERSION_Q2PRO || cls.serverProtocol == PROTOCOL_VERSION_RERELEASE)
        && state->number == cl.frame.clientNum + 1
        && cl.frame.ps.pmove.pm_type < PM_DEAD;
}

static inline void
entity_update_new(centity_t *ent, const entity_state_t *state, const vec_t *origin)
{
    static int entity_ctr;
    ent->id = ++entity_ctr;
    ent->trailcount = 1024;     // for diminishing rocket / grenade trails
    ent->flashlightfrac = 1.0f;

    // duplicate the current state so lerping doesn't hurt anything
    ent->prev = *state;
#if USE_FPS
    ent->prev_frame = state->frame;
    ent->event_frame = cl.frame.number;
#endif

// KEX
    ent->current_frame = ent->last_frame = state->frame;
    ent->frame_servertime = cl.servertime;
    ent->stair_time = cls.realtime;
// KEX

    if (state->event == EV_PLAYER_TELEPORT ||
        state->event == EV_OTHER_TELEPORT ||
        (state->renderfx & RF_BEAM)) {
        // no lerping if teleported
        VectorCopy(origin, ent->lerp_origin);
        return;
    }

    // old_origin is valid for new entities,
    // so use it as starting point for interpolating between
    VectorCopy(state->old_origin, ent->prev.origin);
    VectorCopy(state->old_origin, ent->lerp_origin);
}

#define	MAX_STEP_CHANGE 32

static inline void
entity_update_old(centity_t *ent, const entity_state_t *state, const vec_t *origin)
{
    int event = state->event;

#if USE_FPS
    // check for new event
    if (state->event != ent->current.event)
        ent->event_frame = cl.frame.number; // new
    else if (cl.frame.number - ent->event_frame >= cl.frametime.div)
        ent->event_frame = cl.frame.number; // refreshed
    else
        event = 0; // duplicated
#endif

// KEX
    if (ent->current_frame != state->frame)
    {
        ent->current_frame = state->frame;
        ent->last_frame = ent->current.frame;
        ent->frame_servertime = cl.servertime;
    }
// KEX

    if (state->modelindex != ent->current.modelindex
        || state->modelindex2 != ent->current.modelindex2
        || state->modelindex3 != ent->current.modelindex3
        || state->modelindex4 != ent->current.modelindex4
        || event == EV_PLAYER_TELEPORT
        || event == EV_OTHER_TELEPORT
        || fabsf(origin[0] - ent->current.origin[0]) > 512
        || fabsf(origin[1] - ent->current.origin[1]) > 512
        || fabsf(origin[2] - ent->current.origin[2]) > 512
        || cl_nolerp->integer == 1) {
        // some data changes will force no lerping
        ent->trailcount = 1024;     // for diminishing rocket / grenade trails
        ent->flashlightfrac = 1.0f;

        // duplicate the current state so lerping doesn't hurt anything
        ent->prev = *state;
#if USE_FPS
        ent->prev_frame = state->frame;
#endif
        // no lerping if teleported or morphed
        VectorCopy(origin, ent->lerp_origin);
        ent->stair_time = cls.realtime;
        return;
    }

#if USE_FPS
    // start alias model animation
    if (state->frame != ent->current.frame) {
        ent->prev_frame = ent->current.frame;
        ent->anim_start = cl.servertime - cl.frametime.time;
    }
#endif

    // stair interpolation support; this is only
    // necessary for > 10 fps
    if (cl.frametime.time != 100) {
        if (state->renderfx & RF_STAIR_STEP) {
            // Code below adapted from Q3A.
            // check for stepping up before a previous step is completed
            float step = origin[2] - ent->current.origin[2];
            float delta = cls.realtime - ent->stair_time;
            float old_step;
            if (delta < STEP_TIME) {
                old_step = ent->stair_height * (STEP_TIME - delta) / STEP_TIME;
            } else {
                old_step = 0;
            }

            // add this amount
            ent->stair_height = Q_clip(old_step + step, -MAX_STEP_CHANGE, MAX_STEP_CHANGE);
            ent->stair_time = cls.realtime;
        }
    }

    // shuffle the last state to previous
    ent->prev = ent->current;
}

static inline bool entity_is_new(const centity_t *ent)
{
    if (!cl.oldframe.valid)
        return true;    // last received frame was invalid

    if (ent->serverframe != cl.oldframe.number)
        return true;    // wasn't in last received frame

    if (cl_nolerp->integer == 2)
        return true;    // developer option, always new

    if (cl_nolerp->integer == 3)
        return false;   // developer option, lerp from last received frame

    if (cl.oldframe.number != cl.frame.number - 1)
        return true;    // previous server frame was dropped

    return false;
}

static void parse_entity_update(const entity_state_t *state)
{
    centity_t *ent = &cl_entities[state->number];
    const vec_t *origin;
    vec3_t origin_v;

    // if entity is solid, decode mins/maxs and add to the list
    if (state->solid && state->number != cl.frame.clientNum + 1
        && cl.numSolidEntities < MAX_PACKET_ENTITIES)
        cl.solidEntities[cl.numSolidEntities++] = ent;

    if (state->solid && state->solid != PACKED_BSP) {
        q2proto_client_unpack_solid(&cls.q2proto_ctx, state->solid, ent->mins, ent->maxs);
        ent->radius = Distance(ent->maxs, ent->mins) * 0.5f;
    } else {
        VectorClear(ent->mins);
        VectorClear(ent->maxs);
        ent->radius = 0;
    }

    // work around Q2PRO server bandwidth optimization
    if (entity_is_optimized(state)) {
        VectorCopy(cl.frame.ps.pmove.origin, origin_v);
        origin = origin_v;
    } else {
        origin = state->origin;
    }

    if (entity_is_new(ent)) {
        // wasn't in last update, so initialize some things
        entity_update_new(ent, state, origin);
    } else {
        entity_update_old(ent, state, origin);
    }

    ent->serverframe = cl.frame.number;
    ent->current = *state;

    // work around Q2PRO server bandwidth optimization
    if (entity_is_optimized(state)) {
        Com_PlayerToEntityState(&cl.frame.ps, &ent->current);
    }
}

// an entity has just been parsed that has an event value
static void parse_entity_event(int number)
{
    const centity_t *cent = &cl_entities[number];

    if (CL_FRAMESYNC) {
        // EF_TELEPORTER acts like an event, but is not cleared each frame
        if (cent->current.effects & EF_TELEPORTER)
            CL_TeleporterParticles(cent->current.origin);

        if (cent->current.effects & EF_TELEPORTER2)
            CL_TeleporterParticles2(cent->current.origin);

        if (cent->current.effects & EF_BARREL_EXPLODING)
            CL_BarrelExplodingParticles(cent->current.origin);
    }

#if USE_FPS
    if (cent->event_frame != cl.frame.number)
        return;
#endif

    switch (cent->current.event) {
    case EV_ITEM_RESPAWN:
        S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("items/respawn1.wav"), 1, ATTN_IDLE, 0);
        CL_ItemRespawnParticles(cent->current.origin);
        break;
    case EV_PLAYER_TELEPORT:
        S_StartSound(NULL, number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
        CL_TeleportParticles(cent->current.origin);
        break;
    case EV_FOOTSTEP:
        if (cl_footsteps->integer)
            CL_PlayFootstepSfx(-1, number, 1.0f, ATTN_NORM);
        break;
    case EV_OTHER_FOOTSTEP:
        if (cl.csr.extended && cl_footsteps->integer)
            CL_PlayFootstepSfx(-1, number, 0.5f, ATTN_IDLE);
        break;
    case EV_LADDER_STEP:
        if (cl.csr.extended && cl_footsteps->integer)
            CL_PlayFootstepSfx(FOOTSTEP_ID_LADDER, number, 0.5f, ATTN_IDLE);
        break;
    case EV_FALLSHORT:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("player/land1.wav"), 1, ATTN_NORM, 0);
        break;
    case EV_FALL:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall2.wav"), 1, ATTN_NORM, 0);
        break;
    case EV_FALLFAR:
        S_StartSound(NULL, number, CHAN_AUTO, S_RegisterSound("*fall1.wav"), 1, ATTN_NORM, 0);
        break;
    }
}

static void set_active_state(void)
{
    cls.state = ca_active;
    Cbuf_ExecuteDeferred(&cmd_buffer);

    cl.serverdelta = cl.frame.number ? Q_align_down(cl.frame.number, CL_FRAMEDIV) : 0;
    cl.time = cl.servertime = 0; // set time, needed for demos
#if USE_FPS
    cl.keytime = cl.keyservertime = 0;
    cl.keyframe = cl.frame; // initialize keyframe to make sure it's valid
#endif

    // initialize oldframe so lerping doesn't hurt anything
    cl.oldframe.valid = false;
    cl.oldframe.ps = cl.frame.ps;
#if USE_FPS
    cl.oldkeyframe.valid = false;
    cl.oldkeyframe.ps = cl.keyframe.ps;
#endif

    cl.frameflags = 0;
    cl.initialSeq = cls.netchan.outgoing_sequence;

    if (cls.demo.playback) {
        // init some demo things
        CL_FirstDemoFrame();
    } else {
        // set initial cl.predicted_origin and cl.predicted_angles
        VectorCopy(cl.frame.ps.pmove.origin, cl.predicted_origin);
        VectorCopy(cl.frame.ps.pmove.velocity, cl.predicted_velocity);
        if (cl.frame.ps.pmove.pm_type < PM_DEAD &&
            cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
            // enhanced servers don't send viewangles
            CL_PredictAngles();
        } else {
            // just use what server provided
            VectorCopy(cl.frame.ps.viewangles, cl.predicted_angles);
        }
        Vector4Copy(cl.frame.ps.screen_blend, cl.predicted_screen_blend);
        cl.predicted_rdflags = cl.frame.ps.rdflags;
        cl.current_viewheight = cl.prev_viewheight = cl.frame.ps.pmove.viewheight;
    }

    cl.viewheight_change_time = 0;

    cl.last_groundentity = NULL;
    memset(&cl.last_groundplane, 0, sizeof(cl.last_groundplane));

    SCR_EndLoadingPlaque();     // get rid of loading plaque
    SCR_LagClear();
    Con_Close(false);           // get rid of connection screen

    CL_CheckForPause();

    CL_UpdateFrameTimes();

    IN_Activate();

    if (!cls.demo.playback) {
        EXEC_TRIGGER(cl_beginmapcmd);
        Cmd_ExecTrigger("#cl_enterlevel");
    }
}

static void
check_player_lerp(server_frame_t *oldframe, server_frame_t *frame, int framediv)
{
    player_state_t *ps, *ops;
    const centity_t *ent;
    int oldnum;

    // find states to interpolate between
    ps = &frame->ps;
    ops = &oldframe->ps;

    // no lerping if previous frame was dropped or invalid
    if (!oldframe->valid)
        goto dup;

    oldnum = frame->number - framediv;
    if (oldframe->number != oldnum)
        goto dup;

    // no lerping if player entity was teleported (origin check)
    if (fabsf(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256 ||
        fabsf(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256 ||
        fabsf(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256) {
        goto dup;
    }

    // no lerping if player entity was teleported (event check)
    ent = &cl_entities[frame->clientNum + 1];
    if (ent->serverframe > oldnum &&
        ent->serverframe <= frame->number &&
#if USE_FPS
        ent->event_frame > oldnum &&
        ent->event_frame <= frame->number &&
#endif
        (ent->current.event == EV_PLAYER_TELEPORT
         || ent->current.event == EV_OTHER_TELEPORT)) {
        goto dup;
    }

    // no lerping if teleport bit was flipped
    if (!cl.csr.extended && (ops->pmove.pm_flags ^ ps->pmove.pm_flags) & PMF_TELEPORT_BIT)
        goto dup;

    if (cl.csr.extended && (ops->rdflags ^ ps->rdflags) & RDF_TELEPORT_BIT)
        goto dup;

    // no lerping if POV number changed
    if (oldframe->clientNum != frame->clientNum)
        goto dup;

    // developer option
    if (cl_nolerp->integer == 1)
        goto dup;

    return;

dup:
    // duplicate the current state so lerping doesn't hurt anything
    *ops = *ps;
}

/*
==================
CL_DeltaFrame

A valid frame has been parsed.
==================
*/
void CL_DeltaFrame(void)
{
    centity_t           *ent;
    int                 i, j;
    int                 framenum;
    int                 prevstate = cls.state;

    // getting a valid frame message ends the connection process
    if (cls.state == ca_precached)
        set_active_state();

    // set server time
    framenum = cl.frame.number - cl.serverdelta;

    if (framenum < 0)
        Com_Error(ERR_DROP, "%s: server time went backwards", __func__);

    if (CL_FRAMETIME && framenum > INT_MAX / CL_FRAMETIME)
        Com_Error(ERR_DROP, "%s: server time overflowed", __func__);

    cl.servertime = framenum * CL_FRAMETIME;
#if USE_FPS
    cl.keyservertime = (framenum / cl.frametime.div) * BASE_FRAMETIME;
#endif

    // rebuild the list of solid entities for this frame
    cl.numSolidEntities = 0;

    // initialize position of the player's own entity from playerstate.
    // this is needed in situations when player entity is invisible, but
    // server sends an effect referencing it's origin (such as MZ_LOGIN, etc)
    ent = &cl_entities[cl.frame.clientNum + 1];
    Com_PlayerToEntityState(&cl.frame.ps, &ent->current);

    // set current and prev, unpack solid, etc
    for (i = 0; i < cl.frame.numEntities; i++) {
        j = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        parse_entity_update(&cl.entityStates[j]);
    }

    // fire events. due to footstep tracing this must be after updating entities.
    for (i = 0; i < cl.frame.numEntities; i++) {
        j = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        parse_entity_event(cl.entityStates[j].number);
    }

    if (cls.demo.recording && !cls.demo.paused && !cls.demo.seeking && CL_FRAMESYNC) {
        CL_EmitDemoFrame();
    }

    if (prevstate == ca_precached)
        CL_GTV_Resume();
    else
        CL_GTV_EmitFrame();

    if (cls.demo.playback) {
        // this delta has nothing to do with local viewangles,
        // clear it to avoid interfering with demo freelook hack
        VectorClear(cl.frame.ps.pmove.delta_angles);
    }

    if (cl.oldframe.ps.pmove.pm_type != cl.frame.ps.pmove.pm_type) {
        IN_Activate();
    }

    check_player_lerp(&cl.oldframe, &cl.frame, 1);

#if USE_FPS
    if (CL_FRAMESYNC)
        check_player_lerp(&cl.oldkeyframe, &cl.keyframe, cl.frametime.div);
#endif

    CL_CheckPredictionError();

    SCR_SetCrosshairColor();
}

#if USE_DEBUG
// for debugging problems when out-of-date entity origin is referenced
void CL_CheckEntityPresent(int entnum, const char *what)
{
    const centity_t *e;

    if (entnum == cl.frame.clientNum + 1) {
        return; // player entity = current
    }

    e = &cl_entities[entnum];
    if (e->serverframe == cl.frame.number) {
        return; // current
    }

    if (e->serverframe) {
        Com_LPrintf(PRINT_DEVELOPER,
                    "SERVER BUG: %s on entity %d last seen %d frames ago\n",
                    what, entnum, cl.frame.number - e->serverframe);
    } else {
        Com_LPrintf(PRINT_DEVELOPER,
                    "SERVER BUG: %s on entity %d never seen before\n",
                    what, entnum);
    }
}
#endif


/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARAMS

==========================================================================
*/

static float lerp_entity_alpha(const centity_t *ent)
{
    float prev = ent->prev.alpha;
    float curr = ent->current.alpha;

    // no lerping from/to default alpha
    if (prev && curr)
        return prev + cl.lerpfrac * (curr - prev);

    return curr ? curr : 1.0f;
}

static bool CL_IsPlayerEntity(const entity_state_t *state)
{
    return state->modelindex == MODELINDEX_PLAYER && state->number <= cl.maxclients;
}

static bool CL_GetPlayerTeamInfo(const entity_state_t *state, uint8_t *team_index, bool *is_dead)
{
    if (team_index)
        *team_index = 0;
    if (is_dead)
        *is_dead = false;

    if (cl.game_api != Q2PROTO_GAME_RERELEASE || !cl.csr.extended)
        return false;

    player_skinnum_t unpacked = { .skinnum = state->skinnum };

    if (team_index)
        *team_index = unpacked.team_index;
    if (is_dead)
        *is_dead = unpacked.poi_icon != 0;

    return true;
}

static color_t CL_BrightskinTeamColor(uint8_t team_index)
{
    switch (team_index) {
    case 1:
        return COLOR_RED;
    case 2:
        return COLOR_BLUE;
    default:
        return COLOR_GREEN;
    }
}

static color_t CL_SelectBrightskinColor(bool use_custom, bool is_enemy, bool is_ally, bool is_self, uint8_t team_index)
{
    if (!use_custom)
        return CL_BrightskinTeamColor(team_index);

    if (is_enemy || (!is_self && !is_ally))
        return brightskin_enemy_color;

    return brightskin_team_color;
}

static bool CL_IsThirdPersonSelf(const entity_state_t *state)
{
    return cl.thirdPersonView && state->number == cl.frame.clientNum + 1;
}

static uint8_t CL_ShellColorToByte(float value)
{
    int scaled = Q_rint(value * 255.0f);
    return (uint8_t)Q_clip(scaled, 0, 255);
}

static bool CL_ShellRimlightColor(renderfx_t flags, color_t *color)
{
    flags &= RF_SHELL_MASK;
    if (!flags)
        return false;

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    if (flags & RF_SHELL_LITE_GREEN) {
        r = 0.56f;
        g = 0.93f;
        b = 0.56f;
    }
    if (flags & RF_SHELL_HALF_DAM) {
        r = 0.56f;
        g = 0.59f;
        b = 0.45f;
    }
    if (flags & RF_SHELL_DOUBLE) {
        r = 0.9f;
        g = 0.7f;
        b = 0.0f;
    }

    if (flags & RF_SHELL_RED)
        r = 1.0f;
    if (flags & RF_SHELL_GREEN)
        g = 1.0f;
    if (flags & RF_SHELL_BLUE)
        b = 1.0f;

    if (r == 0.0f && g == 0.0f && b == 0.0f)
        return false;

    *color = COLOR_RGB(CL_ShellColorToByte(r), CL_ShellColorToByte(g), CL_ShellColorToByte(b));
    return true;
}

/*
===============
CL_AddPacketEntities

===============
*/
static void CL_AddPacketEntities(void)
{
    entity_t                ent;
    const entity_state_t    *s1;
    float                   autorotate, autobob;
    int                     i;
    int                     pnum;
    centity_t               *cent;
    int                     autoanim;
    const clientinfo_t      *ci;
    effects_t               effects;
    renderfx_t              renderfx;
    bool                    has_alpha, has_trail;
    float                   custom_alpha;
    float                   entity_alpha;
    uint64_t                custom_flags;
    float                   enemy_outline_alpha;
    float                   team_outline_alpha;
    bool                    enemy_outline_enabled;
    float                   enemy_rim_alpha;
    bool                    enemy_rim_enabled;
    float                   team_rim_alpha;
    bool                    team_outline_enabled;
    bool                    team_rim_enabled;
    float                   shell_rim_scale;
    uint8_t                 my_team;
    bool                    teamplay;
    bool                    brightskins_custom;
    bool                    brightskins_hide_dead;
    bool                    viewer_is_spectator;

    // bonus items rotate at a fixed rate
    autorotate = anglemod(cl.time * 0.1f);

    // brush models can auto animate their frames
    autoanim = cl.time / 500;

    autobob = 5 * sinf(cl.time / 400.0f);

    viewer_is_spectator = CL_IsSpectatorView();
    enemy_outline_alpha = Cvar_ClampValue(cl_player_outline_enemy, 0.0f, 1.0f);
    team_outline_alpha = Cvar_ClampValue(cl_player_outline_team, 0.0f, 1.0f);
    enemy_outline_enabled = enemy_outline_alpha > 0.0f;
    team_outline_enabled = team_outline_alpha > 0.0f;
    enemy_rim_alpha = Cvar_ClampValue(cl_player_rimlight_enemy, 0.0f, 1.0f);
    team_rim_alpha = Cvar_ClampValue(cl_player_rimlight_team, 0.0f, 1.0f);
    enemy_rim_enabled = enemy_rim_alpha > 0.0f;
    team_rim_enabled = team_rim_alpha > 0.0f;
    shell_rim_scale = cl_player_rimlight_shell ? Cvar_ClampValue(cl_player_rimlight_shell, 0.0f, 1.0f) : 0.0f;
    my_team = cl.frame.ps.team_id;
    teamplay = my_team != 0;
    brightskins_custom = cl_brightskins_custom->integer != 0 && !viewer_is_spectator;
    brightskins_hide_dead = cl_brightskins_dead->integer != 0;
    CL_UpdateForcedModels();

    memset(&ent, 0, sizeof(ent));

    for (pnum = 0; pnum < cl.frame.numEntities; pnum++) {
        i = (cl.frame.firstEntity + pnum) & PARSE_ENTITIES_MASK;
        s1 = &cl.entityStates[i];

        // handled elsewhere
        if (s1->renderfx & RF_CASTSHADOW) {
            continue;
        }

        cent = &cl_entities[s1->number];
        ent.id = cent->id + RESERVED_ENTITY_COUNT;

        has_trail = false;
        bool is_player = CL_IsPlayerEntity(s1);
        const clientinfo_t *render_ci = NULL;
        qhandle_t brightskin = 0;
        bool is_self = false;
        uint8_t other_team = 0;
        bool is_dead = false;
        bool has_team_info = false;
        bool is_enemy = false;
        bool is_ally = false;
        bool valid_player = false;
        bool shell_rim_active = false;
        color_t shell_rim_color = COLOR_WHITE;
        float shell_rim_alpha = 0.0f;

        effects = s1->effects;
        renderfx = s1->renderfx;

        // set frame
        if (effects & EF_ANIM01)
            ent.frame = autoanim & 1;
        else if (effects & EF_ANIM23)
            ent.frame = 2 + (autoanim & 1);
        else if (effects & EF_ANIM_ALL)
            ent.frame = autoanim;
        else if (effects & EF_ANIM_ALLFAST)
            ent.frame = cl.time / 100;
        else
            ent.frame = s1->frame;

        // quad and pent can do different things on client
        if (effects & EF_PENT) {
            effects &= ~EF_PENT;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_RED;
        }

        if (effects & EF_QUAD) {
            effects &= ~EF_QUAD;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_BLUE;
        }

        if (effects & EF_DOUBLE) {
            effects &= ~EF_DOUBLE;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_DOUBLE;
        }

        if (effects & EF_HALF_DAMAGE) {
            effects &= ~EF_HALF_DAMAGE;
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_HALF_DAM;
        }

        if (effects & EF_DUALFIRE) {
            effects |= EF_COLOR_SHELL;
            renderfx |= RF_SHELL_LITE_GREEN;
        }

        // optionally remove the glowing effect
        if (cl_noglow->integer && !(renderfx & RF_BEAM))
            renderfx &= ~RF_GLOW;

        ent.oldframe = cent->prev.frame;
        ent.backlerp = 1.0f - cl.lerpfrac;

// KEX
        if (cl.csr.extended) {
            // TODO: must only do this on alias models
            if (cent->last_frame != cent->current_frame) {
                ent.backlerp = Q_clipf(1.0f - ((cl.time - ((float) cent->frame_servertime - cl.frametime.time)) / 100.f), 0.0f, 1.0f);
                ent.frame = cent->current_frame;
                ent.oldframe = cent->last_frame;
            }
        }
// KEX

        if (renderfx & RF_BEAM) {
            // interpolate start and end points for beams
            LerpVector(cent->prev.origin, cent->current.origin,
                        cl.lerpfrac, ent.origin);
            LerpVector(cent->prev.old_origin, cent->current.old_origin,
                        cl.lerpfrac, ent.oldorigin);
        } else {
            if (s1->number == cl.frame.clientNum + 1) {
                // use predicted origin
                VectorCopy(cl.playerEntityOrigin, ent.origin);
                VectorCopy(cl.playerEntityOrigin, ent.oldorigin);
            } else {
                // interpolate origin
                LerpVector(cent->prev.origin, cent->current.origin,
                           cl.lerpfrac, ent.origin);
                VectorCopy(ent.origin, ent.oldorigin);
            }
#if USE_FPS
            // run alias model animation
            if (cent->prev_frame != s1->frame) {
                int delta = cl.time - cent->anim_start;
                float frac;

                if (delta > BASE_FRAMETIME) {
                    cent->prev_frame = s1->frame;
                    frac = 1;
                } else if (delta > 0) {
                    frac = delta * BASE_1_FRAMETIME;
                } else {
                    frac = 0;
                }

                ent.oldframe = cent->prev_frame;
                ent.backlerp = 1.0f - frac;
            }
#endif
        }

        if (effects & EF_BOB && !cl_nobob->integer) {
            ent.origin[2] += autobob;
            ent.oldorigin[2] += autobob;
        }

        // use predicted values
        unsigned step_delta = cls.realtime - cent->stair_time;

        // smooth out stair climbing
        if (step_delta < STEP_TIME && cent->stair_height) {
            float step_change = cent->stair_height * ((STEP_TIME - step_delta) * (1.f / STEP_TIME));
            ent.origin[2] = cent->current.origin[2] - step_change;
            ent.oldorigin[2] = ent.origin[2];
        }

        // bottom Z position, for shadow fading
        ent.bottom_z = ent.origin[2] + cent->mins[2];

        if (!cl_gibs->integer) {
            if (effects & EF_GIB && !(cl.csr.extended && effects & EF_ROCKET))
                goto skip;
            if (effects & EF_GREENGIB)
                goto skip;
        }

        // create a new entity

        if (cl.csr.extended) {
            if (renderfx & RF_FLARE) {
                if (!cl_flares->integer)
                    goto skip;
                float fade_start = s1->modelindex2;
                float fade_end = s1->modelindex3;
                float d = Distance(cl.refdef.vieworg, ent.origin);
                if (d < fade_start)
                    goto skip;
                if (d > fade_end)
                    ent.alpha = 1;
                else
                    ent.alpha = (d - fade_start) / (fade_end - fade_start);
                ent.skin = 0;
                if (renderfx & RF_CUSTOMSKIN && (unsigned)s1->frame < cl.csr.max_images)
                    ent.skin = cl.image_precache[s1->frame];
                if (!ent.skin)
                    ent.skin = cl_img_flare;
                float s = s1->scale ? s1->scale : 1;
                VectorSet(ent.scale, s, s, s);
                ent.flags = renderfx | RF_TRANSLUCENT;
                if (!s1->skinnum)
                    ent.rgba = COLOR_WHITE;
                else
                    ent.rgba.u32 = BigLong(s1->skinnum);
                ent.skinnum = s1->number;
                V_AddEntity(&ent);
                goto skip;
            }

            if (renderfx & RF_CUSTOM_LIGHT) {
                color_t color;
                if (!s1->skinnum)
                    color = COLOR_WHITE;
                else
                    color.u32 = BigLong(s1->skinnum);
                V_AddLight(ent.origin, DLIGHT_CUTOFF + s1->frame,
                           color.r / 255.0f,
                           color.g / 255.0f,
                           color.b / 255.0f);
                goto skip;
            }

            if ((renderfx & RF_BEAM) && s1->modelindex > 1) {
                CL_DrawBeam(ent.origin, ent.oldorigin, ent.frame, cl.model_draw[s1->modelindex]);
                goto skip;
            }
        }

        // tweak the color of beams
        if (renderfx & RF_BEAM) {
            // the four beam colors are encoded in 32 bits of skinnum (hack)
            ent.alpha = 0.30f;
            ent.skinnum = (s1->skinnum >> ((Com_SlowRand() % 4) * 8)) & 0xff;
            ent.model = 0;
        } else {
            // set skin
            if (s1->modelindex == MODELINDEX_PLAYER) {
                // use custom player skin
                ent.skinnum = 0;
                ci = &cl.clientinfo[s1->skinnum & 0xff];
                ent.skin = ci->skin;
                ent.model = ci->model;
                if (!ent.skin || !ent.model) {
                    ent.skin = cl.baseclientinfo.skin;
                    ent.model = cl.baseclientinfo.model;
                    ci = &cl.baseclientinfo;
                }
                render_ci = ci;
                brightskin = ci->brightskin;
                if (renderfx & RF_USE_DISGUISE) {
                    char buffer[MAX_QPATH];

                    Q_concat(buffer, sizeof(buffer), "players/", ci->model_name, "/disguise.pcx");
                    ent.skin = R_RegisterSkin(buffer);
                    Q_concat(buffer, sizeof(buffer), "players/", ci->model_name, "/disguise_brtskn.png");
                    brightskin = R_RegisterImage(buffer, IT_SKIN,
                                                 static_cast<imageflags_t>(IF_KEEP_EXTENSION | IF_OPTIONAL));
                }
            } else {
                ent.skinnum = s1->skinnum;
                ent.skin = 0;
                ent.model = cl.model_draw[s1->modelindex];
                if (ent.model == cl_mod_laser || ent.model == cl_mod_dmspot)
                    renderfx |= RF_NOSHADOW;
            }
        }

        // allow skin override for remaster
        if (cl.csr.extended && renderfx & RF_CUSTOMSKIN && (unsigned)s1->skinnum < cl.csr.max_images) {
            ent.skin = cl.image_precache[s1->skinnum];
            ent.skinnum = 0;
            brightskin = 0;
        }

        // only used for black hole model right now, FIXME: do better
        if ((renderfx & RF_TRANSLUCENT) && !(renderfx & RF_BEAM))
            ent.alpha = 0.70f;

        // render effects (fullbright, translucent, etc)
        if (effects & EF_COLOR_SHELL)
            ent.flags = 0;  // renderfx go on color shell entity
        else
            ent.flags = renderfx;
        {
            const effects_t shadow_skip_effects =
                EF_ROCKET | EF_BLASTER | EF_HYPERBLASTER | EF_BFG | EF_TRAP |
                EF_TRACKERTRAIL | EF_TRACKER | EF_IONRIPPER | EF_PLASMA |
                EF_BLUEHYPERBLASTER | EF_GRENADE_LIGHT;
            if (effects & shadow_skip_effects)
                ent.flags |= RF_NOSHADOW;
        }

        // calculate angles
        if (effects & EF_ROTATE) {  // some bonus items auto-rotate
            ent.angles[0] = 0;
            ent.angles[1] = autorotate;
            ent.angles[2] = 0;
        } else if (effects & EF_SPINNINGLIGHTS) {
            vec3_t forward;
            vec3_t start;

            ent.angles[0] = 0;
            ent.angles[1] = anglemod(cl.time / 2) + s1->angles[1];
            ent.angles[2] = 180;

            AngleVectors(ent.angles, forward, NULL, NULL);
            VectorMA(ent.origin, 64, forward, start);
            V_AddLight(start, 100, 1, 0, 0);
        } else if (s1->number == cl.frame.clientNum + 1) {
            VectorCopy(cl.playerEntityAngles, ent.angles);      // use predicted angles
        } else { // interpolate angles
            LerpAngles(cent->prev.angles, cent->current.angles,
                       cl.lerpfrac, ent.angles);
            // mimic original ref_gl "leaning" bug (uuugly!)
            if (s1->modelindex == MODELINDEX_PLAYER && cl_rollhack->integer && !cl.csr.extended)
                ent.angles[ROLL] = -ent.angles[ROLL];
        }

        if (effects & EF_FLASHLIGHT) {
            vec3_t forward, start, end;
            trace_t trace;
            contents_t mask = CONTENTS_SOLID;
            bool is_per_pixel = cl_shadowlights->integer && R_SupportsPerPixelLighting();
            
            if (!is_per_pixel)
                mask |= CONTENTS_MONSTER | CONTENTS_PLAYER;

            if (s1->number == cl.frame.clientNum + 1) {
                VectorMA(cl.refdef.vieworg, 256, cl.v_forward, end);
                VectorCopy(cl.refdef.vieworg, start);
                VectorCopy(cl.v_forward, forward);
            } else {
                AngleVectors(ent.angles, forward, NULL, NULL);
                float dist = is_per_pixel ? 1024 : 256;
                VectorMA(ent.origin, dist, forward, end);
                VectorCopy(ent.origin, start);
            }

            CL_Trace(&trace, start, end, vec3_origin, vec3_origin, NULL, mask);

            if (is_per_pixel) {
                cl_shadow_light_t light;
                light.fade_end = light.fade_start = 0;
                light.lightstyle = -1;
                light.resolution = 512.0f;
                light.intensity = 2.0f;
                light.radius = 512.0f;
                light.coneangle = 22.0f;
                VectorCopy(forward, light.conedirection);
                light.color = COLOR_WHITE;
                VectorCopy(start, light.origin);
                if (s1->number == cl.frame.clientNum + 1 && info_hand->integer != 2) {
                    VectorMA(light.origin, info_hand->integer ? -7 : 7, cl.v_right, light.origin);
                }
                V_AddLightEx(&light);
            } else {
                // smooth out distance "jumps"
                LerpVector(start, end, cent->flashlightfrac, end);
                V_AddLight(end, 256, 1, 1, 1);
                CL_AdvanceValue(&cent->flashlightfrac, trace.fraction, 1);
            }
        }

        if (effects & EF_GRENADE_LIGHT)
            V_AddLight(ent.origin, 100, 1, 1, 0);

        if (s1->number == cl.frame.clientNum + 1 && !cl.thirdPersonView) {
            if (effects & EF_FLAG1)
                V_AddLight(ent.origin, 225, 1.0f, 0.1f, 0.1f);
            else if (effects & EF_FLAG2)
                V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1.0f);
            else if (effects & EF_TAGTRAIL)
                V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
            else if (effects & EF_TRACKERTRAIL)
                V_AddLight(ent.origin, 225, -1.0f, -1.0f, -1.0f);
            goto skip;
        }

        // if set to invisible, skip
        if (!s1->modelindex)
            goto skip;

        if (effects & EF_BFG) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.30f;
        }

        if (effects & EF_PLASMA) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.6f;
        }

        if (effects & EF_SPHERETRANS) {
            ent.flags |= RF_TRANSLUCENT;
            if (effects & EF_TRACKERTRAIL)
                ent.alpha = 0.6f;
            else
                ent.alpha = 0.3f;
        }

        // custom alpha overrides any derived value
        custom_alpha = 1.0f;
        custom_flags = 0;
        has_alpha = false;

        if (s1->alpha) {
            custom_alpha = lerp_entity_alpha(cent);
            has_alpha = true;
        }

        if (s1->number == cl.frame.clientNum + 1 && cl.thirdPersonView && cl.thirdPersonAlpha != 1.0f) {
            custom_alpha *= cl.thirdPersonAlpha;
            has_alpha = true;
        }

        if (has_alpha) {
            ent.alpha = custom_alpha;
            if (custom_alpha == 1.0f)
                ent.flags &= ~RF_TRANSLUCENT;
            else
                ent.flags |= RF_TRANSLUCENT;
            custom_flags = ent.flags & RF_TRANSLUCENT;
        }
        entity_alpha = (ent.flags & RF_TRANSLUCENT) ? ent.alpha : 1.0f;

        // tracker effect is duplicated for linked models
        if (IS_TRACKER(effects)) {
            ent.flags    |= RF_TRACKER;
            custom_flags |= RF_TRACKER;
        }

        VectorSet(ent.scale, s1->scale, s1->scale, s1->scale);

        is_self = is_player && CL_IsThirdPersonSelf(s1);
        other_team = 0;
        is_dead = false;
        has_team_info = is_player && CL_GetPlayerTeamInfo(s1, &other_team, &is_dead);
        is_enemy = false;
        is_ally = false;
        valid_player = is_player && !is_dead;

        if (is_player && !is_self) {
            if (teamplay && has_team_info) {
                if (other_team) {
                    if (other_team == my_team)
                        is_ally = true;
                    else
                        is_enemy = true;
                }
            } else {
                is_enemy = true;
            }
        }

        if (!viewer_is_spectator && is_player && !is_self &&
            !(renderfx & (RF_CUSTOMSKIN | RF_USE_DISGUISE))) {
            const forced_model_t *forced = NULL;

            if (is_enemy && cl_forced_enemy_model.active)
                forced = &cl_forced_enemy_model;
            else if (is_ally && cl_forced_team_model.active)
                forced = &cl_forced_team_model;

            if (forced && forced->info.model && forced->info.skin) {
                ent.skin = forced->info.skin;
                ent.model = forced->info.model;
                brightskin = forced->info.brightskin;
                render_ci = &forced->info;
            }
        }

        if (enemy_outline_enabled || team_outline_enabled || enemy_rim_enabled || team_rim_enabled) {
            bool self_effects = is_self && valid_player && cl.thirdPersonView;
            bool outline_enemy = enemy_outline_enabled && is_enemy && valid_player;
            bool outline_team = team_outline_enabled && is_ally && valid_player;
            bool outline_self = team_outline_enabled && self_effects;
            bool rim_enemy = enemy_rim_enabled && is_enemy && valid_player;
            bool rim_team = team_rim_enabled && is_ally && valid_player;
            bool rim_self = team_rim_enabled && self_effects;

            if (outline_enemy || outline_team || outline_self) {
                uint8_t team_index = is_self ? my_team : other_team;
                color_t outline_color = CL_SelectBrightskinColor(brightskins_custom, is_enemy, is_ally, is_self, team_index);
                float outline_scale = outline_enemy ? enemy_outline_alpha : team_outline_alpha;
                float outline_alpha = entity_alpha * outline_scale * (outline_color.a * (1.0f / 255.0f));

                if (outline_alpha > 0.0f) {
                    ent.rgba = COLOR_SETA_F(outline_color, outline_alpha);
                    ent.flags |= RF_OUTLINE;
                    if (outline_team)
                        ent.flags |= RF_OUTLINE_NODEPTH;
                }
            }

            if (rim_enemy || rim_team || rim_self) {
                float rim_scale = rim_enemy ? enemy_rim_alpha : team_rim_alpha;
                uint8_t team_index = is_self ? my_team : other_team;
                color_t rim_color = CL_SelectBrightskinColor(brightskins_custom, is_enemy, is_ally, is_self, team_index);
                float rim_alpha = entity_alpha * rim_scale * (rim_color.a * (1.0f / 255.0f));

                if (rim_alpha > 0.0f) {
                    entity_t rim = ent;
                    rim.flags = RF_RIMLIGHT | RF_TRANSLUCENT;
                    rim.alpha = rim_alpha;
                    rim.rgba = rim_color;
                    V_AddEntity(&rim);
                }
            }
        }

        // add to renderer list
        V_AddEntity(&ent);

        if (is_player && brightskin && (!brightskins_hide_dead || !is_dead)) {
            uint8_t team_index = is_self ? my_team : other_team;
            color_t bright_color = CL_SelectBrightskinColor(brightskins_custom, is_enemy, is_ally, is_self, team_index);
            float bright_alpha = entity_alpha * (bright_color.a * (1.0f / 255.0f));
            if (bright_alpha > 0.0f) {
                entity_t bright = ent;
                bright.skin = brightskin;
                bright.skinnum = 0;
                bright.flags = RF_FULLBRIGHT | RF_TRANSLUCENT | RF_BRIGHTSKIN | RF_NOSHADOW;
                bright.alpha = bright_alpha;
                bright.rgba = bright_color;
                V_AddEntity(&bright);
            }
        }

        // color shells generate a separate entity for the main model
        if (effects & EF_COLOR_SHELL) {
            // PMM - at this point, all of the shells have been handled
            // if we're in the rogue pack, set up the custom mixing, otherwise just
            // keep going
            if (!strcmp(fs_game->string, "rogue")) {
                // all of the solo colors are fine.  we need to catch any of the combinations that look bad
                // (double & half) and turn them into the appropriate color, and make double/quad something special
                if (renderfx & RF_SHELL_HALF_DAM) {
                    // ditch the half damage shell if any of red, blue, or double are on
                    if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
                        renderfx &= ~RF_SHELL_HALF_DAM;
                }

                if (renderfx & RF_SHELL_DOUBLE) {
                    // lose the yellow shell if we have a red, blue, or green shell
                    if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_GREEN))
                        renderfx &= ~RF_SHELL_DOUBLE;
                    // if we have a red shell, turn it to purple by adding blue
                    if (renderfx & RF_SHELL_RED)
                        renderfx |= RF_SHELL_BLUE;
                    // if we have a blue shell (and not a red shell), turn it to cyan by adding green
                    else if (renderfx & RF_SHELL_BLUE) {
                        // go to green if it's on already, otherwise do cyan (flash green)
                        if (renderfx & RF_SHELL_GREEN)
                            renderfx &= ~RF_SHELL_BLUE;
                        else
                            renderfx |= RF_SHELL_GREEN;
                    }
                }
            }

            if (shell_rim_scale > 0.0f && is_player) {
                color_t rim_color;
                if (CL_ShellRimlightColor(renderfx, &rim_color)) {
                    float rim_alpha = entity_alpha * (0.30f * shell_rim_scale);
                    if (rim_alpha > 0.0f) {
                        entity_t rim = ent;
                        rim.flags = RF_RIMLIGHT | RF_TRANSLUCENT;
                        rim.alpha = rim_alpha;
                        rim.rgba = rim_color;
                        V_AddEntity(&rim);
                        shell_rim_active = true;
                        shell_rim_color = rim_color;
                        shell_rim_alpha = rim_alpha;
                    }
                }
            }
            ent.flags = renderfx | RF_TRANSLUCENT;
            ent.alpha = custom_alpha * 0.30f;
            V_AddEntity(&ent);
        }

        ent.skin = 0;       // never use a custom skin on others
        ent.skinnum = 0;
        ent.flags = custom_flags;
        ent.alpha = custom_alpha;

        // duplicate for linked models
        if (s1->modelindex2) {
            if (s1->modelindex2 == MODELINDEX_PLAYER) {
                // custom weapon
                const clientinfo_t *weapon_ci = render_ci;

                if (cl.game_api == Q2PROTO_GAME_RERELEASE) {
                    player_skinnum_t unpacked = { .skinnum = s1->skinnum };
                    ci = &cl.clientinfo[unpacked.client_num];
                    i = unpacked.vwep_index;
                } else {
                    ci = &cl.clientinfo[s1->skinnum & 0xff];
                    i = (s1->skinnum >> 8); // 0 is default weapon model
                }
                if (!weapon_ci)
                    weapon_ci = ci;
                if (i < 0 || i > cl.numWeaponModels - 1)
                    i = 0;
                ent.model = weapon_ci->weaponmodel[i];
                if (!ent.model) {
                    if (i != 0)
                        ent.model = weapon_ci->weaponmodel[0];
                    if (!ent.model)
                        ent.model = cl.baseclientinfo.weaponmodel[0];
                }
            } else
                ent.model = cl.model_draw[s1->modelindex2];

            // PMM - check for the defender sphere shell .. make it translucent
            if (!Q_strcasecmp(cl.configstrings[cl.csr.models + s1->modelindex2], "models/items/shell/tris.md2")) {
                ent.alpha = custom_alpha * 0.32f;
                ent.flags = RF_TRANSLUCENT;
            }

            V_AddEntity(&ent);

            if (shell_rim_active) {
                entity_t rim = ent;
                rim.flags = RF_RIMLIGHT | RF_TRANSLUCENT;
                rim.alpha = shell_rim_alpha;
                rim.rgba = shell_rim_color;
                V_AddEntity(&rim);
            }

            //PGM - make sure these get reset.
            ent.flags = custom_flags;
            ent.alpha = custom_alpha;
        }

        if (s1->modelindex3) {
            ent.model = cl.model_draw[s1->modelindex3];
            V_AddEntity(&ent);
        }

        if (s1->modelindex4) {
            ent.model = cl.model_draw[s1->modelindex4];
            V_AddEntity(&ent);
        }

        if (effects & EF_POWERSCREEN) {
            ent.model = cl_mod_powerscreen;
            ent.oldframe = 0;
            ent.frame = 0;
            ent.flags = RF_TRANSLUCENT;
            ent.alpha = custom_alpha * 0.30f;

            // remaster powerscreen is tiny and needs scaling
            if (cl.need_powerscreen_scale) {
                vec3_t forward, mid, tmp;
                VectorCopy(ent.origin, tmp);
                VectorAvg(cent->mins, cent->maxs, mid);
                VectorAdd(ent.origin, mid, ent.origin);
                AngleVectors(ent.angles, forward, NULL, NULL);
                VectorMA(ent.origin, cent->maxs[0], forward, ent.origin);
                float s = cent->radius * 0.8f;
                VectorSet(ent.scale, s, s, s);
                ent.flags |= RF_FULLBRIGHT;
                V_AddEntity(&ent);
                VectorCopy(tmp, ent.origin);
            } else {
                ent.flags |= RF_SHELL_GREEN;
                V_AddEntity(&ent);
            }
        }

        if (effects & EF_HOLOGRAM)
            CL_HologramParticles(ent.origin);

        // add automatic particle trails
        if (!(effects & EF_TRAIL_MASK))
            goto skip;

        if (effects & EF_ROCKET) {
            if (cl.csr.extended && effects & EF_GIB) {
                CL_DiminishingTrail(cent, ent.origin, DT_FIREBALL);
                has_trail = true;
            } else if (!(cl_disable_particles->integer & NOPART_ROCKET_TRAIL)) {
                CL_DiminishingTrail(cent, ent.origin, DT_ROCKET);
                has_trail = true;
            }
            if (cl_dlight_hacks->integer & DLHACK_ROCKET_COLOR)
                V_AddLight(ent.origin, 200, 1, 0.23f, 0);
            else
                V_AddLight(ent.origin, 200, 1, 1, 0);
        } else if (effects & EF_BLASTER) {
            if (effects & EF_TRACKER) {
                CL_BlasterTrail2(cent, ent.origin);
                V_AddLight(ent.origin, 200, 0, 1, 0);
                has_trail = true;
            } else {
                if (!(cl_disable_particles->integer & NOPART_BLASTER_TRAIL)) {
                    CL_BlasterTrail(cent, ent.origin);
                    has_trail = true;
                }
                V_AddLight(ent.origin, 200, 1, 1, 0);
            }
        } else if (effects & EF_HYPERBLASTER) {
            if (effects & EF_TRACKER)
                V_AddLight(ent.origin, 200, 0, 1, 0);
            else
                V_AddLight(ent.origin, 200, 1, 1, 0);
        } else if (effects & EF_GIB) {
            CL_DiminishingTrail(cent, ent.origin, DT_GIB);
            has_trail = true;
        } else if (effects & EF_GRENADE) {
            if (!(cl_disable_particles->integer & NOPART_GRENADE_TRAIL)) {
                CL_DiminishingTrail(cent, ent.origin, DT_GRENADE);
                has_trail = true;
            }
        } else if (effects & EF_FLIES) {
            CL_FlyEffect(cent, ent.origin);
        } else if (effects & EF_BFG) {
            static const uint16_t bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};
            if (effects & EF_ANIM_ALLFAST) {
                CL_BfgParticles(&ent);
                i = 200;
            } else if (cl.csr.extended || cl_smooth_explosions->integer) {
                i = bfg_lightramp[Q_clip(ent.oldframe, 0, 5)] * ent.backlerp +
                    bfg_lightramp[Q_clip(ent.frame,    0, 5)] * (1.0f - ent.backlerp);
            } else {
                i = bfg_lightramp[Q_clip(s1->frame, 0, 5)];
            }
            V_AddLight(ent.origin, i, 0, 1, 0);
        } else if (effects & EF_TRAP) {
            ent.origin[2] += 32;
            CL_TrapParticles(cent, ent.origin);
            i = (Com_SlowRand() % 100) + 100;
            V_AddLight(ent.origin, i, 1, 0.8f, 0.1f);
        } else if (effects & EF_FLAG1) {
            CL_FlagTrail(cent, ent.origin, 242);
            V_AddLight(ent.origin, 225, 1, 0.1f, 0.1f);
            has_trail = true;
        } else if (effects & EF_FLAG2) {
            CL_FlagTrail(cent, ent.origin, 115);
            V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1);
            has_trail = true;
        } else if (effects & EF_TAGTRAIL) {
            CL_TagTrail(cent, ent.origin, 220);
            V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
            has_trail = true;
        } else if (effects & EF_TRACKERTRAIL) {
            if (effects & EF_TRACKER) {
                float intensity = 50 + (500 * (sinf(cl.time / 500.0f) + 1.0f));
                V_AddLight(ent.origin, intensity, -1.0f, -1.0f, -1.0f);
            } else {
                CL_Tracker_Shell(cent, ent.origin);
                V_AddLight(ent.origin, 155, -1.0f, -1.0f, -1.0f);
            }
        } else if (effects & EF_TRACKER) {
            CL_TrackerTrail(cent, ent.origin);
            V_AddLight(ent.origin, 200, -1, -1, -1);
            has_trail = true;
        } else if (effects & EF_GREENGIB) {
            CL_DiminishingTrail(cent, ent.origin, DT_GREENGIB);
            has_trail = true;
        } else if (effects & EF_IONRIPPER) {
            CL_IonripperTrail(cent, ent.origin);
            V_AddLight(ent.origin, 100, 1, 0.5f, 0.5f);
            has_trail = true;
        } else if (effects & EF_BLUEHYPERBLASTER) {
            V_AddLight(ent.origin, 200, 0, 0, 1);
        } else if (effects & EF_PLASMA) {
            if (effects & EF_ANIM_ALLFAST) {
                CL_BlasterTrail(cent, ent.origin);
                has_trail = true;
            }
            V_AddLight(ent.origin, 130, 1, 0.5f, 0.5f);
        }

skip:
        if (!has_trail)
            VectorCopy(ent.origin, cent->lerp_origin);
    }
}

static const centity_t *get_player_entity(void)
{
    const centity_t *ent = &cl_entities[cl.frame.clientNum + 1];

    if (ent->serverframe != cl.frame.number)
        return NULL;
    if (!ent->current.modelindex)
        return NULL;

    return ent;
}

static int shell_effect_hack(const centity_t *ent)
{
    int flags = 0;

    if (ent->current.effects & EF_PENT)
        flags |= RF_SHELL_RED;
    if (ent->current.effects & EF_QUAD)
        flags |= RF_SHELL_BLUE;
    if (ent->current.effects & EF_DOUBLE)
        flags |= RF_SHELL_DOUBLE;
    if (ent->current.effects & EF_HALF_DAMAGE)
        flags |= RF_SHELL_HALF_DAM;

    if (cl.csr.extended) {
        if (ent->current.effects & EF_DUALFIRE)
            flags |= RF_SHELL_LITE_GREEN;
        if (ent->current.effects & EF_COLOR_SHELL)
            flags |= ent->current.renderfx & RF_SHELL_MASK;
    }

    return flags;
}

/*
==============
CL_AddViewWeapon
==============
*/
static void CL_AddViewWeapon(void)
{
    const centity_t *ent;
    const player_state_t *ps, *ops;
    entity_t    gun;        // view model
    int         i, flags;
    bool        skip_bob;

    // allow the gun to be completely removed
    if (cl_gun->integer < 1) {
        return;
    }

    if (cl_gun->integer == 1) {
        // don't draw gun if in wide angle view
        if (cls.demo.playback && cls.demo.compat && cl.frame.ps.fov > 90) {
            return;
        }
        // don't draw gun if center handed
        if (info_hand->integer == 2) {
            return;
        }
    }

    // find states to interpolate between
    ps = CL_KEYPS;
    ops = CL_OLDKEYPS;

    memset(&gun, 0, sizeof(gun));
    gun.id = RESERVED_ENTITY_GUN;

    if (gun_model) {
        gun.model = gun_model;  // development tool
    } else {
        gun.model = cl.model_draw[ps->gunindex];
        gun.skinnum = ps->gunskin;
    }
    if (!gun.model) {
        return;
    }

    skip_bob = info_bobskip->integer != 0;

    // set up gun position
    if (skip_bob) {
        VectorCopy(cl.refdef.vieworg, gun.origin);
        VectorCopy(cl.refdef.viewangles, gun.angles);
    } else {
        for (i = 0; i < 3; i++) {
            gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] +
                            CL_KEYLERPFRAC * (ps->gunoffset[i] - ops->gunoffset[i]);
            gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle(ops->gunangles[i],
                            ps->gunangles[i], CL_KEYLERPFRAC);
        }
    }

    VectorMA(gun.origin, cl_gun_y->value, cl.v_forward, gun.origin);
    VectorMA(gun.origin, cl_gun_x->value, cl.v_right, gun.origin);
    VectorMA(gun.origin, cl_gun_z->value, cl.v_up, gun.origin);

    VectorCopy(gun.origin, gun.oldorigin);      // don't lerp at all

    if (gun_frame) {
        gun.frame = gun_frame;  // development tool
        gun.oldframe = gun_frame;   // development tool
    } else {
// KEX
        if (cl.game_api == Q2PROTO_GAME_RERELEASE) {
            if (ops->gunindex != ps->gunindex) { // just changed weapons, don't lerp from old
                cl.weapon.frame = cl.weapon.last_frame = ps->gunframe;
                cl.weapon.server_time = cl.servertime;
            } else if (cl.weapon.frame == -1 || cl.weapon.frame != ps->gunframe) {
                cl.weapon.frame = ps->gunframe;
                cl.weapon.last_frame = ops->gunframe;
                cl.weapon.server_time = cl.servertime;
            }

            const float gun_ms = 1.f / (!ps->gunrate ? 10 : ps->gunrate) * 1000.f;
            gun.backlerp = Q_clipf(1.f - ((cl.time - ((float) cl.weapon.server_time - cl.frametime.time)) / gun_ms), 0.0f, 1.f);
            gun.frame = cl.weapon.frame;
            gun.oldframe = cl.weapon.last_frame;
        } else {
// KEX
            gun.frame = ps->gunframe;
            if (gun.frame == 0) {
                gun.oldframe = 0;   // just changed weapons, don't lerp from old
            } else {
                gun.oldframe = ops->gunframe;
                gun.backlerp = 1.0f - CL_KEYLERPFRAC;
            }
        }
    }

    gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
    gun.alpha = Cvar_ClampValue(cl_gunalpha, 0.1f, 1.0f);

    ent = get_player_entity();

    // add alpha from cvar or player entity
    if (ent && gun.alpha == 1.0f)
        gun.alpha = lerp_entity_alpha(ent);

    if (gun.alpha != 1.0f)
        gun.flags |= RF_TRANSLUCENT;

    V_AddEntity(&gun);

    // add shell effect from player entity
    if (ent && (flags = shell_effect_hack(ent))) {
        gun.alpha *= 0.30f;
        gun.flags |= flags | RF_TRANSLUCENT;
        V_AddEntity(&gun);
    }

    // add muzzle flash
    if (!cl.weapon.muzzle.model)
        return;

    if (cl.time - cl.weapon.muzzle.time > 50) {
        cl.weapon.muzzle.model = 0;
        return;
    }

    gun.flags = RF_FULLBRIGHT | RF_DEPTHHACK | RF_WEAPONMODEL | RF_TRANSLUCENT;
    gun.alpha = 1.0f;
    gun.model = cl.weapon.muzzle.model;
    gun.skinnum = 0;
    VectorSet(gun.scale, cl.weapon.muzzle.scale, cl.weapon.muzzle.scale, cl.weapon.muzzle.scale);
    gun.backlerp = 0.0f;
    gun.frame = gun.oldframe = 0;
    gun.backlerp = 0.f;
    gun.frame = gun.oldframe = 0;

    vec3_t forward, right, up;
    AngleVectors(gun.angles, forward, right, up);

    VectorMA(gun.origin, cl.weapon.muzzle.offset[0], forward, gun.origin);
    VectorMA(gun.origin, cl.weapon.muzzle.offset[1], right, gun.origin);
    VectorMA(gun.origin, cl.weapon.muzzle.offset[2], up, gun.origin);

    VectorCopy(cl.refdef.viewangles, gun.angles);
    gun.angles[2] += cl.weapon.muzzle.roll;
            
    V_AddEntity(&gun);
}

static void CL_SetupFirstPersonView(void)
{
    // add kick angles
    if (cl_kickangles->integer && !info_bobskip->integer) {
        vec3_t kickangles;
        LerpAngles(CL_OLDKEYPS->kick_angles, CL_KEYPS->kick_angles, CL_KEYLERPFRAC, kickangles);
        VectorAdd(cl.refdef.viewangles, kickangles, cl.refdef.viewangles);
    }

    // add the weapon
    CL_AddViewWeapon();

    cl.thirdPersonView = false;
}

// need to interpolate bmodel positions, or third person view would be very jerky
static void CL_LerpedTrace(trace_t *tr, const vec3_t start, const vec3_t end,
                           const vec3_t mins, const vec3_t maxs, int contentmask)
{
    trace_t trace;
    const centity_t *ent;
    const mmodel_t *cmodel;
    vec3_t org, ang;

    // check against world
    CM_BoxTrace(tr, start, end, mins, maxs, cl.bsp->nodes, contentmask, cl.csr.extended);
    tr->ent = (struct edict_s *)cl_entities;
    if (tr->fraction == 0)
        return;     // blocked by the world

    // check all other solid models
    for (int i = 0; i < cl.numSolidEntities; i++) {
        ent = cl.solidEntities[i];

        // special value for bmodel
        if (ent->current.solid != PACKED_BSP)
            continue;

        cmodel = cl.model_clip[ent->current.modelindex];
        if (!cmodel)
            continue;

        LerpVector(ent->prev.origin, ent->current.origin, cl.lerpfrac, org);
        LerpAngles(ent->prev.angles, ent->current.angles, cl.lerpfrac, ang);

        CM_TransformedBoxTrace(&trace, start, end, mins, maxs, cmodel->headnode,
                               contentmask, org, ang, cl.csr.extended);

        CM_ClipEntity(tr, &trace, (struct edict_s *)ent);
    }
}

/*
===============
CL_SetupThirdPersionView
===============
*/
static void CL_SetupThirdPersionView(void)
{
    static const vec3_t mins = { -4, -4, -4 };
    static const vec3_t maxs = {  4,  4,  4 };
    vec3_t focus;
    float fscale, rscale;
    float dist, angle, range;
    trace_t trace;

    // if dead, set a nice view angle
    if (cl.frame.ps.stats[STAT_HEALTH] <= 0) {
        cl.refdef.viewangles[ROLL] = 0;
        cl.refdef.viewangles[PITCH] = 10;
    }

    VectorMA(cl.refdef.vieworg, 512, cl.v_forward, focus);
    cl.refdef.vieworg[2] += 8;

    cl.refdef.viewangles[PITCH] *= 0.5f;
    AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

    angle = DEG2RAD(cl_thirdperson_angle->value);
    range = cl_thirdperson_range->value;
    fscale = cosf(angle);
    rscale = sinf(angle);
    VectorMA(cl.refdef.vieworg, -range * fscale, cl.v_forward, cl.refdef.vieworg);
    VectorMA(cl.refdef.vieworg, -range * rscale, cl.v_right, cl.refdef.vieworg);

    CL_LerpedTrace(&trace, cl.playerEntityOrigin, cl.refdef.vieworg, mins, maxs, CONTENTS_SOLID);
    VectorCopy(trace.endpos, cl.refdef.vieworg);
    cl.thirdPersonAlpha = trace.fraction;

    VectorSubtract(focus, cl.refdef.vieworg, focus);
    dist = sqrtf(focus[0] * focus[0] + focus[1] * focus[1]);

    cl.refdef.viewangles[PITCH] = -RAD2DEG(atan2f(focus[2], dist));
    cl.refdef.viewangles[YAW] -= cl_thirdperson_angle->value;

    cl.thirdPersonView = true;
}

static void CL_FinishViewValues(void)
{
    if (cl_thirdperson->integer && get_player_entity())
        CL_SetupThirdPersionView();
    else
        CL_SetupFirstPersonView();
}

static inline float lerp_client_fov(float ofov, float nfov, float lerp)
{
    if (cls.demo.playback && !cls.demo.compat) {
        int fov = info_fov->integer;

        if (fov < 1)
            fov = 90;
        else if (fov > 160)
            fov = 160;

        if (info_uf->integer & UF_LOCALFOV)
            return fov;

        if (!(info_uf->integer & UF_PLAYERFOV)) {
            if (ofov >= 90)
                ofov = fov;
            if (nfov >= 90)
                nfov = fov;
        }
    }

    return ofov + lerp * (nfov - ofov);
}

/*
===============
CL_CalcViewValues

Sets cl.refdef view values and sound spatialization params.
Usually called from CL_AddEntities, but may be directly called from the main
loop if rendering is disabled but sound is running.
===============
*/
void CL_CalcViewValues(void)
{
    const player_state_t *ps, *ops;
    vec3_t viewoffset;
    float lerp;

    if (!cl.frame.valid) {
        return;
    }

    // find states to interpolate between
    ps = &cl.frame.ps;
    ops = &cl.oldframe.ps;

    lerp = cl.lerpfrac;

    float viewheight;

    // calculate the origin
    if (!cls.demo.playback && cl_predict->integer && !(ps->pmove.pm_flags & PMF_NO_PREDICTION)) {
        // use predicted values
        unsigned delta = cls.realtime - cl.predicted_step_time;
        float backlerp = lerp - 1.0f;

        VectorMA(cl.predicted_origin, backlerp, cl.prediction_error, cl.refdef.vieworg);

        // smooth out stair climbing
        if (delta < STEP_TIME) {
            cl.refdef.vieworg[2] -= cl.predicted_step * (STEP_TIME - delta) * (1.f / STEP_TIME);
        }
    } else {
        // just use interpolated values
        for (int i = 0; i < 3; i++) {
            cl.refdef.vieworg[i] = ops->pmove.origin[i] +
                lerp * (ps->pmove.origin[i] - ops->pmove.origin[i]);
        }
    }
    
    // Record viewheight changes
    if (cl.current_viewheight != ps->pmove.viewheight) {
        cl.prev_viewheight = cl.current_viewheight;
        cl.current_viewheight = ps->pmove.viewheight;
        cl.viewheight_change_time = cl.time;
    }

    // if not running a demo or on a locked frame, add the local angle movement
    if (cls.demo.playback) {
        if (cls.key_dest == KEY_GAME && Key_IsDown(K_SHIFT)) {
            VectorCopy(cl.viewangles, cl.refdef.viewangles);
        } else {
            LerpAngles(ops->viewangles, ps->viewangles, lerp,
                       cl.refdef.viewangles);
        }
    } else if (ps->pmove.pm_type < PM_DEAD) {
        // use predicted values
        VectorCopy(cl.predicted_angles, cl.refdef.viewangles);
    } else if (ops->pmove.pm_type < PM_DEAD && cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
        // lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        LerpAngles(cl.predicted_angles, ps->viewangles, lerp, cl.refdef.viewangles);
    } else {
        // just use interpolated values
        LerpAngles(ops->viewangles, ps->viewangles, lerp, cl.refdef.viewangles);
    }

    if (cl.csr.extended) {
        // interpolate blend colors if the last frame wasn't clear
        float blendfrac = ops->screen_blend[3] ? cl.lerpfrac : 1;
        float damageblendfrac = ops->damage_blend[3] ? cl.lerpfrac : 1;
        
        Vector4Lerp(ops->screen_blend, ps->screen_blend, blendfrac, cl.refdef.screen_blend);
        Vector4Lerp(ops->damage_blend, ps->damage_blend, damageblendfrac, cl.refdef.damage_blend);
    } else {
        Vector4Copy(ps->screen_blend, cl.refdef.screen_blend);
        Vector4Copy(ps->damage_blend, cl.refdef.damage_blend);
    }
    // Mix in screen_blend from cgame pmove
    // FIXME: Should also be interpolated?...
    if(cl.predicted_screen_blend[3] > 0) {
        float a2 = cl.refdef.screen_blend[3] + (1 - cl.refdef.screen_blend[3]) * cl.predicted_screen_blend[3]; // new total alpha
        float a3 = cl.refdef.screen_blend[3] / a2;					// fraction of color from old

        LerpVector(cl.predicted_screen_blend, cl.refdef.screen_blend, a3, cl.refdef.screen_blend);
        cl.refdef.screen_blend[3] = a2;
    }
    if (info_bobskip->integer) {
        Vector4Clear(cl.refdef.damage_blend);
    }


#if USE_FPS
    ps = &cl.keyframe.ps;
    ops = &cl.oldkeyframe.ps;

    lerp = cl.keylerpfrac;
#endif

    // interpolate field of view
    cl.fov_x = lerp_client_fov(ops->fov, ps->fov, lerp);
    cl.fov_y = V_CalcFov(cl.fov_x, 4, 3);

    LerpVector(ops->viewoffset, ps->viewoffset, lerp, viewoffset);
    if (info_bobskip->integer) {
        VectorClear(viewoffset);
    }

    AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

    VectorCopy(cl.refdef.vieworg, cl.playerEntityOrigin);
    VectorCopy(cl.refdef.viewangles, cl.playerEntityAngles);

    if (cl.playerEntityAngles[PITCH] > 180) {
        cl.playerEntityAngles[PITCH] -= 360;
    }

    cl.playerEntityAngles[PITCH] = cl.playerEntityAngles[PITCH] / 3;

    VectorAdd(cl.refdef.vieworg, viewoffset, cl.refdef.vieworg);

    // Smooth out view height over 100ms
    float viewheight_lerp = (cl.time - cl.viewheight_change_time);
    viewheight_lerp = 100 - min(viewheight_lerp, 100);
    viewheight = cl.current_viewheight + (float)(cl.prev_viewheight - cl.current_viewheight) * viewheight_lerp * 0.01f;

    cl.refdef.vieworg[2] += viewheight;

    VectorCopy(cl.refdef.vieworg, listener_origin);
    VectorCopy(cl.v_forward, listener_forward);
    VectorCopy(cl.v_right, listener_right);
    VectorCopy(cl.v_up, listener_up);
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the renderer
===============
*/
void CL_AddEntities(void)
{
    CL_CalcViewValues();
    CL_FinishViewValues();
    CL_AddPacketEntities();
    CL_AddTEnts();
    CL_AddParticles();
    CL_AddDLights();
    CL_AddLightStyles();
    CL_AddShadowLights();
    LOC_AddLocationsToScene();
}

/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntitySoundOrigin(unsigned entnum, vec3_t org)
{
    const centity_t *ent;
    const mmodel_t  *mod;
    vec3_t          mid;

    if (entnum >= cl.csr.max_edicts)
        Com_Error(ERR_DROP, "%s: bad entity", __func__);

    if (!entnum || entnum == listener_entnum) {
        // should this ever happen?
        VectorCopy(listener_origin, org);
        return;
    }

    // interpolate origin
    ent = &cl_entities[entnum];
    LerpVector(ent->prev.origin, ent->current.origin, cl.lerpfrac, org);

    // use re-releases algorithm for bmodels & beams
    if (cl.csr.extended) {
        // for BSP models, we want the nearest point from
        // the bmodel to the listener; if we're "inside"
        // the bmodel we want it full strength.
        if (ent->current.solid == PACKED_BSP) {
            mod = cl.model_clip[ent->current.modelindex];
            if (mod) {
                vec3_t absmin, absmax;
                VectorAdd(org, mod->mins, absmin);
                VectorAdd(org, mod->maxs, absmax);

                for (int i = 0; i < 3; i++)
                    org[i] = (listener_origin[i] < absmin[i]) ? absmin[i] :
                             (listener_origin[i] > absmax[i]) ? absmax[i] :
                             listener_origin[i];
            }
        } else if (ent->current.renderfx & RF_BEAM) {
            // for beams, we use the nearest point on the line
            // between the two origins
            vec3_t old_origin;
            LerpVector(ent->prev.old_origin, ent->current.old_origin, cl.lerpfrac, old_origin);

            vec3_t vec, p;
            VectorSubtract(old_origin, org, vec);
            VectorSubtract(listener_origin, org, p);

            float frac = Q_clipf(DotProduct(p, vec) / DotProduct(vec, vec), 0.0f, 1.0f);
            VectorMA(org, frac, vec, org);
        }
    } else {
        // offset the origin for BSP models
        if (ent->current.solid == PACKED_BSP) {
            mod = cl.model_clip[ent->current.modelindex];
            if (mod) {
                VectorAvg(mod->mins, mod->maxs, mid);
                VectorAdd(org, mid, org);
            }
        }
    }
}
