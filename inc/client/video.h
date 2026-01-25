/*
Copyright (C) 2003-2006 Andrey Nazarov

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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    VID_NATIVE_NONE = 0,
    VID_NATIVE_WIN32,
    VID_NATIVE_X11,
    VID_NATIVE_WAYLAND,
    VID_NATIVE_SDL,
} vid_native_platform_t;

typedef struct vid_native_window_s {
    vid_native_platform_t platform;
    union {
        struct {
            void *hinstance;
            void *hwnd;
            void *hdc;
        } win32;
        struct {
            void *display;
            uintptr_t window;
        } x11;
        struct {
            void *display;
            void *surface;
        } wayland;
        struct {
            void *window;
        } sdl;
    } handle;
} vid_native_window_t;

typedef struct {
    const char *name;

    bool (*probe)(void);
    bool (*init)(void);
    void (*shutdown)(void);
    void (*fatal_shutdown)(void);
    void (*pump_events)(void);

    char *(*get_mode_list)(void);
    int (*get_dpi_scale)(void);
    void (*set_mode)(void);
    void (*update_gamma)(const byte *table);

    void *(*get_proc_addr)(const char *sym);
    void (*swap_buffers)(void);
    void (*swap_interval)(int val);

    char *(*get_selection_data)(void);
    char *(*get_clipboard_data)(void);
    void (*set_clipboard_data)(const char *data);

    bool (*init_mouse)(void);
    void (*shutdown_mouse)(void);
    void (*grab_mouse)(bool grab);
    void (*warp_mouse)(int x, int y);
    bool (*get_mouse_motion)(int *dx, int *dy);
    bool (*get_native_window)(vid_native_window_t *out);
} vid_driver_t;

extern cvar_t       *r_display;
extern cvar_t       *r_geometry;
extern cvar_t       *r_modelist;
extern cvar_t       *r_fullscreen;
extern cvar_t       *_r_fullscreen;
extern cvar_t       *r_fullscreen_exclusive;

extern const vid_driver_t   *vid;

bool VID_GetFullscreen(vrect_t *rc, int *freq_p, int *depth_p);
bool VID_GetGeometry(vrect_t *rc);
void VID_SetGeometry(const vrect_t *rc);
void VID_SetModeList(const char *modelist);
void VID_ToggleFullscreen(void);

#ifdef __cplusplus
}
#endif
