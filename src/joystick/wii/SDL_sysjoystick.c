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

#if SDL_JOYSTICK_WII

#include <wiiuse/wpad.h>
#include <ogc/pad.h>

#include <stdio.h>      /* For the definition of NULL */
#include <stdlib.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include "SDL_events.h"
#include "SDL_error.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"


#define PI 					3.14159265f

#define MAX_GC_JOYSTICKS	4
#define MAX_WII_JOYSTICKS	4
#define MAX_JOYSTICKS		(MAX_GC_JOYSTICKS + MAX_WII_JOYSTICKS)

#define MAX_GC_AXES			6
#define MAX_GC_BUTTONS		8
#define	MAX_GC_HATS			1

#define MAX_WII_AXES		9
#define MAX_WII_BUTTONS		20
#define	MAX_WII_HATS		1

#define	JOYNAMELEN			10

#define AXIS_MIN	-32768  /* minimum value for axis coordinate */
#define AXIS_MAX	32767   /* maximum value for axis coordinate */


typedef struct joystick_paddata_t
{
    u16 prev_buttons;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerL;
    u8 triggerR;
}joystick_paddata;

typedef struct joystick_wpaddata_t
{
    u32 prev_buttons;
    s8 nunchuk_stickX;
    s8 nunchuk_stickY;
    s8 classicL_stickX;
    s8 classicL_stickY;
    s8 classicR_stickX;
    s8 classicR_stickY;
    u8 classic_triggerL;
    u8 classic_triggerR;
    s8 wiimote_pitch;
    s8 wiimote_roll;
    s8 wiimote_yaw;
}joystick_wpaddata;

/* The private structure used to keep track of a joystick */
struct joystick_hwdata
{
    int index;
    int type;
    union
    {
        joystick_paddata gamecube;
        joystick_wpaddata wiimote;
    };
} joystick_hwdata;

static const u32 sdl_buttons_wii[] =
{
    WPAD_BUTTON_A,
    WPAD_BUTTON_B,
    WPAD_BUTTON_1,
    WPAD_BUTTON_2,
    WPAD_BUTTON_MINUS,
    WPAD_BUTTON_PLUS,
    WPAD_BUTTON_HOME,
    WPAD_NUNCHUK_BUTTON_Z, /* 7 */
    WPAD_NUNCHUK_BUTTON_C, /* 8 */
    WPAD_CLASSIC_BUTTON_A, /* 9 */
    WPAD_CLASSIC_BUTTON_B,
    WPAD_CLASSIC_BUTTON_X,
    WPAD_CLASSIC_BUTTON_Y,
    WPAD_CLASSIC_BUTTON_FULL_L,
    WPAD_CLASSIC_BUTTON_FULL_R,
    WPAD_CLASSIC_BUTTON_ZL,
    WPAD_CLASSIC_BUTTON_ZR,
    WPAD_CLASSIC_BUTTON_MINUS,
    WPAD_CLASSIC_BUTTON_PLUS,
    WPAD_CLASSIC_BUTTON_HOME
};

static const u16 sdl_buttons_gc[] =
{
    PAD_BUTTON_A,
    PAD_BUTTON_B,
    PAD_BUTTON_X,
    PAD_BUTTON_Y,
    PAD_TRIGGER_Z,
    PAD_TRIGGER_R,
    PAD_TRIGGER_L,
    PAD_BUTTON_START
};

static int __jswpad_enabled = 1;
static int __jspad_enabled = 1;
static int __numwiijoysticks = 4;
static int __numgcjoysticks = 4;


/* Helpers to separate nunchuk vs classic buttons which share the
 * same scan codes. In particular, up on the classic controller is
 * the same as Z on the nunchuk. The numbers refer to the sdl_buttons_wii
 * list above. */
static int wii_button_is_nunchuk(int idx)
{
    return idx == 7 || idx == 8;
}

static int wii_button_is_classic(int idx)
{
    return idx >= 9;
}

static s16 WPAD_Orient(WPADData *data, int motion)
{
    float out;

    if (motion == 0)
        out = data->orient.pitch;
    else if (motion == 1)
        out = data->orient.roll;
    else
        out = data->orient.yaw;

    return (s16)((out / 180.0) * 128.0);
}

static s16 WPAD_Pitch(WPADData *data)
{
    return WPAD_Orient(data, 0);
}

static s16 WPAD_Roll(WPADData *data)
{
    return WPAD_Orient(data, 1);
}

static s16 WPAD_Yaw(WPADData *data)
{
    return WPAD_Orient(data, 2);
}

static s16 WPAD_Stick(WPADData *data, u8 right, int axis)
{
    float mag = 0.0;
    float ang = 0.0;
    double val;

    switch (data->exp.type) {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
        if (right == 0)
        {
            mag = data->exp.nunchuk.js.mag;
            ang = data->exp.nunchuk.js.ang;
        }
        break;

    case WPAD_EXP_CLASSIC:
        if (right == 0) {
            mag = data->exp.classic.ljs.mag;
            ang = data->exp.classic.ljs.ang;
        } else {
            mag = data->exp.classic.rjs.mag;
            ang = data->exp.classic.rjs.ang;
        }
        break;

    default:
        break;
    }

    /* calculate x/y value (angle need to be converted into radian) */
    if (mag > 1.0) {
        mag = 1.0;
    } else if (mag < -1.0) {
        mag = -1.0;
    }

    if(axis == 0) /* x-axis */
        val = mag * SDL_sin((PI * ang)/180.0f);
    else /* y-axis */
        val = mag * SDL_cos((PI * ang)/180.0f);

    return (s16)(val * 128.0f);
}

static void _HandleWiiJoystickUpdate(SDL_Joystick* joystick, int device_index)
{
    u32 buttons, prev_buttons, changed;
    u32 exp_type;
    struct expansion_t exp;
    int i, axis;
    struct joystick_hwdata *prev_state;
    WPADData *data;

    buttons = WPAD_ButtonsHeld(device_index);

    if (WPAD_Probe(device_index, &exp_type) != 0)
        exp_type = WPAD_EXP_NONE;

     data = WPAD_Data(device_index);
     WPAD_Expansion(device_index, &exp);

    prev_state = (struct joystick_hwdata *)joystick->hwdata;
    prev_buttons = prev_state->wiimote.prev_buttons;
    changed = buttons ^ prev_buttons;

    if(exp_type == WPAD_EXP_CLASSIC) // classic controller
    {
        if(changed & (WPAD_CLASSIC_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_RIGHT |
            WPAD_CLASSIC_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_UP))
        {
            int hat = SDL_HAT_CENTERED;
            if(buttons & WPAD_CLASSIC_BUTTON_UP) hat |= SDL_HAT_UP;
            if(buttons & WPAD_CLASSIC_BUTTON_DOWN) hat |= SDL_HAT_DOWN;
            if(buttons & WPAD_CLASSIC_BUTTON_LEFT) hat |= SDL_HAT_LEFT;
            if(buttons & WPAD_CLASSIC_BUTTON_RIGHT) hat |= SDL_HAT_RIGHT;
            SDL_PrivateJoystickHat(joystick, 0, hat);
        }
    }
    else // wiimote
    {
        if(changed & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_DOWN | WPAD_BUTTON_UP))
        {
            int hat = SDL_HAT_CENTERED;
            if(buttons & WPAD_BUTTON_UP) hat |= SDL_HAT_LEFT;
            if(buttons & WPAD_BUTTON_DOWN) hat |= SDL_HAT_RIGHT;
            if(buttons & WPAD_BUTTON_LEFT) hat |= SDL_HAT_DOWN;
            if(buttons & WPAD_BUTTON_RIGHT) hat |= SDL_HAT_UP;
            SDL_PrivateJoystickHat(joystick, 0, hat);
        }
    }

    for(i = 0; i < (int)(sizeof(sdl_buttons_wii) / sizeof(sdl_buttons_wii[0])); i++)
    {
        if ( (exp_type == WPAD_EXP_CLASSIC && wii_button_is_nunchuk(i)) ||
             (exp_type == WPAD_EXP_NUNCHUK && wii_button_is_classic(i)) )
            continue;

        if (changed & sdl_buttons_wii[i])
            SDL_PrivateJoystickButton(joystick, i,
                (buttons & sdl_buttons_wii[i]) ? SDL_PRESSED : SDL_RELEASED);
    }
    prev_state->wiimote.prev_buttons = buttons;

    if(exp_type == WPAD_EXP_CLASSIC)
    {
        axis = WPAD_Stick(data, 0, 0);
        if(prev_state->wiimote.classicL_stickX != axis)
        {
            s16 value;
            if (axis >= 128) value = AXIS_MAX;
            else if (axis <=-128) value = AXIS_MIN;
            else value = axis << 8;
            SDL_PrivateJoystickAxis(joystick, 0, value);
            prev_state->wiimote.classicL_stickX = axis;
        }
        axis = WPAD_Stick(data, 0, 1);
        if(prev_state->wiimote.classicL_stickY != axis)
        {
            s16 value;
            if (axis >= 128) value = AXIS_MAX;
            else if (axis <=-128) value = AXIS_MIN;
            else value = axis << 8;
            SDL_PrivateJoystickAxis(joystick, 1, -value);
            prev_state->wiimote.classicL_stickY = axis;
        }
        axis = WPAD_Stick(data, 1, 0);
        if(prev_state->wiimote.classicR_stickX != axis)
        {
            SDL_PrivateJoystickAxis(joystick, 2, axis << 8);
            prev_state->wiimote.classicR_stickX = axis;
        }
        axis = WPAD_Stick(data, 1, 1);
        if(prev_state->wiimote.classicR_stickY != axis)
        {
            SDL_PrivateJoystickAxis(joystick, 3, -(axis << 8));
            prev_state->wiimote.classicR_stickY = axis;
        }
        axis = exp.classic.r_shoulder*255;
        if(prev_state->wiimote.classic_triggerR != axis)
        {
            SDL_PrivateJoystickAxis(joystick, 4, axis << 7);
            prev_state->wiimote.classic_triggerR = axis;
        }
        axis = exp.classic.l_shoulder*255;
        if(prev_state->wiimote.classic_triggerL != axis)
        {
            SDL_PrivateJoystickAxis(joystick, 5, axis << 7);
            prev_state->wiimote.classic_triggerL = axis;
        }
    }
    else if(exp_type == WPAD_EXP_NUNCHUK)
    {
        axis = WPAD_Stick(data, 0, 0);
        if(prev_state->wiimote.nunchuk_stickX != axis)
        {
            s16 value;
            if (axis >= 128) value = AXIS_MAX;
            else if (axis <=-128) value = AXIS_MIN;
            else value = axis << 8;
            SDL_PrivateJoystickAxis(joystick, 0, value);
            prev_state->wiimote.nunchuk_stickX = axis;
        }
        axis = WPAD_Stick(data, 0, 1);
        if(prev_state->wiimote.nunchuk_stickY != axis)
        {
            s16 value;
            if (axis >= 128) value = AXIS_MAX;
            else if (axis <=-128) value = AXIS_MIN;
            else value = axis << 8;
            SDL_PrivateJoystickAxis(joystick, 1, -value);
            prev_state->wiimote.nunchuk_stickY = axis;
        }
    }

    axis = WPAD_Pitch(data);
    if(prev_state->wiimote.wiimote_pitch != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 6, -(axis << 8));
        prev_state->wiimote.wiimote_pitch = axis;
    }
    axis = WPAD_Roll(data);
    if(prev_state->wiimote.wiimote_roll != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 7, axis << 8);
        prev_state->wiimote.wiimote_roll = axis;
    }
    axis = WPAD_Yaw(data);
    if(prev_state->wiimote.wiimote_yaw != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 8, axis << 8);
        prev_state->wiimote.wiimote_yaw = axis;
    }
}

static void _HandleGCJoystickUpdate(SDL_Joystick* joystick, int device_index)
{
    u16 buttons, prev_buttons, changed;
    int i;
    int axis;
    struct joystick_hwdata *prev_state;

    buttons = PAD_ButtonsHeld(device_index - 4);
    prev_state = (struct joystick_hwdata *)joystick->hwdata;
    prev_buttons = prev_state->gamecube.prev_buttons;
    changed = buttons ^ prev_buttons;

    if(changed & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP))
    {
        int hat = SDL_HAT_CENTERED;
        if(buttons & PAD_BUTTON_UP) hat |= SDL_HAT_UP;
        if(buttons & PAD_BUTTON_DOWN) hat |= SDL_HAT_DOWN;
        if(buttons & PAD_BUTTON_LEFT) hat |= SDL_HAT_LEFT;
        if(buttons & PAD_BUTTON_RIGHT) hat |= SDL_HAT_RIGHT;
        SDL_PrivateJoystickHat(joystick, 0, hat);
    }

    for(i = 0; i < (int)(sizeof(sdl_buttons_gc) / sizeof(sdl_buttons_gc[0])); i++)
    {
        if (changed & sdl_buttons_gc[i])
            SDL_PrivateJoystickButton(joystick, i,
                (buttons & sdl_buttons_gc[i]) ? SDL_PRESSED : SDL_RELEASED);
    }

    prev_state->gamecube.prev_buttons = buttons;
    axis = PAD_StickX(device_index-4);
    if(prev_state->gamecube.stickX != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 0, axis << 8);
        prev_state->gamecube.stickX = axis;
    }

    axis = PAD_StickY(device_index-4);
    if(prev_state->gamecube.stickY != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 1, (-axis) << 8);
        prev_state->gamecube.stickY = axis;
    }

    axis = PAD_SubStickX(device_index-4);
    if(prev_state->gamecube.substickX != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 2, axis << 8);
        prev_state->gamecube.substickX = axis;
    }

    axis = PAD_SubStickY(device_index-4);
    if(prev_state->gamecube.substickY != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 3, (-axis) << 8);
        prev_state->gamecube.substickY = axis;
    }

    axis = PAD_TriggerL(device_index-4);
    if(prev_state->gamecube.triggerL != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 4, axis << 7);
        prev_state->gamecube.triggerL = axis;
    }

    axis = PAD_TriggerR(device_index-4);
    if(prev_state->gamecube.triggerR != axis)
    {
        SDL_PrivateJoystickAxis(joystick, 5, axis << 7);
        prev_state->gamecube.triggerR = axis;
    }
}



/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return number of joysticks, or -1 on an unrecoverable fatal error.
 */
static int Wii_JoystickInit(void)
{
    int i = 0;
    /* Setup input */
    WPAD_Init();
    PAD_Init();

    for (i = 0; i < 1; ++i) {
        SDL_PrivateJoystickAdded(i);
    }

    return 1;
}

static int Wii_NumJoysticks(void)
{
    return 1;
}

static void Wii_JoystickDetect(void)
{
}

#if 0
/* Function to get the device-dependent name of a joystick */
static const char *PSP_JoystickName(int idx)
{
    if (idx == 0) return "PSP controller";
    SDL_SetError("No joystick available with that index");
    return NULL;
}
#endif

/* Function to get the device-dependent name of a joystick */
static const char *Wii_JoystickGetDeviceName(int device_index)
{
    switch(device_index)
    {
    case WPAD_CHAN_ALL:
        return "<all>";
    case WPAD_CHAN_0:
        return "WII Remote #1";
    case WPAD_CHAN_1:
        return "WII Remote #2";
    case WPAD_CHAN_2:
        return "WII Remote #3";
    case WPAD_CHAN_3:
        return "WII Remote #4";
    case WPAD_BALANCE_BOARD:
        return "WII Balance board";
    default:
        return "WII Remote";
    }
}

static const char *Wii_JoystickGetDevicePath(int device_index)
{
    (void)device_index;
    return NULL;
}

static int Wii_JoystickGetDevicePlayerIndex(int device_index)
{
    (void)device_index;
    return -1;
}

static void
Wii_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    (void)device_index;
    (void)player_index;
}

static SDL_JoystickGUID Wii_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    const char *name;

    /* the GUID is just the first 16 chars of the name for now */
    name = Wii_JoystickGetDeviceName(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));

    return guid;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID Wii_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
static int Wii_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    /* allocate memory for system specific hardware data */
    joystick->hwdata = (struct joystick_hwdata*)SDL_malloc(sizeof(struct joystick_hwdata));
    if (joystick->hwdata == NULL)
    {
        SDL_OutOfMemory();
        return(-1);
    }

    SDL_memset(joystick->hwdata, 0, sizeof(struct joystick_hwdata));
    if((device_index < 4) && (__jswpad_enabled))
    {
        if(device_index < __numwiijoysticks)
        {
            ((struct joystick_hwdata*)(joystick->hwdata))->index = device_index;
            ((struct joystick_hwdata*)(joystick->hwdata))->type = 0;
            joystick->nbuttons = MAX_WII_BUTTONS;
            joystick->naxes = MAX_WII_AXES;
            joystick->nhats = MAX_WII_HATS;
        }
    }
    else if((device_index < 8) && (__jspad_enabled))
    {
        if(device_index < (__numgcjoysticks + 4))
        {
            ((struct joystick_hwdata*)(joystick->hwdata))->index = device_index - 4;
            ((struct joystick_hwdata*)(joystick->hwdata))->type = 1;
            joystick->nbuttons = MAX_GC_BUTTONS;
            joystick->naxes = MAX_GC_AXES;
            joystick->nhats = MAX_GC_HATS;
        }
    }

    return 0;
}

static int
Wii_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    struct joystick_hwdata *j = ((struct joystick_hwdata*)(joystick->hwdata));
    int index;

    if (!j) {
        return 0;
    }

    index = j->index;

    if((index < 4) && (__jswpad_enabled))
    {
        if(index < __numwiijoysticks)
        {
            WPAD_Rumble(index, (low_frequency_rumble > 0 || high_frequency_rumble > 0) ? 1 : 0);
        }
    }
    else if((index < 8) && (__jspad_enabled))
    {
        if(index < (__numgcjoysticks + 4))
        {
            PAD_ControlMotor(index - 4, (low_frequency_rumble > 0 || high_frequency_rumble > 0) ? 1 : 0);
        }
    }

    return 0;
}

static int
Wii_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    (void)joystick;
    (void)left_rumble;
    (void)right_rumble;
    return SDL_Unsupported();
}

static Uint32 Wii_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    (void)joystick;
    return SDL_JOYCAP_RUMBLE;
}

static int
Wii_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    (void)joystick;
    (void)red;
    (void)green;
    (void)blue;

    return SDL_Unsupported();
}

static int
Wii_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    (void)joystick;
    (void)data;
    (void)size;
    return SDL_Unsupported();
}

static int Wii_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    (void)joystick;
    (void)enabled;
    return SDL_Unsupported();
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void Wii_JoystickUpdate(SDL_Joystick *joystick)
{
    struct joystick_hwdata* j;
    if(!joystick || !joystick->hwdata)
        return;

    WPAD_ScanPads();
    PAD_ScanPads();

    j = ((struct joystick_hwdata*)(joystick->hwdata));

    SDL_assert_always(j);

    switch(j->type)
    {
    case 0:
        if (__jswpad_enabled) {
            _HandleWiiJoystickUpdate(joystick, j->index);
        }
        break;
    case 1:
        if (__jspad_enabled) {
            _HandleGCJoystickUpdate(joystick, j->index);
        }
        break;
    default:
        break;
    }
}

/* Function to close a joystick after use */
static void Wii_JoystickClose(SDL_Joystick *joystick)
{
    if(!joystick || !joystick->hwdata) // joystick already closed
        return;

    SDL_free(joystick->hwdata);
}

/* Function to perform any system-specific joystick related cleanup */
static void Wii_JoystickQuit(void)
{
    WPAD_Shutdown();
}

SDL_JoystickDriver SDL_WII_JoystickDriver =
{
    Wii_JoystickInit,
    Wii_NumJoysticks,
    Wii_JoystickDetect,
    Wii_JoystickGetDeviceName,
    Wii_JoystickGetDevicePath,
    Wii_JoystickGetDevicePlayerIndex,
    Wii_JoystickSetDevicePlayerIndex,
    Wii_JoystickGetDeviceGUID,
    Wii_JoystickGetDeviceInstanceID,
    Wii_JoystickOpen,
    Wii_JoystickRumble,
    Wii_JoystickRumbleTriggers,
    Wii_JoystickGetCapabilities,
    Wii_JoystickSetLED,
    Wii_JoystickSendEffect,
    Wii_JoystickSetSensorsEnabled,
    Wii_JoystickUpdate,
    Wii_JoystickClose,
    Wii_JoystickQuit,
    NULL
};

#endif /* SDL_JOYSTICK_WII */

/* vim: ts=4 sw=4 */
