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

// client.h -- primary header for client

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "shared/shared.h"
#include "shared/list.h"
#include "shared/game.h"

#include "common/bsp.h"
#include "common/cmd.h"
#include "common/cmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/field.h"
#include "common/files.h"
#include "common/math.h"
#include "common/msg.h"
#include "common/net/chan.h"
#include "common/net/net.h"
#include "common/pmove.h"
#include "common/prompt.h"
#include "common/protocol.h"
#include "common/q2proto_shared.h"
#include "common/sizebuf.h"
#include "common/zone.h"

#include "renderer/renderer.h"
#include "server/server.h"
#include "system/system.h"

#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/sound/sound.h"
#include "client/ui.h"
#include "client/video.h"
#include "client/cgame_entity.h"

#include "q2proto/q2proto.h"

#if USE_ZLIB
#include <zlib.h>
#endif

//=============================================================================

typedef struct font_s font_t;

#include "client/client_state.h"

extern centity_t    cl_entities[MAX_EDICTS];
extern client_state_t   cl;
extern client_static_t      cls;

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

#define FOR_EACH_DLQ(q) \
    LIST_FOR_EACH(dlqueue_t, q, &cls.download.queue, entry)
#define FOR_EACH_DLQ_SAFE(q, n) \
    LIST_FOR_EACH_SAFE(dlqueue_t, q, n, &cls.download.queue, entry)

// performance measurement
#define C_FPS       cls.measure.fps[0]
#define R_FPS       cls.measure.fps[1]
#define C_MPS       cls.measure.fps[2]
#define C_PPS       cls.measure.fps[3]
#define C_FRAMES    cls.measure.frames[0]
#define R_FRAMES    cls.measure.frames[1]
#define M_FRAMES    cls.measure.frames[2]
#define P_FRAMES    cls.measure.frames[3]

extern cmdbuf_t     cl_cmdbuf;
extern char         cl_cmdbuf_text[MAX_STRING_CHARS];

//=============================================================================

#define NOPART_GRENADE_EXPLOSION    BIT(0)
#define NOPART_GRENADE_TRAIL        BIT(1)
#define NOPART_ROCKET_EXPLOSION     BIT(2)
#define NOPART_ROCKET_TRAIL         BIT(3)
#define NOPART_BLOOD                BIT(4)
#define NOPART_BLASTER_TRAIL        BIT(5)

#define NOEXP_GRENADE               BIT(0)
#define NOEXP_ROCKET                BIT(1)

#define DLHACK_ROCKET_COLOR         BIT(0)
#define DLHACK_SMALLER_EXPLOSION    BIT(1)
#define DLHACK_NO_MUZZLEFLASH       BIT(2)

//
// cvars
//
extern cvar_t   *cl_gun;
extern cvar_t   *cl_gunalpha;
extern cvar_t   *cl_gunfov;
extern cvar_t   *cl_gun_x;
extern cvar_t   *cl_gun_y;
extern cvar_t   *cl_gun_z;
extern cvar_t   *cl_weapon_bar;
extern cvar_t   *cl_predict;
extern cvar_t   *cl_footsteps;
extern cvar_t   *cl_noskins;
extern cvar_t   *cl_kickangles;
extern cvar_t   *cl_rollhack;
extern cvar_t   *cl_noglow;
extern cvar_t   *cl_nobob;
extern cvar_t   *cl_nolerp;
extern cvar_t   *cl_colorize_items;
extern cvar_t   *cl_shadowlights;
extern cvar_t   *cl_player_outline_enemy;
extern cvar_t   *cl_player_outline_team;
extern cvar_t   *cl_player_outline_width;
extern cvar_t   *cl_player_rimlight_enemy;
extern cvar_t   *cl_player_rimlight_team;
extern cvar_t   *cl_player_rimlight_shell;
extern cvar_t   *cl_force_enemy_model;
extern cvar_t   *cl_force_team_model;
// legacy outline/rimlight cvars (deprecated)
extern cvar_t   *cl_enemy_outline;
extern cvar_t   *cl_enemy_outline_self;
extern cvar_t   *cl_enemy_rimlight;
extern cvar_t   *cl_enemy_rimlight_self;

#if USE_DEBUG
#define SHOWNET(level, ...) \
    do { if (cl_shownet->integer >= level) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__); } while (0)
#define SHOWCLAMP(level, ...) \
    do { if (cl_showclamp->integer >= level) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__); } while (0)
#define SHOWMISS(...) \
    do { if (cl_showmiss->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__); } while (0)
extern cvar_t   *cl_shownet;
extern cvar_t   *cl_showmiss;
extern cvar_t   *cl_showclamp;
#else
#define SHOWNET(...)
#define SHOWCLAMP(...)
#define SHOWMISS(...)
#endif

extern cvar_t   *cl_vwep;

extern cvar_t   *cl_disable_particles;
extern cvar_t   *cl_disable_explosions;
extern cvar_t   *cl_dlight_hacks;
extern cvar_t   *cl_smooth_explosions;

extern cvar_t   *cl_cgame_notify;
extern cvar_t   *cl_chat_notify;
extern cvar_t   *cl_chat_sound;
extern cvar_t   *cl_chat_filter;

extern cvar_t   *cl_disconnectcmd;
extern cvar_t   *cl_changemapcmd;
extern cvar_t   *cl_beginmapcmd;

extern cvar_t   *cl_gibs;
extern cvar_t   *cl_flares;

extern cvar_t   *cl_thirdperson;
extern cvar_t   *cl_thirdperson_angle;
extern cvar_t   *cl_thirdperson_range;

extern cvar_t   *cl_async;

//
// userinfo
//
extern cvar_t   *info_password;
extern cvar_t   *info_spectator;
extern cvar_t   *info_name;
extern cvar_t   *info_dogtag;
extern cvar_t   *info_skin;
extern cvar_t   *info_rate;
extern cvar_t   *info_fov;
extern cvar_t   *info_msg;
extern cvar_t   *info_hand;
extern cvar_t   *info_gender;
extern cvar_t   *info_uf;
extern cvar_t   *info_bobskip;

//=============================================================================

#ifdef __cplusplus
#ifndef restrict
#define restrict __restrict
#define Q_RESTRICT_UNDEF_CLIENT_H
#endif
#endif

static inline void CL_AdvanceValue(float *restrict val, float target, float speed)
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

#ifdef __cplusplus
#ifdef Q_RESTRICT_UNDEF_CLIENT_H
#undef restrict
#undef Q_RESTRICT_UNDEF_CLIENT_H
#endif
#endif

//
// main.c
//

void CL_Init(void);
void CL_Quit_f(void);
void CL_Disconnect(error_type_t type);
void CL_UpdateRecordingSetting(void);
void CL_Begin(void);
void CL_CheckForResend(void);
void CL_ClearState(void);
void CL_RestartFilesystem(bool total);
void CL_RestartRenderer(bool total);
void CL_ClientCommand(const char *string);
void CL_SendRcon(const netadr_t *adr, const char *pass, const char *cmd);
const char *CL_Server_g(const char *partial, int argnum, int state);
void CL_CheckForPause(void);
void CL_UpdateFrameTimes(void);
void CL_AddHitMarker(int damage);
bool CL_CheckForIgnore(const char *s);
void CL_LoadFilterList(string_entry_t **list, const char *name, const char *comments, size_t maxlen);

void cl_timeout_changed(cvar_t *self);

//
// precache.c
//

typedef enum {
    LOAD_NONE,
    LOAD_MAP,
    LOAD_MODELS,
    LOAD_IMAGES,
    LOAD_CLIENTS,
    LOAD_SOUNDS
} load_state_t;

void CL_ParsePlayerSkin(char *name, char *model, char *skin, char *dogtag, bool parse_dogtag, const char *s);
void CL_LoadClientinfo(clientinfo_t *ci, const char *s);
void CL_LoadState(load_state_t state);
void CL_RegisterSounds(void);
void CL_RegisterBspModels(void);
void CL_RegisterVWepModels(void);
void CL_PrepRenderer(void);
void CL_UpdateConfigstring(int index);

//
// download.c
//
int CL_QueueDownload(const char *path, dltype_t type);
bool CL_IgnoreDownload(const char *path);
void CL_FinishDownload(dlqueue_t *q);
void CL_CleanupDownloads(void);
void CL_LoadDownloadIgnores(void);
void CL_HandleDownload(const byte *data, int size, int percent);
bool CL_CheckDownloadExtension(const char *ext);
void CL_StartNextDownload(void);
void CL_RequestNextDownload(void);
void CL_ResetPrecacheCheck(void);
void CL_InitDownloads(void);


//
// input.c
//
void IN_Init(void);
void IN_Shutdown(void);
void IN_Frame(void);
void IN_Activate(void);

void CL_RegisterInput(void);
void CL_UpdateCmd(int msec);
void CL_FinalizeCmd(void);
void CL_SendCmd(void);


//
// parse.c
//

#define CL_ES_EXTENDED_MASK \
    (MSG_ES_LONGSOLID | MSG_ES_UMASK | MSG_ES_BEAMORIGIN | MSG_ES_SHORTANGLES | MSG_ES_EXTENSIONS)

#define CL_ES_EXTENDED_MASK_2 (CL_ES_EXTENDED_MASK | MSG_ES_EXTENSIONS_2)
#define CL_PS_EXTENDED_MASK_2 (MSG_PS_EXTENSIONS | MSG_PS_EXTENSIONS_2 | MSG_PS_MOREBITS)

extern tent_params_t    te;
extern mz_params_t      mz;
extern q2proto_sound_t  snd;

void CL_ParseServerMessage(void);
bool CL_SeekDemoMessage(void);


//
// entities.c
//

#define EF_TRAIL_MASK   (EF_ROCKET | EF_BLASTER | EF_HYPERBLASTER | EF_GIB | EF_GRENADE | \
                         EF_FLIES | EF_BFG | EF_TRAP | EF_FLAG1 | EF_FLAG2 | EF_TAGTRAIL | \
                         EF_TRACKERTRAIL | EF_TRACKER | EF_GREENGIB | EF_IONRIPPER | \
                         EF_BLUEHYPERBLASTER | EF_PLASMA)

#define IS_TRACKER(effects) \
    (((effects) & (EF_TRACKERTRAIL | EF_TRACKER)) == EF_TRACKERTRAIL)

void CL_DeltaFrame(void);
void CL_AddEntities(void);
void CL_CalcViewValues(void);

#if USE_DEBUG
void CL_CheckEntityPresent(int entnum, const char *what);
#endif

// the sound code makes callbacks to the client for entity position
// information, so entities can be dynamically re-spatialized
void CL_GetEntitySoundOrigin(unsigned entnum, vec3_t org);


//
// view.c
//
extern int          gun_frame;
extern qhandle_t    gun_model;

void V_Init(void);
void V_Shutdown(void);
void V_RenderView(void);
void V_AddEntity(const entity_t *ent);
bool V_AddParticle(const particle_t *p);
void V_AddLightEx(cl_shadow_light_t *light);
void V_AddLight(const vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle(int style, float value);
void CL_UpdateBlendSetting(void);
void V_FogParamsChanged(unsigned bits, unsigned color_bits, unsigned hf_start_color_bits, unsigned hf_end_color_bits, const cl_fog_params_t *params, int time);

float CL_Wheel_TimeScale(void);

//
// tent.c
//

typedef struct cl_sustain_s {
    int     id;
    int     type;
    int     endtime;
    int     nextthink;
    vec3_t  org;
    vec3_t  dir;
    int     color;
    int     count;
    int     magnitude;
    void    (*think)(struct cl_sustain_s *self);
} cl_sustain_t;

typedef enum {
    MFLASH_MACHN,
    MFLASH_SHOTG2,
    MFLASH_SHOTG,
    MFLASH_ROCKET,
    MFLASH_RAIL,
    MFLASH_LAUNCH,
    MFLASH_ETF_RIFLE,
    MFLASH_DIST,
    MFLASH_BOOMER,
    MFLASH_BLAST, // 0 = orange, 1 = blue, 2 = green
    MFLASH_BFG,
    MFLASH_BEAMER,

    MFLASH_TOTAL
} cl_muzzlefx_t;

void CL_AddWeaponMuzzleFX(cl_muzzlefx_t fx, const vec3_t offset, float scale);
void CL_AddMuzzleFX(const vec3_t origin, const vec3_t angles, cl_muzzlefx_t fx, int skin, float scale);
void CL_AddHelpPath(const vec3_t origin, const vec3_t dir, bool first);

void CL_SmokeAndFlash(const vec3_t origin);
void CL_DrawBeam(const vec3_t org, const vec3_t end, float model_length, qhandle_t model);
void CL_PlayFootstepSfx(int step_id, int entnum, float volume, float attenuation);

void CL_RegisterTEntSounds(void);
void CL_RegisterTEntModels(void);
void CL_ParseTEnt(void);
void CL_AddTEnts(void);
void CL_ClearTEnts(void);
void CL_InitBrightskins(void);
void CL_MigratePlayerCvars(void);
void CL_RegisterForcedModels(void);
void CL_InitTEnts(void);


//
// predict.c
//
void CL_PredictAngles(void);
void CL_PredictMovement(void);
void CL_CheckPredictionError(void);
void CL_Trace(trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const struct edict_s *passent, contents_t contentmask);


//
// effects.c
//
#define PARTICLE_GRAVITY    40
#define INSTANT_PARTICLE    -10000.0f

typedef struct cparticle_s {
    struct cparticle_s    *next;

    int     time;
    vec3_t  org;
    vec3_t  vel;
    vec3_t  accel;
    int     color;      // -1 => use rgba
    float   scale;
    float   alpha;
    float   alphavel;
    color_t rgba;
} cparticle_t;

typedef struct {
    int     key;        // so entities can reuse same entry
    vec3_t  color;
    vec3_t  origin;
    float   radius;
    int     die;        // stop lighting after this time
    int     start;
    bool    fade;
} cdlight_t;

typedef enum {
    DT_GIB,
    DT_GREENGIB,
    DT_ROCKET,
    DT_GRENADE,
    DT_FIREBALL,

    DT_COUNT
} diminishing_trail_t;

void CL_BigTeleportParticles(const vec3_t org);
void CL_DiminishingTrail(centity_t *ent, const vec3_t end, diminishing_trail_t type);
void CL_FlyEffect(centity_t *ent, const vec3_t origin);
void CL_BfgParticles(const entity_t *ent);
void CL_ItemRespawnParticles(const vec3_t org);
void CL_InitEffects(void);
void CL_ClearEffects(void);
void CL_BlasterParticles(const vec3_t org, const vec3_t dir);
void CL_ExplosionParticles(const vec3_t org);
void CL_BFGExplosionParticles(const vec3_t org);
void CL_BlasterTrail(centity_t *ent, const vec3_t end);
void CL_OldRailTrail(void);
void CL_BubbleTrail(const vec3_t start, const vec3_t end);
void CL_FlagTrail(centity_t *ent, const vec3_t end, int color);
void CL_MuzzleFlash(void);
void CL_MuzzleFlash2(void);
void CL_TeleporterParticles(const vec3_t org);
void CL_TeleportParticles(const vec3_t org);
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, int color, int count);
cparticle_t *CL_AllocParticle(void);
void CL_AddParticles(void);
cdlight_t *CL_AllocDlight(int key);
void CL_AddDLights(void);
void CL_SetLightStyle(int index, const char *s);
void CL_AddLightStyles(void);
void CL_AddShadowLights(void);

//
// newfx.c
//

void CL_BlasterParticles2(const vec3_t org, const vec3_t dir, unsigned int color);
void CL_BlasterTrail2(centity_t *ent, const vec3_t end);
void CL_DebugTrail(const vec3_t start, const vec3_t end);
void CL_Flashlight(int ent, const vec3_t pos);
void CL_ForceWall(const vec3_t start, const vec3_t end, int color);
void CL_BubbleTrail2(const vec3_t start, const vec3_t end, int dist);
void CL_Heatbeam(const vec3_t start, const vec3_t end);
void CL_ParticleSteamEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);
void CL_TrackerTrail(centity_t *ent, const vec3_t end);
void CL_TagTrail(centity_t *ent, const vec3_t end, int color);
void CL_ColorFlash(const vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_Tracker_Shell(const centity_t *cent, const vec3_t origin);
void CL_MonsterPlasma_Shell(const vec3_t origin);
void CL_ColorExplosionParticles(const vec3_t org, int color, int run);
void CL_ParticleSmokeEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);
void CL_Widowbeamout(cl_sustain_t *self);
void CL_Nukeblast(cl_sustain_t *self);
void CL_WidowSplash(void);
void CL_IonripperTrail(centity_t *ent, const vec3_t end);
void CL_TrapParticles(centity_t *ent, const vec3_t origin);
void CL_ParticleEffect3(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleSteamEffect2(cl_sustain_t *self);
void CL_BerserkSlamParticles(const vec3_t org, const vec3_t dir);
void CL_PowerSplash(void);
void CL_TeleporterParticles2(const vec3_t org);
void CL_HologramParticles(const vec3_t org);
void CL_BarrelExplodingParticles(const vec3_t org);


//
// demo.c
//
void CL_InitDemos(void);
void CL_CleanupDemos(void);
void CL_DemoFrame(void);
bool CL_WriteDemoMessage(sizebuf_t *buf);
void CL_PackEntity(entity_packed_t *out, const entity_state_t *in);
void CL_EmitDemoFrame(void);
void CL_EmitDemoSnapshot(void);
void CL_FreeDemoSnapshots(void);
void CL_FirstDemoFrame(void);
void CL_Stop_f(void);
bool CL_GetDemoInfo(const char *path, demoInfo_t *info);

extern q2protoio_ioarg_t demo_q2protoio_ioarg;
#define Q2PROTO_IOARG_DEMO_WRITE    ((uintptr_t)&demo_q2protoio_ioarg)


//
// locs.c
//
void LOC_Init(void);
void LOC_LoadLocations(void);
void LOC_FreeLocations(void);
void LOC_AddLocationsToScene(void);


//
// console.c
//
void Con_Init(void);
void Con_PostInit(void);
void Con_Shutdown(void);
void Con_DrawConsole(void);
void Con_RunConsole(void);
void Con_Print(const char *txt);
void Con_ClearNotify_f(void);
void Con_ToggleConsole_f(void);
void Con_ClearTyping(void);
void Con_Close(bool force);
void Con_Popup(bool force);
void Con_SkipNotify(bool skip);
void Con_RegisterMedia(void);
void Con_RendererShutdown(void);
void Con_CheckResize(void);
const char *Con_GetChatPromptText(int *skip_chars);
inputField_t *Con_GetChatInputField(void);

void Key_Console(int key);
void Key_Message(int key);
void Char_Console(int key);
void Char_Message(int key);


//
// renderer.c
//
void    CL_InitRenderer(void);
void    CL_ShutdownRenderer(void);
void    CL_RunRenderer(void);


//
// screen.c
//
#define STAT_PICS       11
#define STAT_MINUS      (STAT_PICS - 1)  // num frame for '-' stats digit

typedef struct {
    int         damage;
    vec3_t      color;
    vec3_t      dir;
    int         time;
} scr_damage_entry_t;

#define MAX_DAMAGE_ENTRIES      32
#define DAMAGE_ENTRY_BASE_SIZE  3

typedef struct {
    int         id;
    int         time;
    int         color;
    int         flags;
    qhandle_t   image;
    int         width, height;
    vec3_t      position;
} scr_poi_t;

#define MAX_TRACKED_POIS        32

typedef struct {
    bool        initialized;        // ready to draw

    qhandle_t   crosshair_pic;
    int         crosshair_width, crosshair_height;
    color_t     crosshair_color;

    qhandle_t   pause_pic;

    qhandle_t   loading_pic;
    bool        draw_loading;

    qhandle_t   hit_marker_pic;
    int         hit_marker_time;
    int         hit_marker_width, hit_marker_height;

    qhandle_t   damage_display_pic;
    int         damage_display_width, damage_display_height;
    scr_damage_entry_t  damage_entries[MAX_DAMAGE_ENTRIES];

    scr_poi_t   pois[MAX_TRACKED_POIS];

    qhandle_t   sb_pics[2][STAT_PICS];
    qhandle_t   inven_pic;
    qhandle_t   field_pic;

    qhandle_t   backtile_pic;

    qhandle_t   net_pic;
    qhandle_t   font_pic;
    font_t      *font;
    qhandle_t   ui_font_pic;
    font_t      *ui_font;

    int         canvas_width, canvas_height;
    float       virtual_scale;
    int         virtual_width, virtual_height;
    int         hud_width, hud_height;
    float       hud_scale;
    vrect_t     vrect;        // position of render window
    
    kfont_t     kfont;

} cl_scr_t;

extern cl_scr_t scr;

void    SCR_Init(void);
void    SCR_Shutdown(void);
void    SCR_UpdateScreen(void);
void    SCR_SizeUp(void);
void    SCR_SizeDown(void);
void    SCR_BeginLoadingPlaque(void);
void    SCR_EndLoadingPlaque(void);
void    SCR_RegisterMedia(void);
void    SCR_ModeChanged(void);
void    SCR_LagSample(void);
void    SCR_LagClear(void);
void    SCR_SetCrosshairColor(void);
void    SCR_NotifyPickupPulse(void);
void    SCR_AddNetgraph(void);

float   SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime);
int     SCR_DrawStringStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, qhandle_t font);
void    SCR_DrawStringMultiStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, qhandle_t font);
void    SCR_DrawKStringMultiStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, const kfont_t *kfont);
int     SCR_MeasureString(const char *text, size_t max_chars);
bool    SCR_GetBindIconForKey(int keynum, qhandle_t *pic, int *w, int *h);
int     SCR_DrawBindIcon(const char *binding, int x, int y, int size, color_t color, const char **out_keyname);

#define SCR_DrawString(x, y, flags, color, string) \
    SCR_DrawStringStretch(x, y, 1, flags, MAX_STRING_CHARS, string, color, scr.ui_font_pic)

void    SCR_ClearChatHUD_f(void);
void    SCR_AddToChatHUD(const char *text);
void    SCR_AddToNotifyHUD(const char *text, bool is_chat);
void    SCR_NotifyScrollLines(float delta);
void    SCR_NotifyMouseEvent(int x, int y);
void    SCR_NotifyMouseDown(int button);

void    SCR_AddToDamageDisplay(int damage, const vec3_t color, const vec3_t dir);
void    SCR_RemovePOI(int id);
void    SCR_AddPOI(int id, int time, const vec3_t p, int image, int color, int flags);
void    SCR_Clear(void);

//
// cin.c
//

#if USE_AVCODEC

typedef struct {
    const char *ext;
    const char *fmt;
    int codec_id;
} avformat_t;

void    SCR_InitCinematics(void);
void    SCR_StopCinematic(void);
void    SCR_FinishCinematic(void);
void    SCR_RunCinematic(void);
void    SCR_DrawCinematic(void);
void    SCR_ReloadCinematic(void);
void    SCR_PlayCinematic(const char *name);

#else

static inline void SCR_FinishCinematic(void)
{
    // tell the server to advance to the next map / cinematic
    CL_ClientCommand(va("nextserver %i\n", cl.servercount));
}

#define SCR_InitCinematics()    (void)0
#define SCR_StopCinematic()     (void)0
#define SCR_RunCinematic()      (void)0
#define SCR_DrawCinematic()     (void)0
#define SCR_ReloadCinematic()   (void)0
#define SCR_PlayCinematic(name) SCR_FinishCinematic()

#endif

//
// ascii.c
//
void CL_InitAscii(void);


//
// http.c
//
#if USE_CURL
void HTTP_Init(void);
void HTTP_Shutdown(void);
void HTTP_SetServer(const char *url);
int HTTP_QueueDownload(const char *path, dltype_t type);
void HTTP_RunDownloads(void);
void HTTP_CleanupDownloads(void);
#else
#define HTTP_Init()                     (void)0
#define HTTP_Shutdown()                 (void)0
#define HTTP_SetServer(url)             (void)0
#define HTTP_QueueDownload(path, type)  Q_ERR(ENOSYS)
#define HTTP_RunDownloads()             (void)0
#define HTTP_CleanupDownloads()         (void)0
#endif

//
// gtv.c
//

#if USE_CLIENT_GTV
void CL_GTV_EmitFrame(void);
void CL_GTV_WriteMessage(const byte *data, size_t len);
void CL_GTV_Resume(void);
void CL_GTV_Suspend(void);
void CL_GTV_Transmit(void);
void CL_GTV_Run(void);
void CL_GTV_Init(void);
void CL_GTV_Shutdown(void);
#else
#define CL_GTV_EmitFrame()              (void)0
#define CL_GTV_WriteMessage(data, len)  (void)0
#define CL_GTV_Resume()                 (void)0
#define CL_GTV_Suspend()                (void)0
#define CL_GTV_Transmit()               (void)0
#define CL_GTV_Run()                    (void)0
#define CL_GTV_Init()                   (void)0
#define CL_GTV_Shutdown()               (void)0
#endif

//
// cgame.c
//

extern const cgame_export_t *cgame;
extern const cgame_entity_export_t *cgame_entity;

void CG_Init(void);
void CG_Load(const char* new_game, bool is_rerelease_server);
void CG_Unload(void);

#ifdef __cplusplus
}
#endif
