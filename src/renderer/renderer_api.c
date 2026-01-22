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

#include "renderer/renderer_api.h"

renderer_import_t ri;
extern uint32_t d_8to24table[256];

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
    ri.AngleVectors(angles, forward, right, up);
}

box_plane_t BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_t *p)
{
    return ri.BoxOnPlaneSide(emins, emaxs, p);
}

void *SZ_GetSpace(sizebuf_t *buf, size_t len)
{
    return ri.SZ_GetSpace(buf, len);
}

static const renderer_export_t renderer_exports = {
    .Init                   = R_Init,
    .Shutdown               = R_Shutdown,
    .BeginRegistration      = R_BeginRegistration,
    .RegisterModel          = R_RegisterModel,
    .RegisterImage          = R_RegisterImage,
    .RegisterRawImage       = R_RegisterRawImage,
    .UnregisterImage        = R_UnregisterImage,
    .SetSky                 = R_SetSky,
    .EndRegistration        = R_EndRegistration,
    .RenderFrame            = R_RenderFrame,
    .LightPoint             = R_LightPoint,
    .SetClipRect            = R_SetClipRect,
    .ClampScale             = R_ClampScale,
    .SetScale               = R_SetScale,
    .DrawChar               = R_DrawChar,
    .DrawStretchChar        = R_DrawStretchChar,
    .DrawStringStretch      = R_DrawStringStretch,
    .KFontLookup            = SCR_KFontLookup,
    .LoadKFont              = SCR_LoadKFont,
    .DrawKFontChar          = R_DrawKFontChar,
    .GetPicSize             = R_GetPicSize,
    .DrawPic                = R_DrawPic,
    .DrawStretchPic         = R_DrawStretchPic,
    .DrawStretchSubPic      = R_DrawStretchSubPic,
    .DrawStretchRotatePic   = R_DrawStretchRotatePic,
    .DrawKeepAspectPic      = R_DrawKeepAspectPic,
    .DrawStretchRaw         = R_DrawStretchRaw,
    .UpdateRawPic           = R_UpdateRawPic,
    .UpdateImageRGBA        = R_UpdateImageRGBA,
    .TileClear              = R_TileClear,
    .DrawFill8              = R_DrawFill8,
    .DrawFill32             = R_DrawFill32,
    .BeginFrame             = R_BeginFrame,
    .EndFrame               = R_EndFrame,
    .ModeChanged            = R_ModeChanged,
    .VideoSync              = R_VideoSync,
    .ExpireDebugObjects     = GL_ExpireDebugObjects,
    .SupportsPerPixelLighting = R_SupportsPerPixelLighting,
    .GetGLConfig            = R_GetGLConfig,
    .ClearDebugLines        = R_ClearDebugLines,
    .AddDebugLine           = R_AddDebugLine,
    .AddDebugPoint          = R_AddDebugPoint,
    .AddDebugAxis           = R_AddDebugAxis,
    .AddDebugBounds         = R_AddDebugBounds,
    .AddDebugSphere         = R_AddDebugSphere,
    .AddDebugCircle         = R_AddDebugCircle,
    .AddDebugCylinder       = R_AddDebugCylinder,
    .DrawArrowCap           = R_DrawArrowCap,
    .AddDebugArrow          = R_AddDebugArrow,
    .AddDebugCurveArrow     = R_AddDebugCurveArrow,
    .AddDebugText           = R_AddDebugText,
    .PaletteTable           = d_8to24table,
    .Config                 = &r_config,
};

RENDERER_API const renderer_export_t *Renderer_GetAPI(const renderer_import_t *import)
{
    if (!import) {
        return NULL;
    }

    ri = *import;
    return &renderer_exports;
}
