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

#if SDL_THREAD_OGC

/* Semaphore functions for the libOGC. */

#include <stdio.h>
#include <stdlib.h>

#include "SDL_error.h"
#include "SDL_thread.h"
#include "SDL_timer.h"

#include <ogc/semaphore.h>

struct SDL_semaphore {
    sem_t  semid;
};


/* Create a semaphore */
SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem;
    s32 ret;

    sem = (SDL_sem *) SDL_malloc(sizeof(*sem));
    if (sem != NULL) {
        sem->semid = LWP_SEM_NULL;

        ret = LWP_SemInit(&sem->semid, initial_value, 255);
        if (ret < 0) {
            SDL_SetError("Couldn't create semaphore");
            SDL_free(sem);
            sem = NULL;
        }
    } else {
        SDL_OutOfMemory();
    }

    return sem;
}

/* Free the semaphore */
void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (sem != NULL) {
        if (sem->semid > 0) {
            LWP_SemDestroy(sem->semid);
            sem->semid = LWP_SEM_NULL;
        }

        SDL_free(sem);
    }
}

/* TODO: This routine is a bit overloaded.
 * If the timeout is 0 then just poll the semaphore; if it's SDL_MUTEX_MAXWAIT, pass
 * NULL to sceKernelWaitSema() so that it waits indefinitely; and if the timeout
 * is specified, convert it to microseconds. */
int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
    s32 res;
    u32 val;
    Uint32 ticks;

    if (sem == NULL) {
        SDL_InvalidParamError("sem");
        return 0;
    }

    if (timeout == 0) {
        LWP_SemGetValue(sem->semid, &val);
        if (val == 0) {
            return SDL_MUTEX_TIMEDOUT;
        } else {
            LWP_SemWait(sem->semid);
            return 0;
        }
    }

    if (timeout == SDL_MUTEX_MAXWAIT) {
        res = LWP_SemWait(sem->semid);
        if (res < 0) {
            return SDL_MUTEX_TIMEDOUT;
        }
        return 0;
    }

    ticks = SDL_GetTicks();
    while (SDL_GetTicks() - ticks < timeout) {
        val = SDL_SemValue(sem);
        if (val > 0) {
            LWP_SemWait(sem->semid);
            return 0;
        }
        SDL_Delay(1);
    }

    return SDL_MUTEX_TIMEDOUT;
}

int SDL_SemTryWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, 0);
}

int SDL_SemWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

/* Returns the current count of the semaphore */
Uint32 SDL_SemValue(SDL_sem *sem)
{
    u32 val;
    s32 ret;

    if (sem == NULL) {
        SDL_InvalidParamError("sem");
        return 0;
    }

    ret = LWP_SemGetValue(sem->semid, &val);
    if (ret >= 0) {
        return val;
    }

    return 0;
}

int SDL_SemPost(SDL_sem *sem)
{
    int res;

    if (sem == NULL) {
        return SDL_InvalidParamError("sem");
    }

    res = LWP_SemPost(sem->semid);
    if (res < 0) {
        return SDL_SetError("LWP_SemPost() failed");
    }

    return 0;
}

#endif /* SDL_THREAD_OGC */

/* vi: set ts=4 sw=4 expandtab: */
