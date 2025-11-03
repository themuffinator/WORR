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

#include "shared/shared.hpp"
#include "common/cvar.hpp"
#include "common/common.hpp"
#include "common/files.hpp"
#include "common/zone.hpp"
#include "client/client.hpp"
#include "client/input.hpp"
#include "client/keys.hpp"
#include "client/ui.hpp"
#include "client/video.hpp"
#include "refresh/refresh.hpp"
#include "system/system.hpp"
#include "../res/q2pro.xbm"
#include <SDL.h>

#include <array>

static struct {
    SDL_Window      *window;
    SDL_GLContext   context;
    vidFlags_t      flags;

    int             width;
    int             height;
    int             win_width;
    int             win_height;

    bool            wayland;
    int             focus_hack;
} sdl;

static constexpr size_t MAX_GAME_CONTROLLERS = 8;
static constexpr int PAD_STICK_DEADZONE = 16000;
static constexpr int PAD_TRIGGER_DEADZONE = 16000;

enum class PadAxisKey : size_t {
    LSTICK_LEFT,
    LSTICK_RIGHT,
    LSTICK_UP,
    LSTICK_DOWN,
    RSTICK_LEFT,
    RSTICK_RIGHT,
    RSTICK_UP,
    RSTICK_DOWN,
    LTRIGGER,
    RTRIGGER,
    COUNT
};

static constexpr std::array<unsigned, static_cast<size_t>(PadAxisKey::COUNT)> pad_axis_keys = {
    K_PAD_LSTICK_LEFT,
    K_PAD_LSTICK_RIGHT,
    K_PAD_LSTICK_UP,
    K_PAD_LSTICK_DOWN,
    K_PAD_RSTICK_LEFT,
    K_PAD_RSTICK_RIGHT,
    K_PAD_RSTICK_UP,
    K_PAD_RSTICK_DOWN,
    K_PAD_LTRIGGER,
    K_PAD_RTRIGGER,
};

struct controller_state_t {
    SDL_GameController                           *controller = nullptr;
    SDL_JoystickID                               instance = -1;
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX>  button_down{};
    std::array<bool, static_cast<size_t>(PadAxisKey::COUNT)> axis_down{};
};

struct joystick_state_t {
    SDL_Joystick                                 *joystick = nullptr;
    SDL_JoystickID                               instance = -1;
    std::array<bool, 32>                         button_down{};
    std::array<bool, static_cast<size_t>(PadAxisKey::COUNT)> axis_down{};
    std::array<bool, 4>                          hat_down{};
};

static std::array<controller_state_t, MAX_GAME_CONTROLLERS> controllers;
static std::array<joystick_state_t, MAX_GAME_CONTROLLERS> joysticks;

static controller_state_t *alloc_controller(void)
{
    for (auto &c : controllers) {
        if (!c.controller)
            return &c;
    }
    return NULL;
}

static controller_state_t *find_controller(SDL_JoystickID instance)
{
    for (auto &c : controllers) {
        if (c.controller && c.instance == instance)
            return &c;
    }
    return NULL;
}

static joystick_state_t *alloc_joystick(void)
{
    for (auto &j : joysticks) {
        if (!j.joystick)
            return &j;
    }
    return NULL;
}

static joystick_state_t *find_joystick(SDL_JoystickID instance)
{
    for (auto &j : joysticks) {
        if (j.joystick && j.instance == instance)
            return &j;
    }
    return NULL;
}

static void pad_axis_change(std::array<bool, static_cast<size_t>(PadAxisKey::COUNT)> &states,
                            PadAxisKey axis, bool down, unsigned timestamp)
{
    size_t idx = static_cast<size_t>(axis);
    if (states[idx] == down)
        return;

    states[idx] = down;
    Key_Event(pad_axis_keys[idx], down, timestamp);
}

static void clear_axis_state(std::array<bool, static_cast<size_t>(PadAxisKey::COUNT)> &states,
                             unsigned timestamp)
{
    for (size_t i = 0; i < states.size(); i++) {
        if (states[i]) {
            states[i] = false;
            Key_Event(pad_axis_keys[i], false, timestamp);
        }
    }
}

static unsigned controller_button_key(SDL_GameControllerButton button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:             return K_PAD_A;
    case SDL_CONTROLLER_BUTTON_B:             return K_PAD_B;
    case SDL_CONTROLLER_BUTTON_X:             return K_PAD_X;
    case SDL_CONTROLLER_BUTTON_Y:             return K_PAD_Y;
    case SDL_CONTROLLER_BUTTON_BACK:          return K_PAD_BACK;
    case SDL_CONTROLLER_BUTTON_GUIDE:         return K_PAD_GUIDE;
    case SDL_CONTROLLER_BUTTON_START:         return K_PAD_START;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return K_PAD_LSTICK;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return K_PAD_RSTICK;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return K_PAD_LSHOULDER;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_PAD_RSHOULDER;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return K_PAD_DPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return K_PAD_DPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return K_PAD_DPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return K_PAD_DPAD_RIGHT;
#ifdef SDL_CONTROLLER_BUTTON_MISC1
    case SDL_CONTROLLER_BUTTON_MISC1:         return K_PAD_MISC1;
#endif
#ifdef SDL_CONTROLLER_BUTTON_PADDLE1
    case SDL_CONTROLLER_BUTTON_PADDLE1:       return K_PAD_PADDLE1;
    case SDL_CONTROLLER_BUTTON_PADDLE2:       return K_PAD_PADDLE2;
    case SDL_CONTROLLER_BUTTON_PADDLE3:       return K_PAD_PADDLE3;
    case SDL_CONTROLLER_BUTTON_PADDLE4:       return K_PAD_PADDLE4;
#endif
#ifdef SDL_CONTROLLER_BUTTON_TOUCHPAD
    case SDL_CONTROLLER_BUTTON_TOUCHPAD:      return K_PAD_TOUCHPAD;
#endif
    default:
        return 0;
    }
}

static unsigned joystick_button_key(size_t index)
{
    switch (index) {
    case 0:  return K_PAD_A;
    case 1:  return K_PAD_B;
    case 2:  return K_PAD_X;
    case 3:  return K_PAD_Y;
    case 4:  return K_PAD_LSHOULDER;
    case 5:  return K_PAD_RSHOULDER;
    case 6:  return K_PAD_BACK;
    case 7:  return K_PAD_START;
    case 8:  return K_PAD_LSTICK;
    case 9:  return K_PAD_RSTICK;
    case 10: return K_PAD_MISC1;
    case 11: return K_PAD_MISC2;
    case 12: return K_PAD_MISC3;
    case 13: return K_PAD_MISC4;
    case 14: return K_PAD_MISC5;
    case 15: return K_PAD_MISC6;
    case 16: return K_PAD_MISC7;
    case 17: return K_PAD_MISC8;
    default: return 0;
    }
}

static void close_controller(controller_state_t &c)
{
    if (!c.controller)
        return;

    unsigned timestamp = Sys_Milliseconds();
    for (size_t i = 0; i < c.button_down.size(); i++) {
        if (c.button_down[i]) {
            c.button_down[i] = false;
            unsigned key = controller_button_key(static_cast<SDL_GameControllerButton>(i));
            if (key)
                Key_Event(key, false, timestamp);
        }
    }

    clear_axis_state(c.axis_down, timestamp);

    SDL_GameControllerClose(c.controller);
    c.controller = NULL;
    c.instance = -1;
}

static void close_joystick(joystick_state_t &j)
{
    if (!j.joystick)
        return;

    unsigned timestamp = Sys_Milliseconds();
    for (size_t i = 0; i < j.button_down.size(); i++) {
        if (j.button_down[i]) {
            j.button_down[i] = false;
            unsigned key = joystick_button_key(i);
            if (key)
                Key_Event(key, false, timestamp);
        }
    }

    clear_axis_state(j.axis_down, timestamp);

    for (size_t i = 0; i < j.hat_down.size(); i++) {
        if (j.hat_down[i]) {
            j.hat_down[i] = false;
            unsigned key = 0;
            switch (i) {
            case 0: key = K_PAD_DPAD_UP; break;
            case 1: key = K_PAD_DPAD_DOWN; break;
            case 2: key = K_PAD_DPAD_LEFT; break;
            case 3: key = K_PAD_DPAD_RIGHT; break;
            }
            if (key)
                Key_Event(key, false, timestamp);
        }
    }

    SDL_JoystickClose(j.joystick);
    j.joystick = NULL;
    j.instance = -1;
}

static void controller_added(int device_index)
{
    SDL_GameController *gc = SDL_GameControllerOpen(device_index);
    if (!gc) {
        Com_EPrintf("Couldn't open game controller %d: %s\n", device_index, SDL_GetError());
        return;
    }

    SDL_Joystick *joy = SDL_GameControllerGetJoystick(gc);
    SDL_JoystickID instance = SDL_JoystickInstanceID(joy);

    if (find_controller(instance)) {
        SDL_GameControllerClose(gc);
        return;
    }

    controller_state_t *slot = alloc_controller();
    if (!slot) {
        Com_DPrintf("No free controller slots for device %d\n", device_index);
        SDL_GameControllerClose(gc);
        return;
    }

    slot->controller = gc;
    slot->instance = instance;
    slot->button_down.fill(false);
    slot->axis_down.fill(false);

    const char *name = SDL_GameControllerName(gc);
    Com_Printf("Gamepad connected: %s\n", name ? name : "Unknown");
}

static void controller_removed(SDL_JoystickID instance)
{
    controller_state_t *slot = find_controller(instance);
    if (!slot)
        return;

    const char *name = SDL_GameControllerName(slot->controller);
    Com_Printf("Gamepad disconnected: %s\n", name ? name : "Unknown");
    close_controller(*slot);
}

static void controller_button_event(SDL_ControllerButtonEvent *event)
{
    controller_state_t *slot = find_controller(event->which);
    if (!slot)
        return;

    if (event->button >= SDL_CONTROLLER_BUTTON_MAX)
        return;

    bool down = event->state == SDL_PRESSED;
    size_t idx = event->button;

    if (slot->button_down[idx] == down)
        return;

    slot->button_down[idx] = down;
    unsigned key = controller_button_key(static_cast<SDL_GameControllerButton>(event->button));
    if (key)
        Key_Event(key, down, event->timestamp);
}

static void controller_axis_event(SDL_ControllerAxisEvent *event)
{
    controller_state_t *slot = find_controller(event->which);
    if (!slot)
        return;

    int value = event->value;
    unsigned time = event->timestamp;

    switch (event->axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_LEFT, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_RIGHT, value >= PAD_STICK_DEADZONE, time);
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_UP, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_DOWN, value >= PAD_STICK_DEADZONE, time);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_LEFT, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_RIGHT, value >= PAD_STICK_DEADZONE, time);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_UP, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_DOWN, value >= PAD_STICK_DEADZONE, time);
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        pad_axis_change(slot->axis_down, PadAxisKey::LTRIGGER, value >= PAD_TRIGGER_DEADZONE, time);
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        pad_axis_change(slot->axis_down, PadAxisKey::RTRIGGER, value >= PAD_TRIGGER_DEADZONE, time);
        break;
    default:
        break;
    }
}

static void joystick_added(int device_index)
{
    if (SDL_IsGameController(device_index))
        return;

    SDL_Joystick *joy = SDL_JoystickOpen(device_index);
    if (!joy) {
        Com_EPrintf("Couldn't open joystick %d: %s\n", device_index, SDL_GetError());
        return;
    }

    SDL_JoystickID instance = SDL_JoystickInstanceID(joy);

    if (find_joystick(instance)) {
        SDL_JoystickClose(joy);
        return;
    }

    joystick_state_t *slot = alloc_joystick();
    if (!slot) {
        Com_DPrintf("No free joystick slots for device %d\n", device_index);
        SDL_JoystickClose(joy);
        return;
    }

    slot->joystick = joy;
    slot->instance = instance;
    slot->button_down.fill(false);
    slot->axis_down.fill(false);
    slot->hat_down.fill(false);

    const char *name = SDL_JoystickName(joy);
    Com_Printf("Joystick connected: %s\n", name ? name : "Unknown");
}

static void joystick_removed(SDL_JoystickID instance)
{
    joystick_state_t *slot = find_joystick(instance);
    if (!slot)
        return;

    const char *name = SDL_JoystickName(slot->joystick);
    Com_Printf("Joystick disconnected: %s\n", name ? name : "Unknown");
    close_joystick(*slot);
}

static void joystick_button_event(SDL_JoyButtonEvent *event)
{
    joystick_state_t *slot = find_joystick(event->which);
    if (!slot)
        return;

    size_t idx = event->button;
    if (idx >= slot->button_down.size())
        return;

    bool down = event->state == SDL_PRESSED;
    if (slot->button_down[idx] == down)
        return;

    slot->button_down[idx] = down;
    unsigned key = joystick_button_key(idx);
    if (key)
        Key_Event(key, down, event->timestamp);
}

static void joystick_axis_event(SDL_JoyAxisEvent *event)
{
    joystick_state_t *slot = find_joystick(event->which);
    if (!slot)
        return;

    int value = event->value;
    unsigned time = event->timestamp;

    switch (event->axis) {
    case 0:
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_LEFT, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_RIGHT, value >= PAD_STICK_DEADZONE, time);
        break;
    case 1:
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_UP, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::LSTICK_DOWN, value >= PAD_STICK_DEADZONE, time);
        break;
    case 2:
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_LEFT, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_RIGHT, value >= PAD_STICK_DEADZONE, time);
        break;
    case 3:
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_UP, value <= -PAD_STICK_DEADZONE, time);
        pad_axis_change(slot->axis_down, PadAxisKey::RSTICK_DOWN, value >= PAD_STICK_DEADZONE, time);
        break;
    case 4:
        pad_axis_change(slot->axis_down, PadAxisKey::LTRIGGER, value >= PAD_TRIGGER_DEADZONE, time);
        break;
    case 5:
        pad_axis_change(slot->axis_down, PadAxisKey::RTRIGGER, value >= PAD_TRIGGER_DEADZONE, time);
        break;
    default:
        break;
    }
}

static void joystick_hat_event(SDL_JoyHatEvent *event)
{
    joystick_state_t *slot = find_joystick(event->which);
    if (!slot)
        return;

    if (event->hat != 0)
        return;

    auto update_hat = [&](size_t idx, unsigned key, bool down) {
        if (slot->hat_down[idx] == down)
            return;
        slot->hat_down[idx] = down;
        Key_Event(key, down, event->timestamp);
    };

    Uint8 value = event->value;
    update_hat(0, K_PAD_DPAD_UP,    (value & SDL_HAT_UP) != 0);
    update_hat(1, K_PAD_DPAD_DOWN,  (value & SDL_HAT_DOWN) != 0);
    update_hat(2, K_PAD_DPAD_LEFT,  (value & SDL_HAT_LEFT) != 0);
    update_hat(3, K_PAD_DPAD_RIGHT, (value & SDL_HAT_RIGHT) != 0);
}

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
    SDL_GL_SwapWindow(sdl.window);
}

static void swap_interval(int val)
{
    if (SDL_GL_SetSwapInterval(val) < 0)
        Com_EPrintf("Couldn't set swap interval %d: %s\n", val, SDL_GetError());
}

/*
===============================================================================

VIDEO

===============================================================================
*/

static void mode_changed(void)
{
    SDL_GetWindowSize(sdl.window, &sdl.win_width, &sdl.win_height);

    SDL_GL_GetDrawableSize(sdl.window, &sdl.width, &sdl.height);

    Uint32 flags = SDL_GetWindowFlags(sdl.window);
    if (flags & SDL_WINDOW_FULLSCREEN)
        sdl.flags |= QVF_FULLSCREEN;
    else
        sdl.flags &= ~QVF_FULLSCREEN;

    R_ModeChanged(sdl.width, sdl.height, sdl.flags);
    SCR_ModeChanged();
}

static void set_mode(void)
{
    Uint32 flags;
    vrect_t rc;
    int freq;

    if (vid_fullscreen->integer) {
        if (VID_GetFullscreen(&rc, &freq, NULL)) {
            SDL_DisplayMode mode = {
                .format         = SDL_PIXELFORMAT_UNKNOWN,
                .w              = rc.width,
                .h              = rc.height,
                .refresh_rate   = freq,
                .driverdata     = NULL
            };
            SDL_SetWindowDisplayMode(sdl.window, &mode);
            flags = SDL_WINDOW_FULLSCREEN;
        } else {
            flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
    } else {
        if (VID_GetGeometry(&rc)) {
            SDL_SetWindowSize(sdl.window, rc.width, rc.height);
            SDL_SetWindowPosition(sdl.window, rc.x, rc.y);
        }
        flags = 0;
    }

    SDL_SetWindowFullscreen(sdl.window, flags);
    mode_changed();
}

static void fatal_shutdown(void)
{
    SDL_SetWindowGrab(sdl.window, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
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
    std::array<Uint16, 256> ramp{};
    int i;

    if (sdl.flags & QVF_GAMMARAMP) {
        for (i = 0; i < 256; i++) {
            ramp[i] = table[i] << 8;
        }
        SDL_SetWindowGammaRamp(sdl.window, ramp.data(), ramp.data(), ramp.data());
    }
}

static int my_event_filter(void *userdata, SDL_Event *event)
{
    // SDL uses relative time, we need absolute
    event->common.timestamp = Sys_Milliseconds();
    return 1;
}

static char *get_mode_list(void)
{
    SDL_DisplayMode mode;
    size_t size, len;
    char *buf;
    int i, num_modes;

    num_modes = SDL_GetNumDisplayModes(0);
    if (num_modes < 1)
        return Z_CopyString(VID_MODELIST);

    size = 8 + num_modes * 32 + 1;
    buf = Z_Malloc(size);

    len = Q_strlcpy(buf, "desktop ", size);
    for (i = 0; i < num_modes; i++) {
        if (SDL_GetDisplayMode(0, i, &mode) < 0)
            break;
        if (mode.refresh_rate == 0)
            continue;
        len += Q_scnprintf(buf + len, size - len, "%dx%d@%d ",
                           mode.w, mode.h, mode.refresh_rate);
    }
    buf[len - 1] = 0;

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
    for (auto &c : controllers)
        close_controller(c);
    for (auto &j : joysticks)
        close_joystick(j);

    if (sdl.context)
        SDL_GL_DeleteContext(sdl.context);

    if (sdl.window)
        SDL_DestroyWindow(sdl.window);

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    controllers = {};
    joysticks = {};
    memset(&sdl, 0, sizeof(sdl));
}

static bool create_window_and_context(const vrect_t *rc)
{
    sdl.window = SDL_CreateWindow(PRODUCT, rc->x, rc->y, rc->width, rc->height,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!sdl.window) {
        Com_EPrintf("Couldn't create SDL window: %s\n", SDL_GetError());
        return false;
    }

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

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1) {
        Com_EPrintf("Couldn't initialize SDL video: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == -1) {
        Com_EPrintf("Couldn't initialize SDL controller support: %s\n", SDL_GetError());
    } else {
        SDL_GameControllerEventState(SDL_ENABLE);
        SDL_JoystickEventState(SDL_ENABLE);

        int num = SDL_NumJoysticks();
        for (int i = 0; i < num; i++) {
            if (SDL_IsGameController(i))
                controller_added(i);
            else
                joystick_added(i);
        }
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

    SDL_SetWindowMinimumSize(sdl.window, 320, 240);

    SDL_Surface *icon = SDL_CreateRGBSurfaceWithFormatFrom(q2icon_bits, q2icon_width, q2icon_height,
                                                           1, q2icon_width / 8, SDL_PIXELFORMAT_INDEX1LSB);
    if (icon) {
        std::array<SDL_Color, 2> colors{{
            { 255, 255, 255 },
            {   0, 128, 128 }
        }};
        SDL_SetPaletteColors(icon->format->palette, colors.data(), 0, 2);
        SDL_SetColorKey(icon, SDL_TRUE, 0);
        SDL_SetWindowIcon(sdl.window, icon);
        SDL_FreeSurface(icon);
    }

    cvar_t *vid_hwgamma = Cvar_Get("vid_hwgamma", "0", CVAR_REFRESH);
    if (vid_hwgamma->integer) {
        std::array<std::array<Uint16, 256>, 3> gamma{};

        if (SDL_GetWindowGammaRamp(sdl.window, gamma[0].data(), gamma[1].data(), gamma[2].data()) == 0 &&
            SDL_SetWindowGammaRamp(sdl.window, gamma[0].data(), gamma[1].data(), gamma[2].data()) == 0) {
            Com_Printf("...enabling hardware gamma\n");
            sdl.flags |= QVF_GAMMARAMP;
        } else {
            Com_Printf("...hardware gamma not supported\n");
            Cvar_Reset(vid_hwgamma);
        }
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
    Uint32 flags = SDL_GetWindowFlags(sdl.window);
    active_t active;
    vrect_t rc;

    // wayland doesn't set SDL_WINDOW_*_FOCUS flags
    if (sdl.wayland) {
        switch (event->event) {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            sdl.focus_hack = SDL_WINDOW_INPUT_FOCUS;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            sdl.focus_hack = 0;
            break;
        }
        flags |= sdl.focus_hack;
    }

    switch (event->event) {
    case SDL_WINDOWEVENT_MINIMIZED:
    case SDL_WINDOWEVENT_RESTORED:
    case SDL_WINDOWEVENT_ENTER:
    case SDL_WINDOWEVENT_LEAVE:
    case SDL_WINDOWEVENT_FOCUS_GAINED:
    case SDL_WINDOWEVENT_FOCUS_LOST:
    case SDL_WINDOWEVENT_SHOWN:
    case SDL_WINDOWEVENT_HIDDEN:
        if (flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) {
            active = ACT_ACTIVATED;
        } else if (flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) {
            active = ACT_MINIMIZED;
        } else {
            active = ACT_RESTORED;
        }
        CL_Activate(active);
        break;

    case SDL_WINDOWEVENT_MOVED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowSize(sdl.window, &rc.width, &rc.height);
            rc.x = event->data1;
            rc.y = event->data2;
            VID_SetGeometry(&rc);
        }
        break;

    case SDL_WINDOWEVENT_RESIZED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowPosition(sdl.window, &rc.x, &rc.y);
            rc.width = event->data1;
            rc.height = event->data2;
            VID_SetGeometry(&rc);
        }
        mode_changed();
        break;
    }
}

static const byte scantokey[] = {
    #include "keytables/sdl.hpp"
};

static const byte scantokey2[] = {
    K_LCTRL, K_LSHIFT, K_LALT, K_LWINKEY, K_RCTRL, K_RSHIFT, K_RALT, K_RWINKEY
};

static void key_event(SDL_KeyboardEvent *event)
{
    unsigned key = event->keysym.scancode;

    if (key < q_countof(scantokey))
        key = scantokey[key];
    else if (key >= SDL_SCANCODE_LCTRL && key < SDL_SCANCODE_LCTRL + q_countof(scantokey2))
        key = scantokey2[key - SDL_SCANCODE_LCTRL];
    else
        key = 0;

    if (!key) {
        Com_DPrintf("%s: unknown scancode %d\n", __func__, event->keysym.scancode);
        return;
    }

    Key_Event2(key, event->state, event->timestamp);
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

    Key_Event(key, event->state, event->timestamp);
}

static void mouse_wheel_event(SDL_MouseWheelEvent *event)
{
    if (event->x > 0) {
        Key_Event(K_MWHEELRIGHT, true, event->timestamp);
        Key_Event(K_MWHEELRIGHT, false, event->timestamp);
    } else if (event->x < 0) {
        Key_Event(K_MWHEELLEFT, true, event->timestamp);
        Key_Event(K_MWHEELLEFT, false, event->timestamp);
    }

    if (event->y > 0) {
        Key_Event(K_MWHEELUP, true, event->timestamp);
        Key_Event(K_MWHEELUP, false, event->timestamp);
    } else if (event->y < 0) {
        Key_Event(K_MWHEELDOWN, true, event->timestamp);
        Key_Event(K_MWHEELDOWN, false, event->timestamp);
    }
}

static void pump_events(void)
{
    SDL_Event    event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            Com_Quit(NULL, ERR_DISCONNECT);
            break;
        case SDL_WINDOWEVENT:
            window_event(&event.window);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            key_event(&event.key);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            controller_added(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            controller_removed(event.cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            controller_button_event(&event.cbutton);
            break;
        case SDL_CONTROLLERAXISMOTION:
            controller_axis_event(&event.caxis);
            break;
        case SDL_JOYDEVICEADDED:
            joystick_added(event.jdevice.which);
            break;
        case SDL_JOYDEVICEREMOVED:
            joystick_removed(event.jdevice.which);
            break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            joystick_button_event(&event.jbutton);
            break;
        case SDL_JOYAXISMOTION:
            joystick_axis_event(&event.jaxis);
            break;
        case SDL_JOYHATMOTION:
            joystick_hat_event(&event.jhat);
            break;
        case SDL_MOUSEMOTION:
            if (sdl.win_width && sdl.win_height)
                UI_MouseEvent(event.motion.x * sdl.width / sdl.win_width,
                              event.motion.y * sdl.height / sdl.win_height);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            mouse_button_event(&event.button);
            break;
        case SDL_MOUSEWHEEL:
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
    if (!SDL_GetRelativeMouseMode()) {
        return false;
    }
    SDL_GetRelativeMouseState(dx, dy);
    return true;
}

static void warp_mouse(int x, int y)
{
    SDL_WarpMouseInWindow(sdl.window, x, y);
    SDL_GetRelativeMouseState(NULL, NULL);
}

static void shutdown_mouse(void)
{
    SDL_SetWindowGrab(sdl.window, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
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
    SDL_SetWindowGrab(sdl.window, grab);
    SDL_SetRelativeMouseMode(grab && !(Key_GetDest() & KEY_MENU));
    SDL_GetRelativeMouseState(NULL, NULL);
    SDL_ShowCursor(!(sdl.flags & QVF_FULLSCREEN));
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
