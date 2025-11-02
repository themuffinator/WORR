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

#include <vector>

#include "SoundSystem.hpp"

#include "sound.hpp"

// =======================================================================
// Internal sound data & structures
// =======================================================================

SoundBackend    s_started;
bool            s_active;
bool            s_supports_float;
const sndapi_t  *s_api;

bool        s_registering;

int         s_paintedtime;  // sample PAIRS

cvar_t      *s_volume;
cvar_t      *s_ambient;
#if USE_DEBUG
cvar_t      *s_show;
#endif
cvar_t      *s_underwater;
cvar_t      *s_underwater_gain_hf;
cvar_t      *s_num_channels;
cvar_t      *s_debug_soundorigins;

static cvar_t   *s_enable;
static cvar_t   *s_auto_focus;

// =======================================================================
// Console functions
// =======================================================================

static void S_SoundInfo_f(void)
{
    if (s_started == SoundBackend::Not) {
        Com_Printf("Sound system not started.\n");
        return;
    }

    s_api->sound_info();
}

static void S_SoundList_f(void)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    int count = 0;
    sfxcache_t  *sc;
    size_t  total;

    total = 0;
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
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

static void S_StopAllSounds_Cmd(void)
{
    S_GetSoundSystem().StopAllSounds();
}

static const cmdreg_t c_sound[] = {
    { "stopsound", S_StopAllSounds_Cmd },
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

/*
================
S_Init
================
*/
void S_Init(void)
{
    s_enable = Cvar_Get("s_enable", "2", CVAR_SOUND);
    if (s_enable->integer <= static_cast<int>(SoundBackend::Not)) {
        Com_Printf("Sound initialization disabled.\n");
        return;
    }

    Com_Printf("------- S_Init -------\n");

    SoundSystem &soundSystem = S_GetSoundSystem();
    soundSystem.Configure({ SoundSystem::kDefaultMaxSfx, SoundSystem::kDefaultMaxPlaysounds });

    s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE);
    s_ambient = Cvar_Get("s_ambient", "1", 0);
#if USE_DEBUG
    s_show = Cvar_Get("s_show", "0", 0);
#endif
    s_auto_focus = Cvar_Get("s_auto_focus", "2", 0);
    s_underwater = Cvar_Get("s_underwater", "1", 0);
    s_underwater_gain_hf = Cvar_Get("s_underwater_gain_hf", "0.25", 0);
    s_num_channels = Cvar_Get("s_num_channels", "64", CVAR_SOUND);
    s_debug_soundorigins = Cvar_Get("s_debugSoundOrigins", "0", 0);

    int max_channels = Cvar_ClampInteger(s_num_channels, 16, 256);
    soundSystem.set_channel_capacity(max_channels);
    soundSystem.num_channels() = 0;

    // start one of available sound engines
    s_started = SoundBackend::Not;

#if USE_OPENAL
    if (s_started == SoundBackend::Not && s_enable->integer >= static_cast<int>(SoundBackend::OpenAL) && snd_openal.init()) {
        s_started = SoundBackend::OpenAL;
        s_api = &snd_openal;
    }
#endif

#if USE_SNDDMA
    if (s_started == SoundBackend::Not && s_enable->integer >= static_cast<int>(SoundBackend::DMA) && snd_dma.init()) {
        s_started = SoundBackend::DMA;
        s_api = &snd_dma;
    }
#endif

    if (s_started == SoundBackend::Not) {
        Com_EPrintf("Sound failed to initialize.\n");
        goto fail;
    }

    Cmd_Register(c_sound);

    // init playsound list
    // clear DMA buffer
    soundSystem.StopAllSounds();

    s_auto_focus->changed = s_auto_focus_changed;
    s_auto_focus_changed(s_auto_focus);

    soundSystem.set_num_sfx(0);

    s_paintedtime = 0;

    soundSystem.reset_registration_sequence();

    // start the cd track
    OGG_Play();

fail:
    Cvar_SetInteger(s_enable, static_cast<int>(s_started), FROM_CODE);
    Com_Printf("----------------------\n");
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

static void S_FreeSound(sfx_t *sfx)
{
    if (s_started != SoundBackend::Not && s_api->delete_sfx)
        s_api->delete_sfx(sfx);
    Z_Free(sfx->cache);
    Z_Free(sfx->truename);
    memset(sfx, 0, sizeof(*sfx));
}

void S_FreeAllSounds(void)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();

    // free all sounds
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (!sfx->name[0])
            continue;
        S_FreeSound(sfx);
    }

    soundSystem.set_num_sfx(0);
}

void S_Shutdown(void)
{
    if (s_started == SoundBackend::Not)
        return;

    S_GetSoundSystem().StopAllSounds();
    S_FreeAllSounds();
    OGG_Stop();

    SoundSystem &soundSystem = S_GetSoundSystem();
    soundSystem.clear_channels();
    soundSystem.num_channels() = 0;
    soundSystem.max_channels() = 0;
    soundSystem.ResetPlaysounds();

    s_api->shutdown();
    s_api = NULL;

    s_started = SoundBackend::Not;
    s_active = false;
    s_supports_float = false;

    s_auto_focus->changed = NULL;

    Cmd_Deregister(c_sound);

    Z_LeakTest(TAG_SOUND);
}

void S_Activate(void)
{
    bool active;
    active_t level;

    if (s_started == SoundBackend::Not)
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
sfx_t *SoundSystem::SfxForHandle(qhandle_t hSfx)
{
    if (!hSfx) {
        return NULL;
    }

    Q_assert(hSfx > 0 && hSfx <= num_sfx_);
    return &known_sfx_[hSfx - 1];
}

static sfx_t *S_AllocSfx(void)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();

    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (!sfx->name[0]) {
            return sfx;
        }
    }

    if (num_sfx == soundSystem.max_sfx())
        return NULL;

    sfx_t *sfx = &known_sfx[num_sfx];
    soundSystem.set_num_sfx(num_sfx + 1);
    return sfx;
}

/*
==================
S_FindName

==================
*/
static sfx_t *S_FindName(const char *name, size_t namelen)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();

    // see if already loaded
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (!FS_pathcmp(sfx->name, name)) {
            sfx->registration_sequence = soundSystem.registration_sequence();
            return sfx;
        }
    }

    // allocate new one
    sfx_t *sfx = S_AllocSfx();
    if (sfx) {
        memcpy(sfx->name, name, namelen + 1);
        sfx->registration_sequence = soundSystem.registration_sequence();
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
    S_GetSoundSystem().increment_registration_sequence();
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

    if (s_started == SoundBackend::Not)
        return 0;

    Q_assert(name);

    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();

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
        soundSystem.LoadSound(sfx);
    }

    return static_cast<qhandle_t>((sfx - known_sfx) + 1);
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
    if (sfx && !sfx->truename && !s_registering && !S_GetSoundSystem().LoadSound(sfx)) {
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
    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();

    std::vector<int> sounds;
    sounds.reserve(num_sfx);

    // find sexed sounds
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (sfx->name[0] != '*')
            continue;
        if (sfx->registration_sequence != soundSystem.registration_sequence())
            continue;
        sounds.push_back(i);
    }

    // register sounds for baseclientinfo and other valid clientinfos
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        if (i > 0 && !cl.clientinfo[i - 1].model_name[0])
            continue;
        for (int index : sounds) {
            sfx_t *sfx = &known_sfx[index];
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
    SoundSystem &soundSystem = S_GetSoundSystem();
    sfx_t *known_sfx = soundSystem.known_sfx_data();
    int num_sfx = soundSystem.num_sfx();

    S_RegisterSexedSounds();

    // clear playsound list, so we don't free sfx still present there
    soundSystem.StopAllSounds();

    // free any sounds not from this registration sequence
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (!sfx->name[0])
            continue;
        if (sfx->registration_sequence != soundSystem.registration_sequence()) {
            // don't need this sound
            S_FreeSound(sfx);
            continue;
        }
        // make sure it is paged in
    if (s_started != SoundBackend::Not && s_api->page_in_sfx)
        s_api->page_in_sfx(sfx);
    }

    // load everything in
    for (int i = 0; i < num_sfx; i++) {
        sfx_t *sfx = &known_sfx[i];
        if (!sfx->name[0])
            continue;
        soundSystem.LoadSound(sfx);
    }

    if (s_started != SoundBackend::Not && s_api->end_registration)
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
channel_t *SoundSystem::PickChannel(int entnum, int entchannel)
{
    int         ch_idx;
    int         first_to_die;
    int         life_left;
    channel_t   *ch;

    if (channels_.empty() || num_channels_ <= 0)
        return NULL;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = INT_MAX;
    for (ch_idx = 0; ch_idx < num_channels_; ch_idx++) {
        ch = &channels_[ch_idx];
        // channel 0 never overrides unless out of channels
        if (ch->entnum == entnum && ch->entchannel == entchannel && entchannel != 0) {
            if (entchannel > 255 && ch->sfx)
                return NULL;    // channels >255 only allow single sfx on that channel
            // always override sound from same entity
            first_to_die = ch_idx;
            break;
        }

        // don't let monster sounds override player sounds
        if (ch->entnum == listener_entnum_ && entnum != listener_entnum_ && ch->sfx)
            continue;

        if (ch->end - s_paintedtime < life_left) {
            life_left = ch->end - s_paintedtime;
            first_to_die = ch_idx;
        }
    }

    if (first_to_die == -1)
        return NULL;

    ch = &channels_[first_to_die];
    if (s_api->stop_channel)
        s_api->stop_channel(ch);
    memset(ch, 0, sizeof(*ch));
    ch->has_spatial_offset = false;

    return ch;
}

/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void SoundSystem::IssuePlaysound(playsound_t *ps)
{
    channel_t   *ch;
    sfxcache_t  *sc;

#if USE_DEBUG
    if (s_show->integer)
        Com_Printf("Issue %i\n", ps->begin);
#endif
    // pick a channel to play on
    ch = PickChannel(ps->entnum, ps->entchannel);
    if (!ch) {
        FreePlaysound(ps);
        return;
    }

    sc = LoadSound(ps->sfx);
    if (!sc) {
        Com_Printf("S_IssuePlaysound: couldn't load %s\n", ps->sfx->name);
        FreePlaysound(ps);
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
    FreePlaysound(ps);
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
void SoundSystem::StartSound(const vec3_t origin, int entnum, int entchannel, qhandle_t hSfx, float vol, float attenuation, float timeofs)
{
    sfxcache_t  *sc;
    playsound_t *ps;
    sfx_t       *sfx;

    if (s_started == SoundBackend::Not)
        return;
    if (!s_active)
        return;
    if (!(sfx = SfxForHandle(hSfx)))
        return;

    if (sfx->name[0] == '*') {
        sfx = S_RegisterSexedSound(entnum, sfx->name);
        if (!sfx)
            return;
    }

    // make sure the sound is loaded
    sc = LoadSound(sfx);
    if (!sc)
        return;     // couldn't load the sound's data

    // make the playsound_t
    ps = AllocatePlaysound();
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
    QueuePendingPlay(ps);
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

    S_GetSoundSystem().StartSound(snd.has_position ? snd.pos : NULL,
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
    if (s_started != SoundBackend::Not) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_GetSoundSystem().StartSound(NULL, listener_entnum, 0, sfx, 1, ATTN_NONE, 0);
    }
}

void S_StartLocalSoundOnce(const char *sound)
{
    if (s_started != SoundBackend::Not) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_GetSoundSystem().StartSound(NULL, listener_entnum, 256, sfx, 1, ATTN_NONE, 0);
    }
}

/*
==================
S_StopAllSounds
==================
*/
void SoundSystem::StopAllSounds()
{
    if (s_started == SoundBackend::Not)
        return;

    ResetPlaysounds();

    if (s_api && s_api->stop_all_sounds)
        s_api->stop_all_sounds();

    clear_channels();
}

void S_RawSamples(int samples, int rate, int width, int channels, const void *data)
{
    if (s_started != SoundBackend::Not && s_active)
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

int SoundSystem::BuildSoundList(int *sounds)
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
        } else if (s_ambient->integer == 3 && ent->number != listener_entnum_) {
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
void SoundSystem::SpatializeOrigin(const vec3_t origin, float master_vol, float dist_mult, float *left_vol, float *right_vol, bool stereo)
{
    vec_t       dot;
    vec_t       dist;
    vec_t       lscale, rscale, scale;
    vec3_t      source_vec;

// calculate stereo separation and distance attenuation
    VectorSubtract(origin, listener_origin_, source_vec);

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
        dot = DotProduct(listener_right_, source_vec);
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

/*
============
S_Update

Called once each time through the main loop
============
*/
void SoundSystem::Update()
{
    if (cvar_modified & CVAR_SOUND) {
        Cbuf_AddText(&cmd_buffer, "snd_restart\n");
        cvar_modified &= ~CVAR_SOUND;
        return;
    }

    if (s_started == SoundBackend::Not)
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
        listener_entnum_ = -1;
    } else {
        listener_entnum_ = cl.frame.clientNum + 1;
    }

    OGG_Update();

    s_api->update();
}
