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
        switch (event->type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            sdl.focus_hack = SDL_WINDOW_INPUT_FOCUS;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            sdl.focus_hack = 0;
            break;
        }
        flags |= sdl.focus_hack;
    }

    switch (event->type) {
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
    SDL_SetWindowRelativeMouseMode(sdl.window, grab && !(Key_GetDest() & KEY_MENU));
    SDL_GetRelativeMouseState(NULL, NULL);
    set_cursor_visible(!(sdl.flags & QVF_FULLSCREEN));
}

static bool probe(void)
{
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
};
