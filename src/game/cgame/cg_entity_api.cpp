// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "cg_entity_local.h"

const cgame_entity_import_t *cgei = nullptr;
cvar_t *cg_sv_paused = nullptr;

cvar_t *cl_gun = nullptr;
cvar_t *cl_gunalpha = nullptr;
cvar_t *cl_gunfov = nullptr;
cvar_t *cl_gun_x = nullptr;
cvar_t *cl_gun_y = nullptr;
cvar_t *cl_gun_z = nullptr;
cvar_t *cl_footsteps = nullptr;
cvar_t *cl_predict = nullptr;
cvar_t *cl_kickangles = nullptr;
cvar_t *cl_rollhack = nullptr;
cvar_t *cl_noglow = nullptr;
cvar_t *cl_nobob = nullptr;
cvar_t *cl_nolerp = nullptr;
cvar_t *cl_colorize_items = nullptr;
cvar_t *cl_thirdperson = nullptr;
cvar_t *cl_thirdperson_angle = nullptr;
cvar_t *cl_thirdperson_range = nullptr;
cvar_t *cl_disable_particles = nullptr;
cvar_t *cl_disable_explosions = nullptr;
cvar_t *cl_dlight_hacks = nullptr;
cvar_t *cl_smooth_explosions = nullptr;
cvar_t *cl_gibs = nullptr;
cvar_t *cl_flares = nullptr;
cvar_t *cl_force_enemy_model = nullptr;
cvar_t *cl_force_team_model = nullptr;
cvar_t *cl_enemy_outline = nullptr;
cvar_t *cl_enemy_outline_self = nullptr;
cvar_t *cl_enemy_rimlight = nullptr;
cvar_t *cl_enemy_rimlight_self = nullptr;
cvar_t *cl_player_outline_enemy = nullptr;
cvar_t *cl_player_outline_team = nullptr;
cvar_t *cl_player_rimlight_enemy = nullptr;
cvar_t *cl_player_rimlight_team = nullptr;
cvar_t *cl_player_rimlight_shell = nullptr;
cvar_t *cl_beginmapcmd = nullptr;
cvar_t *info_hand = nullptr;
cvar_t *info_fov = nullptr;
cvar_t *info_uf = nullptr;
cvar_t *info_bobskip = nullptr;

static bool cg_entity_cvars_initialized = false;

void CG_Entity_InitCvars(void)
{
    if (!cgei || cg_entity_cvars_initialized)
        return;

    cl_gun = Cvar_Get("cl_gun", "1", 0);
    cl_gunalpha = Cvar_Get("cl_gunalpha", "1", 0);
    cl_gunfov = Cvar_Get("cl_gunfov", "90", 0);
    cl_gun_x = Cvar_Get("cl_gun_x", "0", 0);
    cl_gun_y = Cvar_Get("cl_gun_y", "0", 0);
    cl_gun_z = Cvar_Get("cl_gun_z", "0", 0);
    cl_footsteps = Cvar_Get("cl_footsteps", "1", 0);
    cl_predict = Cvar_Get("cl_predict", "1", 0);
    cl_kickangles = Cvar_Get("cl_kickangles", "1", CVAR_CHEAT);
    cl_rollhack = Cvar_Get("cl_rollhack", "1", 0);
    cl_noglow = Cvar_Get("cl_noglow", "0", 0);
    cl_nobob = Cvar_Get("cl_nobob", "0", 0);
    cl_nolerp = Cvar_Get("cl_nolerp", "0", 0);
    cl_colorize_items = Cvar_Get("cl_colorize_items", "0", CVAR_ARCHIVE);
    cl_player_outline_enemy = Cvar_Get("cl_player_outline_enemy", "0", CVAR_ARCHIVE);
    cl_player_outline_team = Cvar_Get("cl_player_outline_team", "0", CVAR_ARCHIVE);
    cl_player_rimlight_enemy = Cvar_Get("cl_player_rimlight_enemy", "0", CVAR_ARCHIVE);
    cl_player_rimlight_team = Cvar_Get("cl_player_rimlight_team", "0", CVAR_ARCHIVE);
    cl_player_rimlight_shell = Cvar_Get("cl_player_rimlight_shell", "1", CVAR_ARCHIVE);
    cl_force_enemy_model = Cvar_Get("cl_force_enemy_model", "", CVAR_ARCHIVE);
    cl_force_team_model = Cvar_Get("cl_force_team_model", "", CVAR_ARCHIVE);
    cl_enemy_outline = Cvar_Get("cl_enemy_outline", "0", CVAR_ARCHIVE);
    cl_enemy_outline_self = Cvar_Get("cl_enemy_outline_self", "0", CVAR_ARCHIVE);
    cl_enemy_rimlight = Cvar_Get("cl_enemy_rimlight", "0", CVAR_ARCHIVE);
    cl_enemy_rimlight_self = Cvar_Get("cl_enemy_rimlight_self", "0", CVAR_ARCHIVE);
    cl_thirdperson = Cvar_Get("cl_thirdperson", "0", CVAR_CHEAT);
    cl_thirdperson_angle = Cvar_Get("cl_thirdperson_angle", "0", 0);
    cl_thirdperson_range = Cvar_Get("cl_thirdperson_range", "60", 0);
    cl_disable_particles = Cvar_Get("cl_disable_particles", "0", 0);
    cl_disable_explosions = Cvar_Get("cl_disable_explosions", "0", 0);
    cl_dlight_hacks = Cvar_Get("cl_dlight_hacks", "0", 0);

    // Ensure sv_paused is valid even if the import pointer is not yet initialized.
    cg_sv_paused = Cvar_Get("sv_paused", "0", CVAR_ROM);
    cl_smooth_explosions = Cvar_Get("cl_smooth_explosions", "1", 0);
    cl_gibs = Cvar_Get("cl_gibs", "1", 0);
    cl_flares = Cvar_Get("cl_flares", "1", 0);
    cl_beginmapcmd = Cvar_Get("cl_beginmapcmd", "", 0);
    info_hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
    info_fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
    info_uf = Cvar_Get("uf", "", CVAR_USERINFO);
    info_bobskip = Cvar_Get("bobskip", "0", CVAR_USERINFO | CVAR_ARCHIVE);
    cl_shadowlights = Cvar_Get("cl_shadowlights", "1", 0);

    cg_entity_cvars_initialized = true;
}

void CG_Entity_SetImport(const cgame_entity_import_t *import)
{
    cgei = import;
    cg_entity_cvars_initialized = false;
    CG_Entity_InitCvars();
}

void CL_InitEffects(void);
void CL_ClearEffects(void);
void CL_InitTEnts(void);
void CL_ClearTEnts(void);
void CL_InitBrightskins(void);
void CL_MigratePlayerCvars(void);
void CL_RegisterForcedModels(void);
void CL_RegisterTEntSounds(void);
void CL_RegisterTEntModels(void);
void CL_SetLightStyle(int index, const char *s);
void CL_DeltaFrame(void);
void CL_CalcViewValues(void);
void CL_AddEntities(void);
void CL_GetEntitySoundOrigin(unsigned entnum, vec3_t org);
void CL_ParseTEnt(void);
void CL_MuzzleFlash(void);
void CL_MuzzleFlash2(void);
void CL_AddHelpPath(const vec3_t origin, const vec3_t dir, bool first);
#if USE_DEBUG
void CL_CheckEntityPresent(int entnum, const char *what);
#endif

static cgame_entity_export_t cg_entity_exports = {
    .api_version = CGAME_ENTITY_API_VERSION,

    .InitEffects = CL_InitEffects,
    .ClearEffects = CL_ClearEffects,
    .InitTEnts = CL_InitTEnts,
    .ClearTEnts = CL_ClearTEnts,
    .InitBrightskins = CL_InitBrightskins,
    .MigratePlayerCvars = CL_MigratePlayerCvars,
    .RegisterForcedModels = CL_RegisterForcedModels,
    .RegisterTEntSounds = CL_RegisterTEntSounds,
    .RegisterTEntModels = CL_RegisterTEntModels,
    .SetLightStyle = CL_SetLightStyle,

    .DeltaFrame = CL_DeltaFrame,
    .CalcViewValues = CL_CalcViewValues,
    .AddEntities = CL_AddEntities,

    .GetEntitySoundOrigin = CL_GetEntitySoundOrigin,

    .ParseTempEntity = CL_ParseTEnt,
    .ParseMuzzleFlash = CL_MuzzleFlash,
    .ParseMuzzleFlash2 = CL_MuzzleFlash2,
    .AddHelpPath = CL_AddHelpPath,

#if USE_DEBUG
    .CheckEntityPresent = CL_CheckEntityPresent,
#endif
};

extern "C" const cgame_entity_export_t *CG_GetEntityAPI(void)
{
    if (!cgei)
        return nullptr;

    return &cg_entity_exports;
}
