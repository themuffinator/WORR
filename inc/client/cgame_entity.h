/*
Copyright (C) 2026

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

#pragma once

#include "client/cgame_entity_ext.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#endif
#include "client/client_state.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include "common/cmd.h"
#include "common/cmodel.h"
#include "common/error.h"
#include "common/zone.h"
#include "renderer/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

// renderer.h maps these to renderer_export_t macros when building the engine.
// Preserve the macro values, but allow struct field names here.
#ifdef R_RegisterModel
#pragma push_macro("R_RegisterModel")
#undef R_RegisterModel
#define CGAME_ENTITY_POP_R_REGISTERMODEL 1
#endif
#ifdef R_RegisterImage
#pragma push_macro("R_RegisterImage")
#undef R_RegisterImage
#define CGAME_ENTITY_POP_R_REGISTERIMAGE 1
#endif
#ifdef R_RegisterSkin
#pragma push_macro("R_RegisterSkin")
#undef R_RegisterSkin
#define CGAME_ENTITY_POP_R_REGISTERSKIN 1
#endif
#ifdef R_SupportsPerPixelLighting
#pragma push_macro("R_SupportsPerPixelLighting")
#undef R_SupportsPerPixelLighting
#define CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING 1
#endif

typedef struct cgame_entity_import_s {
    int api_version;

    client_state_t *cl;
    client_static_t *cls;
    centity_t *cl_entities;
    tent_params_t *te;
    const mz_params_t *mz;
    const csurface_t *null_surface;
    int *gun_frame;
    qhandle_t *gun_model;
    cmdbuf_t *cmd_buffer;

    void (*Com_Printf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_DPrintf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_WPrintf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_LPrintf)(print_type_t type, const char *fmt, ...) q_printf(2, 3);
    void (*Com_Error)(error_type_t code, const char *fmt, ...) q_printf(2, 3);

    void (*Com_PlayerToEntityState)(const player_state_t *ps, entity_state_t *ent);
    uint32_t (*Com_BlockChecksum)(const void *buffer, size_t length);
    uint32_t (*Com_SlowRand)(void);
    xgenerator_t Com_Color_g;

    cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags);
    cvar_t *(*Cvar_FindVar)(const char *name);
    void (*Cvar_SetByVar)(cvar_t *var, const char *value, from_t from);
    void (*Cvar_SetValue)(cvar_t *var, float value, from_t from);
    void (*Cvar_SetInteger)(cvar_t *var, int value, from_t from);
    void (*Cvar_Reset)(cvar_t *var);
    int (*Cvar_ClampInteger)(cvar_t *var, int min, int max);
    float (*Cvar_ClampValue)(cvar_t *var, float min, float max);
    cvar_t *fs_game;
    cvar_t *sv_paused;

    void (*Cbuf_ExecuteDeferred)(cmdbuf_t *buf);
    void (*AddCommandString)(const char *text);
    void (*Cmd_ExecTrigger)(const char *string);

    void (*IN_Activate)(void);
    void (*Con_Close)(bool force);
    void (*SCR_EndLoadingPlaque)(void);
    void (*SCR_LagClear)(void);
    void (*SCR_SetCrosshairColor)(void);
    bool (*SCR_ParseColor)(const char *s, color_t *color);

    int (*Key_IsDown)(int key);

    void (*CL_CheckForPause)(void);
    void (*CL_UpdateFrameTimes)(void);
    void (*CL_EmitDemoFrame)(void);
    void (*CL_FirstDemoFrame)(void);
    void (*CL_GTV_Resume)(void);
    void (*CL_GTV_EmitFrame)(void);
    void (*CL_AddHitMarker)(int damage);
    void (*CL_LoadClientinfo)(clientinfo_t *ci, const char *s);

    void (*CL_Trace)(trace_t *tr, const vec3_t start, const vec3_t end,
                     const vec3_t mins, const vec3_t maxs,
                     const struct edict_s *passent, contents_t contentmask);
    contents_t (*CL_PointContents)(const vec3_t point);
    void (*Pmove)(pmove_t *pmove);

    void *(*Z_Malloc)(size_t size);
    void (*Z_Freep)(void *ptr);

    int (*FS_LoadFileEx)(const char *path, void **buffer, unsigned flags, memtag_t tag);
    void (*FS_FreeFile)(void *buffer);

    void (*CM_BoxTrace)(trace_t *tr, const vec3_t start, const vec3_t end,
                        const vec3_t mins, const vec3_t maxs,
                        const mnode_t *headnode, int mask, bool extended);
    void (*CM_TransformedBoxTrace)(trace_t *tr, const vec3_t start, const vec3_t end,
                                   const vec3_t mins, const vec3_t maxs,
                                   const mnode_t *headnode, int mask,
                                   const vec3_t origin, const vec3_t angles, bool extended);
    void (*CM_ClipEntity)(trace_t *dst, const trace_t *src, struct edict_s *ent);
    int (*BSP_LoadMaterials)(bsp_t *bsp);

    void (*Q2Proto_UnpackSolid)(uint32_t solid, vec3_t mins, vec3_t maxs);

    qhandle_t (*R_RegisterModel)(const char *name);
    qhandle_t (*R_RegisterImage)(const char *name, imagetype_t type, imageflags_t flags);
    qhandle_t (*R_RegisterSkin)(const char *name);
    bool (*R_SupportsPerPixelLighting)(void);
    float (*V_CalcFov)(float fov_x, float width, float height);

    void (*V_AddEntity)(const entity_t *ent);
    bool (*V_AddParticle)(const particle_t *p);
    void (*V_AddLight)(const vec3_t org, float intensity, float r, float g, float b);
    void (*V_AddLightEx)(cl_shadow_light_t *light);
    void (*V_AddLightStyle)(int style, float value);

    qhandle_t (*S_RegisterSound)(const char *name);
    void (*S_StartSound)(const vec3_t origin, int entnum, int entchannel,
                         qhandle_t sfx, float volume, float attenuation, float timeofs);

    void (*LOC_AddLocationsToScene)(void);
} cgame_entity_import_t;

typedef struct cgame_entity_export_s {
    int api_version;

    void (*InitEffects)(void);
    void (*ClearEffects)(void);
    void (*InitTEnts)(void);
    void (*ClearTEnts)(void);
    void (*InitBrightskins)(void);
    void (*MigratePlayerCvars)(void);
    void (*RegisterForcedModels)(void);
    void (*RegisterTEntSounds)(void);
    void (*RegisterTEntModels)(void);
    void (*SetLightStyle)(int index, const char *s);

    void (*DeltaFrame)(void);
    void (*PredictMovement)(void);
    void (*CheckPredictionError)(void);
    void (*CalcViewValues)(void);
    void (*AddEntities)(void);

    void (*GetEntitySoundOrigin)(unsigned entnum, vec3_t org);

    void (*ParseTempEntity)(void);
    void (*ParseMuzzleFlash)(void);
    void (*ParseMuzzleFlash2)(void);
    void (*AddHelpPath)(const vec3_t origin, const vec3_t dir, bool first);

#if USE_DEBUG
    void (*CheckEntityPresent)(int entnum, const char *what);
#endif
} cgame_entity_export_t;

const cgame_entity_export_t *CG_GetEntityAPI(void);

#ifdef CGAME_ENTITY_POP_R_REGISTERMODEL
#pragma pop_macro("R_RegisterModel")
#undef CGAME_ENTITY_POP_R_REGISTERMODEL
#endif
#ifdef CGAME_ENTITY_POP_R_REGISTERIMAGE
#pragma pop_macro("R_RegisterImage")
#undef CGAME_ENTITY_POP_R_REGISTERIMAGE
#endif
#ifdef CGAME_ENTITY_POP_R_REGISTERSKIN
#pragma pop_macro("R_RegisterSkin")
#undef CGAME_ENTITY_POP_R_REGISTERSKIN
#endif
#ifdef CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING
#pragma pop_macro("R_SupportsPerPixelLighting")
#undef CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING
#endif

#ifdef __cplusplus
}
#endif
