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

#include <ogc/lwp_watchdog.h>
#include <tuxedo/sync.h>

struct SDL_cond
{
    int slot;
    KCondVar *cv;
};

/* It's possible to initialize 65535 different condition slots */
static Uint64 s_cond_alloc[1024];

static int s_cond_alloc_slot()
{
    size_t i = 0;
    u64 *used;
    int slot;
    PPCIrqState st = PPCIrqLockByMsr();

    for(i = 0; i < 1024; ++i)
    {
        used = &s_cond_alloc[i];
        slot = __builtin_ffsll(~*used) - 1;

        if (slot >= 0) {
            *used |= 1ULL << slot;
            slot += (i << 6);
            break;
        }
    }

    PPCIrqUnlockByMsr(st);

    return slot;
}

static void s_cond_free_slot(int slot)
{
    PPCIrqState st = PPCIrqLockByMsr();
    size_t i = (slot >> 6) & 1023;
    u64* used = &s_cond_alloc[i];
    *used &= ~(1ULL << slot);
    PPCIrqUnlockByMsr(st);
}

/* Create a condition variable */
SDL_cond *SDL_CreateCond(void)
{
    SDL_cond *cond;

    cond = (SDL_cond *)SDL_calloc(1, sizeof(SDL_cond));
    if (cond) {
        cond->slot = s_cond_alloc_slot();
        if (cond->slot < 0) {
            SDL_SetError("Can't initialize the cond");
            SDL_DestroyCond(cond);
            cond = NULL;
        }

        cond->cv = (KCondVar*)0xc0000000 + cond->slot + 1;
    } else {
        SDL_OutOfMemory();
    }
    return (cond);
}

/* Destroy a condition variable */
void SDL_DestroyCond(SDL_cond * cond)
{
    if (cond) {
        if (cond->slot >= 0) {
            s_cond_free_slot(cond->slot);
        }
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

    KCondVarSignal(cond->cv);

    return 0;
}

/* Restart all threads that are waiting on the condition variable */
int SDL_CondBroadcast(SDL_cond * cond)
{
    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    KCondVarBroadcast(cond->cv);

    return 0;
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
    u32 counter_backup;
    int ret;

    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    if (!cond->cv) {
        return -1;
    }

    /* LWP_CondTimedWait expects relative timeout */
    time.tv_sec = (ms / 1000);
    time.tv_nsec = (ms % 1000) * 1000000;

    counter_backup = mutex->lock.counter;
    mutex->lock.counter = 0;

    ret = KCondVarWaitTimeoutTicks(cond->cv, (KMutex*)&mutex->lock, timespec_to_ticks(&time));

    mutex->lock.counter = counter_backup;

    return ret ? 0 : SDL_MUTEX_TIMEDOUT;
}

/* Wait on the condition variable forever */
int SDL_CondWait(SDL_cond * cond, SDL_mutex * mutex)
{
    u32 counter_backup;

    if (!cond) {
        SDL_SetError("Passed a NULL condition variable");
        return -1;
    }

    if (!cond->cv) {
        return -1;
    }

    counter_backup = mutex->lock.counter;
    mutex->lock.counter = 0;

    KCondVarWait(cond->cv, (KMutex*)&mutex->lock);

    mutex->lock.counter = counter_backup;

    return 0;
}

#endif /* SDL_THREAD_OGC */

/* vi: set ts=4 sw=4 expandtab: */
