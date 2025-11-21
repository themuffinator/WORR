/*
Copyright (C) 2010 Andrey Nazarov

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

#include "SoundSystem.hpp"
#include "sound.hpp"
#include "qal.hpp"
#include "common/json.hpp"
#include "common/hash_map.hpp"
#include "system/system.hpp"

#include <climits>
#include <cstdint>

// translates from AL coordinate system to quake
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

// OpenAL implementation should support at least this number of sources
#define MIN_CHANNELS    16

static cvar_t       *s_use_reverb;
static cvar_t       *al_reverb_lerp_time;

static cvar_t       *al_timescale;
static cvar_t       *al_merge_looping;

static ALuint       *s_srcnums;
static int          s_numalsources;
static ALuint       s_stream;
static ALuint       s_stream_buffers;
static bool         s_stream_paused;
static bool         s_loop_points;
static bool         s_source_spatialize;
static ALuint       s_framecount;
static uint64_t     s_paintedtime_monotonic;
static uint64_t     s_previous_update_time;
static ALint        s_merge_looping_minval;

static ALuint       s_underwater_filter;
static bool         s_underwater_flag;

// reverb stuff
typedef struct {
    char    material[16];
    int16_t step_id;
} al_reverb_material_t;

typedef struct {
    al_reverb_material_t    *materials; // if null, matches everything
    size_t                  num_materials;
    uint8_t                 preset;
} al_reverb_entry_t;

typedef struct {
    float               dimension;
    al_reverb_entry_t   *reverbs;
    size_t              num_reverbs;
} al_reverb_environment_t;

static size_t                   s_num_reverb_environments;
static al_reverb_environment_t  *s_reverb_environments;

static ALuint       s_reverb_effect;
static ALuint       s_reverb_slot;

static const EFXEAXREVERBPROPERTIES s_reverb_parameters[] = {
    EFX_REVERB_PRESET_GENERIC,
    EFX_REVERB_PRESET_PADDEDCELL,
    EFX_REVERB_PRESET_ROOM,
    EFX_REVERB_PRESET_BATHROOM,
    EFX_REVERB_PRESET_LIVINGROOM,
    EFX_REVERB_PRESET_STONEROOM,
    EFX_REVERB_PRESET_AUDITORIUM,
    EFX_REVERB_PRESET_CONCERTHALL,
    EFX_REVERB_PRESET_CAVE,
    EFX_REVERB_PRESET_ARENA,
    EFX_REVERB_PRESET_HANGAR,
    EFX_REVERB_PRESET_CARPETEDHALLWAY,
    EFX_REVERB_PRESET_HALLWAY,
    EFX_REVERB_PRESET_STONECORRIDOR,
    EFX_REVERB_PRESET_ALLEY,
    EFX_REVERB_PRESET_FOREST,
    EFX_REVERB_PRESET_CITY,
    EFX_REVERB_PRESET_MOUNTAINS,
    EFX_REVERB_PRESET_QUARRY,
    EFX_REVERB_PRESET_PLAIN,
    EFX_REVERB_PRESET_PARKINGLOT,
    EFX_REVERB_PRESET_SEWERPIPE,
    EFX_REVERB_PRESET_UNDERWATER,
    EFX_REVERB_PRESET_DRUGGED,
    EFX_REVERB_PRESET_DIZZY,
    EFX_REVERB_PRESET_PSYCHOTIC
};

static EFXEAXREVERBPROPERTIES   s_active_reverb;
static EFXEAXREVERBPROPERTIES   s_reverb_lerp_to, s_reverb_lerp_result;
static int                      s_reverb_lerp_start, s_reverb_lerp_time;
static uint8_t                  s_reverb_current_preset;

static const char *const s_reverb_names[] = {
    "generic",
    "padded_cell",
    "room",
    "bathroom",
    "living_room",
    "stone_room",
    "auditorium",
    "concert_hall",
    "cave",
    "arena",
    "hangar",
    "carpeted_hallway",
    "hallway",
    "stone_corridor",
    "alley",
    "forest",
    "city",
    "mountains",
    "quarry",
    "plain",
    "parking_lot",
    "sewer_pipe",
    "underwater",
    "drugged",
    "dizzy",
    "psychotic"
};

static void AL_LoadEffect(const EFXEAXREVERBPROPERTIES *reverb)
{
    qalEffectf(s_reverb_effect, AL_EAXREVERB_DENSITY, reverb->flDensity);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_GAIN, reverb->flGain);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_GAINHF, reverb->flGainHF);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_GAINLF, reverb->flGainLF);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_DECAY_TIME, reverb->flDecayTime);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_DECAY_HFRATIO, reverb->flDecayHFRatio);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
    qalEffectfv(s_reverb_effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
    qalEffectfv(s_reverb_effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference);
    qalEffectf(s_reverb_effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
    qalEffecti(s_reverb_effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);

    qalAuxiliaryEffectSloti(s_reverb_slot, AL_EFFECTSLOT_EFFECT, s_reverb_effect);
}

static const vec3_t             s_reverb_probes[] = {
    { 0.00000000f,    0.00000000f,     -1.00000000f },
    { 0.00000000f,    0.00000000f,     1.00000000f },
    { 0.707106769f,   0.00000000f,     0.707106769f },
    { 0.353553385f,   0.612372458f,    0.707106769f },
    { -0.353553444f,  0.612372458f,    0.707106769f },
    { -0.707106769f, -6.18172393e-08f, 0.707106769f },
    { -0.353553325f, -0.612372518f,    0.707106769f },
    { 0.353553355f,  -0.612372458f,    0.707106769f },
    { 1.00000000f,   0.00000000f,      -4.37113883e-08f },
    { 0.499999970f,  0.866025448f,     -4.37113883e-08f },
    { -0.500000060f, 0.866025388f,     -4.37113883e-08f },
    { -1.00000000f,  -8.74227766e-08f, -4.37113883e-08f },
    { -0.499999911f, -0.866025448f,    -4.37113883e-08f },
    { 0.499999911f,  -0.866025448f,    -4.37113883e-08f },
};
static int                      s_reverb_probe_time;
static int                      s_reverb_probe_index;
static vec3_t                   s_reverb_probe_results[q_countof(s_reverb_probes)];
static float                    s_reverb_probe_avg;

static const al_reverb_environment_t  *s_reverb_active_environment;

static bool AL_EstimateDimensions(void)
{
    if (!s_reverb_environments)
        return false;

    if (s_reverb_probe_time > cl.time)
        return false;

    s_reverb_probe_time = cl.time + 13;
    SoundSystem &soundSystem = S_GetSoundSystem();
    const vec3_t &listener_origin = soundSystem.listener_origin();
    vec3_t end;
    VectorMA(listener_origin, 8192.0f, s_reverb_probes[s_reverb_probe_index], end);

    trace_t tr;
    CL_Trace(&tr, listener_origin, end, vec3_origin, vec3_origin, NULL, MASK_SOLID);

    VectorSubtract(tr.endpos, listener_origin, s_reverb_probe_results[s_reverb_probe_index]);

    if (s_reverb_probe_index == 1 && (tr.surface->flags & SURF_SKY)) {
        s_reverb_probe_results[s_reverb_probe_index][2] += 4096.f;
    }

    vec3_t mins, maxs;
    ClearBounds(mins, maxs);

    for (size_t i = 0; i < q_countof(s_reverb_probes); i++)
        AddPointToBounds(s_reverb_probe_results[i], mins, maxs);

    vec3_t extents;
    VectorSubtract(maxs, mins, extents);

    s_reverb_probe_avg = (extents[0] + extents[1] + extents[2]) / 3.0f;

    s_reverb_probe_index = (s_reverb_probe_index + 1) % q_countof(s_reverb_probes);

    // check if we expanded or shrank the environment
    bool changed = false;

    while (s_reverb_active_environment != s_reverb_environments + s_num_reverb_environments - 1 &&
        s_reverb_probe_avg > s_reverb_active_environment->dimension) {
        s_reverb_active_environment++;
        changed = true;
    }

    if (!changed) {
        while (s_reverb_active_environment != s_reverb_environments && s_reverb_probe_avg < (s_reverb_active_environment - 1)->dimension) {
            s_reverb_active_environment--;
            changed = true;
        }
    }

    return changed;
}

static inline float AL_CalculateReverbFrac(void)
{
    float frac = (cl.time - (float) s_reverb_lerp_start) / (s_reverb_lerp_time - (float) s_reverb_lerp_start);
    float bfrac = 1.0f - frac;
    float f = Q_clipf(1.0f - (bfrac * bfrac * bfrac), 0.0f, 1.0f);
    return f;
}

static void AL_UpdateReverb(void)
{
    if (!s_reverb_environments)
        return;

    if (!cl.bsp)
        return;

    AL_EstimateDimensions();

    SoundSystem &soundSystem = S_GetSoundSystem();
    const vec3_t &listener_origin = soundSystem.listener_origin();

    trace_t tr;
    const vec3_t mins = { -16, -16, 0 };
    const vec3_t maxs = { 16, 16, 0 };
    const vec3_t listener_start = { listener_origin[0], listener_origin[1], listener_origin[2] + 1.0f };
    const vec3_t listener_down = { listener_start[0], listener_start[1], listener_start[2] - 256.0f };
    CL_Trace(&tr, listener_down, mins, maxs, listener_start, NULL, MASK_SOLID);

    uint8_t new_preset = s_reverb_current_preset;

    if (tr.fraction < 1.0f && tr.surface->id) {
        const mtexinfo_t *surf_info = cl.bsp->texinfo + (tr.surface->id - 1);
        int16_t id = surf_info->step_id;

        for (size_t i = 0; i < s_reverb_active_environment->num_reverbs; i++) {
            const al_reverb_entry_t *entry = &s_reverb_active_environment->reverbs[i];

            if (!entry->num_materials) {
                new_preset = entry->preset;
                break;
            }

            size_t m = 0;

            for (; m < entry->num_materials; m++)
                if (entry->materials[m].step_id == id) {
                    new_preset = entry->preset;
                    break;
                }

            if (m != entry->num_materials)
                break;
        }
    } else {
        new_preset = 19;
    }

    if (new_preset != s_reverb_current_preset) {
        s_reverb_current_preset = new_preset;

        if (s_reverb_lerp_time) {
            memcpy(&s_active_reverb, &s_reverb_lerp_result, sizeof(s_reverb_lerp_result));
        }

        s_reverb_lerp_start = cl.time;
        s_reverb_lerp_time = cl.time + (al_reverb_lerp_time->value * 1000);
        memcpy(&s_reverb_lerp_to, &s_reverb_parameters[s_reverb_current_preset], sizeof(s_active_reverb));
    }

    if (s_reverb_lerp_time) {
        if (cl.time >= s_reverb_lerp_time) {
            s_reverb_lerp_time = 0;
            memcpy(&s_active_reverb, &s_reverb_lerp_to, sizeof(s_active_reverb));
            AL_LoadEffect(&s_active_reverb);
        } else {
            float f = AL_CalculateReverbFrac();

#define AL_LERP(prop) \
                s_reverb_lerp_result.prop = FASTLERP(s_active_reverb.prop, s_reverb_lerp_to.prop, f)
            
            AL_LERP(flDensity);
            AL_LERP(flDiffusion);
            AL_LERP(flGain);
            AL_LERP(flGainHF);
            AL_LERP(flGainLF);
            AL_LERP(flDecayTime);
            AL_LERP(flDecayHFRatio);
            AL_LERP(flDecayLFRatio);
            AL_LERP(flReflectionsGain);
            AL_LERP(flReflectionsDelay);
            AL_LERP(flReflectionsPan[0]);
            AL_LERP(flReflectionsPan[1]);
            AL_LERP(flReflectionsPan[2]);
            AL_LERP(flLateReverbGain);
            AL_LERP(flLateReverbDelay);
            AL_LERP(flLateReverbPan[0]);
            AL_LERP(flLateReverbPan[1]);
            AL_LERP(flLateReverbPan[2]);
            AL_LERP(flEchoTime);
            AL_LERP(flEchoDepth);
            AL_LERP(flModulationTime);
            AL_LERP(flModulationDepth);
            AL_LERP(flAirAbsorptionGainHF);
            AL_LERP(flHFReference);
            AL_LERP(flLFReference);
            AL_LERP(flRoomRolloffFactor);
            AL_LERP(iDecayHFLimit);

            AL_LoadEffect(&s_reverb_lerp_result);
        }
    }
}

static void AL_LoadReverbEntry(json_parse_t *parser, al_reverb_entry_t *out_entry)
{
    size_t fields = parser->pos->size;
    Json_EnsureNext(parser, JSMN_OBJECT);

    for (size_t i = 0; i < fields; i++) {
        if (!Json_Strcmp(parser, "materials")) {
            parser->pos++;

            if (parser->pos->type == JSMN_STRING) {
                if (parser->buffer[parser->pos->start] != '*')
                    Json_Error(parser, parser->pos, "expected string to start with *\n");

                parser->pos++;
            } else {
                size_t n = parser->pos->size;
                Json_EnsureNext(parser, JSMN_ARRAY);
                out_entry->num_materials = n;
                out_entry->materials = Z_TagMalloc(sizeof(*out_entry->materials) * n, TAG_SOUND);

                for (size_t m = 0; m < n; m++, parser->pos++) {
                    Json_Ensure(parser, JSMN_STRING);
                    Q_strnlcpy(out_entry->materials[m].material,
                        parser->buffer + parser->pos->start,
                        parser->pos->end - parser->pos->start,
                        sizeof(out_entry->materials[m].material));
                }
            }

        } else if (!Json_Strcmp(parser, "preset")) {
            parser->pos++;

            Json_Ensure(parser, JSMN_STRING);
            size_t p = 0;

            for (; p < q_countof(s_reverb_names); p++)
                if (!Json_Strcmp(parser, s_reverb_names[p]))
                    break;

            if (p == q_countof(s_reverb_names)) {
                Com_WPrintf("missing sound environment preset\n");
                out_entry->preset = 19; // plain
            } else {
                out_entry->preset = p;
            }

            parser->pos++;
        } else {
            parser->pos++;
            Json_SkipToken(parser);
        }
    }
}

static void AL_LoadReverbEnvironment(json_parse_t *parser, al_reverb_environment_t *out_environment)
{
    size_t fields = parser->pos->size;
    Json_EnsureNext(parser, JSMN_OBJECT);

    for (size_t i = 0; i < fields; i++) {
        if (!Json_Strcmp(parser, "dimension")) {
            Json_Next(parser);
            Json_Ensure(parser, JSMN_PRIMITIVE);
            out_environment->dimension = atof(parser->buffer + parser->pos->start);
            parser->pos++;
        } else if (!Json_Strcmp(parser, "reverbs")) {
            Json_Next(parser);

            out_environment->num_reverbs = parser->pos->size;
            Json_EnsureNext(parser, JSMN_ARRAY);
            out_environment->reverbs = Z_TagMallocz(sizeof(al_reverb_entry_t) * out_environment->num_reverbs, TAG_SOUND);

            for (size_t r = 0; r < out_environment->num_reverbs; r++)
                AL_LoadReverbEntry(parser, &out_environment->reverbs[r]);
        } else {
            parser->pos++;
            Json_SkipToken(parser);
        }
    }
}

static void AL_FreeReverbEnvironments(al_reverb_environment_t *environments, size_t num_environments)
{
    for (size_t i = 0; i < num_environments; i++) {
        for (size_t n = 0; n < environments[i].num_reverbs; n++) {
            Z_Free(environments[i].reverbs[n].materials);
        }

        Z_Free(environments[i].reverbs);
    }

    Z_Free(environments);
}

/*
=============
AL_FindStepID

Looks up the footstep ID for the given material using the BSP material cache.
=============
*/
static int16_t AL_FindStepID(const char *material)
{
	if (!strcmp(material, "") || !strcmp(material, "default"))
		return FOOTSTEP_ID_DEFAULT;
	else if (!strcmp(material, "ladder"))
		return FOOTSTEP_ID_LADDER;

	if (!cl.bsp || !cl.bsp->material_step_ids)
		return FOOTSTEP_ID_DEFAULT;

	int *step_id = HashMap_Lookup(int, cl.bsp->material_step_ids, &material);
	if (step_id)
		return *step_id;

	return FOOTSTEP_ID_DEFAULT;
}

static void AL_SetReverbStepIDs(void)
{
    for (size_t i = 0; i < s_num_reverb_environments; i++) {
        for (size_t n = 0; n < s_reverb_environments[i].num_reverbs; n++) {
            al_reverb_entry_t *entry = &s_reverb_environments[i].reverbs[n];

            for (size_t e = 0; e < entry->num_materials; e++) {
                entry->materials[e].step_id = AL_FindStepID(entry->materials[e].material);
            }
        }
    }
}

static void AL_LoadReverbEnvironments(void)
{
	json_parse_t parser = {0};
	al_reverb_environment_t *environments = NULL;
	size_t n = 0;

	if (Json_ErrorHandler(&parser)) {
		Com_WPrintf("Couldn't load sound/default.environments[%s]; %s\n", parser.error_loc, parser.error);
		AL_FreeReverbEnvironments(environments, n);
		return;
	}

	Json_Load("sound/default.environments", &parser);

	Json_EnsureNext(&parser, JSMN_OBJECT);

    if (Json_Strcmp(&parser, "environments")) 
        Json_Error(&parser, parser.pos, "expected \"environments\" key\n");

    Json_Next(&parser);

    n = parser.pos->size;
    if (n == 0) {
        s_reverb_environments = NULL;
        s_num_reverb_environments = 0;
        Json_Free(&parser);
        return;
    }
    Json_EnsureNext(&parser, JSMN_ARRAY);

    environments = Z_TagMallocz(sizeof(al_reverb_environment_t) * n, TAG_SOUND);

    for (size_t i = 0; i < n; i++)
        AL_LoadReverbEnvironment(&parser, &environments[i]);

    s_reverb_environments = environments;
    s_num_reverb_environments = n;

    Json_Free(&parser);
}

static void AL_Reverb_stat(void)
{
    SCR_StatKeyValue("dimensions", va("%g", s_reverb_probe_avg));
    SCR_StatKeyValue("env dim", va("%g", s_reverb_active_environment->dimension));
    SCR_StatKeyValue("preset", s_reverb_names[s_reverb_current_preset]);

#define AL_STATF(e) SCR_StatKeyValue(#e, va("%g", s_reverb_lerp_result.e))
#define AL_STATI(e) SCR_StatKeyValue(#e, va("%d", s_reverb_lerp_result.e))
    
    AL_STATF(flDensity);
    AL_STATF(flDiffusion);
    AL_STATF(flGain);
    AL_STATF(flGainHF);
    AL_STATF(flGainLF);
    AL_STATF(flDecayTime);
    AL_STATF(flDecayHFRatio);
    AL_STATF(flDecayLFRatio);
    AL_STATF(flReflectionsGain);
    AL_STATF(flReflectionsDelay);
    AL_STATF(flLateReverbGain);
    AL_STATF(flLateReverbDelay);
    AL_STATF(flEchoTime);
    AL_STATF(flEchoDepth);
    AL_STATF(flModulationTime);
    AL_STATF(flModulationDepth);
    AL_STATF(flAirAbsorptionGainHF);
    AL_STATF(flHFReference);
    AL_STATF(flLFReference);
    AL_STATF(flRoomRolloffFactor);
    AL_STATI(iDecayHFLimit);

    SCR_StatKeyValue("lerp", !s_reverb_lerp_time ? "none" : va("%g", AL_CalculateReverbFrac()));
}

static void AL_StreamStop(void);
static void AL_StopChannel(channel_t *ch);

static void AL_SoundInfo(void)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));
    Com_Printf("Number of sources: %d\n", soundSystem.num_channels());
}

static void s_underwater_gain_hf_changed(cvar_t *self)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    if (s_underwater_flag) {
        int num_channels = soundSystem.num_channels();
        for (int i = 0; i < num_channels; i++)
            qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, 0);
        s_underwater_flag = false;
    }

    qalFilterf(s_underwater_filter, AL_LOWPASS_GAINHF, Cvar_ClampValue(self, 0.001f, 1));
}

static void al_reverb_changed(cvar_t *self)
{
    S_GetSoundSystem().StopAllSounds();
}

static void al_merge_looping_changed(cvar_t *self)
{
    int         i;
    channel_t   *ch;

    SoundSystem &soundSystem = S_GetSoundSystem();
    channel_t *channels = soundSystem.channels_data();
    int num_channels = soundSystem.num_channels();

    for (i = 0, ch = channels; channels && i < num_channels; i++, ch++) {
        if (ch->autosound)
            AL_StopChannel(ch);
    }
}

static void s_volume_changed(cvar_t *self)
{
    qalListenerf(AL_GAIN, self->value);
}

static bool AL_Init(void)
{
    int i;
    int max_channels = 0;

    SoundSystem &soundSystem = S_GetSoundSystem();

    i = QAL_Init();
    if (i < 0)
        goto fail0;
    s_merge_looping_minval = i + 1;

    Com_DPrintf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_DPrintf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_DPrintf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_DDPrintf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

    // generate source names
    qalGetError();
    qalGenSources(1, &s_stream);

    max_channels = soundSystem.max_channels();
    s_srcnums = Z_TagMalloc(sizeof(*s_srcnums) * max_channels, TAG_SOUND);

    for (i = 0; i < max_channels; i++) {
        qalGenSources(1, &s_srcnums[i]);
        if (qalGetError() != AL_NO_ERROR) {
            break;
        }
        s_numalsources++;
    }

    if (s_numalsources != max_channels)
        s_srcnums = Z_Realloc(s_srcnums, sizeof(*s_srcnums) * s_numalsources);

    Com_DPrintf("Got %d AL sources\n", i);

    if (i < MIN_CHANNELS) {
        Com_SetLastError("Insufficient number of AL sources");
        goto fail1;
    }

    soundSystem.num_channels() = i;

    s_volume->changed = s_volume_changed;
    s_volume_changed(s_volume);

    al_merge_looping = Cvar_Get("al_merge_looping", "1", 0);
    al_merge_looping->changed = al_merge_looping_changed;

    s_loop_points = qalIsExtensionPresent("AL_SOFT_loop_points");
    s_source_spatialize = qalIsExtensionPresent("AL_SOFT_source_spatialize");
    s_supports_float = qalIsExtensionPresent("AL_EXT_float32");

    // init distance model
    qalDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

    // init stream source
    qalSourcef(s_stream, AL_ROLLOFF_FACTOR, 0.0f);
    qalSourcei(s_stream, AL_SOURCE_RELATIVE, AL_TRUE);
    if (s_source_spatialize)
        qalSourcei(s_stream, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);

    if (qalIsExtensionPresent("AL_SOFT_direct_channels_remix"))
        qalSourcei(s_stream, AL_DIRECT_CHANNELS_SOFT, AL_REMIX_UNMATCHED_SOFT);
    else if (qalIsExtensionPresent("AL_SOFT_direct_channels"))
        qalSourcei(s_stream, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);

    // init underwater filter
    if (qalGenFilters && qalGetEnumValue("AL_FILTER_LOWPASS")) {
        qalGenFilters(1, &s_underwater_filter);
        qalFilteri(s_underwater_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        s_underwater_gain_hf->changed = s_underwater_gain_hf_changed;
        s_underwater_gain_hf_changed(s_underwater_gain_hf);
    }

    if (qalGenEffects && qalGetEnumValue("AL_EFFECT_EAXREVERB")) {
        qalGenEffects(1, &s_reverb_effect);
        qalGenAuxiliaryEffectSlots(1, &s_reverb_slot);
        qalEffecti(s_reverb_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    }

    s_use_reverb = Cvar_Get("s_use_reverb", "1", 0);
    s_use_reverb->changed = al_reverb_changed;
    al_reverb_lerp_time = Cvar_Get("al_reverb_lerp_time", "3.0", 0);

    al_timescale = Cvar_Get("al_timescale", "1", 0);

    SCR_RegisterStat("s_use_reverb", AL_Reverb_stat);

    Com_Printf("OpenAL initialized.\n");
    return true;

fail1:
    QAL_Shutdown();
fail0:
    Com_EPrintf("Failed to initialize OpenAL: %s\n", Com_GetLastError());
    return false;
}

static void AL_Shutdown(void)
{
    Com_Printf("Shutting down OpenAL.\n");

    SoundSystem &soundSystem = S_GetSoundSystem();
    int num_channels = soundSystem.num_channels();

    if (num_channels) {
        // delete source names
        qalDeleteSources(num_channels, s_srcnums);
        Z_Free(s_srcnums);
        s_srcnums = NULL;
        s_numalsources = 0;
        soundSystem.num_channels() = 0;
        soundSystem.clear_channels();
    }

    if (s_stream) {
        AL_StreamStop();
        qalDeleteSources(1, &s_stream);
        s_stream = 0;
    }

    if (s_underwater_filter) {
        qalDeleteFilters(1, &s_underwater_filter);
        s_underwater_filter = 0;
    }

    if (s_reverb_effect) {
        qalDeleteEffects(1, &s_reverb_effect);
        s_reverb_effect = 0;
    }

    if (s_reverb_slot) {
        qalDeleteAuxiliaryEffectSlots(1, &s_reverb_slot);
        s_reverb_slot = 0;
    }

    AL_FreeReverbEnvironments(s_reverb_environments, s_num_reverb_environments);
    s_reverb_environments = NULL;
    s_num_reverb_environments = 0;

    s_underwater_flag = false;
    s_underwater_gain_hf->changed = NULL;
    s_volume->changed = NULL;
    al_merge_looping->changed = NULL;

    SCR_UnregisterStat("s_use_reverb");

    QAL_Shutdown();
}

static ALenum AL_GetSampleFormat(int width, int channels)
{
    if (channels < 1 || channels > 2)
        return 0;

    switch (width) {
    case 1:
        return AL_FORMAT_MONO8 + (channels - 1) * 2;
    case 2:
        return AL_FORMAT_MONO16 + (channels - 1) * 2;
    case 4:
        if (!s_supports_float)
            return 0;
        return AL_FORMAT_MONO_FLOAT32 + (channels - 1);
    default:
        return 0;
    }
}

static sfxcache_t *AL_UploadSfx(sfx_t *s)
{
    ALsizei size = s_info.samples * s_info.width * s_info.channels;
    ALenum format = AL_GetSampleFormat(s_info.width, s_info.channels);
    ALuint buffer = 0;
    sfxcache_t *sc = nullptr;

    if (!format) {
        Com_SetLastError("Unsupported sample format");
        goto fail;
    }

    qalGetError();
    qalGenBuffers(1, &buffer);
    if (qalGetError()) {
        Com_SetLastError("Failed to generate buffer");
        goto fail;
    }

    qalBufferData(buffer, format, s_info.data, size, s_info.rate);
    if (qalGetError()) {
        Com_SetLastError("Failed to upload samples");
        qalDeleteBuffers(1, &buffer);
        goto fail;
    }

    // specify OpenAL-Soft style loop points
    if (s_info.loopstart > 0 && s_loop_points) {
        ALint points[2] = { s_info.loopstart, s_info.samples };
        qalBufferiv(buffer, AL_LOOP_POINTS_SOFT, points);
    }

    // allocate placeholder sfxcache
    sc = s->cache = S_Malloc(sizeof(*sc));
    sc->length = s_info.samples * 1000LL / s_info.rate; // in msec
    sc->loopstart = s_info.loopstart;
    sc->width = s_info.width;
    sc->channels = s_info.channels;
    sc->size = size;
    sc->bufnum = buffer;

    return sc;

fail:
    s->error = Q_ERR_LIBRARY_ERROR;
    return NULL;
}

static void AL_DeleteSfx(sfx_t *s)
{
    sfxcache_t *sc = s->cache;
    if (sc) {
        ALuint name = sc->bufnum;
        qalDeleteBuffers(1, &name);
    }
}

static int AL_GetBeginofs(float timeofs)
{
    return s_paintedtime + timeofs * 1000;
}

static void AL_Spatialize(channel_t *ch)
{
    ch->has_spatial_offset = false;

    // merged autosounds are handled differently
    if (ch->autosound && al_merge_looping->integer >= s_merge_looping_minval)
        return;

    // anything coming from the view entity will always be full volume
    bool fullvolume = S_IsFullVolume(ch);
    bool channel_fullvolume = ch->fullvolume != 0;

    // update fullvolume flag if needed
    if (channel_fullvolume != fullvolume) {
        if (s_source_spatialize) {
            qalSourcei(ch->srcnum, AL_SOURCE_SPATIALIZE_SOFT, !fullvolume);
        }
        qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, fullvolume);
        if (fullvolume) {
            qalSource3f(ch->srcnum, AL_POSITION, 0, 0, 0);
        } else if (ch->fixed_origin) {
            qalSource3f(ch->srcnum, AL_POSITION, AL_UnpackVector(ch->origin));
        }
        ch->fullvolume = fullvolume ? 1 : 0;
        channel_fullvolume = fullvolume;
    }

    // update position if needed
    if (!ch->fixed_origin && !channel_fullvolume) {
        vec3_t origin, offset;
        ch->has_spatial_offset = false;
        if (CL_GetEntitySoundOrigin(ch->entnum, origin, offset)) {
            ch->has_spatial_offset = true;
            VectorCopy(offset, ch->spatial_offset);
            if (s_debug_soundorigins->integer) {
                vec3_t base;
                VectorSubtract(origin, offset, base);
                CL_DebugTrail(base, origin);
            }
        }
        qalSource3f(ch->srcnum, AL_POSITION, AL_UnpackVector(origin));
    } else {
        ch->has_spatial_offset = false;
    }

    if (al_timescale->integer) {
        const float engine_scale = Cvar_VariableValue("timescale");
        const float time_scale = CL_ActiveTimeScale() * engine_scale;
        const float min_pitch = cl.slow_time.active ? 0.1f : 0.75f;
        qalSourcef(ch->srcnum, AL_PITCH, max(min_pitch, time_scale));
    } else {
        qalSourcef(ch->srcnum, AL_PITCH, 1.0f);
    }
}

static void AL_StopChannel(channel_t *ch)
{
    if (!ch->sfx)
        return;

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    // stop it
    qalSourceStop(ch->srcnum);
    qalSourcei(ch->srcnum, AL_BUFFER, AL_NONE);
    memset(ch, 0, sizeof(*ch));
}

static void AL_PlayChannel(channel_t *ch)
{
    sfxcache_t *sc = ch->sfx->cache;

    SoundSystem &soundSystem = S_GetSoundSystem();
    channel_t *channels = soundSystem.channels_data();

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    if (channels) {
        ch->srcnum = s_srcnums[ch - channels];
    } else {
        ch->srcnum = 0;
    }
    qalGetError();
    qalSourcei(ch->srcnum, AL_BUFFER, sc->bufnum);
    qalSourcei(ch->srcnum, AL_LOOPING, ch->autosound || sc->loopstart >= 0);
    qalSourcef(ch->srcnum, AL_GAIN, ch->master_vol);
    qalSourcef(ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME);
    qalSourcef(ch->srcnum, AL_MAX_DISTANCE, 8192);
    qalSourcef(ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * (8192 - SOUND_FULLVOLUME));

    if (cl.bsp && s_reverb_slot && s_use_reverb->integer) {
        qalSource3i(ch->srcnum, AL_AUXILIARY_SEND_FILTER, s_reverb_slot, 0, AL_FILTER_NULL);
    } else {
        qalSource3i(ch->srcnum, AL_AUXILIARY_SEND_FILTER, AL_EFFECT_NULL, 0, AL_FILTER_NULL);
    }

    // force update
    ch->fullvolume = -1;
    AL_Spatialize(ch);

    // play it
    qalSourcePlay(ch->srcnum);
    if (qalGetError() != AL_NO_ERROR) {
        AL_StopChannel(ch);
    } else {
        if (ch->autosound) {
            qalSourcef(ch->srcnum, AL_SEC_OFFSET, fmodf(cls.realtime / 1000.f, ch->sfx->cache->length / 1000.f));
        }
    }
}

static void AL_IssuePlaysounds(void)
{
    SoundSystem &soundSystem = S_GetSoundSystem();
    // start any playsounds
    while (1) {
        playsound_t *ps = soundSystem.PeekPendingPlay();
        if (!ps)
            break;  // no more pending sounds
        if (ps->begin > s_paintedtime)
            break;
        S_GetSoundSystem().IssuePlaysound(ps);
    }
}

static void AL_StopAllSounds(void)
{
    int         i;
    channel_t   *ch;

    SoundSystem &soundSystem = S_GetSoundSystem();
    channel_t *channels = soundSystem.channels_data();
    int num_channels = soundSystem.num_channels();

    for (i = 0, ch = channels; channels && i < num_channels; i++, ch++) {
        if (!ch->sfx)
            continue;
        AL_StopChannel(ch);
    }
}

static channel_t *AL_FindLoopingSound(int entnum, const sfx_t *sfx)
{
    int         i;
    channel_t   *ch;

    SoundSystem &soundSystem = S_GetSoundSystem();
    channel_t *channels = soundSystem.channels_data();
    int num_channels = soundSystem.num_channels();

    for (i = 0, ch = channels; channels && i < num_channels; i++, ch++) {
        if (!ch->autosound)
            continue;
        if (entnum && ch->entnum != entnum)
            continue;
        if (ch->sfx != sfx)
            continue;
        return ch;
    }

    return NULL;
}

static void AL_MergeLoopSounds(void)
{
    int         i, j;
    int         sounds[MAX_EDICTS];
    float       left, right, left_total, right_total, vol, att;
    float       pan, pan2, gain;
    channel_t   *ch;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t *ent;
    vec3_t      origin;

    if (!S_GetSoundSystem().BuildSoundList(sounds))
        return;

    for (i = 0; i < cl.frame.numEntities; i++) {
        if (!sounds[i])
            continue;

        sfx = S_GetSoundSystem().SfxForHandle(cl.sound_precache[sounds[i]]);
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];

        vol = S_GetEntityLoopVolume(ent);
        att = S_GetEntityLoopDistMult(ent);

        // find the total contribution of all sounds of this type
        vec3_t offset;
        bool anchored = CL_GetEntitySoundOrigin(ent->number, origin, offset);
        if (anchored && s_debug_soundorigins->integer) {
            vec3_t base;
            VectorSubtract(origin, offset, base);
            CL_DebugTrail(base, origin);
        }
        S_GetSoundSystem().SpatializeOrigin(origin, vol, att,
                           &left_total, &right_total,
                           S_GetEntityLoopStereoPan(ent));
        for (j = i + 1; j < cl.frame.numEntities; j++) {
            if (sounds[j] != sounds[i])
                continue;
            sounds[j] = 0;  // don't check this again later

            num = (cl.frame.firstEntity + j) & PARSE_ENTITIES_MASK;
            ent = &cl.entityStates[num];

            anchored = CL_GetEntitySoundOrigin(ent->number, origin, offset);
            if (anchored && s_debug_soundorigins->integer) {
                vec3_t base;
                VectorSubtract(origin, offset, base);
                CL_DebugTrail(base, origin);
            }
            S_GetSoundSystem().SpatializeOrigin(origin,
                               S_GetEntityLoopVolume(ent),
                               S_GetEntityLoopDistMult(ent),
                               &left, &right,
                               S_GetEntityLoopStereoPan(ent));
            left_total += left;
            right_total += right;
        }

        if (left_total == 0 && right_total == 0)
            continue;       // not audible

        left_total = min(1.0f, left_total);
        right_total = min(1.0f, right_total);

        gain = left_total + right_total;

        pan  = (right_total - left_total) / (left_total + right_total);
        pan2 = -sqrtf(1.0f - pan * pan);

        ch = AL_FindLoopingSound(0, sfx);
        if (ch) {
            qalSourcef(ch->srcnum, AL_GAIN, gain);
            qalSource3f(ch->srcnum, AL_POSITION, pan, 0.0f, pan2);
            ch->autoframe = s_framecount;
            ch->end = s_paintedtime + sc->length;
            continue;
        }

        // allocate a channel
        ch = S_GetSoundSystem().PickChannel(0, 0);
        if (!ch)
            continue;

        channel_t *channels = S_GetSoundSystem().channels_data();
        if (channels) {
            ch->srcnum = s_srcnums[ch - channels];
        } else {
            ch->srcnum = 0;
        }
        qalGetError();
        qalSourcei(ch->srcnum, AL_BUFFER, sc->bufnum);
        qalSourcei(ch->srcnum, AL_LOOPING, AL_TRUE);
        qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_TRUE);
        if (s_source_spatialize) {
            qalSourcei(ch->srcnum, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
        }
        qalSourcef(ch->srcnum, AL_ROLLOFF_FACTOR, 0.0f);
        qalSourcef(ch->srcnum, AL_GAIN, gain);
        qalSource3f(ch->srcnum, AL_POSITION, pan, 0.0f, pan2);

        ch->autosound = true;   // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = vol;
        ch->dist_mult = att;
        ch->end = s_paintedtime + sc->length;

        // play it
        qalSourcePlay(ch->srcnum);
        if (qalGetError() != AL_NO_ERROR) {
            AL_StopChannel(ch);
        }
    }
}

static void AL_AddLoopSounds(void)
{
    int         i;
    int         sounds[MAX_EDICTS];
    channel_t   *ch;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t *ent;

    if (cls.state != ca_active || sv_paused->integer || !s_ambient->integer)
        return;

    S_GetSoundSystem().BuildSoundList(sounds);

    for (i = 0; i < cl.frame.numEntities; i++) {
        if (!sounds[i])
            continue;

        sfx = S_GetSoundSystem().SfxForHandle(cl.sound_precache[sounds[i]]);
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];

        ch = AL_FindLoopingSound(ent->number, sfx);
        if (ch) {
            ch->autoframe = s_framecount;
            ch->end = s_paintedtime + sc->length;
            continue;
        }

        // allocate a channel
        ch = S_GetSoundSystem().PickChannel(0, 0);
        if (!ch)
            continue;

        ch->autosound = true;   // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = S_GetEntityLoopVolume(ent);
        ch->dist_mult = S_GetEntityLoopDistMult(ent);
        ch->end = s_paintedtime + sc->length;

        AL_PlayChannel(ch);
    }
}

#define MAX_STREAM_BUFFERS  32

static void AL_StreamUpdate(void)
{
    ALint num_buffers = 0;
    qalGetSourcei(s_stream, AL_BUFFERS_PROCESSED, &num_buffers);

    while (num_buffers > 0) {
        ALuint buffers[MAX_STREAM_BUFFERS];
        ALsizei n = min(num_buffers, q_countof(buffers));
        Q_assert(s_stream_buffers >= n);

        qalSourceUnqueueBuffers(s_stream, n, buffers);
        qalDeleteBuffers(n, buffers);

        s_stream_buffers -= n;
        num_buffers -= n;
    }
}

static void AL_StreamStop(void)
{
    qalSourceStop(s_stream);
    AL_StreamUpdate();
    Q_assert(!s_stream_buffers);
    s_stream_paused = false;
}

static void AL_StreamPause(bool paused)
{
    s_stream_paused = paused;

    // force pause if not active
    if (!s_active)
        paused = true;

    ALint state = 0;
    qalGetSourcei(s_stream, AL_SOURCE_STATE, &state);

    if (paused && state == AL_PLAYING)
        qalSourcePause(s_stream);

    if (!paused && state != AL_PLAYING && s_stream_buffers)
        qalSourcePlay(s_stream);
}

static int AL_NeedRawSamples(void)
{
    return s_stream_buffers < MAX_STREAM_BUFFERS ? MAX_RAW_SAMPLES : 0;
}

static int AL_HaveRawSamples(void)
{
    return s_stream_buffers * MAX_RAW_SAMPLES;
}

static bool AL_RawSamples(int samples, int rate, int width, int channels, const void *data, float volume)
{
    ALenum format = AL_GetSampleFormat(width, channels);
    if (!format)
        return false;

    if (AL_NeedRawSamples()) {
        ALuint buffer = 0;

        qalGetError();
        qalGenBuffers(1, &buffer);
        if (qalGetError())
            return false;

        qalBufferData(buffer, format, data, samples * width * channels, rate);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }

        qalSourceQueueBuffers(s_stream, 1, &buffer);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }
        s_stream_buffers++;
    }

    qalSourcef(s_stream, AL_GAIN, volume);

    ALint state = AL_PLAYING;
    qalGetSourcei(s_stream, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        qalSourcePlay(s_stream);
        s_stream_paused = false;
    }
    return true;
}

static void AL_UpdateUnderWater(void)
{
    bool underwater = S_IsUnderWater();
    ALint filter = 0;

    if (!s_underwater_filter)
        return;

    if (s_underwater_flag == underwater)
        return;

    if (underwater)
        filter = s_underwater_filter;

    int num_channels = S_GetSoundSystem().num_channels();
    for (int i = 0; i < num_channels; i++)
        qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, filter);

    s_underwater_flag = underwater;
}

static void AL_Activate(void)
{
    S_GetSoundSystem().StopAllSounds();
    AL_StreamPause(s_stream_paused);
}

/*
=============
AL_Update

Update OpenAL listener state, spatialization, and streaming with a monotonic time source.
=============
*/
static void AL_Update(void)
{
	int		i;
	channel_t	*ch;
	ALfloat	orientation[6];
	uint64_t	now;

	SoundSystem &soundSystem = S_GetSoundSystem();
	channel_t *channels = soundSystem.channels_data();
	int num_channels = soundSystem.num_channels();
	const vec3_t &listener_origin = soundSystem.listener_origin();
	const vec3_t &listener_forward = soundSystem.listener_forward();
	const vec3_t &listener_up = soundSystem.listener_up();

	if (!s_active)
		return;

	now = Sys_Milliseconds();
	if (!s_previous_update_time) {
		s_previous_update_time = now;
		s_paintedtime_monotonic = now;
	} else {
		s_paintedtime_monotonic += now - s_previous_update_time;
		s_previous_update_time = now;
	}

	s_paintedtime = s_paintedtime_monotonic >= INT_MAX ? INT_MAX : (int)s_paintedtime_monotonic;

	// set listener parameters
	qalListener3f(AL_POSITION, AL_UnpackVector(listener_origin));
	AL_CopyVector(listener_forward, orientation);
	AL_CopyVector(listener_up, orientation + 3);
	qalListenerfv(AL_ORIENTATION, orientation);

	AL_UpdateUnderWater();

	if (s_use_reverb->integer) {
		AL_UpdateReverb();
	}

	// update spatialization for dynamic sounds
	for (i = 0, ch = channels; channels && i < num_channels; i++, ch++) {
		if (!ch->sfx)
			continue;

		if (ch->autosound) {
			// autosounds are regenerated fresh each frame
			if (ch->autoframe != s_framecount) {
				AL_StopChannel(ch);
				continue;
			}
		} else {
			ALenum state = AL_STOPPED;
			qalGetSourcei(ch->srcnum, AL_SOURCE_STATE, &state);
			if (state == AL_STOPPED) {
				AL_StopChannel(ch);
				continue;
			}
		}

#if USE_DEBUG
		if (s_show->integer) {
			ALfloat offset = 0;
			qalGetSourcef(ch->srcnum, AL_SEC_OFFSET, &offset);
			Com_Printf("%d %.1f %.1f %s\n", i, ch->master_vol, offset, ch->sfx->name);
		}
#endif

		AL_Spatialize(ch);  // respatialize channel
	}

	s_framecount++;

	// add loopsounds
	if (al_merge_looping->integer >= s_merge_looping_minval) {
		AL_MergeLoopSounds();
	} else {
		AL_AddLoopSounds();
	}

	AL_IssuePlaysounds();

	AL_StreamUpdate();
}

static void AL_EndRegistration(void)
{
    AL_FreeReverbEnvironments(s_reverb_environments, s_num_reverb_environments);
    s_reverb_environments = NULL;
    s_num_reverb_environments = 0;

    AL_LoadReverbEnvironments();

    if (!s_reverb_environments)
        return;

    s_reverb_current_preset = 19;
    memcpy(&s_active_reverb, &s_reverb_parameters[s_reverb_current_preset], sizeof(s_active_reverb));
    AL_LoadEffect(&s_active_reverb);
    s_reverb_lerp_start = s_reverb_lerp_time = 0;

    s_reverb_probe_time = 0;
    s_reverb_probe_index = 0;
    for (int i = 0; i < q_countof(s_reverb_probes); i++)
        VectorClear(s_reverb_probe_results[i]);
    s_reverb_probe_avg = (float) 8192;
    s_reverb_active_environment = &s_reverb_environments[s_num_reverb_environments - 1];

    if (cl.bsp)
        AL_SetReverbStepIDs();
}

static constexpr sndapi_t MakeOpenALSndApi()
{
    return sndapi_t{
        AL_Init,
        AL_Shutdown,
        AL_Update,
        AL_Activate,
        AL_SoundInfo,
        AL_UploadSfx,
        AL_DeleteSfx,
        nullptr,
        AL_RawSamples,
        AL_NeedRawSamples,
        AL_HaveRawSamples,
        AL_StreamStop,
        AL_StreamPause,
        AL_GetBeginofs,
        AL_PlayChannel,
        AL_StopChannel,
        AL_StopAllSounds,
        QAL_GetSampleRate,
        AL_EndRegistration,
    };
}

const sndapi_t snd_openal = MakeOpenALSndApi();
