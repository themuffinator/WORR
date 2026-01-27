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

#include "client/cgame_ui.h"
#include "common/common.h"
#include "common/error.h"
#include "common/files.h"

#undef Com_Printf
#undef Com_DPrintf
#undef Com_WPrintf
#undef Com_EPrintf

static const cgame_ui_import_t *uii;

extern "C" void UI_Sys_UpdateTimes(void);
extern "C" void UI_Sys_UpdateNetFrom(void);
extern "C" void UI_Sys_UpdateRefConfig(void);
extern "C" void UI_Sys_UpdateGameDir(void);
extern "C" void UI_Sys_SetMenuBlurRect(const clipRect_t *rect);

extern "C" {
    cvar_t *developer;
    char cmd_buffer_text[CMD_BUFFER_SIZE];
    cmdbuf_t cmd_buffer;
    cmdbuf_t *cmd_current;
    int cmd_optind;
    char *cmd_optarg;
    char *cmd_optopt;
    unsigned com_eventTime;
    unsigned com_localTime;
    unsigned com_localTime2;
    unsigned com_localTime3;
    netadr_t net_from;
    refcfg_t r_config;
    char fs_gamedir[MAX_OSPATH];
}

static char cmd_null_string[] = "";

void UI_SetClipboardData(const char *text)
{
    if (uii && uii->SetClipboardData)
        uii->SetClipboardData(text);
}

static const vid_driver_t ui_vid = []() {
    vid_driver_t driver = {};
    driver.set_clipboard_data = UI_SetClipboardData;
    return driver;
}();

extern "C" const vid_driver_t *vid = &ui_vid;

extern "C" void CG_UI_SetImport(const cgame_ui_import_t *import)
{
    uii = import;

    cmd_buffer.text = cmd_buffer_text;
    cmd_buffer.maxsize = sizeof(cmd_buffer_text);

    if (uii && uii->Cvar_WeakGet)
        developer = uii->Cvar_WeakGet("developer");
    if (!developer && uii && uii->Cvar_Get)
        developer = uii->Cvar_Get("developer", "0", 0);

    UI_Sys_UpdateRefConfig();
    UI_Sys_UpdateGameDir();
    UI_Sys_UpdateTimes();
    UI_Sys_UpdateNetFrom();
}

extern "C" void UI_Sys_UpdateTimes(void)
{
    if (!uii)
        return;

    if (uii->GetEventTime)
        com_eventTime = uii->GetEventTime();
    if (uii->GetLocalTime)
        com_localTime = uii->GetLocalTime();
}

extern "C" void UI_Sys_UpdateNetFrom(void)
{
    if (uii && uii->NET_GetLastAddress)
        uii->NET_GetLastAddress(&net_from);
}

extern "C" void UI_Sys_UpdateRefConfig(void)
{
    if (uii && uii->Re_GetConfig)
        uii->Re_GetConfig(&r_config);
}

extern "C" void UI_Sys_UpdateGameDir(void)
{
    if (uii && uii->FS_GetGameDir) {
        const char *gamedir = uii->FS_GetGameDir();
        if (gamedir)
            Q_strlcpy(fs_gamedir, gamedir, sizeof(fs_gamedir));
    }
}

extern "C" void UI_Sys_SetMenuBlurRect(const clipRect_t *rect)
{
    if (uii && uii->CL_SetMenuBlurRect)
        uii->CL_SetMenuBlurRect(rect);
}

static void UI_PrintFwd(void (*fn)(const char *fmt, ...), const char *fmt, va_list args)
{
    char buffer[MAX_STRING_CHARS];
    const char *format = fmt;

    if (!fn)
        return;

    if (uii && fmt && fmt[0] == '$' && uii->Localize)
        format = uii->Localize(fmt, NULL, 0);

    Q_vsnprintf(buffer, sizeof(buffer), format, args);
    fn("%s", buffer);
}

void Com_LPrintf(print_type_t type, const char *fmt, ...)
{
    void (*fn)(const char *fmt, ...) = NULL;
    va_list args;

    if (!uii)
        return;

    switch (type) {
    case PRINT_WARNING:
        fn = uii->Com_WPrintf;
        break;
    case PRINT_ERROR:
        fn = uii->Com_EPrintf;
        break;
    case PRINT_DEVELOPER:
        fn = uii->Com_DPrintf;
        break;
    default:
        fn = uii->Com_Printf;
        break;
    }

    va_start(args, fmt);
    UI_PrintFwd(fn, fmt, args);
    va_end(args);
}

void Com_Printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    UI_PrintFwd(uii ? uii->Com_Printf : NULL, fmt, args);
    va_end(args);
}

void Com_DPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    UI_PrintFwd(uii ? uii->Com_DPrintf : NULL, fmt, args);
    va_end(args);
}

void Com_WPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    UI_PrintFwd(uii ? uii->Com_WPrintf : NULL, fmt, args);
    va_end(args);
}

void Com_EPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    UI_PrintFwd(uii ? uii->Com_EPrintf : NULL, fmt, args);
    va_end(args);
}

void Com_Error(error_type_t code, const char *fmt, ...)
{
    va_list args;
    char buffer[MAX_STRING_CHARS];
    const char *format = fmt;

    va_start(args, fmt);
    if (uii && fmt && fmt[0] == '$' && uii->Localize)
        format = uii->Localize(fmt, NULL, 0);
    Q_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (uii && uii->Com_Error) {
        uii->Com_Error(code, "%s", buffer);
        abort();
    } else {
        Com_LPrintf(PRINT_ERROR, "%s\n", buffer);
        abort();
    }
}

size_t Com_FormatSize(char *dest, size_t destsize, int64_t bytes)
{
    return uii ? uii->Com_FormatSize(dest, destsize, bytes) : 0;
}

size_t Com_FormatSizeLong(char *dest, size_t destsize, int64_t bytes)
{
    return uii ? uii->Com_FormatSizeLong(dest, destsize, bytes) : 0;
}

const char *Q_ErrorString(int error)
{
    return uii && uii->Q_ErrorString ? uii->Q_ErrorString(error) : "";
}

unsigned Sys_Milliseconds(void)
{
    return uii ? uii->Sys_Milliseconds() : 0;
}

void Cbuf_AddText(cmdbuf_t *buf, const char *text)
{
    (void)buf;
    if (uii && uii->AddCommandString)
        uii->AddCommandString(text);
}

int Cmd_Argc(void)
{
    return uii ? uii->Cmd_Argc() : 0;
}

char *Cmd_Argv(int arg)
{
    return uii ? uii->Cmd_Argv(arg) : cmd_null_string;
}

size_t Cmd_ArgvBuffer(int arg, char *buf, size_t size)
{
    if (!uii || !uii->Cmd_ArgvBuffer)
        return 0;
    return uii->Cmd_ArgvBuffer(arg, buf, size);
}

char *Cmd_ArgsFrom(int arg)
{
    return uii ? uii->Cmd_ArgsFrom(arg) : cmd_null_string;
}

char *Cmd_RawArgsFrom(int arg)
{
    return uii ? uii->Cmd_RawArgsFrom(arg) : cmd_null_string;
}

void Cmd_TokenizeString(const char *text, bool macroExpand)
{
    cmd_optind = 1;
    cmd_optarg = cmd_optopt = cmd_null_string;
    if (uii && uii->Cmd_TokenizeString)
        uii->Cmd_TokenizeString(text, macroExpand);
}

void Cmd_Register(const cmdreg_t *reg)
{
    if (uii && uii->Cmd_Register)
        uii->Cmd_Register(reg);
}

void Cmd_Deregister(const cmdreg_t *reg)
{
    if (uii && uii->Cmd_Deregister)
        uii->Cmd_Deregister(reg);
}

void Cmd_AddCommand(const char *name, xcommand_t function)
{
    if (uii && uii->Cmd_AddCommand)
        uii->Cmd_AddCommand(name, function);
}

void Cmd_RemoveCommand(const char *name)
{
    if (uii && uii->Cmd_RemoveCommand)
        uii->Cmd_RemoveCommand(name);
}

cmd_macro_t *Cmd_FindMacro(const char *name)
{
    return (uii && uii->Cmd_FindMacro) ? uii->Cmd_FindMacro(name) : NULL;
}

int Cmd_ParseOptions(const cmd_option_t *opt)
{
    const cmd_option_t *o;
    char *s, *p;
    int argc = Cmd_Argc();

    cmd_optopt = cmd_null_string;

    if (cmd_optind == argc) {
        cmd_optarg = cmd_null_string;
        return -1;
    }

    s = Cmd_Argv(cmd_optind);
    if (*s != '-') {
        cmd_optarg = s;
        return -1;
    }
    cmd_optopt = s++;

    if (*s == '-') {
        s++;
        if (*s == 0) {
            if (++cmd_optind < argc)
                cmd_optarg = Cmd_Argv(cmd_optind);
            else
                cmd_optarg = cmd_null_string;
            return -1;
        }

        if ((p = strchr(s, '=')) != NULL) {
            *p = 0;
        }

        for (o = opt; o->sh; o++) {
            if (!strcmp(o->lo, s))
                break;
        }
        if (!o->sh)
            goto unknown;

        if (p) {
            if (o->sh[1] != ':') {
                Com_Printf("$ui_cmd_option_no_argument", Cmd_Argv(cmd_optind));
                Cmd_PrintHint();
                return '!';
            }
            cmd_optarg = p + 1;
        }
    } else {
        for (o = opt; o->sh; o++) {
            if (o->sh[0] == *s)
                break;
        }
        if (!o->sh || s[1])
            goto unknown;
        p = NULL;
    }

    if (!p && o->sh[1] == ':') {
        if (cmd_optind + 1 == argc) {
            Com_Printf("$ui_cmd_missing_argument", Cmd_Argv(cmd_optind));
            Cmd_PrintHint();
            return ':';
        }
        cmd_optarg = Cmd_Argv(++cmd_optind);
    }

    cmd_optind++;
    return o->sh[0];

unknown:
    Com_Printf("$ui_cmd_unknown_option", Cmd_Argv(cmd_optind));
    Cmd_PrintHint();
    return '?';
}

void Cmd_PrintUsage(const cmd_option_t *opt, const char *suffix)
{
    Com_Printf("$ui_cmd_usage_prefix", Cmd_Argv(0));
    while (opt->sh) {
        Com_Printf("%c", opt->sh[0]);
        if (opt->sh[1] == ':')
            Com_Printf(":");
        opt++;
    }
    if (suffix)
        Com_Printf("$ui_cmd_usage_suffix", suffix);
    else
        Com_Printf("]\n");
}

void Cmd_PrintHelp(const cmd_option_t *opt)
{
    const cmd_option_t *o;
    char buffer[32];
    int width = 0;

    for (o = opt; o->sh; o++) {
        int len = strlen(o->lo);
        if (o->sh[1] == ':')
            len += 3 + strlen(o->sh + 2);
        width = max(width, min(len, 31));
    }

    Com_Printf("$ui_cmd_options_header");
    while (opt->sh) {
        if (opt->sh[1] == ':')
            Q_concat(buffer, sizeof(buffer), opt->lo, "=<", opt->sh + 2, ">");
        else
            Q_strlcpy(buffer, opt->lo, sizeof(buffer));
        Com_Printf("-%c | --%*s | %s\n", opt->sh[0], -width, buffer, opt->help);
        opt++;
    }
    Com_Printf("\n");
}

void Cmd_PrintHint(void)
{
    Com_Printf("$ui_cmd_help_hint", Cmd_Argv(0));
}

void Cmd_Option_c(const cmd_option_t *opt, xgenerator_t g, genctx_t *ctx, int argnum)
{
    (void)opt;
    (void)argnum;
    if (g && ctx->partial[0] != '-')
        g(ctx);
}

cvar_t *Cvar_Get(const char *name, const char *value, int flags)
{
    return uii ? uii->Cvar_Get(name, value, flags) : NULL;
}

cvar_t *Cvar_WeakGet(const char *name)
{
    return uii ? uii->Cvar_WeakGet(name) : NULL;
}

cvar_t *Cvar_FindVar(const char *name)
{
    return uii ? uii->Cvar_FindVar(name) : NULL;
}

cvar_t *Cvar_Set(const char *name, const char *value)
{
    return uii ? uii->Cvar_Set(name, value) : NULL;
}

cvar_t *Cvar_SetEx(const char *name, const char *value, from_t from)
{
    return uii ? uii->Cvar_SetEx(name, value, from) : NULL;
}

void Cvar_SetByVar(cvar_t *var, const char *value, from_t from)
{
    if (uii && uii->Cvar_SetByVar)
        uii->Cvar_SetByVar(var, value, from);
}

void Cvar_SetInteger(cvar_t *var, int value, from_t from)
{
    if (uii && uii->Cvar_SetInteger)
        uii->Cvar_SetInteger(var, value, from);
}

void Cvar_SetValue(cvar_t *var, float value, from_t from)
{
    if (uii && uii->Cvar_SetValue)
        uii->Cvar_SetValue(var, value, from);
}

cvar_t *Cvar_UserSet(const char *name, const char *value)
{
    return uii ? uii->Cvar_UserSet(name, value) : NULL;
}

int Cvar_ClampInteger(cvar_t *var, int min, int max)
{
    return uii ? uii->Cvar_ClampInteger(var, min, max) : min;
}

float Cvar_ClampValue(cvar_t *var, float min, float max)
{
    return uii ? uii->Cvar_ClampValue(var, min, max) : min;
}

const char *Cvar_VariableString(const char *name)
{
    return uii ? uii->Cvar_VariableString(name) : "";
}

int Cvar_VariableInteger(const char *name)
{
    return uii ? uii->Cvar_VariableInteger(name) : 0;
}

void *Z_TagMalloc(size_t size, memtag_t tag)
{
    return uii ? uii->Z_TagMalloc(size, tag) : NULL;
}

void *Z_TagMallocz(size_t size, memtag_t tag)
{
    return uii ? uii->Z_TagMallocz(size, tag) : NULL;
}

char *Z_TagCopyString(const char *in, memtag_t tag)
{
    return uii ? uii->Z_TagCopyString(in, tag) : NULL;
}

void Z_Free(void *ptr)
{
    if (uii && uii->Z_Free)
        uii->Z_Free(ptr);
}

void Z_Freep(void *ptr)
{
    if (uii && uii->Z_Freep)
        uii->Z_Freep(ptr);
}

void *Z_Realloc(void *ptr, size_t size)
{
    return uii ? uii->Z_Realloc(ptr, size) : NULL;
}

void *Z_ReallocArray(void *ptr, size_t nmemb, size_t size, memtag_t tag)
{
    return uii ? uii->Z_ReallocArray(ptr, nmemb, size, tag) : NULL;
}

void Z_FreeTags(memtag_t tag)
{
    if (uii && uii->Z_FreeTags)
        uii->Z_FreeTags(tag);
}

void Z_LeakTest(memtag_t tag)
{
    if (uii && uii->Z_LeakTest)
        uii->Z_LeakTest(tag);
}

void *Z_Malloc(size_t size)
{
    return uii ? uii->Z_TagMalloc(size, TAG_GENERAL) : NULL;
}

int FS_LoadFileEx(const char *path, void **buffer, unsigned flags, memtag_t tag)
{
    return uii ? uii->FS_LoadFileEx(path, buffer, flags, tag) : Q_ERR(ENOSYS);
}

void **FS_ListFiles(const char *path, const char *filter, unsigned flags, int *count_p)
{
    return uii ? uii->FS_ListFiles(path, filter, flags, count_p) : NULL;
}

void FS_FreeList(void **list)
{
    if (uii && uii->FS_FreeList)
        uii->FS_FreeList(list);
}

int64_t FS_OpenFile(const char *filename, qhandle_t *f, unsigned mode)
{
    return uii ? uii->FS_OpenFile(filename, f, mode) : Q_ERR(ENOSYS);
}

int FS_CloseFile(qhandle_t f)
{
    return uii ? uii->FS_CloseFile(f) : Q_ERR(ENOSYS);
}

int FS_FPrintf(qhandle_t f, const char *format, ...)
{
    va_list args;
    char buffer[MAX_STRING_CHARS];
    int ret = Q_ERR(ENOSYS);

    if (!uii || !uii->FS_FPrintf)
        return ret;

    va_start(args, format);
    Q_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ret = uii->FS_FPrintf(f, "%s", buffer);
    return ret;
}

char *SV_GetSaveInfo(const char *dir)
{
    return (uii && uii->SV_GetSaveInfo) ? uii->SV_GetSaveInfo(dir) : NULL;
}

const char *NET_AdrToString(const netadr_t *a)
{
    return uii ? uii->NET_AdrToString(a) : "";
}

bool NET_StringToAdr(const char *s, netadr_t *a, int default_port)
{
    return uii ? uii->NET_StringToAdr(s, a, default_port) : false;
}

keydest_t Key_GetDest(void)
{
    return uii ? uii->Key_GetDest() : KEY_GAME;
}

void Key_SetDest(keydest_t dest)
{
    if (uii && uii->Key_SetDest)
        uii->Key_SetDest(dest);
}

int Key_IsDown(int key)
{
    return uii ? uii->Key_IsDown(key) : 0;
}

int Key_EnumBindings(int key, const char *binding)
{
    return uii ? uii->Key_EnumBindings(key, binding) : -1;
}

const char *Key_KeynumToString(int key)
{
    return uii ? uii->Key_KeynumToString(key) : "";
}

void Key_WaitKey(keywaitcb_t wait, void *arg)
{
    if (uii && uii->Key_WaitKey)
        uii->Key_WaitKey(wait, arg);
}

void Key_SetBinding(int key, const char *binding)
{
    if (uii && uii->Key_SetBinding)
        uii->Key_SetBinding(key, binding);
}

bool Key_GetOverstrikeMode(void)
{
    return uii ? uii->Key_GetOverstrikeMode() : false;
}

void Key_SetOverstrikeMode(bool overstrike)
{
    if (uii && uii->Key_SetOverstrikeMode)
        uii->Key_SetOverstrikeMode(overstrike);
}

extern "C" void IN_WarpMouse(int x, int y)
{
    if (uii && uii->IN_WarpMouse)
        uii->IN_WarpMouse(x, y);
}

void SCR_UpdateScreen(void)
{
    if (uii && uii->SCR_UpdateScreen)
        uii->SCR_UpdateScreen();
}

bool SCR_ParseColor(const char *s, color_t *color)
{
    return uii ? uii->SCR_ParseColor(s, color) : false;
}

float V_CalcFov(float fov_x, float width, float height)
{
    return uii ? uii->V_CalcFov(fov_x, width, height) : fov_x;
}

bool CL_GetDemoInfo(const char *path, demoInfo_t *info)
{
    return uii ? uii->CL_GetDemoInfo(path, info) : false;
}

void CL_SendStatusRequest(const netadr_t *address)
{
    if (uii && uii->CL_SendStatusRequest)
        uii->CL_SendStatusRequest(address);
}

int HTTP_FetchFile(const char *url, void **data)
{
    return (uii && uii->HTTP_FetchFile) ? uii->HTTP_FetchFile(url, data) : Q_ERR(ENOSYS);
}

void S_StartLocalSound(const char *sound)
{
    if (uii && uii->S_StartLocalSound)
        uii->S_StartLocalSound(sound);
}

void S_StopAllSounds(void)
{
    if (uii && uii->S_StopAllSounds)
        uii->S_StopAllSounds();
}

extern "C" qhandle_t R_RegisterModel(const char *name)
{
    return uii ? uii->Re_RegisterModel(name) : 0;
}

extern "C" qhandle_t R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    return uii ? uii->Re_RegisterImage(name, type, flags) : 0;
}

extern "C" bool R_GetPicSize(int *w, int *h, qhandle_t pic)
{
    if (uii && uii->Re_GetPicSize)
        return uii->Re_GetPicSize(w, h, pic);
    if (w && h) {
        *w = 0;
        *h = 0;
    }
    return false;
}

extern "C" void R_RenderFrame(const refdef_t *fd)
{
    if (uii && uii->Re_RenderFrame)
        uii->Re_RenderFrame(fd);
}

extern "C" void R_SetClipRect(const clipRect_t *clip)
{
    if (uii && uii->Re_SetClipRect)
        uii->Re_SetClipRect(clip);
}

extern "C" void R_SetScale(float scale)
{
    if (uii && uii->Re_SetScale)
        uii->Re_SetScale(scale);
}

extern "C" float R_ClampScale(cvar_t *var)
{
    return uii ? uii->Re_ClampScale(var) : 1.0f;
}

extern "C" void R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font)
{
    if (uii && uii->Re_DrawChar)
        uii->Re_DrawChar(x, y, flags, ch, color, font);
}

extern "C" int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                        const char *string, color_t color, qhandle_t font)
{
    if (!uii || !uii->Re_DrawStringStretch)
        return 0;
    return uii->Re_DrawStringStretch(x, y, scale, flags, maxChars, string, color, font);
}

extern "C" int UI_FontDrawString(int x, int y, int flags, size_t maxChars,
                                 const char *string, color_t color)
{
    if (!uii || !uii->UI_FontDrawString)
        return 0;
    return uii->UI_FontDrawString(x, y, flags, maxChars, string, color);
}

extern "C" int UI_FontMeasureString(int flags, size_t maxChars, const char *string,
                                    int *out_height)
{
    if (!uii || !uii->UI_FontMeasureString)
        return 0;
    return uii->UI_FontMeasureString(flags, maxChars, string, out_height);
}

extern "C" int UI_FontLineHeight(int scale)
{
    if (!uii || !uii->UI_FontLineHeight)
        return CONCHAR_HEIGHT * max(scale, 1);
    return uii->UI_FontLineHeight(scale);
}

extern "C" int UI_FontDrawStringSized(int x, int y, int flags, size_t maxChars,
                                      const char *string, color_t color, int size)
{
    if (uii && uii->UI_FontDrawStringSized)
        return uii->UI_FontDrawStringSized(x, y, flags, maxChars, string, color, size);
    if (uii && uii->UI_FontDrawString)
        return uii->UI_FontDrawString(x, y, flags, maxChars, string, color);
    return 0;
}

extern "C" int UI_FontMeasureStringSized(int flags, size_t maxChars, const char *string,
                                         int *out_height, int size)
{
    if (uii && uii->UI_FontMeasureStringSized)
        return uii->UI_FontMeasureStringSized(flags, maxChars, string, out_height, size);
    if (uii && uii->UI_FontMeasureString)
        return uii->UI_FontMeasureString(flags, maxChars, string, out_height);
    if (out_height)
        *out_height = 0;
    return 0;
}

extern "C" int UI_FontLineHeightSized(int size)
{
    if (uii && uii->UI_FontLineHeightSized)
        return uii->UI_FontLineHeightSized(size);
    if (uii && uii->UI_FontLineHeight)
        return uii->UI_FontLineHeight(1);
    return CONCHAR_HEIGHT;
}

extern "C" qhandle_t UI_FontLegacyHandle(void)
{
    if (!uii || !uii->UI_FontLegacyHandle)
        return 0;
    return uii->UI_FontLegacyHandle();
}

extern "C" void R_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    if (uii && uii->Re_DrawPic)
        uii->Re_DrawPic(x, y, color, pic);
}

extern "C" void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    if (uii && uii->Re_DrawKeepAspectPic)
        uii->Re_DrawKeepAspectPic(x, y, w, h, color, pic);
}

extern "C" void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    if (uii && uii->Re_DrawKeepAspectPic) {
        uii->Re_DrawKeepAspectPic(x, y, w, h, color, pic);
    } else if (uii && uii->Re_DrawPic) {
        uii->Re_DrawPic(x, y, color, pic);
    }
}

extern "C" void R_DrawFill8(int x, int y, int w, int h, int c)
{
    if (uii && uii->Re_DrawFill8)
        uii->Re_DrawFill8(x, y, w, h, c);
}

extern "C" void R_DrawFill32(int x, int y, int w, int h, color_t color)
{
    if (uii && uii->Re_DrawFill32)
        uii->Re_DrawFill32(x, y, w, h, color);
}

void Con_Close(bool force)
{
    if (uii && uii->Con_Close)
        uii->Con_Close(force);
}

