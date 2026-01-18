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

#include <stdio.h>

#include "shared/shared.h"
#include "common/async.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/hash_map.h"
#include "common/math.h"
#include "common/mdfour.h"
#include "common/prompt.h"
#include "common/sizebuf.h"
#include "common/utils.h"
#ifdef JSMN_HEADER
#include "common/jsmn.h"
#else
#define JSMN_HEADER
#include "common/jsmn.h"
#undef JSMN_HEADER
#endif
#include "system/hunk.h"
#include "client/video.h"
#include "client/client.h"
#include "renderer/renderer.h"

#if defined(_WIN32) && defined(RENDERER_DLL)
#define RENDERER_API __declspec(dllexport)
#elif defined(RENDERER_DLL)
#define RENDERER_API __attribute__((visibility("default")))
#else
#define RENDERER_API
#endif

#ifdef __cplusplus
#ifndef restrict
#define restrict __restrict
#define Q_RESTRICT_UNDEF_RENDERER_API
#endif
#endif

typedef struct renderer_import_s {
    void (*Com_LPrintf)(print_type_t type, const char *fmt, ...);
    void (*Com_Error)(error_type_t type, const char *fmt, ...);
    void (*Com_SetLastError)(const char *msg);
    const char *(*Com_GetLastError)(void);
    char *(*Com_MakePrintable)(const char *s);
    void (*Com_QueueAsyncWork)(asyncwork_t *work);
    bool (*Com_WildCmpEx)(const char *filter, const char *string, int term, bool ignorecase);
    void (*Com_Color_g)(genctx_t *ctx);
    void (*Com_PageInMemory)(void *buffer, size_t size);
    uint32_t (*Com_SlowRand)(void);
    unsigned (*Com_HashStringLen)(const char *s, size_t len, unsigned size);

    cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags);
    void (*Cvar_Reset)(cvar_t *var);
    void (*Cvar_Set)(const char *name, const char *value);
    int (*Cvar_VariableInteger)(const char *name);
    float (*Cvar_VariableValue)(const char *name);
    float (*Cvar_ClampValue)(cvar_t *var, float min, float max);
    int (*Cvar_ClampInteger)(cvar_t *var, int min, int max);
    cvar_t *(*Cvar_FindVar)(const char *name);

    void (*Cmd_AddCommand)(const char *cmd_name, xcommand_t function);
    void (*Cmd_RemoveCommand)(const char *cmd_name);
    void (*Cmd_AddMacro)(const char *name, xmacro_t function);
    void (*Cmd_Register)(const cmdreg_t *reg);
    void (*Cmd_Deregister)(const cmdreg_t *reg);
    int (*Cmd_Argc)(void);
    char *(*Cmd_Argv)(int arg);
    int (*Cmd_ParseOptions)(const cmd_option_t *opt);
    void (*Cmd_PrintHelp)(const cmd_option_t *opt);
    void (*Cmd_PrintUsage)(const cmd_option_t *opt, const char *suffix);
    void (*Cmd_Option_c)(const cmd_option_t *opt, xgenerator_t g, genctx_t *ctx, int argnum);

    int (*FS_LoadFileEx)(const char *path, void **buffer, unsigned flags, memtag_t tag);
    int64_t (*FS_OpenFile)(const char *filename, qhandle_t *f, unsigned mode);
    int (*FS_Read)(void *buffer, size_t len, qhandle_t f);
    int (*FS_CloseFile)(qhandle_t f);
    size_t (*FS_NormalizePathBuffer)(char *out, const char *in, size_t size);
    void (*FS_CleanupPath)(char *s);
    int (*FS_CreatePath)(char *path);

    void *(*Z_TagMalloc)(size_t size, memtag_t tag);
    void *(*Z_TagMallocz)(size_t size, memtag_t tag);
    char *(*Z_TagCopyString)(const char *in, memtag_t tag);
    void *(*Z_Malloc)(size_t size);
    void *(*Z_Mallocz)(size_t size);
    void (*Z_Free)(void *ptr);

    void (*Hunk_Begin)(memhunk_t *hunk, size_t maxsize);
    void *(*Hunk_TryAlloc)(memhunk_t *hunk, size_t size, size_t align);
    void (*Hunk_Free)(memhunk_t *hunk);
    void (*Hunk_End)(memhunk_t *hunk);
    void (*Hunk_FreeToWatermark)(memhunk_t *hunk, size_t size);

    hash_map_t *(*HashMap_CreateImpl)(uint32_t key_size, uint32_t value_size,
                                      uint32_t (*hasher)(const void *),
                                      bool (*comp)(const void *, const void *),
                                      memtag_t tag);
    void (*HashMap_Destroy)(hash_map_t *map);
    void (*HashMap_Reserve)(hash_map_t *map, uint32_t capacity);
    bool (*HashMap_InsertImpl)(hash_map_t *map, uint32_t key_size, uint32_t value_size,
                               const void *key, const void *value);
    void *(*HashMap_LookupImpl)(const hash_map_t *map, uint32_t key_size, const void *key);
    void *(*HashMap_GetKeyImpl)(const hash_map_t *map, uint32_t index);
    void *(*HashMap_GetValueImpl)(const hash_map_t *map, uint32_t index);
    uint32_t (*HashMap_Size)(const hash_map_t *map);

    int (*BSP_Load)(const char *name, bsp_t **bsp_p);
    void (*BSP_Free)(bsp_t *bsp);
    const char *(*BSP_ErrorString)(int err);
    void (*BSP_LightPoint)(lightpoint_t *point, const vec3_t start, const vec3_t end,
                           const mnode_t *headnode, int nolm_mask);
    void (*BSP_TransformedLightPoint)(lightpoint_t *point, const vec3_t start, const vec3_t end,
                                      const mnode_t *headnode, int nolm_mask,
                                      const vec3_t origin, const vec3_t angles);
    const lightgrid_sample_t *(*BSP_LookupLightgrid)(const lightgrid_t *grid, const uint32_t point[3]);
    void (*BSP_ClusterVis)(const bsp_t *bsp, visrow_t *mask, int cluster, int vis);
    const mleaf_t *(*BSP_PointLeaf)(const mnode_t *node, const vec3_t p);

    void (*Prompt_AddMatch)(genctx_t *ctx, const char *s);

    void (*SCR_DrawStats)(void);
    void (*SCR_RegisterStat)(const char *name, xcommand_t cb);
    void (*SCR_UnregisterStat)(const char *name);
    bool (*SCR_StatActive)(void);
    void (*SCR_StatKeyValue)(const char *key, const char *value);
    bool (*SCR_ParseColor)(const char *s, color_t *color);

    void (*CL_SetSky)(void);

    char *(*COM_ParseEx)(const char **data_p, int flags);
    size_t (*COM_ParseToken)(const char **data_p, char *buffer, size_t size, int flags);
    size_t (*COM_StripExtension)(char *out, const char *in, size_t size);
    size_t (*COM_DefaultExtension)(char *path, const char *ext, size_t size);
    char *(*COM_FileExtension)(const char *in);
    void (*COM_SplitPath)(const char *in, char *name, size_t name_size,
                          char *path, size_t path_size, bool strip_ext);

    void (*mdfour_begin)(mdfour_t *md);
    void (*mdfour_update)(mdfour_t *md, const uint8_t *in, size_t n);
    void (*mdfour_result)(mdfour_t *md, uint8_t *out);

    void (*jsmn_init)(jsmn_parser *parser);
    int (*jsmn_parse)(jsmn_parser *parser, const char *js, size_t len,
                      jsmntok_t *tokens, const unsigned int num_tokens);

    float (*V_CalcFov)(float fov_x, float width, float height);

    void (*AngleVectors)(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
    vec_t (*VectorNormalize)(vec3_t v);
    vec_t (*VectorNormalize2)(const vec3_t v, vec3_t out);
    void (*ClearBounds)(vec3_t mins, vec3_t maxs);
    void (*UnionBounds)(const vec3_t a[2], const vec3_t b[2], vec3_t c[2]);
    vec_t (*RadiusFromBounds)(const vec3_t mins, const vec3_t maxs);

    void (*vectoangles2)(const vec3_t value1, vec3_t angles);
    void (*MakeNormalVectors)(const vec3_t forward, vec3_t right, vec3_t up);
    void (*SetPlaneSignbits)(cplane_t *plane);
    box_plane_t (*BoxOnPlaneSide)(const vec3_t emins, const vec3_t emaxs, const cplane_t *p);
    void (*SetupRotationMatrix)(vec3_t matrix[3], const vec3_t dir, float degrees);
    void (*RotatePointAroundVector)(vec3_t out, const vec3_t dir, const vec3_t in, float degrees);
    void (*Matrix_Frustum)(float fov_x, float fov_y, float reflect_x, float znear, float zfar, float *matrix);
    void (*Matrix_FromOriginAxis)(const vec3_t origin, const vec3_t axis[3], mat4_t out);
#if USE_MD5
    void (*Quat_ComputeW)(quat_t q);
    void (*Quat_SLerp)(const quat_t qa, const quat_t qb, float backlerp, float frontlerp, quat_t out);
    float (*Quat_Normalize)(quat_t q);
    void (*Quat_MultiplyQuat)(const float *restrict qa, const float *restrict qb, quat_t out);
    void (*Quat_Conjugate)(const quat_t in, quat_t out);
    void (*Quat_RotatePoint)(const quat_t q, const vec3_t in, vec3_t out);
    void (*Quat_ToAxis)(const quat_t q, vec3_t axis[3]);
#endif

    void (*SZ_Init)(sizebuf_t *buf, void *data, size_t size, const char *tag);
    void (*SZ_InitRead)(sizebuf_t *buf, const void *data, size_t size);
    void *(*SZ_ReadData)(sizebuf_t *buf, size_t len);
    int (*SZ_ReadByte)(sizebuf_t *sb);
    int (*SZ_ReadWord)(sizebuf_t *sb);
    void (*SZ_Clear)(sizebuf_t *buf);
    void *(*SZ_GetSpace)(sizebuf_t *buf, size_t len);

    int (*strnatcasecmp)(const char *s1, const char *s2);
    int (*strnatcasencmp)(const char *s1, const char *s2, size_t n);

    void *(*Q_memccpy)(void *dst, const void *src, int c, size_t size);
    int (*Q_atoi)(const char *s);

    int (*Q_strcasecmp)(const char *s1, const char *s2);
    int (*Q_strncasecmp)(const char *s1, const char *s2, size_t n);
    char *(*Q_strcasestr)(const char *s1, const char *s2);
    char *(*Q_strchrnul)(const char *s, int c);
    size_t (*Q_strlcpy)(char *dst, const char *src, size_t size);
    size_t (*Q_strlcat)(char *dst, const char *src, size_t size);
    size_t (*Q_concat_array)(char *dest, size_t size, const char **arr);
    size_t (*Q_vsnprintf)(char *dest, size_t size, const char *fmt, va_list argptr);
    size_t (*Q_snprintf)(char *dest, size_t size, const char *fmt, ...);
    FILE *(*Q_fopen)(const char *path, const char *mode);
    const char *(*Q_ErrorString)(int err);
    char *(*va)(const char *fmt, ...);

    char *fs_gamedir;
    const vid_driver_t **vid;
    unsigned *com_eventTime;
    unsigned (*Sys_Milliseconds)(void);
    unsigned *com_linenum;
    const char (*com_env_suf)[3];
    int *cmd_optind;
    const vec3_t *bytedirs;
    cvar_t *cl_async;
    cvar_t *cl_gunfov;
    cvar_t *cl_adjustfov;
    cvar_t *cl_gun;
    cvar_t *info_hand;
    cvar_t *developer;
} renderer_import_t;

#ifdef __cplusplus
#ifdef Q_RESTRICT_UNDEF_RENDERER_API
#undef restrict
#undef Q_RESTRICT_UNDEF_RENDERER_API
#endif
#endif

RENDERER_API const renderer_export_t *Renderer_GetAPI(const renderer_import_t *import);

#if defined(RENDERER_DLL)
extern renderer_import_t ri;

#define Com_LPrintf ri.Com_LPrintf
#define Com_Error ri.Com_Error
#define Com_SetLastError ri.Com_SetLastError
#define Com_GetLastError ri.Com_GetLastError
#define Com_MakePrintable ri.Com_MakePrintable
#define Com_QueueAsyncWork ri.Com_QueueAsyncWork
#define Com_WildCmpEx ri.Com_WildCmpEx
#define Com_Color_g ri.Com_Color_g
#define Com_PageInMemory ri.Com_PageInMemory
#define Com_SlowRand ri.Com_SlowRand
#define Com_HashStringLen ri.Com_HashStringLen
#define Sys_Milliseconds ri.Sys_Milliseconds

#define Cvar_Get ri.Cvar_Get
#ifdef Cvar_Reset
#undef Cvar_Reset
#endif
#define Cvar_Reset ri.Cvar_Reset
#define Cvar_Set ri.Cvar_Set
#define Cvar_SetByVar(var, value, from) Cvar_Set((var)->name, (value))
#define Cvar_VariableInteger ri.Cvar_VariableInteger
#define Cvar_VariableValue ri.Cvar_VariableValue
#define Cvar_ClampValue ri.Cvar_ClampValue
#define Cvar_ClampInteger ri.Cvar_ClampInteger
#define Cvar_FindVar ri.Cvar_FindVar

#define Cmd_AddCommand ri.Cmd_AddCommand
#define Cmd_RemoveCommand ri.Cmd_RemoveCommand
#define Cmd_AddMacro ri.Cmd_AddMacro
#define Cmd_Register ri.Cmd_Register
#define Cmd_Deregister ri.Cmd_Deregister
#define Cmd_Argc ri.Cmd_Argc
#define Cmd_Argv ri.Cmd_Argv
#define Cmd_ParseOptions ri.Cmd_ParseOptions
#define Cmd_PrintHelp ri.Cmd_PrintHelp
#define Cmd_PrintUsage ri.Cmd_PrintUsage
#define Cmd_Option_c ri.Cmd_Option_c

#define FS_LoadFileEx ri.FS_LoadFileEx
#define FS_OpenFile ri.FS_OpenFile
#define FS_Read ri.FS_Read
#define FS_CloseFile ri.FS_CloseFile
#define FS_NormalizePathBuffer ri.FS_NormalizePathBuffer
#define FS_CleanupPath ri.FS_CleanupPath
#define FS_CreatePath ri.FS_CreatePath

#define Z_TagMalloc ri.Z_TagMalloc
#define Z_TagMallocz ri.Z_TagMallocz
#define Z_TagCopyString ri.Z_TagCopyString
#define Z_Malloc ri.Z_Malloc
#define Z_Mallocz ri.Z_Mallocz
#define Z_Free ri.Z_Free

#define Hunk_Begin ri.Hunk_Begin
#define Hunk_TryAlloc ri.Hunk_TryAlloc
#define Hunk_Free ri.Hunk_Free
#define Hunk_End ri.Hunk_End
#define Hunk_FreeToWatermark ri.Hunk_FreeToWatermark

#define HashMap_CreateImpl ri.HashMap_CreateImpl
#define HashMap_Destroy ri.HashMap_Destroy
#define HashMap_Reserve ri.HashMap_Reserve
#define HashMap_InsertImpl ri.HashMap_InsertImpl
#define HashMap_LookupImpl ri.HashMap_LookupImpl
#define HashMap_GetKeyImpl ri.HashMap_GetKeyImpl
#define HashMap_GetValueImpl ri.HashMap_GetValueImpl
#define HashMap_Size ri.HashMap_Size

#define BSP_Load ri.BSP_Load
#define BSP_Free ri.BSP_Free
#define BSP_ErrorString ri.BSP_ErrorString
#define BSP_LightPoint ri.BSP_LightPoint
#define BSP_TransformedLightPoint ri.BSP_TransformedLightPoint
#define BSP_LookupLightgrid ri.BSP_LookupLightgrid
#define BSP_ClusterVis ri.BSP_ClusterVis
#define BSP_PointLeaf ri.BSP_PointLeaf

#define Prompt_AddMatch ri.Prompt_AddMatch

#define SCR_DrawStats ri.SCR_DrawStats
#define SCR_RegisterStat ri.SCR_RegisterStat
#define SCR_UnregisterStat ri.SCR_UnregisterStat
#define SCR_StatActive ri.SCR_StatActive
#define SCR_StatKeyValue ri.SCR_StatKeyValue
#define SCR_ParseColor ri.SCR_ParseColor

#define CL_SetSky ri.CL_SetSky

#define COM_ParseEx ri.COM_ParseEx
#define COM_ParseToken ri.COM_ParseToken
#define COM_StripExtension ri.COM_StripExtension
#define COM_DefaultExtension ri.COM_DefaultExtension
#define COM_FileExtension ri.COM_FileExtension
#define COM_SplitPath ri.COM_SplitPath

#define mdfour_begin ri.mdfour_begin
#define mdfour_update ri.mdfour_update
#define mdfour_result ri.mdfour_result

#define jsmn_init ri.jsmn_init
#define jsmn_parse ri.jsmn_parse

#define V_CalcFov ri.V_CalcFov

#define VectorNormalize ri.VectorNormalize
#define VectorNormalize2 ri.VectorNormalize2
#define ClearBounds ri.ClearBounds
#define UnionBounds ri.UnionBounds
#define RadiusFromBounds ri.RadiusFromBounds

#define vectoangles2 ri.vectoangles2
#define MakeNormalVectors ri.MakeNormalVectors
#define SetPlaneSignbits ri.SetPlaneSignbits
#define SetupRotationMatrix ri.SetupRotationMatrix
#define RotatePointAroundVector ri.RotatePointAroundVector
#define Matrix_Frustum ri.Matrix_Frustum
#define Matrix_FromOriginAxis ri.Matrix_FromOriginAxis
#if USE_MD5
#define Quat_ComputeW ri.Quat_ComputeW
#define Quat_SLerp ri.Quat_SLerp
#define Quat_Normalize ri.Quat_Normalize
#define Quat_MultiplyQuat ri.Quat_MultiplyQuat
#define Quat_Conjugate ri.Quat_Conjugate
#define Quat_RotatePoint ri.Quat_RotatePoint
#define Quat_ToAxis ri.Quat_ToAxis
#endif

#define SZ_Init ri.SZ_Init
#define SZ_InitRead ri.SZ_InitRead
#define SZ_ReadData ri.SZ_ReadData
#define SZ_ReadByte ri.SZ_ReadByte
#define SZ_ReadWord ri.SZ_ReadWord
#define SZ_Clear ri.SZ_Clear

#define strnatcasecmp ri.strnatcasecmp
#define strnatcasencmp ri.strnatcasencmp

#undef Q_memccpy
#undef Q_atoi
#undef Q_strcasecmp
#undef Q_strncasecmp
#undef Q_strcasestr
#undef Q_strchrnul

#define Q_memccpy ri.Q_memccpy
#define Q_atoi ri.Q_atoi
#define Q_strcasecmp ri.Q_strcasecmp
#define Q_strncasecmp ri.Q_strncasecmp
#define Q_strcasestr ri.Q_strcasestr
#define Q_strchrnul ri.Q_strchrnul
#define Q_strlcpy ri.Q_strlcpy
#define Q_strlcat ri.Q_strlcat
#define Q_concat_array ri.Q_concat_array
#define Q_vsnprintf ri.Q_vsnprintf
#define Q_snprintf ri.Q_snprintf
#define Q_fopen ri.Q_fopen
#define Q_ErrorString ri.Q_ErrorString
#define va ri.va

#define fs_gamedir ri.fs_gamedir
#define vid (*ri.vid)
#define com_eventTime (*ri.com_eventTime)
#define com_linenum (*ri.com_linenum)
#define com_env_suf ri.com_env_suf
#define cmd_optind (*ri.cmd_optind)
#define bytedirs ri.bytedirs
#define cl_async ri.cl_async
#define cl_gunfov ri.cl_gunfov
#define cl_adjustfov ri.cl_adjustfov
#define cl_gun ri.cl_gun
#define info_hand ri.info_hand
#if USE_DEBUG
#define developer ri.developer
#endif
#endif
