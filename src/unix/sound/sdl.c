/*
Copyright (C) 2007-2008 Andrey Nazarov

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

//
// snd_sdl.c
//

#include "shared/shared.h"
#include "common/zone.h"
#include "client/sound/dma.h"
#include <SDL3/SDL.h>

static SDL_AudioStream *sdl_stream;
static SDL_AudioDeviceID sdl_device;

static void SDLCALL StreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    int bytes_per_sample = dma.samplebits / 8;
    int buffer_bytes = dma.samples * bytes_per_sample;
    int remaining = additional_amount;

    (void)userdata;
    (void)total_amount;

    if (!dma.buffer || buffer_bytes <= 0)
        return;

    while (remaining > 0) {
        int chunk = min(remaining, buffer_bytes);
        int pos = dma.samplepos * bytes_per_sample;
        int wrapped = pos + chunk - buffer_bytes;

        if (wrapped < 0) {
            SDL_PutAudioStreamData(stream, dma.buffer + pos, chunk);
            dma.samplepos += chunk / bytes_per_sample;
        } else {
            int head = buffer_bytes - pos;
            SDL_PutAudioStreamData(stream, dma.buffer + pos, head);
            SDL_PutAudioStreamData(stream, dma.buffer, wrapped);
            dma.samplepos = wrapped / bytes_per_sample;
        }

        remaining -= chunk;
    }
}

static void Shutdown(void)
{
    Com_Printf("Shutting down SDL audio.\n");

    if (sdl_stream) {
        SDL_DestroyAudioStream(sdl_stream);
        sdl_stream = NULL;
    }
    sdl_device = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    Z_Freep(&dma.buffer);
}

static sndinitstat_t Init(void)
{
    SDL_AudioSpec desired;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Com_EPrintf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
        return SIS_FAILURE;
    }

    memset(&desired, 0, sizeof(desired));
    switch (s_khz->integer) {
    case 48:
        desired.freq = 48000;
        break;
    case 44:
        desired.freq = 44100;
        break;
    case 22:
        desired.freq = 22050;
        break;
    default:
        desired.freq = 11025;
        break;
    }

    desired.format = SDL_AUDIO_S16LE;
    desired.channels = 2;
    sdl_stream = SDL_OpenAudioDeviceStream(0, &desired, StreamCallback, NULL);
    if (!sdl_stream) {
        Com_EPrintf("Couldn't open SDL audio: %s\n", SDL_GetError());
        goto fail1;
    }

    sdl_device = SDL_GetAudioStreamDevice(sdl_stream);
    if (!sdl_device) {
        Com_EPrintf("Couldn't get SDL audio device: %s\n", SDL_GetError());
        goto fail2;
    }

    dma.speed = desired.freq;
    dma.channels = desired.channels;
    dma.samples = 0x8000 * desired.channels;
    dma.submission_chunk = 1;
    dma.samplebits = 16;
    dma.buffer = Z_Mallocz(dma.samples * 2);
    dma.samplepos = 0;

    Com_Printf("Using SDL audio driver: %s\n", SDL_GetCurrentAudioDriver());

    if (!SDL_ResumeAudioDevice(sdl_device)) {
        Com_EPrintf("Couldn't start SDL audio: %s\n", SDL_GetError());
        goto fail3;
    }

    return SIS_SUCCESS;

fail3:
    Z_Freep(&dma.buffer);
fail2:
    SDL_DestroyAudioStream(sdl_stream);
    sdl_stream = NULL;
    sdl_device = 0;
fail1:
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return SIS_FAILURE;
}

static void BeginPainting(void)
{
    if (sdl_stream)
        SDL_LockAudioStream(sdl_stream);
}

static void Submit(void)
{
    if (sdl_stream)
        SDL_UnlockAudioStream(sdl_stream);
}

static void Activate(bool active)
{
    if (!sdl_device)
        return;

    if (active)
        SDL_ResumeAudioDevice(sdl_device);
    else
        SDL_PauseAudioDevice(sdl_device);
}

const snddma_driver_t snddma_sdl = {
    .name = "sdl",
    .init = Init,
    .shutdown = Shutdown,
    .begin_painting = BeginPainting,
    .submit = Submit,
    .activate = Activate,
};
