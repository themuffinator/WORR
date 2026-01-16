/*
Copyright (C) 2026

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

#include "client/cgame_ui_ext.h"
#include "shared/shared.h"
#include "common/cmd.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/net/net.h"
#include "common/zone.h"
#include "client/client.h"
#include "client/ui.h"
#include "client/keys.h"
#include "client/sound/sound.h"
#include "client/video.h"
#include "renderer/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cgame_ui_import_s {
    int api_version;

    void (*Com_Printf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_DPrintf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_WPrintf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_EPrintf)(const char *fmt, ...) q_printf(1, 2);
    void (*Com_Error)(error_type_t code, const char *fmt, ...) q_printf(2, 3);
    size_t (*Com_FormatSize)(char *dest, size_t destsize, int64_t bytes);
    size_t (*Com_FormatSizeLong)(char *dest, size_t destsize, int64_t bytes);
    const char *(*Q_ErrorString)(int error);

    unsigned (*Sys_Milliseconds)(void);

    void (*AddCommandString)(const char *text);
    int (*Cmd_Argc)(void);
    char *(*Cmd_Argv)(int arg);
    size_t (*Cmd_ArgvBuffer)(int arg, char *buf, size_t size);
    char *(*Cmd_ArgsFrom)(int arg);
    char *(*Cmd_RawArgsFrom)(int arg);
    void (*Cmd_TokenizeString)(const char *text, bool macroExpand);
    void (*Cmd_Register)(const cmdreg_t *reg);
    void (*Cmd_Deregister)(const cmdreg_t *reg);
    void (*Cmd_AddCommand)(const char *name, xcommand_t function);
    void (*Cmd_RemoveCommand)(const char *name);
    cmd_macro_t *(*Cmd_FindMacro)(const char *name);

    cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags);
    cvar_t *(*Cvar_WeakGet)(const char *name);
    cvar_t *(*Cvar_FindVar)(const char *name);
    cvar_t *(*Cvar_Set)(const char *name, const char *value);
    cvar_t *(*Cvar_SetEx)(const char *name, const char *value, from_t from);
    void (*Cvar_SetByVar)(cvar_t *var, const char *value, from_t from);
    void (*Cvar_SetInteger)(cvar_t *var, int value, from_t from);
    void (*Cvar_SetValue)(cvar_t *var, float value, from_t from);
    cvar_t *(*Cvar_UserSet)(const char *name, const char *value);
    int (*Cvar_ClampInteger)(cvar_t *var, int min, int max);
    float (*Cvar_ClampValue)(cvar_t *var, float min, float max);
    const char *(*Cvar_VariableString)(const char *name);
    int (*Cvar_VariableInteger)(const char *name);

    void *(*Z_TagMalloc)(size_t size, memtag_t tag);
    void *(*Z_TagMallocz)(size_t size, memtag_t tag);
    char *(*Z_TagCopyString)(const char *in, memtag_t tag);
    void (*Z_Free)(void *ptr);
    void (*Z_Freep)(void *ptr);
    void *(*Z_Realloc)(void *ptr, size_t size);
    void *(*Z_ReallocArray)(void *ptr, size_t nmemb, size_t size, memtag_t tag);
    void (*Z_FreeTags)(memtag_t tag);
    void (*Z_LeakTest)(memtag_t tag);

    int (*FS_LoadFileEx)(const char *path, void **buffer, unsigned flags, memtag_t tag);
    void **(*FS_ListFiles)(const char *path, const char *filter, unsigned flags, int *count_p);
    void (*FS_FreeList)(void **list);
    int64_t (*FS_OpenFile)(const char *filename, qhandle_t *f, unsigned mode);
    int (*FS_CloseFile)(qhandle_t f);
    int (*FS_FPrintf)(qhandle_t f, const char *format, ...) q_printf(2, 3);
    const char *(*FS_GetGameDir)(void);
    char *(*SV_GetSaveInfo)(const char *dir);

    bool (*NET_IsEqualBaseAdr)(const netadr_t *a, const netadr_t *b);
    const char *(*NET_AdrToString)(const netadr_t *a);
    bool (*NET_StringToAdr)(const char *s, netadr_t *a, int default_port);
    void (*NET_GetLastAddress)(netadr_t *out);

    keydest_t (*Key_GetDest)(void);
    void (*Key_SetDest)(keydest_t dest);
    int (*Key_IsDown)(int key);
    int (*Key_EnumBindings)(int key, const char *binding);
    const char *(*Key_KeynumToString)(int key);
    void (*Key_WaitKey)(keywaitcb_t wait, void *arg);
    void (*Key_SetBinding)(int key, const char *binding);
    bool (*Key_GetOverstrikeMode)(void);
    void (*Key_SetOverstrikeMode)(bool overstrike);

    void (*IN_WarpMouse)(int x, int y);

    void (*SCR_UpdateScreen)(void);
    bool (*SCR_ParseColor)(const char *s, color_t *color);
    float (*V_CalcFov)(float fov_x, float width, float height);

    bool (*CL_GetDemoInfo)(const char *path, demoInfo_t *info);
    void (*CL_SendStatusRequest)(const netadr_t *address);
    int (*HTTP_FetchFile)(const char *url, void **data);

    void (*S_StartLocalSound)(const char *sound);
    void (*S_StopAllSounds)(void);

    qhandle_t (*Re_RegisterModel)(const char *name);
    qhandle_t (*Re_RegisterImage)(const char *name, imagetype_t type, imageflags_t flags);
    bool (*Re_GetPicSize)(int *w, int *h, qhandle_t pic);
    void (*Re_RenderFrame)(const refdef_t *fd);
    void (*Re_SetClipRect)(const clipRect_t *clip);
    void (*Re_SetScale)(float scale);
    float (*Re_ClampScale)(cvar_t *var);
    void (*Re_DrawChar)(int x, int y, int flags, int ch, color_t color, qhandle_t font);
    int (*Re_DrawStringStretch)(int x, int y, int scale, int flags, size_t maxChars,
                               const char *string, color_t color, qhandle_t font);
    void (*Re_DrawPic)(int x, int y, color_t color, qhandle_t pic);
    void (*Re_DrawKeepAspectPic)(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void (*Re_DrawFill8)(int x, int y, int w, int h, int c);
    void (*Re_DrawFill32)(int x, int y, int w, int h, color_t color);
    void (*Re_GetConfig)(refcfg_t *out);

    void (*SetClipboardData)(const char *text);
    unsigned (*GetEventTime)(void);
    unsigned (*GetLocalTime)(void);
    void (*Con_Close)(bool force);
} cgame_ui_import_t;

typedef struct cgame_ui_export_s {
    int api_version;

    void (*Init)(void);
    void (*Shutdown)(void);
    void (*ModeChanged)(void);
    void (*KeyEvent)(int key, bool down);
    void (*CharEvent)(int key);
    void (*Draw)(unsigned realtime);
    void (*OpenMenu)(uiMenu_t menu);
    void (*Frame)(int msec);
    void (*StatusEvent)(const serverStatus_t *status);
    void (*ErrorEvent)(const netadr_t *from);
    void (*MouseEvent)(int x, int y);
    bool (*IsTransparent)(void);
} cgame_ui_export_t;

const cgame_ui_export_t *CG_UI_GetExport(void);

#ifdef __cplusplus
}
#endif
