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

// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake renderer engine.


#include "client.h"
#include "system/system.h"
#if USE_EXTERNAL_RENDERERS
#include "renderer/renderer_api.h"
#endif

// Console variables that we need to access from this module
cvar_t      *vid_geometry;
cvar_t      *vid_modelist;
cvar_t      *vid_fullscreen;
cvar_t      *_vid_fullscreen;

const vid_driver_t  *vid;

#if USE_EXTERNAL_RENDERERS
extern cvar_t *cl_async;
extern cvar_t *cl_gunfov;
extern cvar_t *cl_adjustfov;
extern cvar_t *cl_gun;
extern cvar_t *info_hand;

static refcfg_t renderer_stub_config;
static float R_ClampScaleStub(cvar_t *var)
{
    if (!var) {
        return 1.0f;
    }

    if (var->value) {
        return 1.0f / Cvar_ClampValue(var, 1.0f, 10.0f);
    }

    return 1.0f;
}

renderer_export_t re = {
    .ClampScale = R_ClampScaleStub,
    .Config = &renderer_stub_config
};
static void *renderer_handle;
static const renderer_export_t *(*renderer_get_api)(const renderer_import_t *import);
static cvar_t *r_renderer;
#endif

#define MODE_GEOMETRY   1
#define MODE_FULLSCREEN 2
#define MODE_MODELIST   4

static int  mode_changed;

/*
==========================================================================

HELPER FUNCTIONS

==========================================================================
*/

// 640x480 800x600 1024x768
// 640x480@75
// 640x480@75:32
// 640x480:32@75
bool VID_GetFullscreen(vrect_t *rc, int *freq_p, int *depth_p)
{
    unsigned long w, h, freq, depth;
    char *s;
    int mode;

    // fill in default parameters
    rc->x = 0;
    rc->y = 0;
    rc->width = 640;
    rc->height = 480;

    if (freq_p)
        *freq_p = 0;
    if (depth_p)
        *depth_p = 0;

    if (!vid_modelist || !vid_fullscreen)
        return false;

    s = vid_modelist->string;
    while (Q_isspace(*s))
        s++;
    if (!*s)
        return false;

    mode = 1;
    while (1) {
        if (!strncmp(s, "desktop", 7)) {
            s += 7;
            if (*s && !Q_isspace(*s)) {
                Com_DPrintf("Mode %d is malformed\n", mode);
                return false;
            }
            w = h = freq = depth = 0;
        } else {
            w = strtoul(s, &s, 10);
            if (*s != 'x' && *s != 'X') {
                Com_DPrintf("Mode %d is malformed\n", mode);
                return false;
            }
            h = strtoul(s + 1, &s, 10);
            freq = depth = 0;
            if (*s == '@') {
                freq = strtoul(s + 1, &s, 10);
                if (*s == ':') {
                    depth = strtoul(s + 1, &s, 10);
                }
            } else if (*s == ':') {
                depth = strtoul(s + 1, &s, 10);
                if (*s == '@') {
                    freq = strtoul(s + 1, &s, 10);
                }
            }
        }
        if (mode == vid_fullscreen->integer) {
            break;
        }
        while (Q_isspace(*s))
            s++;
        if (!*s) {
            Com_DPrintf("Mode %d not found\n", vid_fullscreen->integer);
            return false;
        }
        mode++;
    }

    // sanity check
    if (w < VIRTUAL_SCREEN_WIDTH || w > 8192 || h < VIRTUAL_SCREEN_HEIGHT || h > 8192 || freq > 1000 || depth > 32) {
        Com_DPrintf("Mode %lux%lu@%lu:%lu doesn't look sane\n", w, h, freq, depth);
        return false;
    }

    rc->width = w;
    rc->height = h;

    if (freq_p)
        *freq_p = freq;
    if (depth_p)
        *depth_p = depth;

    return true;
}

// 640x480
// 640x480+0
// 640x480+0+0
// 640x480-100-100
bool VID_GetGeometry(vrect_t *rc)
{
    unsigned long w, h;
    long x, y;
    char *s;

    // fill in default parameters
    rc->x = 0;
    rc->y = 0;
    rc->width = 640;
    rc->height = 480;

    if (!vid_geometry)
        return false;

    s = vid_geometry->string;
    if (!*s)
        return false;

    w = strtoul(s, &s, 10);
    if (*s != 'x' && *s != 'X') {
        Com_DPrintf("Geometry string is malformed\n");
        return false;
    }
    h = strtoul(s + 1, &s, 10);
    x = y = 0;
    if (*s == '+' || *s == '-') {
        x = strtol(s, &s, 10);
        if (*s == '+' || *s == '-') {
            y = strtol(s, &s, 10);
        }
    }

    // sanity check
    if (w < VIRTUAL_SCREEN_WIDTH || w > 8192 || h < VIRTUAL_SCREEN_HEIGHT || h > 8192) {
        Com_DPrintf("Geometry %lux%lu doesn't look sane\n", w, h);
        return false;
    }

    rc->x = x;
    rc->y = y;
    rc->width = w;
    rc->height = h;

    return true;
}

void VID_SetGeometry(const vrect_t *rc)
{
    char buffer[MAX_QPATH];

    if (!vid_geometry)
        return;

    Q_snprintf(buffer, sizeof(buffer), "%dx%d%+d%+d",
               rc->width, rc->height, rc->x, rc->y);
    Cvar_SetByVar(vid_geometry, buffer, FROM_CODE);
}

void VID_ToggleFullscreen(void)
{
    if (!vid_fullscreen || !_vid_fullscreen)
        return;

    if (!vid_fullscreen->integer) {
        if (!_vid_fullscreen->integer) {
            Cvar_Set("_vid_fullscreen", "1");
        }
        Cbuf_AddText(&cmd_buffer, "set vid_fullscreen $_vid_fullscreen\n");
    } else {
        Cbuf_AddText(&cmd_buffer, "set vid_fullscreen 0\n");
    }
}

/*
==========================================================================

LOADING / SHUTDOWN

==========================================================================
*/

extern "C" {
#ifdef _WIN32
extern const vid_driver_t   vid_win32wgl;
#endif

#if USE_WIN32EGL
extern const vid_driver_t   vid_win32egl;
#endif

#if USE_WAYLAND
extern const vid_driver_t   vid_wayland;
#endif

#if USE_X11
extern const vid_driver_t   vid_x11;
#endif

#if USE_SDL
extern const vid_driver_t   vid_sdl;
#endif
}

static const vid_driver_t *const vid_drivers[] = {
#ifdef _WIN32
    &vid_win32wgl,
#endif
#if USE_WIN32EGL
    &vid_win32egl,
#endif
#if USE_WAYLAND
    &vid_wayland,
#endif
#if USE_X11
    &vid_x11,
#endif
#if USE_SDL
    &vid_sdl,
#endif
    NULL
};

#if USE_EXTERNAL_RENDERERS
static const char *R_NormalizeRendererName(const char *name)
{
    if (!name || !name[0]) {
        return "opengl";
    }

    if (!Q_strcasecmp(name, "gl")) {
        return "opengl";
    }

    if (!Q_strcasecmp(name, "vk")) {
        return "vulkan";
    }

    return name;
}

static void R_BuildRendererLibName(char *buffer, size_t size, const char *renderer_name)
{
    char product[MAX_QPATH];

    Q_strlcpy(product, PRODUCT, sizeof(product));
    Q_strlwr(product);

    if (Q_snprintf(buffer, size, "%s_%s_%s%s", product, renderer_name, CPUSTRING, LIBSUFFIX) >= size) {
        Com_Error(ERR_FATAL, "Renderer library name too long.");
    }
}

static void R_Cvar_Reset(cvar_t *var)
{
    Cvar_Reset(var);
}

static void R_Cvar_Set(const char *name, const char *value)
{
    (void)Cvar_Set(name, value);
}

static int R_Q_atoi(const char *s)
{
    return Q_atoi(s);
}

void R_ClearDebugLines(void)
{
    if (re.ClearDebugLines) {
        re.ClearDebugLines();
    }
}

void R_AddDebugLine(const vec3_t start, const vec3_t end, color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugLine) {
        re.AddDebugLine(start, end, color, time, depth_test);
    }
}

void R_AddDebugPoint(const vec3_t point, float size, color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugPoint) {
        re.AddDebugPoint(point, size, color, time, depth_test);
    }
}

void R_AddDebugAxis(const vec3_t origin, const vec3_t angles, float size, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugAxis) {
        re.AddDebugAxis(origin, angles, size, time, depth_test);
    }
}

void R_AddDebugBounds(const vec3_t mins, const vec3_t maxs, color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugBounds) {
        re.AddDebugBounds(mins, maxs, color, time, depth_test);
    }
}

void R_AddDebugSphere(const vec3_t origin, float radius, color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugSphere) {
        re.AddDebugSphere(origin, radius, color, time, depth_test);
    }
}

void R_AddDebugCircle(const vec3_t origin, float radius, color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugCircle) {
        re.AddDebugCircle(origin, radius, color, time, depth_test);
    }
}

void R_AddDebugCylinder(const vec3_t origin, float half_height, float radius, color_t color,
                        uint32_t time, qboolean depth_test)
{
    if (re.AddDebugCylinder) {
        re.AddDebugCylinder(origin, half_height, radius, color, time, depth_test);
    }
}

void R_DrawArrowCap(const vec3_t apex, const vec3_t dir, float size, color_t color,
                    uint32_t time, qboolean depth_test)
{
    if (re.DrawArrowCap) {
        re.DrawArrowCap(apex, dir, size, color, time, depth_test);
    }
}

void R_AddDebugArrow(const vec3_t start, const vec3_t end, float size, color_t line_color,
                     color_t arrow_color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugArrow) {
        re.AddDebugArrow(start, end, size, line_color, arrow_color, time, depth_test);
    }
}

void R_AddDebugCurveArrow(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                          color_t line_color, color_t arrow_color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugCurveArrow) {
        re.AddDebugCurveArrow(start, ctrl, end, size, line_color, arrow_color, time, depth_test);
    }
}

void R_AddDebugText(const vec3_t origin, const vec3_t angles, const char *text, float size,
                    color_t color, uint32_t time, qboolean depth_test)
{
    if (re.AddDebugText) {
        re.AddDebugText(origin, angles, text, size, color, time, depth_test);
    }
}

static renderer_import_t R_BuildRendererImports(void)
{
    renderer_import_t import = {
        .Com_LPrintf = Com_LPrintf,
        .Com_Error = Com_Error,
        .Com_SetLastError = Com_SetLastError,
        .Com_GetLastError = Com_GetLastError,
        .Com_MakePrintable = Com_MakePrintable,
        .Com_QueueAsyncWork = Com_QueueAsyncWork,
        .Com_WildCmpEx = Com_WildCmpEx,
        .Com_Color_g = Com_Color_g,
        .Com_PageInMemory = Com_PageInMemory,
        .Com_SlowRand = Com_SlowRand,
        .Com_HashStringLen = Com_HashStringLen,
        .Com_AddConfigFile = Com_AddConfigFile,

        .Cvar_Get = Cvar_Get,
        .Cvar_Reset = R_Cvar_Reset,
        .Cvar_Set = R_Cvar_Set,
        .Cvar_SetValue = Cvar_SetValue,
        .Cvar_SetInteger = Cvar_SetInteger,
        .Cvar_VariableInteger = Cvar_VariableInteger,
        .Cvar_VariableValue = Cvar_VariableValue,
        .Cvar_ClampValue = Cvar_ClampValue,
        .Cvar_ClampInteger = Cvar_ClampInteger,
        .Cvar_FindVar = Cvar_FindVar,

        .Cmd_AddCommand = Cmd_AddCommand,
        .Cmd_RemoveCommand = Cmd_RemoveCommand,
        .Cmd_AddMacro = Cmd_AddMacro,
        .Cmd_Register = Cmd_Register,
        .Cmd_Deregister = Cmd_Deregister,
        .Cmd_Argc = Cmd_Argc,
        .Cmd_Argv = Cmd_Argv,
        .Cmd_ParseOptions = Cmd_ParseOptions,
        .Cmd_PrintHelp = Cmd_PrintHelp,
        .Cmd_PrintUsage = Cmd_PrintUsage,
        .Cmd_Option_c = Cmd_Option_c,

        .FS_LoadFileEx = FS_LoadFileEx,
        .FS_OpenFile = FS_OpenFile,
        .FS_EasyOpenFile = FS_EasyOpenFile,
        .FS_Read = FS_Read,
        .FS_CloseFile = FS_CloseFile,
        .FS_FPrintf = FS_FPrintf,
        .FS_Length = FS_Length,
        .FS_ListFiles = FS_ListFiles,
        .FS_FreeList = FS_FreeList,
        .FS_LastModified = FS_LastModified,
        .FS_NormalizePathBuffer = FS_NormalizePathBuffer,
        .FS_CleanupPath = FS_CleanupPath,
        .FS_CreatePath = FS_CreatePath,

        .Z_TagMalloc = Z_TagMalloc,
        .Z_TagMallocz = Z_TagMallocz,
        .Z_TagCopyString = Z_TagCopyString,
        .Z_Malloc = Z_Malloc,
        .Z_Mallocz = Z_Mallocz,
        .Z_Free = Z_Free,

        .Hunk_Begin = Hunk_Begin,
        .Hunk_TryAlloc = Hunk_TryAlloc,
        .Hunk_Free = Hunk_Free,
        .Hunk_End = Hunk_End,
        .Hunk_FreeToWatermark = Hunk_FreeToWatermark,

        .HashMap_CreateImpl = HashMap_CreateImpl,
        .HashMap_Destroy = HashMap_Destroy,
        .HashMap_Reserve = HashMap_Reserve,
        .HashMap_InsertImpl = HashMap_InsertImpl,
        .HashMap_LookupImpl = HashMap_LookupImpl,
        .HashMap_GetKeyImpl = HashMap_GetKeyImpl,
        .HashMap_GetValueImpl = HashMap_GetValueImpl,
        .HashMap_Size = HashMap_Size,

        .BSP_Load = BSP_Load,
        .BSP_Free = BSP_Free,
        .BSP_ErrorString = BSP_ErrorString,
        .BSP_LightPoint = BSP_LightPoint,
        .BSP_TransformedLightPoint = BSP_TransformedLightPoint,
        .BSP_LookupLightgrid = BSP_LookupLightgrid,
        .BSP_ClusterVis = BSP_ClusterVis,
        .BSP_PointLeaf = BSP_PointLeaf,
        .BSP_GetPvs = BSP_GetPvs,
        .BSP_GetPvs2 = BSP_GetPvs2,
        .BSP_SavePatchedPVS = BSP_SavePatchedPVS,

        .Prompt_AddMatch = Prompt_AddMatch,

        .SCR_DrawStats = SCR_DrawStats,
        .SCR_RegisterStat = SCR_RegisterStat,
        .SCR_UnregisterStat = SCR_UnregisterStat,
        .SCR_StatActive = SCR_StatActive,
        .SCR_StatKeyValue = SCR_StatKeyValue,
        .SCR_ParseColor = SCR_ParseColor,

        .CL_SetSky = CL_SetSky,

        .Key_IsDown = Key_IsDown,
        .Key_GetBindingForKey = Key_GetBindingForKey,

        .COM_ParseEx = COM_ParseEx,
        .COM_ParseToken = COM_ParseToken,
        .COM_StripExtension = COM_StripExtension,
        .COM_DefaultExtension = COM_DefaultExtension,
        .COM_FileExtension = COM_FileExtension,
        .COM_SplitPath = COM_SplitPath,

        .mdfour_begin = mdfour_begin,
        .mdfour_update = mdfour_update,
        .mdfour_result = mdfour_result,

        .jsmn_init = jsmn_init,
        .jsmn_parse = jsmn_parse,

        .V_CalcFov = V_CalcFov,

        .AngleVectors = AngleVectors,
        .VectorNormalize = VectorNormalize,
        .VectorNormalize2 = VectorNormalize2,
        .ClearBounds = ClearBounds,
        .UnionBounds = UnionBounds,
        .RadiusFromBounds = RadiusFromBounds,

        .vectoangles2 = vectoangles2,
        .MakeNormalVectors = MakeNormalVectors,
        .SetPlaneSignbits = SetPlaneSignbits,
        .BoxOnPlaneSide = BoxOnPlaneSide,
        .SetupRotationMatrix = SetupRotationMatrix,
        .RotatePointAroundVector = RotatePointAroundVector,
        .Matrix_Frustum = Matrix_Frustum,
        .Matrix_FromOriginAxis = Matrix_FromOriginAxis,
#if USE_MD5
        .Quat_ComputeW = Quat_ComputeW,
        .Quat_SLerp = Quat_SLerp,
        .Quat_Normalize = Quat_Normalize,
        .Quat_MultiplyQuat = Quat_MultiplyQuat,
        .Quat_Conjugate = Quat_Conjugate,
        .Quat_RotatePoint = Quat_RotatePoint,
        .Quat_ToAxis = Quat_ToAxis,
#endif

        .SZ_Init = SZ_Init,
        .SZ_InitRead = SZ_InitRead,
        .SZ_ReadData = SZ_ReadData,
        .SZ_ReadByte = SZ_ReadByte,
        .SZ_ReadWord = SZ_ReadWord,
        .SZ_Clear = SZ_Clear,
        .SZ_GetSpace = SZ_GetSpace,

        .strnatcasecmp = strnatcasecmp,
        .strnatcasencmp = strnatcasencmp,

#ifdef HAVE_MEMCCPY
        .Q_memccpy = memccpy,
#else
        .Q_memccpy = Q_memccpy,
#endif
        .Q_atoi = R_Q_atoi,

        .Q_strcasecmp = Q_strcasecmp,
        .Q_strncasecmp = Q_strncasecmp,
        .Q_strcasestr = Q_strcasestr,
        .Q_strchrnul = Q_strchrnul,
        .Q_strlcpy = Q_strlcpy,
        .Q_strlcat = Q_strlcat,
        .Q_concat_array = Q_concat_array,
        .Q_vsnprintf = Q_vsnprintf,
        .Q_snprintf = Q_snprintf,
        .Q_fopen = Q_fopen,
        .Q_ErrorString = Q_ErrorString,
        .va = va,

        .fs_gamedir = fs_gamedir,
        .fs_game = fs_game,
        .vid = &vid,
        .com_eventTime = &com_eventTime,
        .Sys_Milliseconds = Sys_Milliseconds,
        .com_linenum = &com_linenum,
        .com_env_suf = com_env_suf,
        .cmd_optind = &cmd_optind,
        .bytedirs = bytedirs,
        .cl_async = cl_async,
        .cl_gunfov = cl_gunfov,
        .cl_adjustfov = cl_adjustfov,
        .cl_gun = cl_gun,
        .info_hand = info_hand,
#if USE_DEBUG
        .developer = developer,
#else
        .developer = NULL,
#endif
    };

    return import;
}

static void R_UnloadExternalRenderer(void);

static bool R_UseRendererAPI(const char *renderer_name)
{
    renderer_import_t import = R_BuildRendererImports();
    const renderer_export_t *exports = renderer_get_api(&import);

    if (!exports) {
        Com_SetLastError("Renderer_GetAPI returned no exports");
        R_UnloadExternalRenderer();
        return false;
    }

    re = *exports;
    Com_Printf("Loaded renderer '%s'.\n", renderer_name);
    return true;
}

static bool R_LoadExternalRenderer(const char *renderer_name)
{
    char libname[MAX_QPATH];
    char path[MAX_OSPATH];
    bool attempted = false;
    const char *search_dirs[] = {
        sys_libdir ? sys_libdir->string : "",
        sys_basedir ? sys_basedir->string : "",
        NULL
    };

    R_BuildRendererLibName(libname, sizeof(libname), renderer_name);

    for (int i = 0; search_dirs[i]; i++) {
        if (!search_dirs[i][0]) {
            continue;
        }

        if (Q_concat(path, sizeof(path), search_dirs[i], PATH_SEP_STRING, libname) >= sizeof(path)) {
            continue;
        }

        if (os_access(path, X_OK)) {
            continue;
        }

        renderer_get_api = (const renderer_export_t *(*)(const renderer_import_t *))
            Sys_LoadLibrary(path, "Renderer_GetAPI", &renderer_handle);
        attempted |= renderer_get_api != NULL;
        if (renderer_get_api && R_UseRendererAPI(renderer_name)) {
            return true;
        }
    }

    renderer_get_api = (const renderer_export_t *(*)(const renderer_import_t *))
        Sys_LoadLibrary(libname, "Renderer_GetAPI", &renderer_handle);
    attempted |= renderer_get_api != NULL;
    if (renderer_get_api && R_UseRendererAPI(renderer_name)) {
        return true;
    }

    if (!attempted) {
        Com_SetLastError(va("Renderer library '%s' not found", libname));
    }
    return false;
}

static void R_UnloadExternalRenderer(void)
{
    if (renderer_handle) {
        Sys_FreeLibrary(renderer_handle);
        renderer_handle = NULL;
    }
    renderer_get_api = NULL;
    memset(&re, 0, sizeof(re));
    re.ClampScale = R_ClampScaleStub;
    re.Config = &renderer_stub_config;
}

static void r_renderer_g(genctx_t *ctx)
{
    Prompt_AddMatch(ctx, "opengl");
#if USE_VULKAN
    Prompt_AddMatch(ctx, "vulkan");
#endif
}
#endif

/*
============
CL_RunRenderer
============
*/
void CL_RunRenderer(void)
{
    if (!cls.ref_initialized) {
        return;
    }

    vid->pump_events();

    if (mode_changed) {
        if (mode_changed & MODE_FULLSCREEN) {
            vid->set_mode();
            if (vid_fullscreen->integer) {
                Cvar_Set("_vid_fullscreen", vid_fullscreen->string);
            }
        } else {
            if (vid_fullscreen->integer) {
                if (mode_changed & MODE_MODELIST) {
                    vid->set_mode();
                }
            } else {
                if (mode_changed & MODE_GEOMETRY) {
                    vid->set_mode();
                }
            }
        }
        mode_changed = 0;
    }

    if (cvar_modified & CVAR_RENDERER) {
        CL_RestartRenderer(true);
        cvar_modified &= ~CVAR_RENDERER;
    } else if (cvar_modified & CVAR_FILES) {
        CL_RestartRenderer(false);
        cvar_modified &= ~CVAR_FILES;
    }
}

static void vid_geometry_changed(cvar_t *self)
{
    mode_changed |= MODE_GEOMETRY;
}

static void vid_fullscreen_changed(cvar_t *self)
{
    mode_changed |= MODE_FULLSCREEN;
}

static void vid_modelist_changed(cvar_t *self)
{
    mode_changed |= MODE_MODELIST;
}

static void vid_driver_g(genctx_t *ctx)
{
    for (int i = 0; vid_drivers[i]; i++)
        Prompt_AddMatch(ctx, vid_drivers[i]->name);
}

/*
============
CL_InitRenderer
============
*/
void CL_InitRenderer(void)
{
    char *modelist;
    int i;

    if (cls.ref_initialized) {
        return;
    }

#if USE_EXTERNAL_RENDERERS
    r_renderer = Cvar_Get("r_renderer", "opengl", CVAR_RENDERER);
    r_renderer->generator = r_renderer_g;
    const char *renderer_name = R_NormalizeRendererName(r_renderer->string);
    if (!R_LoadExternalRenderer(renderer_name)) {
        Com_Error(ERR_FATAL, "Couldn't load renderer '%s': %s", renderer_name, Com_GetLastError());
    }
    Cvar_Get("vid_ref", renderer_name, CVAR_ROM);
#else
    Cvar_Get("vid_ref", "opengl", CVAR_ROM);
#endif

    // Create the video variables so we know how to start the graphics drivers
    cvar_t *vid_driver = Cvar_Get("vid_driver", "", CVAR_RENDERER);
    vid_driver->generator = vid_driver_g;
    vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
    _vid_fullscreen = Cvar_Get("_vid_fullscreen", "1", CVAR_ARCHIVE);
    vid_geometry = Cvar_Get("vid_geometry", VID_GEOMETRY, CVAR_ARCHIVE);

    if (vid_fullscreen->integer) {
        Cvar_Set("_vid_fullscreen", vid_fullscreen->string);
    } else if (!_vid_fullscreen->integer) {
        Cvar_Set("_vid_fullscreen", "1");
    }

    Com_SetLastError("No available video driver");

    // Try to initialize selected driver first
    bool ok = false;
    for (i = 0; vid_drivers[i]; i++) {
        if (!strcmp(vid_drivers[i]->name, vid_driver->string)) {
            vid = vid_drivers[i];
            ok = R_Init(true);
            break;
        }
    }

    if (!vid_drivers[i] && vid_driver->string[0]) {
        Com_Printf("No such video driver: %s.\n"
                   "Available video drivers: ", vid_driver->string);
        for (int j = 0; vid_drivers[j]; j++) {
            if (j)
                Com_Printf(", ");
            Com_Printf("%s", vid_drivers[j]->name);
        }
        Com_Printf(".\n");
    }

    // Fall back to other available drivers
    if (!ok) {
        int tried = i;
        for (i = 0; vid_drivers[i]; i++) {
            if (i == tried || !vid_drivers[i]->probe || !vid_drivers[i]->probe())
                continue;
            vid = vid_drivers[i];
            if ((ok = R_Init(true)))
                break;
        }
        Cvar_Reset(vid_driver);
    }

    if (!ok)
        Com_Error(ERR_FATAL, "Couldn't initialize renderer: %s", Com_GetLastError());

    modelist = vid->get_mode_list();
    vid_modelist = Cvar_Get("vid_modelist", modelist, 0);
    Z_Free(modelist);

    vid->set_mode();

    cls.ref_initialized = true;

    vid_geometry->changed = vid_geometry_changed;
    vid_fullscreen->changed = vid_fullscreen_changed;
    vid_modelist->changed = vid_modelist_changed;

    mode_changed = 0;

    // Initialize the rest of graphics subsystems
    V_Init();
    SCR_Init();
    CG_Init();
    UI_Init();

    SCR_RegisterMedia();
    Con_RegisterMedia();

    cvar_modified &= ~(CVAR_FILES | CVAR_RENDERER);
}

/*
============
CL_ShutdownRenderer
============
*/
void CL_ShutdownRenderer(void)
{
    if (!cls.ref_initialized) {
        return;
    }

    // Shutdown the rest of graphics subsystems
    V_Shutdown();
    SCR_Shutdown();
    UI_Shutdown();

    vid_geometry->changed = NULL;
    vid_fullscreen->changed = NULL;
    vid_modelist->changed = NULL;

    R_Shutdown(true);
#if USE_EXTERNAL_RENDERERS
    R_UnloadExternalRenderer();
#endif

    vid = NULL;

    cls.ref_initialized = false;

    // no longer active
    cls.active = ACT_MINIMIZED;

    Z_LeakTest(TAG_RENDERER);
}
