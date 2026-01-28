// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/shared.h"
#include "common/math.h"

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

#define EF_TRAIL_MASK   (EF_ROCKET | EF_BLASTER | EF_HYPERBLASTER | EF_GIB | EF_GRENADE | \
                         EF_FLIES | EF_BFG | EF_TRAP | EF_FLAG1 | EF_FLAG2 | EF_TAGTRAIL | \
                         EF_TRACKERTRAIL | EF_TRACKER | EF_GREENGIB | EF_IONRIPPER | \
                         EF_BLUEHYPERBLASTER | EF_PLASMA)

#define IS_TRACKER(effects) \
    (((effects) & (EF_TRACKERTRAIL | EF_TRACKER)) == EF_TRACKERTRAIL)

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
void CL_DrawBeam(const vec3_t org, const vec3_t end, float model_length, qhandle_t model);
void CL_PlayFootstepSfx(int step_id, int entnum, float volume, float attenuation);

void CL_DiminishingTrail(centity_t *ent, const vec3_t end, diminishing_trail_t type);
void CL_FlyEffect(centity_t *ent, const vec3_t origin);
void CL_BfgParticles(const entity_t *ent);
void CL_ItemRespawnParticles(const vec3_t org);
void CL_TeleporterParticles(const vec3_t org);
void CL_TeleportParticles(const vec3_t org);
void CL_TeleporterParticles2(const vec3_t org);
void CL_HologramParticles(const vec3_t org);
void CL_BarrelExplodingParticles(const vec3_t org);

#if USE_DEBUG
void CL_CheckEntityPresent(int entnum, const char *what);
#endif

void CL_SmokeAndFlash(const vec3_t origin);
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, int color, int count);
void CL_ParticleEffect3(const vec3_t org, const vec3_t dir, int color, int count);

cparticle_t *CL_AllocParticle(void);
cdlight_t *CL_AllocDlight(int key);

void CL_OldRailTrail(void);
void CL_Heatbeam(const vec3_t start, const vec3_t forward);
void CL_ParticleSteamEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);
void CL_ParticleSteamEffect2(cl_sustain_t *self);
void CL_MonsterPlasma_Shell(const vec3_t origin);
void CL_Widowbeamout(cl_sustain_t *self);
void CL_Nukeblast(cl_sustain_t *self);

void CL_BlasterTrail(centity_t *ent, const vec3_t end);
void CL_BlasterTrail2(centity_t *ent, const vec3_t end);
void CL_BlasterParticles(const vec3_t origin, const vec3_t direction);
void CL_BlasterParticles2(const vec3_t origin, const vec3_t direction, unsigned int color);
void CL_TrapParticles(centity_t *ent, const vec3_t origin);
void CL_FlagTrail(centity_t *ent, const vec3_t end, int color);
void CL_TagTrail(centity_t *ent, const vec3_t end, int color);
void CL_Tracker_Shell(const centity_t *ent, const vec3_t origin);
void CL_TrackerTrail(centity_t *ent, const vec3_t end);
void CL_IonripperTrail(centity_t *ent, const vec3_t end);

void CL_ExplosionParticles(const vec3_t org);
void CL_BFGExplosionParticles(const vec3_t org);
void CL_BubbleTrail(const vec3_t start, const vec3_t end);
void CL_BigTeleportParticles(const vec3_t org);
void CL_DebugTrail(const vec3_t start, const vec3_t end);
void CL_Flashlight(int ent, const vec3_t pos);
void CL_ForceWall(const vec3_t start, const vec3_t end, int color);
void CL_BubbleTrail2(const vec3_t start, const vec3_t end, int dist);
void CL_ParticleSmokeEffect(const vec3_t org, const vec3_t dir, int color, int count, int magnitude);

void CL_AddTEnts(void);
void CL_AddParticles(void);
void CL_AddDLights(void);
void CL_AddLightStyles(void);
void CL_AddShadowLights(void);

void CL_ColorFlash(const vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_ColorExplosionParticles(const vec3_t org, int color, int run);
void CL_WidowSplash(void);
void CL_BerserkSlamParticles(const vec3_t org, const vec3_t dir);
void CL_PowerSplash(void);

void CL_PredictAngles(void);
void CL_PredictMovement(void);
void CL_CheckPredictionError(void);
