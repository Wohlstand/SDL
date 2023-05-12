/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_OGC

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h> /* memalign() */

#include "SDL_audio.h"
#include "SDL_error.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_ogcaudio.h"

#include <gccore.h>
#include <asndlib.h>

#define OGC_AUDIO_SAMPLE_ALIGN(s)   (((s) + 63) & ~63)
#define OGC_AUDIO_MAX_VOLUME    255

/* The tag name used by VITA audio */
#define OGCAUD_DRIVER_NAME     "ogc"

static struct SDL_AudioDevice *s_callback_data[8];


static void OGCAUD_Deinitialize(void)
{
    ASND_End();
}

static int
OGCAUD_OpenDevice(_THIS, const char *devname)
{
    int mixlen, i;

    (void)devname;

    this->hidden = (struct SDL_PrivateAudioData *)SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(this->hidden);

    switch (SDL_AUDIO_BITSIZE(this->spec.format)) {
        case 8:
            if(SDL_AUDIO_ISUNSIGNED(this->spec.format)) {
                this->spec.format = AUDIO_U8;
                if (this->spec.channels == 1) {
                    this->hidden->output_type = VOICE_MONO_8BIT_U;
                } else {
                    this->hidden->output_type = VOICE_STEREO_8BIT_U;
                    this->spec.channels = 2;
                }
            } else {
                this->spec.format = AUDIO_S8;
                if (this->spec.channels == 1) {
                    this->hidden->output_type = VOICE_MONO_8BIT;
                } else {
                    this->hidden->output_type = VOICE_STEREO_8BIT;
                    this->spec.channels = 2;
                }
            }
        case 16:
            if(SDL_AUDIO_ISSIGNED(this->spec.format)) {
                this->spec.format = AUDIO_S16SYS;
                if (this->spec.channels == 1) {
                    this->hidden->output_type = VOICE_MONO_16BIT;
                } else {
                    this->hidden->output_type = VOICE_STEREO_16BIT;
                    this->spec.channels = 2;
                }
            } else {
                if (this->hidden) {
                    SDL_free(this->hidden);
                    this->hidden = NULL;
                }
                return SDL_SetError("OGC: Unsupported audio format");
            }
            break;
        default:
            if (this->hidden) {
                SDL_free(this->hidden);
                this->hidden = NULL;
            }
            return SDL_SetError("OGC: Unsupported audio format");
    }

    if (this->spec.freq != 32000 && this->spec.freq != 48000) {
        this->spec.freq = 32000;
    }

    /* The sample count must be a multiple of 64. */
    this->spec.samples = OGC_AUDIO_SAMPLE_ALIGN(this->spec.samples);

    this->hidden->volume = OGC_AUDIO_MAX_VOLUME;

    /* Update the fragment size as size in bytes. */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate the mixing buffer.  Its size and starting address must
       be a multiple of 64 bytes.  Our sample count is already a multiple of
       64, so spec->size should be a multiple of 64 as well. */
    mixlen = this->spec.size * NUM_BUFFERS;
    this->hidden->rawbuf = (Uint8 *) memalign(64, mixlen);
    if (this->hidden->rawbuf == NULL) {
        SDL_free(this->hidden);
        this->hidden = NULL;
        return SDL_SetError("OGC: Couldn't allocate mixing buffer");
    }

    this->hidden->channel = 0;
    this->hidden->first_time = 1;

    this->hidden->queue = LWP_TQUEUE_NULL;

    LWP_InitQueue(&this->hidden->queue);

    SDL_memset(this->hidden->rawbuf, 0, mixlen);
    for (i = 0; i < NUM_BUFFERS; i++) {
        this->hidden->mixbufs[i] = &this->hidden->rawbuf[i * this->spec.size];
    }

    s_callback_data[this->hidden->channel] = this;
    this->hidden->next_buffer = 0;
    this->hidden->cur_buffer = 0;

    ASND_Init();
    ASND_ChangeVolumeVoice(this->hidden->channel, OGC_AUDIO_MAX_VOLUME, OGC_AUDIO_MAX_VOLUME);
    ASND_Pause(0);

    return 0;
}

static void ogc_play_callback(int voice)
{
    struct SDL_AudioDevice *this = s_callback_data[voice];
    if (!this) {
        return;
    }

    LWP_ThreadSignal(this->hidden->queue);
}

static void OGCAUD_PlayDevice(_THIS)
{
    Uint8 *mixbuf = this->hidden->mixbufs[this->hidden->next_buffer];

    this->hidden->cur_buffer = this->hidden->next_buffer;

    if(ASND_StatusVoice(this->hidden->channel) == SND_UNUSED || this->hidden->first_time) {

        this->hidden->first_time = 0;

        ASND_SetVoice(this->hidden->channel,
                      this->hidden->output_type,
                      this->spec.freq, 0,
                      mixbuf,
                      this->spec.size,
                      this->hidden->volume,
                      this->hidden->volume,
                      ogc_play_callback);
    } else {
        ASND_AddVoice(this->hidden->channel, mixbuf, this->spec.size);
    }

    this->hidden->next_buffer = (this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void OGCAUD_WaitDevice(_THIS)
{
    if(ASND_TestPointer(this->hidden->channel, this->hidden->mixbufs[this->hidden->cur_buffer]) &&
       ASND_StatusVoice(this->hidden->channel) != SND_UNUSED) {
        LWP_ThreadSleep(this->hidden->queue);
    }
}

static Uint8 *OGCAUD_GetDeviceBuf(_THIS)
{
    return this->hidden->mixbufs[this->hidden->next_buffer];
}

static void OGCAUD_CloseDevice(_THIS)
{
    ASND_StopVoice(this->hidden->channel);

    if(this->hidden->queue != LWP_TQUEUE_NULL)
    {
        LWP_ThreadSignal(this->hidden->queue);
        LWP_CloseQueue(this->hidden->queue);
        this->hidden->queue = LWP_TQUEUE_NULL;
    }

    if (this->hidden->rawbuf != NULL) {
        free(this->hidden->rawbuf);         /* this uses memalign(), not SDL_malloc(). */
        this->hidden->rawbuf = NULL;
    }

    s_callback_data[this->hidden->channel] = 0;
    if (this->hidden) {
        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

static void OGCAUD_ThreadInit(_THIS)
{
    /* Increase the priority of this audio thread by 1 to put it
       ahead of other SDL threads. */

    LWP_SetThreadPriority(LWP_THREAD_NULL, LWP_PRIO_HIGHEST - 5);
    
    (void)this;
}

static SDL_bool
OGCAUD_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = OGCAUD_OpenDevice;
    impl->PlayDevice = OGCAUD_PlayDevice;
    impl->WaitDevice = OGCAUD_WaitDevice;
    impl->GetDeviceBuf = OGCAUD_GetDeviceBuf;
    impl->CloseDevice = OGCAUD_CloseDevice;
    impl->Deinitialize = OGCAUD_Deinitialize;
    impl->ThreadInit = OGCAUD_ThreadInit;

    /* OGC audio device */
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    /*
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultInputDevice = SDL_TRUE;
    */
    return SDL_TRUE;   /* this audio target is available. */
}

AudioBootStrap OGCAUD_bootstrap = {
    "ogc", "OGC audio driver", OGCAUD_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_OGC */

/* vi: set ts=4 sw=4 expandtab: */
