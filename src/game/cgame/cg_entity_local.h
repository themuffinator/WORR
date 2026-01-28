// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "client/cgame_entity.h"
#include "common/zone.h"
#include "cg_client_defs.h"

extern const cgame_entity_import_t *cgei;
extern cvar_t *cg_sv_paused;

void CG_Entity_SetImport(const cgame_entity_import_t *import);
void CG_Entity_InitCvars(void);

extern cvar_t *cl_gun;
extern cvar_t *cl_gunalpha;
extern cvar_t *cl_gunfov;
extern cvar_t *cl_gun_x;
extern cvar_t *cl_gun_y;
extern cvar_t *cl_gun_z;
extern cvar_t *cl_footsteps;
extern cvar_t *cl_predict;
extern cvar_t *cl_kickangles;
extern cvar_t *cl_rollhack;
extern cvar_t *cl_noglow;
extern cvar_t *cl_nobob;
extern cvar_t *cl_nolerp;
extern cvar_t *cl_colorize_items;
extern cvar_t *cl_thirdperson;
extern cvar_t *cl_thirdperson_angle;
extern cvar_t *cl_thirdperson_range;
extern cvar_t *cl_disable_particles;
extern cvar_t *cl_disable_explosions;
extern cvar_t *cl_dlight_hacks;
extern cvar_t *cl_smooth_explosions;
extern cvar_t *cl_gibs;
extern cvar_t *cl_flares;
extern cvar_t *cl_force_enemy_model;
extern cvar_t *cl_force_team_model;
extern cvar_t *cl_enemy_outline;
extern cvar_t *cl_enemy_outline_self;
extern cvar_t *cl_enemy_rimlight;
extern cvar_t *cl_enemy_rimlight_self;
extern cvar_t *cl_player_outline_enemy;
extern cvar_t *cl_player_outline_team;
extern cvar_t *cl_player_rimlight_enemy;
extern cvar_t *cl_player_rimlight_team;
extern cvar_t *cl_player_rimlight_shell;
extern cvar_t *cl_beginmapcmd;
extern cvar_t *info_hand;
extern cvar_t *info_fov;
extern cvar_t *info_uf;
extern cvar_t *info_bobskip;
extern cvar_t *cl_shadowlights;

#define cl (*cgei->cl)
#define cls (*cgei->cls)
#define cl_entities (cgei->cl_entities)
#define te (*cgei->te)
#define mz (*cgei->mz)
#define gun_frame (*cgei->gun_frame)
#define gun_model (*cgei->gun_model)
#define cmd_buffer (*cgei->cmd_buffer)

// variable server FPS
#define CL_FRAMETIME    cl.frametime.time
#define CL_1_FRAMETIME  cl.frametime_inv
#define CL_FRAMEDIV     cl.frametime.div
#if USE_FPS
#define CL_FRAMESYNC    !(cl.frame.number % cl.frametime.div)
#define CL_KEYPS        (&cl.keyframe.ps)
#define CL_OLDKEYPS     (&cl.oldkeyframe.ps)
#define CL_KEYLERPFRAC  cl.keylerpfrac
#else
#define CL_FRAMESYNC    1
#define CL_KEYPS        (&cl.frame.ps)
#define CL_OLDKEYPS     (&cl.oldframe.ps)
#define CL_KEYLERPFRAC  cl.lerpfrac
#endif

static inline void CL_AdvanceValue(float *val, float target, float speed)
{
    if (*val < target) {
        *val += speed * cls.frametime;
        if (*val > target)
            *val = target;
    } else if (*val > target) {
        *val -= speed * cls.frametime;
        if (*val < target)
            *val = target;
    }
}

#undef Com_Printf
#undef Com_DPrintf
#undef Com_WPrintf
#define Com_Printf cgei->Com_Printf
#define Com_DPrintf cgei->Com_DPrintf
#define Com_WPrintf cgei->Com_WPrintf
#define Com_LPrintf cgei->Com_LPrintf
#define Com_Error cgei->Com_Error

#define Com_PlayerToEntityState cgei->Com_PlayerToEntityState
#define Com_BlockChecksum cgei->Com_BlockChecksum
#define Com_SlowRand cgei->Com_SlowRand
#define Com_Color_g cgei->Com_Color_g

#define Cvar_Get cgei->Cvar_Get
#define Cvar_FindVar cgei->Cvar_FindVar
#define Cvar_SetByVar cgei->Cvar_SetByVar
#define Cvar_SetValue cgei->Cvar_SetValue
#define Cvar_SetInteger cgei->Cvar_SetInteger
#undef Cvar_Reset
#define Cvar_Reset cgei->Cvar_Reset
#define Cvar_ClampInteger cgei->Cvar_ClampInteger
#define Cvar_ClampValue cgei->Cvar_ClampValue
static inline cvar_t *CG_SvPausedVar(void)
{
    return (cgei && cgei->sv_paused) ? cgei->sv_paused : cg_sv_paused;
}

#define fs_game (cgei->fs_game)
#define sv_paused (cgei->sv_paused)

#define Cbuf_ExecuteDeferred cgei->Cbuf_ExecuteDeferred
#define AddCommandString cgei->AddCommandString
#define Cmd_ExecTrigger cgei->Cmd_ExecTrigger

#define IN_Activate cgei->IN_Activate
#define Con_Close cgei->Con_Close
#define SCR_EndLoadingPlaque cgei->SCR_EndLoadingPlaque
#define SCR_LagClear cgei->SCR_LagClear
#define SCR_SetCrosshairColor cgei->SCR_SetCrosshairColor
#define SCR_ParseColor cgei->SCR_ParseColor

#define Key_IsDown cgei->Key_IsDown

#define CL_CheckForPause cgei->CL_CheckForPause
#define CL_UpdateFrameTimes cgei->CL_UpdateFrameTimes
#define CL_EmitDemoFrame cgei->CL_EmitDemoFrame
#define CL_FirstDemoFrame cgei->CL_FirstDemoFrame
#define CL_GTV_Resume cgei->CL_GTV_Resume
#define CL_GTV_EmitFrame cgei->CL_GTV_EmitFrame
#define CL_AddHitMarker cgei->CL_AddHitMarker
#define CL_LoadClientinfo cgei->CL_LoadClientinfo

#define CL_Trace cgei->CL_Trace
#define CL_PointContents cgei->CL_PointContents

#define Z_Malloc cgei->Z_Malloc
#define Z_Freep cgei->Z_Freep
#define FS_LoadFile(path, buf) cgei->FS_LoadFileEx(path, buf, 0, TAG_FILESYSTEM)
#define FS_FreeFile cgei->FS_FreeFile

#define CM_BoxTrace cgei->CM_BoxTrace
#define CM_TransformedBoxTrace cgei->CM_TransformedBoxTrace
#define CM_ClipEntity cgei->CM_ClipEntity
#define BSP_LoadMaterials cgei->BSP_LoadMaterials

#define R_RegisterModel cgei->R_RegisterModel
#define R_RegisterImage cgei->R_RegisterImage
#define R_RegisterSkin cgei->R_RegisterSkin
#define R_SupportsPerPixelLighting cgei->R_SupportsPerPixelLighting
#define V_CalcFov cgei->V_CalcFov

#define V_AddEntity cgei->V_AddEntity
#define V_AddParticle cgei->V_AddParticle
#define V_AddLight cgei->V_AddLight
#define V_AddLightEx cgei->V_AddLightEx
#define V_AddLightStyle cgei->V_AddLightStyle

#define S_RegisterSound cgei->S_RegisterSound
#define S_StartSound cgei->S_StartSound

#define LOC_AddLocationsToScene cgei->LOC_AddLocationsToScene

#undef EXEC_TRIGGER
#define EXEC_TRIGGER(var) \
    do { \
        if ((var)->string[0]) { \
            AddCommandString((var)->string); \
            AddCommandString("\n"); \
        } \
    } while (0)
