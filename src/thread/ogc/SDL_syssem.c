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

int SDL_SemTryWait(SDL_sem *sem)
{
    int retval;
    u32 val;

    if(!sem) {
        return SDL_InvalidParamError("sem");
    }

    retval = SDL_MUTEX_TIMEDOUT;

    if (LWP_SemGetValue(sem->semid, &val) == 0 && LWP_SemWait(sem->semid) == 0) {
        retval = 0;
    }

    return retval;
}

int SDL_SemWait(SDL_sem *sem)
{
    int retval;

    if(!sem) {
        return SDL_InvalidParamError("sem");
    }

    retval = SDL_MUTEX_TIMEDOUT;

    if (LWP_SemWait(sem->semid) == 0) {
        retval = 0;
    }

    return retval;
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
    u32 val;
    Uint32 ticks;

    if (!sem) {
        return SDL_InvalidParamError("sem");
    }

    /* Try the easy cases first */
    if (timeout == 0) {
        return SDL_SemTryWait(sem);
    }
    if (timeout == SDL_MUTEX_MAXWAIT) {
        return SDL_SemWait(sem);
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

/* Returns the current count of the semaphore */
Uint32 SDL_SemValue(SDL_sem *sem)
{
    u32 val;

    if (!sem) {
        SDL_InvalidParamError("sem");
        return 0;
    }

    if (LWP_SemGetValue(sem->semid, &val) == 0) {
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
