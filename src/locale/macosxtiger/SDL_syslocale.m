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
#include "../SDL_syslocale.h"

#import <Foundation/Foundation.h>

void
SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *nsstr = [[NSLocale currentLocale] localeIdentifier];
    char *ptr;

    if (nsstr == nil) {
        [pool drain];
        return;
    }

    [nsstr getCString:buf maxLength:buflen encoding:NSASCIIStringEncoding];

    // convert '-' to '_'...
    //  These are always full lang-COUNTRY, so we search from the back,
    //  so things like zh-Hant-CN find the right '-' to convert.
    if ((ptr = SDL_strrchr(buf, '-')) != NULL) {
        *ptr = '_';
    }

    [pool drain];
}

/* vi: set ts=4 sw=4 expandtab: */
