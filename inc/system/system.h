/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "common/utils.h"

typedef struct {
    const char  *filter;
    unsigned    flags;
    unsigned    baselen;
    void        **files;
    int         count;
} listfiles_t;

// loads the dll and returns entry pointer
void    *Sys_LoadLibrary(const char *path, const char *sym, void **handle);
void    Sys_FreeLibrary(void *handle);
void    *Sys_GetProcAddress(void *handle, const char *sym);

unsigned    Sys_Milliseconds(void);
void        Sys_Sleep(int msec);

void    Sys_Init(void);
void    Sys_AddDefaultConfig(void);

const char  *Sys_ErrorString(int err);

#if USE_SYSCON
void    Sys_RunConsole(void);
void    Sys_ConsoleOutput(const char *text, size_t len);
void    Sys_SetConsoleTitle(const char *title);
void    Sys_SetConsoleColor(color_index_t color);
void    Sys_LoadHistory(void);
void    Sys_SaveHistory(void);
#else
#define Sys_RunConsole()                (void)0
#define Sys_ConsoleOutput(text, len)    (void)0
#define Sys_SetConsoleTitle(title)      (void)0
#define Sys_SetConsoleColor(color)      (void)0
#define Sys_LoadHistory()               (void)0
#define Sys_SaveHistory()               (void)0
#endif

#if USE_CLIENT
typedef enum {
    SYS_TASKBAR_PROGRESS_NONE,
    SYS_TASKBAR_PROGRESS_INDETERMINATE,
    SYS_TASKBAR_PROGRESS_NORMAL
} sys_taskbar_progress_t;

void    Sys_SetTaskbarProgress(sys_taskbar_progress_t state, int percent);
#endif

q_noreturn q_printf(1, 2)
void    Sys_Error(const char *error, ...);

q_noreturn
void    Sys_Quit(void);

void    Sys_ListFiles_r(listfiles_t *list, const char *path, int depth);

typedef enum {
    RERELEASE_MODE_NO = 0, // use vanilla game
    RERELEASE_MODE_YES = 1, // use re-release game
    RERELEASE_MODE_NEVER = -1 // do not attempt any sort of auto-detection
} rerelease_mode_t;

typedef bool (*sys_getinstalledgamepath_func_t)(rerelease_mode_t rr_mode, char *path, size_t path_length);

extern const sys_getinstalledgamepath_func_t gamepath_funcs[];

void    Sys_DebugBreak(void);
bool    Sys_IsMainThread(void);

#if USE_AC_CLIENT
bool    Sys_GetAntiCheatAPI(void);
#endif

#ifndef _WIN32
bool    Sys_SetNonBlock(int fd, bool nb);
#endif

#if USE_MEMORY_TRACES
void Sys_BackTrace(void **output, size_t count, size_t offset);
#endif

extern cvar_t   *sys_basedir;
extern cvar_t   *sys_libdir;
extern cvar_t   *sys_homedir;

#ifdef _WIN32
bool Sys_GetRereleaseHomeDir(char *path, size_t path_length);
#endif

#ifdef __cplusplus
}
#endif

