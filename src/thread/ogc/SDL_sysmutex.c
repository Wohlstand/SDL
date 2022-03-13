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

#include <ogc/mutex.h>

struct SDL_mutex
{
    mutex_t lock;
};

/* Create a mutex */
SDL_mutex *
SDL_CreateMutex(void)
{
    SDL_mutex *mutex = NULL;
    s32 res = 0;

    /* Allocate mutex memory */
    mutex = (SDL_mutex *) SDL_malloc(sizeof(*mutex));
    if (mutex) {
        mutex->lock = LWP_MUTEX_NULL;

        res = LWP_MutexInit(&mutex->lock, 0);

        if (res < 0) {
            mutex->lock = LWP_MUTEX_NULL;
            SDL_SetError("Error trying to create mutex: %x", res);
        }
    } else {
        SDL_OutOfMemory();
    }
    return mutex;
}

/* Free the mutex */
void
SDL_DestroyMutex(SDL_mutex * mutex)
{
    if (mutex) {
        if (mutex->lock != LWP_MUTEX_NULL) {
            LWP_MutexDestroy(mutex->lock);
        }
        SDL_free(mutex);
    }
}

/* Try to lock the mutex */
int
SDL_TryLockMutex(SDL_mutex * mutex)
{
#if SDL_THREADS_DISABLED
    return 0;
#else
    s32 res = 0;
    if (mutex == NULL || mutex->lock == LWP_MUTEX_NULL) {
        return SDL_InvalidParamError("mutex");
    }

    res = LWP_MutexTryLock(mutex->lock);
    switch (res) {
        case 0:
            return 0;
            break;
        case 1:
            return SDL_MUTEX_TIMEDOUT;
            break;
        default:
            return SDL_SetError("Error trying to lock mutex: %x", res);
            break;
    }

    return -1;
#endif /* SDL_THREADS_DISABLED */
}


/* Lock the mutex */
int
SDL_mutexP(SDL_mutex * mutex)
{
#if SDL_THREADS_DISABLED
    return 0;
#else
    s32 res = 0;
    if (mutex == NULL || mutex->lock == LWP_MUTEX_NULL) {
        return SDL_InvalidParamError("mutex");
    }

    res = LWP_MutexLock(mutex->lock);
    if (res < 0) {
        return SDL_SetError("Error trying to lock mutex: %x", res);
    }

    return 0;
#endif /* SDL_THREADS_DISABLED */
}

/* Unlock the mutex */
int
SDL_mutexV(SDL_mutex * mutex)
{
#if SDL_THREADS_DISABLED
    return 0;
#else
    s32 res = 0;

    if (mutex == NULL || mutex->lock == LWP_MUTEX_NULL) {
        return SDL_InvalidParamError("mutex");
    }

    res = LWP_MutexUnlock(mutex->lock);
    if (res < 0) {
        return SDL_SetError("Error trying to unlock mutex: %x", res);
    }

    return 0;
#endif /* SDL_THREADS_DISABLED */
}

#endif /* SDL_THREAD_OGC */

/* vi: set ts=4 sw=4 expandtab: */
