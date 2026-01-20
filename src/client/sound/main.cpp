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
// snd_main.c -- common sound functions

#include "sound.h"

// =======================================================================
// Internal sound data & structures
// =======================================================================

unsigned    s_registration_sequence;

channel_t   *s_channels;
int         s_numchannels, s_maxchannels;

sndstarted_t    s_started;
bool            s_active;
bool            s_supports_float;
const sndapi_t  *s_api;

vec3_t      listener_origin;
vec3_t      listener_forward;
vec3_t      listener_right;
vec3_t      listener_up;
int         listener_entnum;

bool        s_registering;

int         s_paintedtime;  // sample PAIRS

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define     MAX_SFX     (MAX_SOUNDS*2)
static sfx_t        known_sfx[MAX_SFX];
static int          num_sfx;

#define     MAX_PLAYSOUNDS  128
playsound_t s_playsounds[MAX_PLAYSOUNDS];
list_t      s_freeplays;
list_t      s_pendingplays;

cvar_t      *s_volume;
cvar_t      *s_ambient;
#if USE_DEBUG
cvar_t      *s_show;
#endif
cvar_t      *s_underwater;
cvar_t      *s_underwater_gain_hf;
cvar_t      *s_num_channels;
cvar_t      *s_occlusion;
cvar_t      *s_occlusion_strength;

static cvar_t   *s_enable;
static cvar_t   *s_auto_focus;

// =======================================================================
// Console functions
// =======================================================================

static void S_SoundInfo_f(void)
{
    if (!s_started) {
        Com_Printf("Sound system not started.\n");
        return;
    }

    s_api->sound_info();
}

static void S_SoundList_f(void)
{
    int     i, count;
    sfx_t   *sfx;
    sfxcache_t  *sc;
    size_t  total;

    total = count = 0;
    for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        sc = sfx->cache;
        if (sc) {
            total += sc->size;
            if (sc->loopstart >= 0)
                Com_Printf("L");
            else
                Com_Printf(" ");
            Com_Printf("(%2db) (%dch) %6i : %s\n", sc->width * 8, sc->channels, sc->size, sfx->name);
        } else {
            if (sfx->name[0] == '*')
                Com_Printf("  placeholder : %s\n", sfx->name);
            else
                Com_Printf("  not loaded  : %s (%s)\n",
                           sfx->name, Q_ErrorString(sfx->error));
        }
        count++;
    }
    Com_Printf("Total sounds: %d (out of %d slots)\n", count, num_sfx);
    Com_Printf("Total resident: %zu\n", total);
}

static const cmdreg_t c_sound[] = {
    { "stopsound", S_StopAllSounds },
    { "soundlist", S_SoundList_f },
    { "soundinfo", S_SoundInfo_f },

    { NULL }
};

// =======================================================================
// Init sound engine
// =======================================================================

static void s_auto_focus_changed(cvar_t *self)
{
    S_Activate();
}

static void s_occlusion_changed(cvar_t *self)
{
    (void)self;

    if (!s_started || !s_channels)
        return;

    for (int i = 0; i < s_numchannels; i++)
        S_ResetOcclusion(&s_channels[i]);

#if USE_SNDDMA
    for (int i = 0; i < s_numchannels; i++) {
        s_channels[i].occlusion_z1[0] = 0.0f;
        s_channels[i].occlusion_z1[1] = 0.0f;
        s_channels[i].occlusion_z2[0] = 0.0f;
        s_channels[i].occlusion_z2[1] = 0.0f;
    }
#endif
}

/*
================
S_Init
================
*/
void S_Init(void)
{
    s_enable = Cvar_Get("s_enable", "2", CVAR_SOUND);
    if (s_enable->integer <= SS_NOT) {
        Com_Printf("Sound initialization disabled.\n");
        return;
    }

    Com_Printf("------- S_Init -------\n");

    s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE);
    s_ambient = Cvar_Get("s_ambient", "1", 0);
#if USE_DEBUG
    s_show = Cvar_Get("s_show", "0", 0);
#endif
    s_auto_focus = Cvar_Get("s_auto_focus", "2", 0);
    s_underwater = Cvar_Get("s_underwater", "1", 0);
    s_underwater_gain_hf = Cvar_Get("s_underwater_gain_hf", "0.25", 0);
    s_num_channels = Cvar_Get("s_num_channels", "64", CVAR_SOUND);
    s_occlusion = Cvar_Get("s_occlusion", "1", CVAR_ARCHIVE);
    s_occlusion_strength = Cvar_Get("s_occlusion_strength", "1.0", CVAR_ARCHIVE);

    s_maxchannels = Cvar_ClampInteger(s_num_channels, 16, 256);
    s_channels = static_cast<channel_t *>(Z_TagMalloc(sizeof(*s_channels) * s_maxchannels, TAG_SOUND));

    // start one of available sound engines
    s_started = SS_NOT;

#if USE_OPENAL
    if (s_started == SS_NOT && s_enable->integer >= SS_OAL && snd_openal.init()) {
        s_started = SS_OAL;
        s_api = &snd_openal;
    }
#endif

#if USE_SNDDMA
    if (s_started == SS_NOT && s_enable->integer >= SS_DMA && snd_dma.init()) {
        s_started = SS_DMA;
        s_api = &snd_dma;
    }
#endif

    if (s_started == SS_NOT) {
        Com_EPrintf("Sound failed to initialize.\n");
        goto fail;
    }

    Cmd_Register(c_sound);

    // init playsound list
    // clear DMA buffer
    S_StopAllSounds();

    s_auto_focus->changed = s_auto_focus_changed;
    s_auto_focus_changed(s_auto_focus);
    s_occlusion->changed = s_occlusion_changed;
    s_occlusion_strength->changed = s_occlusion_changed;

    num_sfx = 0;

    s_paintedtime = 0;

    s_registration_sequence = 1;

    // start the cd track
    OGG_Play();

fail:
    Cvar_SetInteger(s_enable, s_started, FROM_CODE);
    Com_Printf("----------------------\n");
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

static void S_FreeSound(sfx_t *sfx)
{
    if (s_started && s_api->delete_sfx)
        s_api->delete_sfx(sfx);
    Z_Free(sfx->cache);
    Z_Free(sfx->truename);
    memset(sfx, 0, sizeof(*sfx));
}

void S_FreeAllSounds(void)
{
    int     i;
    sfx_t   *sfx;

    // free all sounds
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        S_FreeSound(sfx);
    }

    num_sfx = 0;
}

void S_Shutdown(void)
{
    if (!s_started)
        return;

    S_StopAllSounds();
    S_FreeAllSounds();
    OGG_Stop();

    Z_Free(s_channels);
    s_channels = NULL;
    s_maxchannels = 0;

    s_api->shutdown();
    s_api = NULL;

    s_started = SS_NOT;
    s_active = false;
    s_supports_float = false;

    s_auto_focus->changed = NULL;
    s_occlusion->changed = NULL;
    s_occlusion_strength->changed = NULL;

    Cmd_Deregister(c_sound);

    Z_LeakTest(TAG_SOUND);
}

void S_Activate(void)
{
    bool active;
    active_t level;

    if (!s_started)
        return;

    level = static_cast<active_t>(Cvar_ClampInteger(s_auto_focus, ACT_MINIMIZED, ACT_ACTIVATED));

    active = cls.active >= level;

    if (active == s_active)
        return;

    Com_DDDPrintf("%s: %d\n", __func__, active);
    s_active = active;

    s_api->activate();
}

// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_SfxForHandle
==================
*/
sfx_t *S_SfxForHandle(qhandle_t hSfx)
{
    if (!hSfx) {
        return NULL;
    }

    Q_assert(hSfx > 0 && hSfx <= num_sfx);
    return &known_sfx[hSfx - 1];
}

static sfx_t *S_AllocSfx(void)
{
    sfx_t   *sfx;
    int     i;

    // find a free sfx
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            break;
    }

    if (i == num_sfx) {
        if (num_sfx == MAX_SFX)
            return NULL;
        num_sfx++;
    }

    return sfx;
}

/*
==================
S_FindName

==================
*/
static sfx_t *S_FindName(const char *name, size_t namelen)
{
    int     i;
    sfx_t   *sfx;

    // see if already loaded
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!FS_pathcmp(sfx->name, name)) {
            sfx->registration_sequence = s_registration_sequence;
            return sfx;
        }
    }

    // allocate new one
    sfx = S_AllocSfx();
    if (sfx) {
        memcpy(sfx->name, name, namelen + 1);
        sfx->registration_sequence = s_registration_sequence;
    }
    return sfx;
}

/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration(void)
{
    s_registration_sequence++;
    s_registering = true;
}

/*
==================
S_RegisterSound

==================
*/
qhandle_t S_RegisterSound(const char *name)
{
    char    buffer[MAX_QPATH];
    sfx_t   *sfx;
    size_t  len;

    if (!s_started)
        return 0;

    Q_assert(name);

    // empty names are legal, silently ignore them
    if (!*name)
        return 0;

    if (*name == '*') {
        len = Q_strlcpy(buffer, name, MAX_QPATH);
    } else if (*name == '#') {
        len = FS_NormalizePathBuffer(buffer, name + 1, MAX_QPATH);
    } else {
        len = Q_concat(buffer, MAX_QPATH, "sound/", name);
        if (len < MAX_QPATH)
            len = FS_NormalizePath(buffer);
    }

    // this MAY happen after prepending "sound/"
    if (len >= MAX_QPATH) {
        Com_DPrintf("%s: oversize name\n", __func__);
        return 0;
    }

    // normalized to empty name?
    if (len == 0) {
        Com_DPrintf("%s: empty name\n", __func__);
        return 0;
    }

    sfx = S_FindName(buffer, len);
    if (!sfx) {
        Com_DPrintf("%s: out of slots\n", __func__);
        return 0;
    }

    if (!s_registering) {
        S_LoadSound(sfx);
    }

    return (sfx - known_sfx) + 1;
}

/*
====================
S_RegisterSexedSound
====================
*/
static sfx_t *S_RegisterSexedSound(int entnum, const char *base)
{
    sfx_t           *sfx;
    const char      *model;
    char            buffer[MAX_QPATH];

    // determine what model the client is using
    if (entnum > 0 && entnum <= MAX_CLIENTS)
        model = cl.clientinfo[entnum - 1].model_name;
    else
        model = cl.baseclientinfo.model_name;

    // if we can't figure it out, they're male
    if (!*model)
        model = "male";

    // see if we already know of the model specific sound
    if (Q_concat(buffer, MAX_QPATH, "players/", model, "/", base + 1) >= MAX_QPATH
        && Q_concat(buffer, MAX_QPATH, "players/", "male", "/", base + 1) >= MAX_QPATH)
        return NULL;

    sfx = S_FindName(buffer, FS_NormalizePath(buffer));

    // see if it exists
    if (sfx && !sfx->truename && !s_registering && !S_LoadSound(sfx)) {
        // no, revert to the male sound in the pak0.pak
        if (Q_concat(buffer, MAX_QPATH, "sound/player/male/", base + 1) < MAX_QPATH) {
            FS_NormalizePath(buffer);
            sfx->error = Q_ERR_SUCCESS;
            sfx->truename = S_CopyString(buffer);
        }
    }

    return sfx;
}

static void S_RegisterSexedSounds(void)
{
    int     sounds[MAX_SFX];
    int     i, j, total;
    sfx_t   *sfx;

    // find sexed sounds
    total = 0;
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (sfx->name[0] != '*')
            continue;
        if (sfx->registration_sequence != s_registration_sequence)
            continue;
        sounds[total++] = i;
    }

    // register sounds for baseclientinfo and other valid clientinfos
    for (i = 0; i <= MAX_CLIENTS; i++) {
        if (i > 0 && !cl.clientinfo[i - 1].model_name[0])
            continue;
        for (j = 0; j < total; j++) {
            sfx = &known_sfx[sounds[j]];
            S_RegisterSexedSound(i, sfx->name);
        }
    }
}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration(void)
{
    int     i;
    sfx_t   *sfx;

    S_RegisterSexedSounds();

    // clear playsound list, so we don't free sfx still present there
    S_StopAllSounds();

    // free any sounds not from this registration sequence
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        if (sfx->registration_sequence != s_registration_sequence) {
            // don't need this sound
            S_FreeSound(sfx);
            continue;
        }
        // make sure it is paged in
        if (s_started && s_api->page_in_sfx)
            s_api->page_in_sfx(sfx);
    }

    // load everything in
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        S_LoadSound(sfx);
    }

    if (s_started && s_api->end_registration)
        s_api->end_registration();

    s_registering = false;
}


//=============================================================================

/*
=================
S_PickChannel

picks a channel based on priorities, empty slots, number of channels
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
    int         ch_idx;
    int         first_to_die;
    int         life_left;
    channel_t   *ch;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = INT_MAX;
    for (ch_idx = 0; ch_idx < s_numchannels; ch_idx++) {
        ch = &s_channels[ch_idx];
        // channel 0 never overrides unless out of channels
        if (ch->entnum == entnum && ch->entchannel == entchannel && entchannel != 0) {
            if (entchannel > 255 && ch->sfx)
                return NULL;    // channels >255 only allow single sfx on that channel
            // always override sound from same entity
            first_to_die = ch_idx;
            break;
        }

        // don't let monster sounds override player sounds
        if (ch->entnum == listener_entnum && entnum != listener_entnum && ch->sfx)
            continue;

        if (ch->end - s_paintedtime < life_left) {
            life_left = ch->end - s_paintedtime;
            first_to_die = ch_idx;
        }
    }

    if (first_to_die == -1)
        return NULL;

    ch = &s_channels[first_to_die];
    if (s_api->stop_channel)
        s_api->stop_channel(ch);
    memset(ch, 0, sizeof(*ch));

    return ch;
}

/*
=================
S_AllocPlaysound
=================
*/
static playsound_t *S_AllocPlaysound(void)
{
    playsound_t *ps = PS_FIRST(&s_freeplays);

    if (PS_TERM(ps, &s_freeplays))
        return NULL;        // no free playsounds

    // unlink from freelist
    List_Remove(&ps->entry);

    return ps;
}

/*
=================
S_FreePlaysound
=================
*/
static void S_FreePlaysound(playsound_t *ps)
{
    // unlink from channel
    List_Remove(&ps->entry);

    // add to free list
    List_Insert(&s_freeplays, &ps->entry);
}

/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound(playsound_t *ps)
{
    channel_t   *ch;
    sfxcache_t  *sc;

#if USE_DEBUG
    if (s_show->integer)
        Com_Printf("Issue %i\n", ps->begin);
#endif
    // pick a channel to play on
    ch = S_PickChannel(ps->entnum, ps->entchannel);
    if (!ch) {
        S_FreePlaysound(ps);
        return;
    }

    sc = S_LoadSound(ps->sfx);
    if (!sc) {
        Com_Printf("S_IssuePlaysound: couldn't load %s\n", ps->sfx->name);
        S_FreePlaysound(ps);
        return;
    }

    // spatialize
    if (ps->attenuation == ATTN_STATIC)
        ch->dist_mult = ps->attenuation * 0.001f;
    else
        ch->dist_mult = ps->attenuation * 0.0005f;
    ch->master_vol = ps->volume;
    ch->entnum = ps->entnum;
    ch->entchannel = ps->entchannel;
    ch->sfx = ps->sfx;
    VectorCopy(ps->origin, ch->origin);
    ch->fixed_origin = ps->fixed_origin;
    ch->pos = 0;
    ch->end = s_paintedtime + sc->length;

    s_api->play_channel(ch);

    // free the playsound
    S_FreePlaysound(ps);
}

// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_StartSound

Validates the params and queues the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
void S_StartSound(const vec3_t origin, int entnum, int entchannel, qhandle_t hSfx, float vol, float attenuation, float timeofs)
{
    sfxcache_t  *sc;
    playsound_t *ps, *sort;
    sfx_t       *sfx;

    if (!s_started)
        return;
    if (!s_active)
        return;
    if (!(sfx = S_SfxForHandle(hSfx)))
        return;

    if (sfx->name[0] == '*') {
        sfx = S_RegisterSexedSound(entnum, sfx->name);
        if (!sfx)
            return;
    }

    // make sure the sound is loaded
    sc = S_LoadSound(sfx);
    if (!sc)
        return;     // couldn't load the sound's data

    // make the playsound_t
    ps = S_AllocPlaysound();
    if (!ps)
        return;

    if (origin) {
        VectorCopy(origin, ps->origin);
        ps->fixed_origin = true;
    } else {
        ps->fixed_origin = false;
    }

    ps->entnum = entnum;
    ps->entchannel = entchannel;
    ps->attenuation = attenuation;
    ps->volume = vol;
    ps->sfx = sfx;
    ps->begin = s_api->get_begin_ofs(timeofs);

    // sort into the pending sound list
    LIST_FOR_EACH(playsound_t, sort, &s_pendingplays, entry)
        if (sort->begin >= ps->begin)
            break;

    List_Append(&sort->entry, &ps->entry);
}

void S_ParseStartSound(void)
{
    qhandle_t handle = cl.sound_precache[snd.index];

    if (!handle)
        return;

#if USE_DEBUG
    if (developer->integer && !snd.has_position)
        CL_CheckEntityPresent(snd.entity, "sound");
#endif

    S_StartSound(snd.has_position ? snd.pos : NULL,
                 snd.entity, snd.channel, handle,
                 snd.volume, snd.attenuation, snd.timeofs);
}

/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound(const char *sound)
{
    if (s_started) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_StartSound(NULL, listener_entnum, 0, sfx, 1, ATTN_NONE, 0);
    }
}

void S_StartLocalSoundOnce(const char *sound)
{
    if (s_started) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_StartSound(NULL, listener_entnum, 256, sfx, 1, ATTN_NONE, 0);
    }
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
    int     i;

    if (!s_started)
        return;

    // clear all the playsounds
    memset(s_playsounds, 0, sizeof(s_playsounds));

    List_Init(&s_freeplays);
    List_Init(&s_pendingplays);

    for (i = 0; i < MAX_PLAYSOUNDS; i++)
        List_Append(&s_freeplays, &s_playsounds[i].entry);

    s_api->stop_all_sounds();

    // clear all the channels
    memset(s_channels, 0, sizeof(*s_channels) * s_numchannels);
}

void S_RawSamples(int samples, int rate, int width, int channels, const void *data)
{
    if (s_started && s_active)
        s_api->raw_samples(samples, rate, width, channels, data, 1.0f);
}

int S_GetSampleRate(void)
{
    if (s_api && s_api->get_sample_rate)
        return s_api->get_sample_rate();
    return 0;
}

bool S_SupportsFloat(void)
{
    return s_supports_float;
}

void S_PauseRawSamples(bool paused)
{
    if (s_api && s_api->pause_raw_samples)
        s_api->pause_raw_samples(paused);
}

// =======================================================================
// Update sound buffer
// =======================================================================

int S_BuildSoundList(int *sounds)
{
    int             i, num, count;
    entity_state_t  *ent;

    if (cls.state != ca_active || !s_active || sv_paused->integer || !s_ambient->integer)
        return 0;

    for (i = count = 0; i < cl.frame.numEntities; i++) {
        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];
        if (s_ambient->integer == 2 && !ent->modelindex) {
            sounds[i] = 0;
        } else if (s_ambient->integer == 3 && ent->number != listener_entnum) {
            sounds[i] = 0;
        } else {
            sounds[i] = ent->sound;
            if (ent->sound)
                count++;
        }
    }

    return count;
}

int32_t volume_modified = 0;

/*
=================
S_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
void S_SpatializeOrigin(const vec3_t origin, float master_vol, float dist_mult, float *left_vol, float *right_vol, bool stereo)
{
    vec_t       dot;
    vec_t       dist;
    vec_t       lscale, rscale, scale;
    vec3_t      source_vec;

// calculate stereo separation and distance attenuation
    VectorSubtract(origin, listener_origin, source_vec);

    dist = VectorNormalize(source_vec);
    dist -= SOUND_FULLVOLUME;
    if (dist < 0)
        dist = 0;           // close enough to be at full volume
    dist *= dist_mult;      // different attenuation levels

    if (!stereo || !dist_mult) {
        // no attenuation = no spatialization
        rscale = 1.0f;
        lscale = 1.0f;
    } else {
        dot = DotProduct(listener_right, source_vec);
        rscale = 0.5f * (1.0f + dot);
        lscale = 0.5f * (1.0f - dot);
    }

    // add in distance effect
    scale = (1.0f - dist) * rscale;
    *right_vol = master_vol * scale;
    if (*right_vol < 0)
        *right_vol = 0;

    scale = (1.0f - dist) * lscale;
    *left_vol = master_vol * scale;
    if (*left_vol < 0)
        *left_vol = 0;
}

static void S_OcclusionTrace(const vec3_t start, const vec3_t end, float *out_weight, float *out_cutoff_hz)
{
    trace_t tr;
    CL_Trace(&tr, start, end, vec3_origin, vec3_origin, NULL, MASK_SOLID);

    if (tr.allsolid || tr.startsolid) {
        *out_weight = 1.0f;
        *out_cutoff_hz = S_OCCLUSION_CUTOFF_DEFAULT_HZ;
        return;
    }

    if (tr.fraction >= 1.0f || (tr.surface && (tr.surface->flags & SURF_SKY))) {
        *out_weight = 0.0f;
        *out_cutoff_hz = S_OCCLUSION_CUTOFF_CLEAR_HZ;
        return;
    }

    float weight = 1.0f;
    float cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
    bool cutoff_matched = false;
    if (tr.contents & CONTENTS_WINDOW) {
        weight = min(weight, S_OCCLUSION_WINDOW_WEIGHT);
        cutoff = min(cutoff, S_OCCLUSION_CUTOFF_GLASS_HZ);
        cutoff_matched = true;
    }
    if (tr.surface && (tr.surface->flags & (SURF_TRANS33 | SURF_TRANS66))) {
        weight = min(weight, S_OCCLUSION_WINDOW_WEIGHT);
        cutoff = min(cutoff, S_OCCLUSION_CUTOFF_GLASS_HZ);
        cutoff_matched = true;
    }

    if (tr.surface && tr.surface->material[0]) {
        static const struct {
            const char *match;
            float weight;
            float cutoff_hz;
        } material_weights[] = {
            { "glass", S_OCCLUSION_GLASS_WEIGHT, S_OCCLUSION_CUTOFF_GLASS_HZ },
            { "window", S_OCCLUSION_GLASS_WEIGHT, S_OCCLUSION_CUTOFF_GLASS_HZ },
            { "grate", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "mesh", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "fence", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "chain", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "grill", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "vent", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "screen", S_OCCLUSION_GRATE_WEIGHT, S_OCCLUSION_CUTOFF_GRATE_HZ },
            { "cloth", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "curtain", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "fabric", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "carpet", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "plaster", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "drywall", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "sheetrock", S_OCCLUSION_SOFT_WEIGHT, S_OCCLUSION_CUTOFF_SOFT_HZ },
            { "wood", S_OCCLUSION_WOOD_WEIGHT, S_OCCLUSION_CUTOFF_WOOD_HZ },
            { "plywood", S_OCCLUSION_WOOD_WEIGHT, S_OCCLUSION_CUTOFF_WOOD_HZ },
            { "metal", S_OCCLUSION_METAL_WEIGHT, S_OCCLUSION_CUTOFF_METAL_HZ },
            { "steel", S_OCCLUSION_METAL_WEIGHT, S_OCCLUSION_CUTOFF_METAL_HZ },
            { "iron", S_OCCLUSION_METAL_WEIGHT, S_OCCLUSION_CUTOFF_METAL_HZ },
            { "concrete", S_OCCLUSION_CONCRETE_WEIGHT, S_OCCLUSION_CUTOFF_CONCRETE_HZ },
            { "cement", S_OCCLUSION_CONCRETE_WEIGHT, S_OCCLUSION_CUTOFF_CONCRETE_HZ },
        };

        for (size_t i = 0; i < q_countof(material_weights); i++) {
            if (Q_strcasestr(tr.surface->material, material_weights[i].match)) {
                weight = min(weight, material_weights[i].weight);
                cutoff = min(cutoff, material_weights[i].cutoff_hz);
                cutoff_matched = true;
                break;
            }
        }
    }

    if (!cutoff_matched)
        cutoff = S_OCCLUSION_CUTOFF_DEFAULT_HZ;

    *out_weight = weight;
    *out_cutoff_hz = cutoff;
    return;
}

static const vec2_t s_occlusion_offsets[] = {
    { 1.0f, 0.0f },
    { -1.0f, 0.0f },
    { 0.0f, 1.0f },
    { 0.0f, -1.0f },
    { M_SQRT1_2f, M_SQRT1_2f },
    { -M_SQRT1_2f, M_SQRT1_2f },
    { -M_SQRT1_2f, -M_SQRT1_2f },
    { M_SQRT1_2f, -M_SQRT1_2f },
};

float S_ComputeOcclusion(const vec3_t origin, float *cutoff_hz)
{
    vec3_t dir;
    VectorSubtract(origin, listener_origin, dir);
    float dist = VectorNormalize(dir);
    if (dist <= 0.0f) {
        if (cutoff_hz)
            *cutoff_hz = S_OCCLUSION_CUTOFF_CLEAR_HZ;
        return 0.0f;
    }

    vec3_t right, up;
    MakeNormalVectors(dir, right, up);

    float source_radius = S_OCCLUSION_RADIUS_BASE + dist * S_OCCLUSION_RADIUS_SCALE;
    source_radius = min(source_radius, S_OCCLUSION_RADIUS_MAX);
    float listener_radius = max(source_radius * 0.6f, S_OCCLUSION_RADIUS_BASE * 0.5f);
    listener_radius = min(listener_radius, S_OCCLUSION_RADIUS_MAX);

    float direct = 0.0f;
    float direct_cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
    S_OcclusionTrace(listener_origin, origin, &direct, &direct_cutoff);
    float blocked = 0.0f;
    float blocked_cutoff_weighted = 0.0f;
    float blocked_cutoff_weight = 0.0f;
    int samples = 0;

    for (int i = 0; i < (int)q_countof(s_occlusion_offsets); i++) {
        vec3_t endpoint;
        VectorMA(origin, s_occlusion_offsets[i][0] * source_radius, right, endpoint);
        VectorMA(endpoint, s_occlusion_offsets[i][1] * source_radius, up, endpoint);
        float weight = 0.0f;
        float cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
        S_OcclusionTrace(listener_origin, endpoint, &weight, &cutoff);
        blocked += weight;
        if (weight > 0.0f) {
            blocked_cutoff_weighted += cutoff * weight;
            blocked_cutoff_weight += weight;
        }
        samples++;
    }

    for (int i = 0; i < (int)q_countof(s_occlusion_offsets); i++) {
        vec3_t start;
        VectorMA(listener_origin, s_occlusion_offsets[i][0] * listener_radius, right, start);
        VectorMA(start, s_occlusion_offsets[i][1] * listener_radius, up, start);
        float weight = 0.0f;
        float cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
        S_OcclusionTrace(start, origin, &weight, &cutoff);
        blocked += weight;
        if (weight > 0.0f) {
            blocked_cutoff_weighted += cutoff * weight;
            blocked_cutoff_weight += weight;
        }
        samples++;
    }

    float spread = samples ? (blocked / (float)samples) : 0.0f;
    float spread_cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
    if (blocked_cutoff_weight > 0.0f)
        spread_cutoff = blocked_cutoff_weighted / blocked_cutoff_weight;
    float occlusion = FASTLERP(direct, spread, S_OCCLUSION_DIFFRACTION_WEIGHT);
    float cutoff = FASTLERP(direct_cutoff, spread_cutoff, S_OCCLUSION_DIFFRACTION_WEIGHT);
    occlusion = Q_clipf((occlusion - S_OCCLUSION_CLEAR_MARGIN) /
                        (1.0f - S_OCCLUSION_CLEAR_MARGIN),
                        0.0f, 1.0f);

    cutoff = Q_clipf(cutoff, S_OCCLUSION_CUTOFF_MIN_HZ, S_OCCLUSION_CUTOFF_CLEAR_HZ);
    if (cutoff_hz)
        *cutoff_hz = cutoff;

    return occlusion;
}

void S_ResetOcclusion(channel_t *ch)
{
    if (!ch)
        return;

    ch->occlusion = 0.0f;
    ch->occlusion_target = 0.0f;
    ch->occlusion_time = 0;
    ch->occlusion_mix = 0.0f;
    ch->occlusion_cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
    ch->occlusion_cutoff_target = S_OCCLUSION_CUTOFF_CLEAR_HZ;
}

float S_SmoothOcclusion(channel_t *ch, float target)
{
    float dt = Q_clipf(cls.frametime, 0.0f, 0.1f);
    float rate = (target > ch->occlusion) ? S_OCCLUSION_ATTACK_RATE : S_OCCLUSION_RELEASE_RATE;
    float lerp = 1.0f - expf(-rate * dt);

    ch->occlusion = FASTLERP(ch->occlusion, target, lerp);
    ch->occlusion_cutoff = FASTLERP(ch->occlusion_cutoff, ch->occlusion_cutoff_target, lerp);
    ch->occlusion_cutoff = Q_clipf(ch->occlusion_cutoff, S_OCCLUSION_CUTOFF_MIN_HZ, S_OCCLUSION_CUTOFF_CLEAR_HZ);
    if (ch->occlusion < 0.001f) {
        ch->occlusion = 0.0f;
        ch->occlusion_cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
    }

    return ch->occlusion;
}

float S_GetOcclusion(channel_t *ch, const vec3_t origin)
{
    if (!ch || !s_occlusion || !s_occlusion->integer || cls.state != ca_active || !cl.bsp) {
        S_ResetOcclusion(ch);
        return 0.0f;
    }

    if (cl.time >= ch->occlusion_time) {
        float cutoff = S_OCCLUSION_CUTOFF_CLEAR_HZ;
        ch->occlusion_target = S_ComputeOcclusion(origin, &cutoff);
        ch->occlusion_cutoff_target = cutoff;
        int jitter = (ch->entnum & 7) * 3;
        ch->occlusion_time = cl.time + S_OCCLUSION_UPDATE_MS + jitter;
    }

    return S_SmoothOcclusion(ch, ch->occlusion_target);
}

float S_MapOcclusion(float occlusion)
{
    if (occlusion <= 0.0f)
        return 0.0f;

    float strength = 1.0f;
    if (s_occlusion_strength)
        strength = Q_clipf(s_occlusion_strength->value, 0.0f, 2.0f);

    float shaped = powf(occlusion, S_OCCLUSION_CURVE);
    return Q_clipf(shaped * strength, 0.0f, 1.0f);
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(void)
{
    if (cvar_modified & CVAR_SOUND) {
        Cbuf_AddText(&cmd_buffer, "snd_restart\n");
        cvar_modified &= ~CVAR_SOUND;
        return;
    }

    if (!s_started)
        return;

    // if the loading plaque is up, clear everything
    // out to make sure we aren't looping a dirty
    // dma buffer while loading
    if (cls.state == ca_loading) {
        // S_ClearBuffer should be already done in S_StopAllSounds
        return;
    }

    // set listener entity number
    // other parameters should be already set up by CL_CalcViewValues
    if (cls.state != ca_active) {
        listener_entnum = -1;
    } else {
        listener_entnum = cl.frame.clientNum + 1;
    }

    OGG_Update();

    s_api->update();
}
