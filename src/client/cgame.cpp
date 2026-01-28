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

#include "client.h"
#include "cgame_classic.h"
#include "client/cgame_entity.h"
#include "client/cgame_ui.h"
#include "client/font.h"
#include "client/ui_font.h"
#include "common/mdfour.h"
#include "common/loc.h"
#include "common/gamedll.h"

static cvar_t   *cl_alpha;

static void CG_AddCommandString(const char *string);
static const char* CG_Localize(const char *base, const char **args, size_t num_args);

static bool CGX_IsExtendedServer(void)
{
    return cl.csr.extended;
}

static int CGX_GetMaxStats(void)
{
    return cl.max_stats;
}

static color_t apply_scr_alpha(color_t color)
{
    color.a *= Cvar_ClampValue(cl_alpha, 0, 1);
    return color;
}

static void CGX_DrawCharEx(int x, int y, int flags, int ch, color_t color)
{
    R_DrawStretchChar(x, y,
                      CONCHAR_WIDTH,
                      CONCHAR_HEIGHT,
                      flags, ch, apply_scr_alpha(color), scr.ui_font_pic);
}

static const pmoveParams_t* CGX_GetPmoveParams(void)
{
    return &cl.pmp;
}

static cgame_q2pro_extended_support_ext_t cgame_q2pro_extended_support = {
    .api_version = 3,

    .IsExtendedServer = CGX_IsExtendedServer,
    .GetMaxStats = CGX_GetMaxStats,
    .DrawCharEx = CGX_DrawCharEx,
    .GetPmoveParams = CGX_GetPmoveParams,
};

static void CG_UI_GetNetFrom(netadr_t *out)
{
    if (out)
        *out = net_from;
}

static const char *CG_UI_GetGameDir(void)
{
    return fs_gamedir;
}

static void CG_UI_GetConfig(refcfg_t *out)
{
    if (out)
        *out = r_config;
}

static void CG_UI_SetClipboardData(const char *text)
{
    if (vid && vid->set_clipboard_data)
        vid->set_clipboard_data(text);
}

static unsigned CG_UI_GetEventTime(void)
{
    return com_eventTime;
}

static unsigned CG_UI_GetLocalTime(void)
{
    return com_localTime;
}

static void CG_UI_SendStatusRequest(const netadr_t *address)
{
#if USE_UI
    CL_SendStatusRequest(address);
#else
    (void)address;
#endif
}

static void CG_UI_Com_Print(print_type_t type, const char *fmt, va_list args)
{
    char buffer[MAX_STRING_CHARS];

    Q_vsnprintf(buffer, sizeof(buffer), fmt, args);
    Com_LPrintf(type, "%s", buffer);
}

static void CG_UI_Com_Printf(const char *fmt, ...) q_printf(1, 2);
static void CG_UI_Com_Printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CG_UI_Com_Print(PRINT_ALL, fmt, args);
    va_end(args);
}

static void CG_UI_Com_DPrintf(const char *fmt, ...) q_printf(1, 2);
static void CG_UI_Com_DPrintf(const char *fmt, ...)
{
    va_list args;

    if (!developer || developer->integer < 1)
        return;

    va_start(args, fmt);
    CG_UI_Com_Print(PRINT_DEVELOPER, fmt, args);
    va_end(args);
}

static void CG_UI_Com_WPrintf(const char *fmt, ...) q_printf(1, 2);
static void CG_UI_Com_WPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CG_UI_Com_Print(PRINT_WARNING, fmt, args);
    va_end(args);
}

static void CG_UI_Com_EPrintf(const char *fmt, ...) q_printf(1, 2);
static void CG_UI_Com_EPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CG_UI_Com_Print(PRINT_ERROR, fmt, args);
    va_end(args);
}

static qhandle_t CG_UI_RegisterModel(const char *name)
{
    return R_RegisterModel(name);
}

static qhandle_t CG_UI_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    return R_RegisterImage(name, type, flags);
}

static bool CG_UI_GetPicSize(int *w, int *h, qhandle_t pic)
{
    return R_GetPicSize(w, h, pic);
}

static void CG_UI_RenderFrame(const refdef_t *fd)
{
    R_RenderFrame(fd);
}

static void CG_UI_SetClipRect(const clipRect_t *clip)
{
    R_SetClipRect(clip);
}

static void CG_UI_SetScale(float scale)
{
    R_SetScale(scale);
}

static float CG_UI_ClampScale(cvar_t *var)
{
    return R_ClampScale(var);
}

static void CG_UI_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font)
{
    R_DrawChar(x, y, flags, ch, color, font);
}

static int CG_UI_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                                   const char *string, color_t color, qhandle_t font)
{
    return R_DrawStringStretch(x, y, scale, flags, maxChars, string, color, font);
}

static int CG_UI_FontDrawString(int x, int y, int flags, size_t maxChars,
                                const char *string, color_t color)
{
    return UI_FontDrawString(x, y, flags, maxChars, string, color);
}

static int CG_UI_FontMeasureString(int flags, size_t maxChars, const char *string,
                                   int *out_height)
{
    return UI_FontMeasureString(flags, maxChars, string, out_height);
}

static int CG_UI_FontLineHeight(int scale)
{
    return UI_FontLineHeight(scale);
}

static int CG_UI_FontDrawStringSized(int x, int y, int flags, size_t maxChars,
                                     const char *string, color_t color, int size)
{
    return UI_FontDrawStringSized(x, y, flags, maxChars, string, color, size);
}

static int CG_UI_FontMeasureStringSized(int flags, size_t maxChars, const char *string,
                                        int *out_height, int size)
{
    return UI_FontMeasureStringSized(flags, maxChars, string, out_height, size);
}

static int CG_UI_FontLineHeightSized(int size)
{
    return UI_FontLineHeightSized(size);
}

static qhandle_t CG_UI_FontLegacyHandle(void)
{
    return UI_FontLegacyHandle();
}

static void CG_UI_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    R_DrawPic(x, y, color, pic);
}

static void CG_UI_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    R_DrawKeepAspectPic(x, y, w, h, color, pic);
}

static void CG_UI_DrawFill8(int x, int y, int w, int h, int c)
{
    R_DrawFill8(x, y, w, h, c);
}

static void CG_UI_DrawFill32(int x, int y, int w, int h, color_t color)
{
    R_DrawFill32(x, y, w, h, color);
}

static void CG_UI_SetMenuBlurRect(const clipRect_t *rect)
{
    if (rect) {
        cl.menu_blur_rect = *rect;
        cl.menu_blur_active = true;
    } else {
        cl.menu_blur_active = false;
        cl.menu_blur_rect = {};
    }
}

static cgame_ui_import_t cg_ui_import = {
    .api_version = CGAME_UI_API_VERSION,

    .Com_Printf = CG_UI_Com_Printf,
    .Com_DPrintf = CG_UI_Com_DPrintf,
    .Com_WPrintf = CG_UI_Com_WPrintf,
    .Com_EPrintf = CG_UI_Com_EPrintf,
    .Com_Error = Com_Error,
    .Com_FormatSize = Com_FormatSize,
    .Com_FormatSizeLong = Com_FormatSizeLong,
    .Q_ErrorString = Q_ErrorString,
    .Localize = CG_Localize,

    .Sys_Milliseconds = Sys_Milliseconds,

    .AddCommandString = CG_AddCommandString,
    .Cmd_Argc = Cmd_Argc,
    .Cmd_Argv = Cmd_Argv,
    .Cmd_ArgvBuffer = Cmd_ArgvBuffer,
    .Cmd_ArgsFrom = Cmd_ArgsFrom,
    .Cmd_RawArgsFrom = Cmd_RawArgsFrom,
    .Cmd_TokenizeString = Cmd_TokenizeString,
    .Cmd_Register = Cmd_Register,
    .Cmd_Deregister = Cmd_Deregister,
    .Cmd_AddCommand = Cmd_AddCommand,
    .Cmd_RemoveCommand = Cmd_RemoveCommand,
    .Cmd_FindMacro = Cmd_FindMacro,

    .Cvar_Get = Cvar_Get,
    .Cvar_WeakGet = Cvar_WeakGet,
    .Cvar_FindVar = Cvar_FindVar,
    .Cvar_Set = Cvar_Set,
    .Cvar_SetEx = Cvar_SetEx,
    .Cvar_SetByVar = Cvar_SetByVar,
    .Cvar_SetInteger = Cvar_SetInteger,
    .Cvar_SetValue = Cvar_SetValue,
    .Cvar_UserSet = Cvar_UserSet,
    .Cvar_ClampInteger = Cvar_ClampInteger,
    .Cvar_ClampValue = Cvar_ClampValue,
    .Cvar_VariableString = Cvar_VariableString,
    .Cvar_VariableInteger = Cvar_VariableInteger,

    .Z_TagMalloc = Z_TagMalloc,
    .Z_TagMallocz = Z_TagMallocz,
    .Z_TagCopyString = Z_TagCopyString,
    .Z_Free = Z_Free,
    .Z_Freep = Z_Freep,
    .Z_Realloc = Z_Realloc,
    .Z_ReallocArray = Z_ReallocArray,
    .Z_FreeTags = Z_FreeTags,
    .Z_LeakTest = Z_LeakTest,

    .FS_LoadFileEx = FS_LoadFileEx,
    .FS_ListFiles = FS_ListFiles,
    .FS_FreeList = FS_FreeList,
    .FS_OpenFile = FS_OpenFile,
    .FS_CloseFile = FS_CloseFile,
    .FS_FPrintf = FS_FPrintf,
    .FS_GetGameDir = CG_UI_GetGameDir,
    .SV_GetSaveInfo = SV_GetSaveInfo,

    .NET_IsEqualBaseAdr = NET_IsEqualBaseAdr,
    .NET_AdrToString = NET_AdrToString,
    .NET_StringToAdr = NET_StringToAdr,
    .NET_GetLastAddress = CG_UI_GetNetFrom,

    .Key_GetDest = Key_GetDest,
    .Key_SetDest = Key_SetDest,
    .Key_IsDown = Key_IsDown,
    .Key_EnumBindings = Key_EnumBindings,
    .Key_KeynumToString = Key_KeynumToString,
    .Key_WaitKey = Key_WaitKey,
    .Key_SetBinding = Key_SetBinding,
    .Key_GetOverstrikeMode = Key_GetOverstrikeMode,
    .Key_SetOverstrikeMode = Key_SetOverstrikeMode,

    .IN_WarpMouse = IN_WarpMouse,

    .SCR_UpdateScreen = SCR_UpdateScreen,
    .SCR_ParseColor = SCR_ParseColor,
    .V_CalcFov = V_CalcFov,

    .CL_GetDemoInfo = CL_GetDemoInfo,
    .CL_SendStatusRequest = CG_UI_SendStatusRequest,
#if USE_CURL
    .HTTP_FetchFile = HTTP_FetchFile,
#else
    .HTTP_FetchFile = nullptr,
#endif

    .S_StartLocalSound = S_StartLocalSound,
    .S_StopAllSounds = S_StopAllSounds,

    .Re_RegisterModel = CG_UI_RegisterModel,
    .Re_RegisterImage = CG_UI_RegisterImage,
    .Re_GetPicSize = CG_UI_GetPicSize,
    .Re_RenderFrame = CG_UI_RenderFrame,
    .Re_SetClipRect = CG_UI_SetClipRect,
    .Re_SetScale = CG_UI_SetScale,
    .Re_ClampScale = CG_UI_ClampScale,
    .Re_DrawChar = CG_UI_DrawChar,
    .Re_DrawStringStretch = CG_UI_DrawStringStretch,
    .UI_FontDrawString = CG_UI_FontDrawString,
    .UI_FontMeasureString = CG_UI_FontMeasureString,
    .UI_FontLineHeight = CG_UI_FontLineHeight,
    .UI_FontDrawStringSized = CG_UI_FontDrawStringSized,
    .UI_FontMeasureStringSized = CG_UI_FontMeasureStringSized,
    .UI_FontLineHeightSized = CG_UI_FontLineHeightSized,
    .UI_FontLegacyHandle = CG_UI_FontLegacyHandle,
    .Re_DrawPic = CG_UI_DrawPic,
    .Re_DrawKeepAspectPic = CG_UI_DrawKeepAspectPic,
    .Re_DrawFill8 = CG_UI_DrawFill8,
    .Re_DrawFill32 = CG_UI_DrawFill32,
    .Re_GetConfig = CG_UI_GetConfig,
    .CL_SetMenuBlurRect = CG_UI_SetMenuBlurRect,

    .SetClipboardData = CG_UI_SetClipboardData,
    .GetEventTime = CG_UI_GetEventTime,
    .GetLocalTime = CG_UI_GetLocalTime,
    .Con_Close = Con_Close,
};

void CG_Init(void)
{
    cl_alpha = Cvar_Get("cl_alpha", "1", 0);
}

static void CG_Print(const char *msg)
{
    Com_Printf("%s", msg);
}

static const char *CG_get_configstring(int index)
{
    if (index < 0 || index >= cl.csr.end)
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);

    return cl.configstrings[index];
}

static void CG_Com_Error(const char *message)
{
    Com_Error(ERR_DROP, "%s", message);
}

static void *CG_TagMalloc(size_t size, int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    return Z_TagMallocz(size, static_cast<memtag_t>(tag + TAG_MAX));
}

static void CG_FreeTags(int tag)
{
    if (tag > UINT16_MAX - TAG_MAX) {
        Com_Error(ERR_DROP, "%s: bad tag", __func__);
    }
    Z_FreeTags(static_cast<memtag_t>(tag + TAG_MAX));
}

static void CG_Cvar_Reset(cvar_t *var)
{
    Cvar_Reset(var);
}

static void CG_Q2Proto_UnpackSolid(uint32_t solid, vec3_t mins, vec3_t maxs)
{
    q2proto_client_unpack_solid(&cls.q2proto_ctx, solid, mins, maxs);
}

static void CG_Entity_Pmove(pmove_t *pmove)
{
    Pmove(pmove, &cl.pmp);
}

static void CG_CL_GTV_Resume(void)
{
    CL_GTV_Resume();
}

static void CG_CL_GTV_EmitFrame(void)
{
    CL_GTV_EmitFrame();
}

static cvar_t *CG_cvar(const char *var_name, const char *value, cvar_flags_t flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("CGame attemped to set extended flags on '%s', masked out.\n", var_name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(var_name, value, flags | CVAR_GAME);
}

static void CG_AddCommandString(const char *string)
{
    if (!strcmp(string, "menu_loadgame\n"))
        string = "pushmenu loadgame\n";
    Cbuf_AddText(&cmd_buffer, string);
}

static qhandle_t CG_R_RegisterSkin(const char *name)
{
    return R_RegisterImage(name, IT_SKIN, IF_NONE);
}

static qhandle_t CG_R_RegisterModel(const char *name)
{
    if (!re.RegisterModel)
        return 0;
    return re.RegisterModel(name);
}

static qhandle_t CG_R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    if (!re.RegisterImage)
        return 0;
    return re.RegisterImage(name, type, flags);
}

static bool CG_R_SupportsPerPixelLighting(void)
{
    return re.SupportsPerPixelLighting ? re.SupportsPerPixelLighting() : false;
}

extern "C" mtexinfo_t nulltexinfo;

#ifdef R_RegisterModel
#pragma push_macro("R_RegisterModel")
#undef R_RegisterModel
#define CGAME_ENTITY_POP_R_REGISTERMODEL 1
#endif
#ifdef R_RegisterImage
#pragma push_macro("R_RegisterImage")
#undef R_RegisterImage
#define CGAME_ENTITY_POP_R_REGISTERIMAGE 1
#endif
#ifdef R_SupportsPerPixelLighting
#pragma push_macro("R_SupportsPerPixelLighting")
#undef R_SupportsPerPixelLighting
#define CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING 1
#endif

static cgame_entity_import_t cg_entity_import = {
    .api_version = CGAME_ENTITY_API_VERSION,

    .cl = &cl,
    .cls = &cls,
    .cl_entities = cl_entities,
    .te = &te,
    .mz = &mz,
    .null_surface = &nulltexinfo.c,
    .gun_frame = &gun_frame,
    .gun_model = &gun_model,
    .cmd_buffer = &cmd_buffer,

    .Com_Printf = CG_UI_Com_Printf,
    .Com_DPrintf = CG_UI_Com_DPrintf,
    .Com_WPrintf = CG_UI_Com_WPrintf,
    .Com_LPrintf = Com_LPrintf,
    .Com_Error = Com_Error,

    .Com_PlayerToEntityState = Com_PlayerToEntityState,
    .Com_BlockChecksum = Com_BlockChecksum,
    .Com_SlowRand = Com_SlowRand,
    .Com_Color_g = Com_Color_g,

    .Cvar_Get = Cvar_Get,
    .Cvar_FindVar = Cvar_FindVar,
    .Cvar_SetByVar = Cvar_SetByVar,
    .Cvar_SetValue = Cvar_SetValue,
    .Cvar_SetInteger = Cvar_SetInteger,
    .Cvar_Reset = CG_Cvar_Reset,
    .Cvar_ClampInteger = Cvar_ClampInteger,
    .Cvar_ClampValue = Cvar_ClampValue,
    .fs_game = fs_game,
    .sv_paused = sv_paused,

    .Cbuf_ExecuteDeferred = Cbuf_ExecuteDeferred,
    .AddCommandString = CG_AddCommandString,
    .Cmd_ExecTrigger = Cmd_ExecTrigger,

    .IN_Activate = IN_Activate,
    .Con_Close = Con_Close,
    .SCR_EndLoadingPlaque = SCR_EndLoadingPlaque,
    .SCR_LagClear = SCR_LagClear,
    .SCR_SetCrosshairColor = SCR_SetCrosshairColor,
    .SCR_ParseColor = SCR_ParseColor,

    .Key_IsDown = Key_IsDown,

    .CL_CheckForPause = CL_CheckForPause,
    .CL_UpdateFrameTimes = CL_UpdateFrameTimes,
    .CL_EmitDemoFrame = CL_EmitDemoFrame,
    .CL_FirstDemoFrame = CL_FirstDemoFrame,
    .CL_GTV_Resume = CG_CL_GTV_Resume,
    .CL_GTV_EmitFrame = CG_CL_GTV_EmitFrame,
    .CL_AddHitMarker = CL_AddHitMarker,
    .CL_LoadClientinfo = CL_LoadClientinfo,

    .CL_Trace = CL_Trace,
    .CL_PointContents = CL_PointContents,
    .Pmove = CG_Entity_Pmove,

    .Z_Malloc = Z_Malloc,
    .Z_Freep = Z_Freep,

    .FS_LoadFileEx = FS_LoadFileEx,
    .FS_FreeFile = Z_Free,

    .CM_BoxTrace = CM_BoxTrace,
    .CM_TransformedBoxTrace = CM_TransformedBoxTrace,
    .CM_ClipEntity = CM_ClipEntity,
    .BSP_LoadMaterials = BSP_LoadMaterials,

    .Q2Proto_UnpackSolid = CG_Q2Proto_UnpackSolid,

    .R_RegisterModel = CG_R_RegisterModel,
    .R_RegisterImage = CG_R_RegisterImage,
    .R_RegisterSkin = CG_R_RegisterSkin,
    .R_SupportsPerPixelLighting = CG_R_SupportsPerPixelLighting,
    .V_CalcFov = V_CalcFov,

    .V_AddEntity = V_AddEntity,
    .V_AddParticle = V_AddParticle,
    .V_AddLight = V_AddLight,
    .V_AddLightEx = V_AddLightEx,
    .V_AddLightStyle = V_AddLightStyle,

    .S_RegisterSound = S_RegisterSound,
    .S_StartSound = S_StartSound,

    .LOC_AddLocationsToScene = LOC_AddLocationsToScene,
};

#ifdef CGAME_ENTITY_POP_R_REGISTERMODEL
#pragma pop_macro("R_RegisterModel")
#undef CGAME_ENTITY_POP_R_REGISTERMODEL
#endif
#ifdef CGAME_ENTITY_POP_R_REGISTERIMAGE
#pragma pop_macro("R_RegisterImage")
#undef CGAME_ENTITY_POP_R_REGISTERIMAGE
#endif
#ifdef CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING
#pragma pop_macro("R_SupportsPerPixelLighting")
#undef CGAME_ENTITY_POP_R_SUPPORTSPERPIXELLIGHTING
#endif

static void * CG_GetExtension(const char *name)
{
    if (!name)
        return NULL;
    if (strcmp(name, cgame_q2pro_extended_support_ext) == 0) {
        return &cgame_q2pro_extended_support;
    }
    if (strcmp(name, CGAME_UI_IMPORT_EXT) == 0) {
        return &cg_ui_import;
    }
    if (strcmp(name, CGAME_ENTITY_IMPORT_EXT) == 0) {
        return &cg_entity_import;
    }
    return NULL;
}

static bool CG_CL_FrameValid(void)
{
    return cl.frame.valid;
}

static float CG_CL_FrameTime(void)
{
    return cls.frametime;
}

static uint64_t CG_CL_ClientTime(void)
{
    return cl.time;
}

static uint64_t CG_CL_ClientRealTime(void)
{
    return com_localTime;
}

static uint64_t CG_CL_ClientRealTimeUnscaled(void)
{
    return com_localTime3;
}

static int32_t CG_CL_ServerFrame(void)
{
    return cl.frame.number;
}

static int32_t CG_CL_ServerProtocol(void)
{
    return cls.serverProtocol;
}

static const char* CG_CL_GetClientName(int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].name;
}

static const char* CG_CL_GetClientPic(int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].icon_name;
}

static const char * CG_CL_GetClientDogtag (int32_t index)
{
    if (index < 0 || index >= MAX_CLIENTS) {
        Com_Error(ERR_DROP, "%s: invalid client index", __func__);
    }
    return cl.clientinfo[index].dogtag_name;
}

static const char* CG_CL_GetKeyBinding(const char *binding)
{
    return Key_GetBinding(binding);
}

static bool CG_Draw_RegisterPic(const char *name)
{
    qhandle_t img = R_RegisterPic(name);
    return img != 0;
}

static void CG_Draw_GetPicSize (int *w, int *h, const char *name)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0) {
        *w = *h = 0;
        return;
    }
    R_GetPicSize(w, h, img);
}

static void CG_SCR_DrawChar(int x, int y, int scale, int num, bool shadow)
{
    int draw_flags = shadow ? UI_DROPSHADOW : 0;
    int glyph_scale = scale > 0 ? scale : 1;

    R_DrawStretchChar(x, y,
                      CONCHAR_WIDTH * glyph_scale,
                      CONCHAR_HEIGHT * glyph_scale,
                      draw_flags, num, apply_scr_alpha(COLOR_WHITE), scr.ui_font_pic);
}

static void CG_SCR_DrawCharStretch(int x, int y, int w, int h, int flags, int ch, const rgba_t *color)
{
    color_t draw_color = apply_scr_alpha(color ? *color : COLOR_WHITE);
    R_DrawStretchChar(x, y, w, h, flags, ch, draw_color, scr.ui_font_pic);
}

static void CG_SCR_DrawPic (int x, int y, int w, int h, const char *name)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0)
        return;

    R_DrawStretchPic(x, y,
                     w, h,
                     apply_scr_alpha(COLOR_WHITE), img);
}

static void CG_SCR_DrawColorPic(int x, int y, int w, int h, const char* name, const rgba_t *color)
{
    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (img == 0)
        return;

    R_DrawStretchPic(x, y,
                     w, h,
                     apply_scr_alpha(*color), img);
}

static void CG_SCR_WarpMouse(int x, int y)
{
    float pixel_scale = scr.hud_scale > 0.0f ? (scr.virtual_scale / scr.hud_scale) : scr.virtual_scale;
    if (pixel_scale <= 0.0f)
        pixel_scale = 1.0f;

    IN_WarpMouse(Q_rint(x * pixel_scale), Q_rint(y * pixel_scale));
}

static void CG_SCR_SetAltTypeface(bool enabled)
{
    // We don't support alternate type faces.
}

static float CG_SCR_FontLineHeight(int scale)
{
    int draw_scale = scale > 0 ? scale : 1;

    if (scr.ui_font)
        return (float)Font_LineHeight(scr.ui_font, draw_scale);

    return (float)(CONCHAR_HEIGHT * draw_scale);
}

static cg_vec2_t CG_SCR_MeasureFontString(const char *str, int scale)
{
    if (!str || !*str)
        return cg_vec2_t{ 0.0f, 0.0f };

    size_t maxlen = strlen(str);
    int num_lines = 1;
    int max_width = 0;
    int draw_scale = scale > 0 ? scale : 1;

    while (*str) {
        const char *p = strchr(str, '\n');
        if (!p) {
            int line_width = scr.ui_font
                ? Font_MeasureString(scr.ui_font, draw_scale, 0, maxlen, str, nullptr)
                : (int)Com_StrlenNoColor(str, maxlen) * CONCHAR_WIDTH * draw_scale;
            if (line_width > max_width)
                max_width = line_width;
            break;
        }

        size_t len = min(p - str, maxlen);
        int line_width = scr.ui_font
            ? Font_MeasureString(scr.ui_font, draw_scale, 0, len, str, nullptr)
            : (int)Com_StrlenNoColor(str, len) * CONCHAR_WIDTH * draw_scale;
        if (line_width > max_width)
            max_width = line_width;
        maxlen -= len;

        ++num_lines;
        str = p + 1;
    }

    return cg_vec2_t{ static_cast<float>(max_width),
                      static_cast<float>(num_lines) * CG_SCR_FontLineHeight(draw_scale) };
}

static cg_vec2_t CG_SCR_MeasureCenterFontString(const char *str, int scale)
{
    if (!str || !*str)
        return cg_vec2_t{ 0.0f, 0.0f };

    const font_t *font = scr.font ? scr.font : scr.ui_font;
    size_t maxlen = strlen(str);
    int num_lines = 1;
    int max_width = 0;
    int draw_scale = scale > 0 ? scale : 1;

    while (*str) {
        const char *p = strchr(str, '\n');
        if (!p) {
            int line_width = font
                ? Font_MeasureString(font, draw_scale, 0, maxlen, str, nullptr)
                : (int)Com_StrlenNoColor(str, maxlen) * CONCHAR_WIDTH * draw_scale;
            if (line_width > max_width)
                max_width = line_width;
            break;
        }

        size_t len = min(p - str, maxlen);
        int line_width = font
            ? Font_MeasureString(font, draw_scale, 0, len, str, nullptr)
            : (int)Com_StrlenNoColor(str, len) * CONCHAR_WIDTH * draw_scale;
        if (line_width > max_width)
            max_width = line_width;
        maxlen -= len;

        ++num_lines;
        str = p + 1;
    }

    float line_height = font
        ? (float)Font_LineHeight(font, draw_scale)
        : (float)(CONCHAR_HEIGHT * draw_scale);
    return cg_vec2_t{ static_cast<float>(max_width),
                      static_cast<float>(num_lines) * line_height };
}

static void CG_SCR_DrawFontString(const char *str, int x, int y, int scale, const rgba_t *color, bool shadow, text_align_t align)
{
    int draw_x = x;
    if (align != LEFT) {
        int text_width = CG_SCR_MeasureFontString(str, scale).x;
        if (align == CENTER)
            draw_x -= text_width / 2;
        else if (align == RIGHT)
            draw_x -= text_width;
    }

    int draw_flags = shadow ? UI_DROPSHADOW : 0;
    color_t draw_color = apply_scr_alpha(*color);
    int draw_scale = scale > 0 ? scale : 1;

    SCR_DrawStringMultiStretch(draw_x, y, draw_scale,
                               draw_flags, strlen(str), str, draw_color, scr.ui_font_pic);
}

static void CG_SCR_DrawCenterFontString(const char *str, int x, int y, int scale,
                                        const rgba_t *color, bool shadow, text_align_t align)
{
    if (!str || !*str)
        return;

    font_t *font = scr.font ? scr.font : scr.ui_font;
    if (!font)
        return;

    int draw_x = x;
    if (align != LEFT) {
        int text_width = CG_SCR_MeasureCenterFontString(str, scale).x;
        if (align == CENTER)
            draw_x -= text_width / 2;
        else if (align == RIGHT)
            draw_x -= text_width;
    }

    int draw_flags = shadow ? UI_DROPSHADOW : 0;
    color_t draw_color = apply_scr_alpha(*color);
    int draw_scale = scale > 0 ? scale : 1;
    const char *line = str;

    while (*line) {
        const char *p = strchr(line, '\n');
        size_t len = p ? (size_t)(p - line) : strlen(line);
        Font_DrawString(font, draw_x, y, draw_scale, draw_flags, len, line, draw_color);
        if (!p)
            break;
        y += Font_LineHeight(font, draw_scale);
        line = p + 1;
    }
}

static bool CG_CL_GetTextInput(const char **msg, bool *is_team)
{
    // FIXME: Hook up with chat prompt
    return false;
}

static int32_t CG_CL_GetWarnAmmoCount(int32_t weapon_id)
{
    return 0;
}

#define NUM_LOC_STRINGS     8
#define LOC_STRING_LENGTH   MAX_STRING_CHARS
static char cg_loc_strings[NUM_LOC_STRINGS][LOC_STRING_LENGTH];
static int cg_loc_string_num = 0;

static const char* CG_Localize (const char *base, const char **args, size_t num_args)
{
    char *out_str = cg_loc_strings[cg_loc_string_num];
    cg_loc_string_num = (cg_loc_string_num + 1) % NUM_LOC_STRINGS;
    Loc_Localize(base, true, args, num_args, out_str, LOC_STRING_LENGTH);
    return out_str;
}

static const rgba_t rgba_white = { .r = 255, .g = 255, .b = 255, .a = 255 };

static int32_t CG_SCR_DrawBind(int32_t isplit, const char *binding, const char *purpose, int x, int y, int scale)
{
    /* - 'binding' is the name of the action/command (eg "+moveup") whose binding should be displayed
     * - 'purpose' is a string describing what that action/key does. Needs localization.
     * - Rerelease has some fancy graphics for keys and such. We ... don't ¯\_(ツ)_/¯
     */
    const char *key_name = NULL;
    color_t draw_color = apply_scr_alpha(rgba_white);
    int line_height = max(1, Q_rint(CG_SCR_FontLineHeight(scale)));
    int icon_size = max(1, Q_rint(line_height * 3.0f));
    int padding = max(2, icon_size / 6);
    int keynum = Key_EnumBindings(0, binding);
    qhandle_t icon = 0;
    int icon_w = 0;
    int icon_h = 0;

    if (keynum >= 0) {
        key_name = Key_KeynumToString(keynum);
        if (SCR_GetBindIconForKey(keynum, &icon, &icon_w, &icon_h)) {
            float icon_scale = icon_h > 0 ? ((float)icon_size / (float)icon_h) : 1.0f;
            icon_w = max(1, Q_rint(icon_w * icon_scale));
        } else {
            icon = 0;
            icon_w = 0;
        }
    }

    const char *purpose_text = CG_Localize(purpose, NULL, 0);
    char str[MAX_STRING_CHARS];
    if (!key_name || !*key_name)
        Q_snprintf(str, sizeof(str), "<unbound> %s", purpose_text);
    else if (icon_w > 0)
        Q_snprintf(str, sizeof(str), "%s", purpose_text);
    else
        Q_snprintf(str, sizeof(str), "[%s] %s", key_name, purpose_text);

    int text_w = CG_SCR_MeasureFontString(str, scale).x;
    int total_w = text_w + (icon_w > 0 ? icon_w + padding : 0);
    int start_x = x - (total_w / 2);
    int text_x = start_x + (icon_w > 0 ? icon_w + padding : 0);

    if (icon_w > 0) {
        int icon_y = y - ((icon_size - line_height) / 2);
        R_DrawStretchPic(start_x, icon_y, icon_w, icon_size, draw_color, icon);
    }

    CG_SCR_DrawFontString(str, text_x, y, scale, &rgba_white, false, LEFT);
    return max(line_height, icon_size);
}

static int32_t CG_SCR_DrawBindIcon(const char *binding, int x, int y, int size, const rgba_t *color, const char **out_keyname)
{
    color_t draw_color = apply_scr_alpha(color ? *color : COLOR_WHITE);
    return SCR_DrawBindIcon(binding, x, y, size, draw_color, out_keyname);
}

static int CG_GetBaseScaleInt(void)
{
    float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
    float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
    float base_scale = max(scale_x, scale_y);
    int base_scale_int = (int)base_scale;

    if (base_scale_int < 1)
        base_scale_int = 1;

    return base_scale_int;
}

static int CG_GetUiScaleInt(int base_scale_int)
{
    float extra_scale = 1.0f;
    cvar_t *scale_var = Cvar_WeakGet("cl_scale");

    if (scale_var && scale_var->value)
        extra_scale = Cvar_ClampValue(scale_var, 0.25f, 10.0f);

    int ui_scale_int = (int)((float)base_scale_int * extra_scale);

    if (ui_scale_int < 1)
        ui_scale_int = 1;

    return ui_scale_int;
}

static void CG_SCR_GetScreenMetrics(cg_screen_metrics_t *out)
{
    if (!out)
        return;

    if (scr.virtual_width <= 0 || scr.virtual_height <= 0 || scr.virtual_scale <= 0.0f) {
        int base_scale_int = CG_GetBaseScaleInt();
        scr.virtual_width = max(1, r_config.width / base_scale_int);
        scr.virtual_height = max(1, r_config.height / base_scale_int);
        scr.virtual_scale = (float)base_scale_int;
    }

    int base_scale_int = CG_GetBaseScaleInt();
    int ui_scale_int = CG_GetUiScaleInt(base_scale_int);
    float hud_scale = scr.hud_scale > 0.0f ? scr.hud_scale : 1.0f;

    out->screen_width = r_config.width;
    out->screen_height = r_config.height;
    out->hud_width = Q_rint(scr.virtual_width * hud_scale);
    out->hud_height = Q_rint(scr.virtual_height * hud_scale);
    out->hud_scale = hud_scale;
    out->virtual_scale = scr.virtual_scale;
    out->base_scale = base_scale_int;
    out->ui_scale = ui_scale_int;
}

static void CG_SCR_SetScale(float scale)
{
    R_SetScale(scale);
}

static int32_t CG_SCR_DrawString(int x, int y, int scale, int flags, size_t max_chars, const char *text, const rgba_t *color)
{
    color_t draw_color = apply_scr_alpha(color ? *color : COLOR_WHITE);
    return SCR_DrawStringStretch(x, y, scale, flags, max_chars, text, draw_color, scr.ui_font_pic);
}

static int32_t CG_SCR_MeasureString(const char *text, size_t max_chars)
{
    return SCR_MeasureString(text, max_chars);
}

static void CG_SCR_DrawStretchRotatePic(int x, int y, int w, int h, const rgba_t *color, float angle,
                                        int pivot_x, int pivot_y, const char *name)
{
    if (!name || !*name)
        return;

    qhandle_t img = R_RegisterImage(name, IT_PIC, IF_NONE);
    if (!img)
        return;

    color_t draw_color = apply_scr_alpha(color ? *color : COLOR_WHITE);
    R_DrawStretchRotatePic(x, y, w, h, draw_color, angle, pivot_x, pivot_y, img);
}

static void CG_CL_GetViewParams(cg_view_params_t *out)
{
    if (!out)
        return;

    VectorCopy(cl.refdef.vieworg, out->vieworg);
    VectorCopy(cl.predicted_angles, out->viewangles);
    out->fov_x = cl.refdef.fov_x;
    out->fov_y = cl.refdef.fov_y;
}

static const char *CG_CL_GetChatPrompt(int *prompt_skip)
{
    return Con_GetChatPromptText(prompt_skip);
}

static bool CG_CL_GetChatInput(cg_chat_input_t *out)
{
    if (!(cls.key_dest & KEY_MESSAGE))
        return false;

    inputField_t *field = Con_GetChatInputField();
    if (!field)
        return false;

    if (out) {
        out->text = field->text;
        out->max_chars = field->maxChars;
        out->visible_chars = field->visibleChars;
        out->cursor_pos = field->cursorPos;
        out->overstrike = Key_GetOverstrikeMode();
    }
    return true;
}

static void CG_CL_SetChatCursor(size_t cursor_pos)
{
    inputField_t *field = Con_GetChatInputField();
    if (!field)
        return;

    field->cursorPos = min(cursor_pos, field->maxChars);
}

static void CG_CL_SetChatVisibleChars(size_t visible_chars)
{
    inputField_t *field = Con_GetChatInputField();
    if (!field)
        return;

    field->visibleChars = min(visible_chars, field->maxChars);
}

static int32_t CG_CL_GetKeyDest(void)
{
    return cls.key_dest;
}

static bool CG_Key_IsDown(int key)
{
    return Key_IsDown(key);
}

static const char *CG_CL_GetImageConfigString(int image_index)
{
    if (image_index < 0 || image_index >= cl.csr.max_images)
        return "";

    return cl.configstrings[cl.csr.images + image_index];
}

#if !USE_EXTERNAL_RENDERERS
extern uint32_t d_8to24table[256];
#endif

static rgba_t CG_CL_GetPaletteColor(int index)
{
    if (index < 0)
        index = 0;
    if (index > 255)
        index = 255;

#if USE_EXTERNAL_RENDERERS
    const uint32_t *palette = re.PaletteTable;
    if (!palette)
        return rgba_white;
    color_t color = { .u32 = palette[index] };
#else
    color_t color = { .u32 = d_8to24table[index] };
#endif

    return color;
}

static bool CG_CL_InAutoDemoLoop(void)
{
    // FIXME: implement
    return false;
}

const cgame_export_t *cgame = NULL;
const cgame_entity_export_t *cgame_entity = NULL;
static char *current_game = NULL;
static bool current_rerelease_server;
static const cgame_ui_export_t *cgame_ui;
static void *cgame_library;

typedef cgame_export_t *(*cgame_entry_t)(cgame_import_t *);

static void CG_FillImports(cgame_import_t *imports)
{
    // cl.frametime.time is unset during early UI boot; avoid divide-by-zero.
    const uint32_t frame_time_ms = CL_FRAMETIME ? (uint32_t)CL_FRAMETIME : BASE_FRAMETIME;

    *imports = cgame_import_t{
        .tick_rate = 1000 / frame_time_ms,
        .frame_time_s = frame_time_ms * 0.001f,
        .frame_time_ms = frame_time_ms,

        .Com_Print = CG_Print,

        .get_configstring = CG_get_configstring,

        .Com_Error = CG_Com_Error,

        .TagMalloc = CG_TagMalloc,
        .TagFree = Z_Free,
        .FreeTags = CG_FreeTags,

        .cvar = CG_cvar,
        .cvar_set = Cvar_UserSet,
        .cvar_forceset = Cvar_Set,

        .AddCommandString = CG_AddCommandString,

        .GetExtension = CG_GetExtension,

        .CL_FrameValid = CG_CL_FrameValid,

        .CL_FrameTime = CG_CL_FrameTime,

        .CL_ClientTime = CG_CL_ClientTime,
        .CL_ClientRealTime = CG_CL_ClientRealTime,
        .CL_ServerFrame = CG_CL_ServerFrame,
        .CL_ServerProtocol = CG_CL_ServerProtocol,
        .CL_GetClientName = CG_CL_GetClientName,
        .CL_GetClientPic = CG_CL_GetClientPic,
        .CL_GetClientDogtag = CG_CL_GetClientDogtag,
        .CL_GetKeyBinding = CG_CL_GetKeyBinding,
        .Draw_RegisterPic = CG_Draw_RegisterPic,
        .Draw_GetPicSize = CG_Draw_GetPicSize,
        .SCR_DrawChar = CG_SCR_DrawChar,
        .SCR_DrawPic = CG_SCR_DrawPic,
        .SCR_DrawColorPic = CG_SCR_DrawColorPic,

    .SCR_SetAltTypeface = CG_SCR_SetAltTypeface,
    .SCR_DrawFontString = CG_SCR_DrawFontString,
    .SCR_MeasureFontString = CG_SCR_MeasureFontString,
    .SCR_DrawCenterFontString = CG_SCR_DrawCenterFontString,
    .SCR_MeasureCenterFontString = CG_SCR_MeasureCenterFontString,
    .SCR_FontLineHeight = CG_SCR_FontLineHeight,

        .CL_GetTextInput = CG_CL_GetTextInput,

        .CL_GetWarnAmmoCount = CG_CL_GetWarnAmmoCount,

        .Localize = CG_Localize,

        .SCR_DrawBind = CG_SCR_DrawBind,

        .CL_InAutoDemoLoop = CG_CL_InAutoDemoLoop,
        .CL_ClientRealTimeUnscaled = CG_CL_ClientRealTimeUnscaled,
        .SCR_DrawCharStretch = CG_SCR_DrawCharStretch,
        .SCR_WarpMouse = CG_SCR_WarpMouse,
        .SCR_DrawBindIcon = CG_SCR_DrawBindIcon,
        .SCR_GetScreenMetrics = CG_SCR_GetScreenMetrics,
        .SCR_SetScale = CG_SCR_SetScale,
        .SCR_DrawString = CG_SCR_DrawString,
        .SCR_MeasureString = CG_SCR_MeasureString,
        .SCR_DrawStretchRotatePic = CG_SCR_DrawStretchRotatePic,
        .CL_GetViewParams = CG_CL_GetViewParams,
        .CL_GetChatPrompt = CG_CL_GetChatPrompt,
        .CL_GetChatInput = CG_CL_GetChatInput,
        .CL_SetChatCursor = CG_CL_SetChatCursor,
        .CL_SetChatVisibleChars = CG_CL_SetChatVisibleChars,
        .CL_GetKeyDest = CG_CL_GetKeyDest,
        .Key_IsDown = CG_Key_IsDown,
        .CL_GetImageConfigString = CG_CL_GetImageConfigString,
        .CL_GetPaletteColor = CG_CL_GetPaletteColor,
    };
}

static const cgame_ui_export_t *CG_LoadUI(void)
{
    static bool warned_missing;
    static bool warned_version;

    if (cgame_ui)
        return cgame_ui;

    if (!cgame_library)
        cgame_library = CGameDll_Load();
    if (!cgame_library)
        return NULL;

    cgame_entry_t entry = reinterpret_cast<cgame_entry_t>(Sys_GetProcAddress(cgame_library, "GetCGameAPI"));
    if (!entry) {
        if (!warned_missing) {
            Com_WPrintf("cgame UI export not available\n");
            warned_missing = true;
        }
        return NULL;
    }

    cgame_import_t cgame_imports;
    CG_FillImports(&cgame_imports);
    const cgame_export_t *exports = entry(&cgame_imports);
    if (!exports || !exports->GetExtension)
        return NULL;

    cgame_ui = static_cast<const cgame_ui_export_t *>(exports->GetExtension(CGAME_UI_EXPORT_EXT));
    if (cgame_ui && cgame_ui->api_version != CGAME_UI_API_VERSION) {
        if (!warned_version) {
            Com_WPrintf("cgame UI API version mismatch\n");
            warned_version = true;
        }
        cgame_ui = NULL;
    }

    return cgame_ui;
}

const cgame_ui_export_t *CG_UI_GetExport(void)
{
    return CG_LoadUI();
}

void CG_Load(const char* new_game, bool is_rerelease_server)
{
    if (!current_game || strcmp(current_game, new_game) != 0 || current_rerelease_server != is_rerelease_server) {
        cgame_import_t cgame_imports;
        CG_FillImports(&cgame_imports);

        cgame_entry_t entry = NULL;
        if (is_rerelease_server) {
            if (!cgame_library)
                cgame_library = CGameDll_Load();
            if (cgame_library)
                entry = reinterpret_cast<cgame_entry_t>(Sys_GetProcAddress(cgame_library, "GetCGameAPI"));
        } else {
            // if we're connected to a "classic" Q2PRO server, always use builtin cgame
            entry = GetClassicCGameAPI;
        }

        if(!entry) {
            Com_Error(ERR_DROP, "cgame functions not available");
            cgame = NULL;
            Z_Freep(&current_game);
            current_rerelease_server = false;
        }

        cgame = entry(&cgame_imports);
        if (!cgame || cgame->apiversion != CGAME_API_VERSION) {
            Com_Error(ERR_DROP, "cgame API version mismatch (got %d, expected %d)",
                      cgame ? cgame->apiversion : -1, CGAME_API_VERSION);
        }
        cgame_entity = NULL;
        if (cgame->GetExtension) {
            cgame_entity = static_cast<const cgame_entity_export_t *>(cgame->GetExtension(CGAME_ENTITY_EXPORT_EXT));
            if (cgame_entity && cgame_entity->api_version != CGAME_ENTITY_API_VERSION) {
                Com_WPrintf("cgame entity API version mismatch\n");
                cgame_entity = NULL;
            }
        }
        if (!cgame_entity) {
            Com_Error(ERR_DROP, "cgame entity extension required");
        }

        CL_InitEffects();
        CL_InitBrightskins();
        CL_InitTEnts();
        Z_Freep(&current_game);
        current_game = Z_CopyString(new_game);
        current_rerelease_server = is_rerelease_server;
    }
}

void CG_Unload(void)
{
    cgame = NULL;
    cgame_entity = NULL;
    cgame_ui = NULL;
    Z_Freep(&current_game);

    Sys_FreeLibrary(cgame_library);
    cgame_library = NULL;
}

float CL_Wheel_TimeScale(void)
{
    if (cgame && cgame->Wheel_TimeScale)
        return cgame->Wheel_TimeScale();

    return 1.0f;
}
