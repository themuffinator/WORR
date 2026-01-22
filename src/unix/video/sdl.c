/*
Copyright (C) 2013 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// video.c
//

#include "shared/shared.h"
#include "common/cvar.h"
#include "common/common.h"
#include "common/files.h"
#include "common/zone.h"
#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/ui.h"
#include "client/video.h"
#include "renderer/renderer.h"
#include "system/system.h"
#include "../res/worr.xbm"
#include <SDL3/SDL.h>

static struct {
    SDL_Window      *window;
    SDL_GLContext   context;
    vidFlags_t      flags;

    int             width;
    int             height;
    int             win_width;
    int             win_height;

    bool            wayland;
    SDL_WindowFlags focus_hack;
} sdl;

typedef struct {
    SDL_Gamepad     *pad;
    SDL_JoystickID  instance_id;
    bool            buttons[SDL_GAMEPAD_BUTTON_COUNT];
} sdl_gamepad_t;

static sdl_gamepad_t sdl_gamepad;

static void sdl_gamepad_close(unsigned time);
static void sdl_gamepad_open_first(void);

/*
===============================================================================

OPENGL STUFF

===============================================================================
*/

static void set_gl_attributes(void)
{
    r_opengl_config_t cfg = R_GetGLConfig();

    int colorbits = cfg.colorbits > 16 ? 8 : 5;
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, colorbits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, colorbits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, colorbits);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, cfg.depthbits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, cfg.stencilbits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (cfg.multisamples) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, cfg.multisamples);
    }

    if (cfg.debug)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    if (cfg.profile) {
        if (cfg.profile == QGL_PROFILE_ES)
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        else
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, cfg.major_ver);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, cfg.minor_ver);
    }
}

static void *get_proc_addr(const char *sym)
{
    return SDL_GL_GetProcAddress(sym);
}

static void swap_buffers(void)
{
    if (!SDL_GL_SwapWindow(sdl.window))
        Com_EPrintf("Couldn't swap buffers: %s\n", SDL_GetError());
}

static void swap_interval(int val)
{
    if (!SDL_GL_SetSwapInterval(val))
        Com_EPrintf("Couldn't set swap interval %d: %s\n", val, SDL_GetError());
}

/*
===============================================================================

VIDEO

===============================================================================
*/

static void mode_changed(void)
{
    if (!SDL_GetWindowSize(sdl.window, &sdl.win_width, &sdl.win_height)) {
        sdl.win_width = 0;
        sdl.win_height = 0;
    }

    if (!SDL_GetWindowSizeInPixels(sdl.window, &sdl.width, &sdl.height)) {
        sdl.width = sdl.win_width;
        sdl.height = sdl.win_height;
    }

    SDL_WindowFlags flags = SDL_GetWindowFlags(sdl.window);
    if (flags & SDL_WINDOW_FULLSCREEN)
        sdl.flags |= QVF_FULLSCREEN;
    else
        sdl.flags &= ~QVF_FULLSCREEN;

    R_ModeChanged(sdl.width, sdl.height, sdl.flags);
    SCR_ModeChanged();
}

static void set_mode(void)
{
    vrect_t rc;
    int freq;

    SDL_DisplayID display_id = SDL_GetDisplayForWindow(sdl.window);
    if (!display_id)
        display_id = SDL_GetPrimaryDisplay();

    if (vid_fullscreen->integer) {
        if (VID_GetFullscreen(&rc, &freq, NULL)) {
            SDL_DisplayMode mode = {0};

            mode.displayID = display_id;
            mode.format = SDL_PIXELFORMAT_UNKNOWN;
            mode.w = rc.width;
            mode.h = rc.height;
            mode.refresh_rate = (float)freq;

            if (!SDL_SetWindowFullscreenMode(sdl.window, &mode)) {
                Com_EPrintf("Couldn't set fullscreen mode: %s\n", SDL_GetError());
                SDL_SetWindowFullscreenMode(sdl.window, NULL);
            }
        } else {
            SDL_SetWindowFullscreenMode(sdl.window, NULL);
        }

        if (!SDL_SetWindowFullscreen(sdl.window, true))
            Com_EPrintf("Couldn't enter fullscreen: %s\n", SDL_GetError());
    } else {
        if (VID_GetGeometry(&rc)) {
            SDL_SetWindowSize(sdl.window, rc.width, rc.height);
            SDL_SetWindowPosition(sdl.window, rc.x, rc.y);
        }
        if (!SDL_SetWindowFullscreen(sdl.window, false))
            Com_EPrintf("Couldn't leave fullscreen: %s\n", SDL_GetError());
        SDL_SetWindowFullscreenMode(sdl.window, NULL);
    }

    mode_changed();
}

static void fatal_shutdown(void)
{
    sdl_gamepad_close(Sys_Milliseconds());
    SDL_SetWindowMouseGrab(sdl.window, false);
    SDL_SetWindowRelativeMouseMode(sdl.window, false);
    SDL_ShowCursor();
    SDL_Quit();
}

static char *get_clipboard_data(void)
{
    char *text = SDL_GetClipboardText();
    char *copy = UTF8_TranslitString(text);
    SDL_free(text);
    return copy;
}

static void set_clipboard_data(const char *data)
{
    SDL_SetClipboardText(data);
}

static void update_gamma(const byte *table)
{
    (void)table;
}

static bool my_event_filter(void *userdata, SDL_Event *event)
{
    // SDL uses relative time, we need absolute
    event->common.timestamp = SDL_MS_TO_NS(Sys_Milliseconds());
    return true;
}

static int get_refresh_rate(const SDL_DisplayMode *mode)
{
    if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0)
        return (mode->refresh_rate_numerator + mode->refresh_rate_denominator / 2) / mode->refresh_rate_denominator;
    if (mode->refresh_rate > 0.0f)
        return (int)(mode->refresh_rate + 0.5f);
    return 0;
}

static char *get_mode_list(void)
{
    SDL_DisplayMode **modes;
    size_t size, len;
    char *buf;
    int i, num_modes;

    SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
    modes = SDL_GetFullscreenDisplayModes(display_id, &num_modes);
    if (!modes || num_modes < 1)
        return Z_CopyString(VID_MODELIST);

    size = 8 + num_modes * 32 + 1;
    buf = Z_Malloc(size);

    len = Q_strlcpy(buf, "desktop ", size);
    for (i = 0; i < num_modes; i++) {
        const SDL_DisplayMode *mode = modes[i];
        int refresh;

        if (!mode)
            break;
        refresh = get_refresh_rate(mode);
        if (refresh == 0)
            continue;
        if (mode->w < VIRTUAL_SCREEN_WIDTH || mode->h < VIRTUAL_SCREEN_HEIGHT)
            continue;
        len += Q_scnprintf(buf + len, size - len, "%dx%d@%d ",
                           mode->w, mode->h, refresh);
    }
    buf[len - 1] = 0;
    SDL_free(modes);

    return buf;
}

static int get_dpi_scale(void)
{
    if (sdl.win_width && sdl.win_height) {
        int scale_x = (sdl.width + sdl.win_width / 2) / sdl.win_width;
        int scale_y = (sdl.height + sdl.win_height / 2) / sdl.win_height;
        if (scale_x == scale_y)
            return Q_clip(scale_x, 1, 10);
    }

    return 1;
}

static void shutdown(void)
{
    sdl_gamepad_close(Sys_Milliseconds());
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);

    if (sdl.context)
        SDL_GL_DestroyContext(sdl.context);

    if (sdl.window)
        SDL_DestroyWindow(sdl.window);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    memset(&sdl, 0, sizeof(sdl));
}

static bool create_window_and_context(const vrect_t *rc)
{
    sdl.window = SDL_CreateWindow(PRODUCT, rc->width, rc->height,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!sdl.window) {
        Com_EPrintf("Couldn't create SDL window: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_WINDOWPOS_ISUNDEFINED(rc->x) && !SDL_WINDOWPOS_ISUNDEFINED(rc->y))
        SDL_SetWindowPosition(sdl.window, rc->x, rc->y);

    sdl.context = SDL_GL_CreateContext(sdl.window);
    if (!sdl.context) {
        Com_EPrintf("Couldn't create OpenGL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(sdl.window);
        sdl.window = NULL;
        return false;
    }

    return true;
}

static bool init(void)
{
    vrect_t rc;

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        Com_EPrintf("Couldn't initialize SDL video: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        Com_WPrintf("Couldn't initialize SDL gamepad: %s\n", SDL_GetError());
    } else {
        SDL_SetGamepadEventsEnabled(true);
        sdl_gamepad_open_first();
    }

    set_gl_attributes();

    SDL_SetEventFilter(my_event_filter, NULL);

    if (!VID_GetGeometry(&rc)) {
        rc.x = SDL_WINDOWPOS_UNDEFINED;
        rc.y = SDL_WINDOWPOS_UNDEFINED;
    }

    if (!create_window_and_context(&rc)) {
        Com_Printf("Falling back to failsafe config\n");
        SDL_GL_ResetAttributes();
        if (!create_window_and_context(&rc)) {
            shutdown();
            return false;
        }
    }

    SDL_SetWindowMinimumSize(sdl.window, VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT);

    SDL_Surface *icon = SDL_CreateSurfaceFrom(q2icon_width, q2icon_height, SDL_PIXELFORMAT_INDEX1LSB,
                                              (void *)q2icon_bits, q2icon_width / 8);
    if (icon) {
        SDL_Palette *palette = SDL_GetSurfacePalette(icon);
        SDL_Color colors[2] = {
            { 255, 255, 255 },
            {   0, 128, 128 }
        };
        if (palette)
            SDL_SetPaletteColors(palette, colors, 0, 2);
        SDL_SetSurfaceColorKey(icon, true, 0);
        SDL_SetWindowIcon(sdl.window, icon);
        SDL_DestroySurface(icon);
    }

    cvar_t *vid_hwgamma = Cvar_Get("vid_hwgamma", "0", CVAR_RENDERER);
    if (vid_hwgamma->integer) {
        Com_Printf("...hardware gamma not supported by SDL3\n");
        Cvar_Reset(vid_hwgamma);
    }

    Com_Printf("Using SDL video driver: %s\n", SDL_GetCurrentVideoDriver());

    // activate disgusting wayland hacks
    sdl.wayland = !strcmp(SDL_GetCurrentVideoDriver(), "wayland");

    return true;
}

/*
==========================================================================

EVENTS

==========================================================================
*/

static void window_event(SDL_WindowEvent *event)
{
    SDL_WindowFlags flags = SDL_GetWindowFlags(sdl.window);
    active_t active;
    vrect_t rc;

    // wayland doesn't set SDL_WINDOW_*_FOCUS flags
    if (sdl.wayland) {
        switch ((int)event->type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            sdl.focus_hack = SDL_WINDOW_INPUT_FOCUS;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            sdl.focus_hack = 0;
            break;
        }
        flags |= sdl.focus_hack;
    }

    switch ((int)event->type) {
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
        if (flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) {
            active = ACT_ACTIVATED;
        } else if (flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
            active = ACT_MINIMIZED;
        } else {
            active = ACT_RESTORED;
        }
        CL_Activate(active);
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        Com_Quit(NULL, ERR_DISCONNECT);
        break;

    case SDL_EVENT_WINDOW_MOVED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowSize(sdl.window, &rc.width, &rc.height);
            rc.x = event->data1;
            rc.y = event->data2;
            VID_SetGeometry(&rc);
        }
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowPosition(sdl.window, &rc.x, &rc.y);
            rc.width = event->data1;
            rc.height = event->data2;
            VID_SetGeometry(&rc);
        }
        mode_changed();
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        mode_changed();
        break;
    }
}

static const byte scantokey[] = {
    #include "keytables/sdl.h"
};

static const byte scantokey2[] = {
    K_LCTRL, K_LSHIFT, K_LALT, K_LWINKEY, K_RCTRL, K_RSHIFT, K_RALT, K_RWINKEY
};

static unsigned sdl_event_time(Uint64 timestamp)
{
    return (unsigned)SDL_NS_TO_MS(timestamp);
}

static int sdl_gamepad_button_to_key(SDL_GamepadButton button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return K_A_BUTTON;
    case SDL_GAMEPAD_BUTTON_EAST:
        return K_B_BUTTON;
    case SDL_GAMEPAD_BUTTON_WEST:
        return K_X_BUTTON;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return K_Y_BUTTON;
    case SDL_GAMEPAD_BUTTON_BACK:
        return K_BACK_BUTTON;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return K_GUIDE_BUTTON;
    case SDL_GAMEPAD_BUTTON_START:
        return K_START_BUTTON;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return K_LEFT_STICK;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return K_RIGHT_STICK;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return K_LEFT_SHOULDER;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return K_RIGHT_SHOULDER;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return K_DPAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return K_DPAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return K_DPAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return K_DPAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
        return K_RIGHT_PADDLE1;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
        return K_LEFT_PADDLE1;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
        return K_RIGHT_PADDLE2;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
        return K_LEFT_PADDLE2;
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
        return K_TOUCHPAD;
    case SDL_GAMEPAD_BUTTON_MISC1:
        return K_MISC1;
    case SDL_GAMEPAD_BUTTON_MISC2:
        return K_MISC2;
    case SDL_GAMEPAD_BUTTON_MISC3:
        return K_MISC3;
    case SDL_GAMEPAD_BUTTON_MISC4:
        return K_MISC4;
    case SDL_GAMEPAD_BUTTON_MISC5:
        return K_MISC5;
    case SDL_GAMEPAD_BUTTON_MISC6:
        return K_MISC6;
    default:
        return 0;
    }
}

static bool sdl_gamepad_axis_to_input(SDL_GamepadAxis axis, in_gamepad_axis_t *out_axis)
{
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        *out_axis = IN_GAMEPAD_AXIS_LEFTX;
        return true;
    case SDL_GAMEPAD_AXIS_LEFTY:
        *out_axis = IN_GAMEPAD_AXIS_LEFTY;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHTX:
        *out_axis = IN_GAMEPAD_AXIS_RIGHTX;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHTY:
        *out_axis = IN_GAMEPAD_AXIS_RIGHTY;
        return true;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        *out_axis = IN_GAMEPAD_AXIS_LEFT_TRIGGER;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        *out_axis = IN_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return true;
    default:
        return false;
    }
}

static void sdl_gamepad_release_buttons(unsigned time)
{
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (!sdl_gamepad.buttons[i])
            continue;
        int key = sdl_gamepad_button_to_key((SDL_GamepadButton)i);
        if (key)
            Key_Event(key, false, time);
        sdl_gamepad.buttons[i] = false;
    }
}

static void sdl_gamepad_close(unsigned time)
{
    if (!sdl_gamepad.pad)
        return;

    sdl_gamepad_release_buttons(time);
    IN_GamepadReset(time);
    SDL_CloseGamepad(sdl_gamepad.pad);
    memset(&sdl_gamepad, 0, sizeof(sdl_gamepad));
}

static void sdl_gamepad_open(SDL_JoystickID instance_id)
{
    if (sdl_gamepad.pad)
        return;

    SDL_Gamepad *pad = SDL_OpenGamepad(instance_id);
    if (!pad) {
        Com_EPrintf("Couldn't open SDL gamepad: %s\n", SDL_GetError());
        return;
    }

    sdl_gamepad.pad = pad;
    sdl_gamepad.instance_id = instance_id;
    memset(sdl_gamepad.buttons, 0, sizeof(sdl_gamepad.buttons));

    const char *name = SDL_GetGamepadName(pad);
    if (name && *name)
        Com_Printf("Opened SDL gamepad: %s\n", name);
    else
        Com_Printf("Opened SDL gamepad.\n");
}

static void sdl_gamepad_open_first(void)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids || count <= 0) {
        SDL_free(ids);
        return;
    }

    for (int i = 0; i < count; i++) {
        sdl_gamepad_open(ids[i]);
        if (sdl_gamepad.pad)
            break;
    }

    SDL_free(ids);
}

static void set_cursor_visible(bool visible)
{
    if (visible)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

static void key_event(SDL_KeyboardEvent *event)
{
    unsigned key = event->scancode;

    if (key < q_countof(scantokey))
        key = scantokey[key];
    else if (key >= SDL_SCANCODE_LCTRL && key < SDL_SCANCODE_LCTRL + q_countof(scantokey2))
        key = scantokey2[key - SDL_SCANCODE_LCTRL];
    else
        key = 0;

    if (!key) {
        Com_DPrintf("%s: unknown scancode %d\n", __func__, event->scancode);
        return;
    }

    Key_Event2(key, event->down, sdl_event_time(event->timestamp));
}

static void mouse_button_event(SDL_MouseButtonEvent *event)
{
    unsigned key;

    switch (event->button) {
    case SDL_BUTTON_LEFT:
        key = K_MOUSE1;
        break;
    case SDL_BUTTON_RIGHT:
        key = K_MOUSE2;
        break;
    case SDL_BUTTON_MIDDLE:
        key = K_MOUSE3;
        break;
    case SDL_BUTTON_X1:
        key = K_MOUSE4;
        break;
    case SDL_BUTTON_X2:
        key = K_MOUSE5;
        break;
    default:
        Com_DPrintf("%s: unknown button %d\n", __func__, event->button);
        return;
    }

    Key_Event(key, event->down, sdl_event_time(event->timestamp));
}

static void mouse_wheel_event(SDL_MouseWheelEvent *event)
{
    float x = event->x;
    float y = event->y;
    unsigned time = sdl_event_time(event->timestamp);

    if (event->direction == SDL_MOUSEWHEEL_FLIPPED) {
        x = -x;
        y = -y;
    }

    if (x > 0) {
        Key_Event(K_MWHEELRIGHT, true, time);
        Key_Event(K_MWHEELRIGHT, false, time);
    } else if (x < 0) {
        Key_Event(K_MWHEELLEFT, true, time);
        Key_Event(K_MWHEELLEFT, false, time);
    }

    if (y > 0) {
        Key_Event(K_MWHEELUP, true, time);
        Key_Event(K_MWHEELUP, false, time);
    } else if (y < 0) {
        Key_Event(K_MWHEELDOWN, true, time);
        Key_Event(K_MWHEELDOWN, false, time);
    }
}

static void pump_events(void)
{
    SDL_Event    event;

    while (SDL_PollEvent(&event)) {
        if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
            window_event(&event.window);
            continue;
        }

        switch (event.type) {
        case SDL_EVENT_QUIT:
            Com_Quit(NULL, ERR_DISCONNECT);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            key_event(&event.key);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (sdl.win_width && sdl.win_height)
                UI_MouseEvent((int)(event.motion.x * sdl.width / sdl.win_width),
                              (int)(event.motion.y * sdl.height / sdl.win_height));
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            mouse_button_event(&event.button);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            mouse_wheel_event(&event.wheel);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!sdl_gamepad.pad)
                sdl_gamepad_open(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (sdl_gamepad.pad && event.gdevice.which == sdl_gamepad.instance_id) {
                sdl_gamepad_close(sdl_event_time(event.gdevice.timestamp));
                sdl_gamepad_open_first();
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (sdl_gamepad.pad && event.gbutton.which == sdl_gamepad.instance_id) {
                int key = sdl_gamepad_button_to_key((SDL_GamepadButton)event.gbutton.button);
                if (key) {
                    if (event.gbutton.down) {
                        if (!IN_GamepadEnabled())
                            break;
                        sdl_gamepad.buttons[event.gbutton.button] = true;
                        Key_Event(key, true, sdl_event_time(event.gbutton.timestamp));
                    } else {
                        if (!sdl_gamepad.buttons[event.gbutton.button])
                            break;
                        sdl_gamepad.buttons[event.gbutton.button] = false;
                        Key_Event(key, false, sdl_event_time(event.gbutton.timestamp));
                    }
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (sdl_gamepad.pad && event.gaxis.which == sdl_gamepad.instance_id) {
                in_gamepad_axis_t axis;
                if (sdl_gamepad_axis_to_input((SDL_GamepadAxis)event.gaxis.axis, &axis)) {
                    IN_GamepadAxisEvent(axis, event.gaxis.value, sdl_event_time(event.gaxis.timestamp));
                }
            }
            break;
        }
    }
}

/*
===============================================================================

MOUSE

===============================================================================
*/

static bool get_mouse_motion(int *dx, int *dy)
{
    float fx = 0.0f;
    float fy = 0.0f;

    if (!SDL_GetWindowRelativeMouseMode(sdl.window)) {
        return false;
    }
    SDL_GetRelativeMouseState(&fx, &fy);
    *dx = (int)fx;
    *dy = (int)fy;
    return true;
}

static void warp_mouse(int x, int y)
{
    SDL_WarpMouseInWindow(sdl.window, (float)x, (float)y);
    SDL_GetRelativeMouseState(NULL, NULL);
}

static void shutdown_mouse(void)
{
    SDL_SetWindowMouseGrab(sdl.window, false);
    SDL_SetWindowRelativeMouseMode(sdl.window, false);
    set_cursor_visible(true);
}

static bool init_mouse(void)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        return false;
    }

    Com_Printf("SDL mouse initialized.\n");
    return true;
}

static void grab_mouse(bool grab)
{
    SDL_SetWindowMouseGrab(sdl.window, grab);
    SDL_SetWindowRelativeMouseMode(sdl.window, grab && !(Key_GetDest() & (KEY_MENU | KEY_MESSAGE)));
    SDL_GetRelativeMouseState(NULL, NULL);
    bool show_cursor = !(sdl.flags & QVF_FULLSCREEN) && !(Key_GetDest() & KEY_MENU);
    set_cursor_visible(show_cursor);
}

static bool probe(void)
{
    return true;
}

static bool sdl_get_native_window(vid_native_window_t *out)
{
    if (!out)
        return false;

    SDL_PropertiesID props = SDL_GetWindowProperties(sdl.window);
    if (props) {
        void *hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (hwnd) {
            out->platform = VID_NATIVE_WIN32;
            out->handle.win32.hwnd = hwnd;
            out->handle.win32.hinstance = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, NULL);
            out->handle.win32.hdc = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HDC_POINTER, NULL);
            return true;
        }

        void *wl_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        void *wl_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        if (wl_display && wl_surface) {
            out->platform = VID_NATIVE_WAYLAND;
            out->handle.wayland.display = wl_display;
            out->handle.wayland.surface = wl_surface;
            return true;
        }

        void *x11_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Sint64 x11_window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (x11_display && x11_window) {
            out->platform = VID_NATIVE_X11;
            out->handle.x11.display = x11_display;
            out->handle.x11.window = (uintptr_t)x11_window;
            return true;
        }
    }

    out->platform = VID_NATIVE_SDL;
    out->handle.sdl.window = sdl.window;
    return true;
}

const vid_driver_t vid_sdl = {
    .name = "sdl",

    .probe = probe,
    .init = init,
    .shutdown = shutdown,
    .fatal_shutdown = fatal_shutdown,
    .pump_events = pump_events,

    .get_mode_list = get_mode_list,
    .get_dpi_scale = get_dpi_scale,
    .set_mode = set_mode,
    .update_gamma = update_gamma,

    .get_proc_addr = get_proc_addr,
    .swap_buffers = swap_buffers,
    .swap_interval = swap_interval,

    .get_clipboard_data = get_clipboard_data,
    .set_clipboard_data = set_clipboard_data,

    .init_mouse = init_mouse,
    .shutdown_mouse = shutdown_mouse,
    .grab_mouse = grab_mouse,
    .warp_mouse = warp_mouse,
    .get_mouse_motion = get_mouse_motion,
    .get_native_window = sdl_get_native_window,
};
