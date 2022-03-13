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

/* libOGC thread management routines for SDL */

#include <stdio.h>
#include <stdlib.h>

#include "SDL_error.h"
#include "SDL_thread.h"
#include "../SDL_systhread.h"
#include "../SDL_thread_c.h"

#include <ogc/lwp.h>

#define OGC_THREAD_STACK_MIN_SIZE 0x1000
#define OGC_THREAD_STACK_MAX_SIZE 0x2000000
#define OGC_THREAD_STACK_SIZE_DEFAULT 0x10000 // 64KiB
#define OGC_THREAD_NAME_MAX 32

#define OGC_THREAD_PRIORITY_LOW 10
#define OGC_THREAD_PRIORITY_NORMAL 80
#define OGC_THREAD_PRIORITY_HIGH 100
#define OGC_THREAD_PRIORITY_TIME_CRITICAL LWP_PRIO_HIGHEST


static void *ThreadEntry(void *argp)
{
    SDL_RunThread(*(SDL_Thread **) argp);
    return NULL;
}

int SDL_SYS_CreateThread(SDL_Thread *thread)
{
    char thread_name[OGC_THREAD_NAME_MAX];
    size_t stack_size = OGC_THREAD_STACK_SIZE_DEFAULT;
    s32 ret;

    thread->handle = LWP_THREAD_NULL;

    SDL_strlcpy(thread_name, "SDL thread", OGC_THREAD_NAME_MAX);
    if (thread->name) {
        SDL_strlcpy(thread_name, thread->name, OGC_THREAD_NAME_MAX);
    }

    if (thread->stacksize) {
        if (thread->stacksize < OGC_THREAD_STACK_MAX_SIZE) {
            thread->stacksize = OGC_THREAD_STACK_MAX_SIZE;
        }
        if (thread->stacksize > OGC_THREAD_STACK_MAX_SIZE) {
            thread->stacksize = OGC_THREAD_STACK_MAX_SIZE;
        }
        stack_size = thread->stacksize;
    }

    /* Create new thread with the same priority as the current thread */
    ret = LWP_CreateThread(&thread->handle,
        ThreadEntry,
        &thread,
        NULL,
        stack_size,
        OGC_THREAD_PRIORITY_NORMAL
    );

    if (ret < 0) {
        return SDL_SetError("LWP_CreateThread() failed");
    }

    return 0;
}

void SDL_SYS_SetupThread(const char *name)
{
    /* Do nothing. */
    (void)name;
}

SDL_threadID SDL_ThreadID(void)
{
    return (SDL_threadID) LWP_GetSelf();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    LWP_JoinThread(thread->handle, NULL);
    thread->handle = LWP_THREAD_NULL;
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    /* Do nothing. */
    thread->handle = LWP_THREAD_NULL;
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    lwp_t self = LWP_GetSelf();
    int value = 0;

    switch(priority) {
        case SDL_THREAD_PRIORITY_LOW:
            value = 0;
            break;
        case SDL_THREAD_PRIORITY_NORMAL:
            value = 1;
            break;
        case SDL_THREAD_PRIORITY_HIGH:
            value = 2;
            break;
        case SDL_THREAD_PRIORITY_TIME_CRITICAL:
            value = 3;
            break;
    }

    LWP_SetThreadPriority(self, value);

    return 0;
}

#endif /* SDL_THREAD_VITA */

/* vi: set ts=4 sw=4 expandtab: */
