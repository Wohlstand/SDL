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

#include "SDL_thread.h"
#include "SDL_systhread_c.h"

#include <tuxedo/sync.h>

struct SDL_mutex
{
    KRMutex lock;
};

/* Create a mutex */
SDL_mutex *SDL_CreateMutex(void)
{
    SDL_mutex *mutex = NULL;

    /* Allocate mutex memory */
    mutex = (SDL_mutex *)SDL_calloc(1, sizeof(SDL_mutex));
    if (!mutex) {
        SDL_OutOfMemory();
    }

    return mutex;
}

/* Free the mutex */
void SDL_DestroyMutex(SDL_mutex * mutex)
{
    if (mutex) {
        SDL_free(mutex);
    }
}

/* Try to lock the mutex */
int SDL_TryLockMutex(SDL_mutex * mutex)
{
    bool rc;
    if (!mutex) {
        return 0;
    }

    rc = KRMutexTryLock(&mutex->lock);

    return rc ? 0 : SDL_MUTEX_TIMEDOUT;
}


/* Lock the mutex */
int SDL_LockMutex(SDL_mutex * mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (!mutex) {
        return 0;
    }

    KRMutexLock(&mutex->lock);
    return 0;
}

/* Unlock the mutex */
int SDL_UnlockMutex(SDL_mutex * mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (!mutex) {
        return 0;
    }

    KRMutexUnlock(&mutex->lock);
    return 0;
}

#endif /* SDL_THREAD_OGC */

/* vi: set ts=4 sw=4 expandtab: */
