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

/* An implementation of condition variables using semaphores and mutexes */
/*
   This implementation borrows heavily from the BeOS condition variable
   implementation, written by Christopher Tate and Owen Smith.  Thanks!
 */

#include "SDL_thread.h"
#include "SDL_sysmutex_c.h"

#include <ogcsys.h>
#include <ogc/cond.h>
#include <ogc/lwp_watchdog.h>

struct SDL_cond
{
    cond_t cond;
};

static KCondVar* lwpc_get_condvar(cond_t cond)
{
    if (!cond || cond == LWP_COND_NULL) {
        return NULL;
    }

    return (KCondVar*)(0xc0000000 + cond);
}

/* Create a condition variable */
SDL_cond *SDL_CreateCond(void)
{
    SDL_cond *cond;

    cond = (SDL_cond *) SDL_malloc(sizeof(SDL_cond));
    if (cond) {
        if (LWP_CondInit(&(cond->cond)) < 0) {
            SDL_DestroyCond(cond);
            cond = NULL;
        }
    } else {
        SDL_OutOfMemory();
    }
    return (cond);
}

/* Destroy a condition variable */
void SDL_DestroyCond(SDL_cond * cond)
{
    if (cond) {
        LWP_CondDestroy(cond->cond);
        SDL_free(cond);
    }
}

/* Restart one of the threads that are waiting on the condition variable */
int SDL_CondSignal(SDL_cond * cond)
{
    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    return LWP_CondSignal(cond->cond) == 0 ? 0 : -1;
}

/* Restart all threads that are waiting on the condition variable */
int SDL_CondBroadcast(SDL_cond * cond)
{
    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    return LWP_CondBroadcast(cond->cond) == 0 ? 0 : -1;
}

/* Wait on the condition variable for at most 'ms' milliseconds.
   The mutex must be locked before entering this function!
   The mutex is unlocked during the wait, and locked again after the wait.

Typical use:

Thread A:
    SDL_LockMutex(lock);
    while ( ! condition ) {
        SDL_CondWait(cond, lock);
    }
    SDL_UnlockMutex(lock);

Thread B:
    SDL_LockMutex(lock);
    ...
    condition = true;
    ...
    SDL_CondSignal(cond);
    SDL_UnlockMutex(lock);
 */
int SDL_CondWaitTimeout(SDL_cond *cond, SDL_mutex *mutex, Uint32 ms)
{
    struct timespec time;
    KCondVar* cv;

    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    cv = lwpc_get_condvar(cond->cond);
    if (!cv) {
        return -1;
    }

    //LWP_CondTimedWait expects relative timeout
    time.tv_sec = (ms / 1000);
    time.tv_nsec = (ms % 1000) * 1000000;

    return KCondVarWaitTimeoutTicks(cv, &mutex->lock.mutex, timespec_to_ticks(&time)) ? 0 : SDL_MUTEX_TIMEDOUT;
}

/* Wait on the condition variable forever */
int SDL_CondWait(SDL_cond * cond, SDL_mutex * mutex)
{
    KCondVar* cv;

    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    cv = lwpc_get_condvar(cond->cond);
    if (!cv) {
        return -1;
    }

    KCondVarWait(cv, &mutex->lock.mutex);

    return 0;
}

#endif /* SDL_THREAD_OGC */

/* vi: set ts=4 sw=4 expandtab: */
