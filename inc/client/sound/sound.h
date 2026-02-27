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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void S_Init(void);
void S_Shutdown(void);

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound(const vec3_t origin, int entnum, int entchannel,
                  qhandle_t sfx, float fvol, float attenuation, float timeofs);
void S_ParseStartSound(void);
void S_StartLocalSound(const char *s);
void S_StartLocalSoundOnce(const char *s);

void S_FreeAllSounds(void);
void S_StopAllSounds(void);
void S_Update(void);

void S_Activate(void);

void S_BeginRegistration(void);
qhandle_t S_RegisterSound(const char *sample);
void S_EndRegistration(void);

#define MAX_RAW_SAMPLES     8192

typedef struct sfx_eax_properties_s {
    float flDensity;
    float flDiffusion;
    float flGain;
    float flGainHF;
    float flGainLF;
    float flDecayTime;
    float flDecayHFRatio;
    float flDecayLFRatio;
    float flReflectionsGain;
    float flReflectionsDelay;
    float flReflectionsPan[3];
    float flLateReverbGain;
    float flLateReverbDelay;
    float flLateReverbPan[3];
    float flEchoTime;
    float flEchoDepth;
    float flModulationTime;
    float flModulationDepth;
    float flAirAbsorptionGainHF;
    float flHFReference;
    float flLFReference;
    float flRoomRolloffFactor;
    int iDecayHFLimit;
} sfx_eax_properties_t;

enum {
    SOUND_EAX_EFFECT_DEFAULT = 0,
    SOUND_EAX_EFFECT_UNDERWATER = 1,
    SOUND_EAX_EFFECT_ABANDONED = 2,
    SOUND_EAX_EFFECT_ALLEY = 3,
    SOUND_EAX_EFFECT_ARENA = 4,
    SOUND_EAX_EFFECT_AUDITORIUM = 5,
    SOUND_EAX_EFFECT_BATHROOM = 6,
    SOUND_EAX_EFFECT_CARPETED_HALLWAY = 7,
    SOUND_EAX_EFFECT_CAVE = 8,
    SOUND_EAX_EFFECT_CHAPEL = 9,
    SOUND_EAX_EFFECT_CITY = 10,
    SOUND_EAX_EFFECT_CITY_STREETS = 11,
    SOUND_EAX_EFFECT_CONCERT_HALL = 12,
    SOUND_EAX_EFFECT_DIZZY = 13,
    SOUND_EAX_EFFECT_DRUGGED = 14,
    SOUND_EAX_EFFECT_DUSTYROOM = 15,
    SOUND_EAX_EFFECT_FOREST = 16,
    SOUND_EAX_EFFECT_HALLWAY = 17,
    SOUND_EAX_EFFECT_HANGAR = 18,
    SOUND_EAX_EFFECT_LIBRARY = 19,
    SOUND_EAX_EFFECT_LIVINGROOM = 20,
    SOUND_EAX_EFFECT_MOUNTAINS = 21,
    SOUND_EAX_EFFECT_MUSEUM = 22,
    SOUND_EAX_EFFECT_PADDED_CELL = 23,
    SOUND_EAX_EFFECT_PARKINGLOT = 24,
    SOUND_EAX_EFFECT_PLAIN = 25,
    SOUND_EAX_EFFECT_PSYCHOTIC = 26,
    SOUND_EAX_EFFECT_QUARRY = 27,
    SOUND_EAX_EFFECT_ROOM = 28,
    SOUND_EAX_EFFECT_SEWERPIPE = 29,
    SOUND_EAX_EFFECT_SMALL_WATERROOM = 30,
    SOUND_EAX_EFFECT_STONE_CORRIDOR = 31,
    SOUND_EAX_EFFECT_STONE_ROOM = 32,
    SOUND_EAX_EFFECT_SUBWAY = 33,
    SOUND_EAX_EFFECT_UNDERPASS = 34,
    SOUND_EAX_EFFECT_MAX = 35
};

const qboolean S_SetEAXEnvironmentProperties(const sfx_eax_properties_t *properties);

void S_RawSamples(int samples, int rate, int width, int channels, const void *data);
int S_GetSampleRate(void);
bool S_SupportsFloat(void);
void S_PauseRawSamples(bool paused);

#if USE_AVCODEC
void OGG_Play(void);
void OGG_Stop(void);
void OGG_Update(void);
void OGG_LoadTrackList(void);
void OGG_Init(void);
void OGG_Shutdown(void);
void OGG_Restart(void);
#else
#define OGG_Play()          (void)0
#define OGG_Stop()          (void)0
#define OGG_Update()        (void)0
#define OGG_LoadTrackList() (void)0
#define OGG_Init()          (void)0
#define OGG_Shutdown()      (void)0
#define OGG_Restart()       (void)0
#endif

extern vec3_t   listener_origin;
extern vec3_t   listener_forward;
extern vec3_t   listener_right;
extern vec3_t   listener_up;
extern int      listener_entnum;

#ifdef __cplusplus
}
#endif
