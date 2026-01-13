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

#include "common/cvar.h"
#include "common/error.h"

#define MAX_DLIGHTS     64
#define MAX_ENTITIES    2048
#define MAX_PARTICLES   8192
#define MAX_LIGHTSTYLES 256

#define POWERSUIT_SCALE     4.0f
#define WEAPONSHELL_SCALE   0.5f

#define RF_TRACKER          BIT_ULL(32)

#define RF_SHELL_MASK       (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | \
                             RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM | RF_SHELL_LITE_GREEN)

#define DLIGHT_CUTOFF       64

typedef struct entity_s {
    qhandle_t           model;          // opaque type outside renderer
    vec3_t              angles;

    /*
    ** most recent data
    */
    vec3_t              origin;     // also used as RF_BEAM's "from"
    unsigned            frame;      // also used as RF_BEAM's diameter

    /*
    ** previous data for lerping
    */
    vec3_t              oldorigin;  // also used as RF_BEAM's "to"
    unsigned            oldframe;

    /*
    ** misc
    */
    float   backlerp;               // 0.0 = current, 1.0 = old
    int     skinnum;                // also used as RF_BEAM's palette index,
                                    // -1 => use rgba

    float   alpha;                  // ignore if RF_TRANSLUCENT isn't set
    color_t rgba;

    uint64_t    flags;

    qhandle_t   skin;           // NULL for inline skin
    vec3_t      scale;
    float       bottom_z; // for shadows

    struct entity_s *next;
} entity_t;

typedef struct {
    vec3_t  origin;
    vec3_t  transformed;
    vec3_t  color;
    float   radius;
    float   intensity; // note: this is different than Q2PRO and is a light
                       // intensity scalar, not a radius!
    vec4_t  cone;
    vec2_t  fade;

    // for culling, calculated at add time
    vec4_t  sphere;
    float   conecos;
} dlight_t;

typedef struct {
    vec3_t  origin;
    int     color;              // -1 => use rgba
    float   scale;
    float   alpha;
    color_t rgba;
} particle_t;

typedef struct {
    float   white;              // highest of RGB
} lightstyle_t;

typedef struct {
    int         x, y, width, height;// in virtual screen coordinates
    float       fov_x, fov_y;
    vec3_t      vieworg;
    vec3_t      viewangles;
    vec4_t      screen_blend;       // rgba 0-1 full screen blend
    vec4_t      damage_blend;       // rgba 0-1 damage blend
    player_fog_t        fog;
    player_heightfog_t  heightfog;
    float       frametime;          // seconds since last video frame
    float       time;               // time is used to auto animate
    int         rdflags;            // RDF_UNDERWATER, etc
    bool        extended;

    byte        *areabits;          // if not NULL, only areas with set bits will be drawn

    lightstyle_t    *lightstyles;   // [MAX_LIGHTSTYLES]

    int         num_entities;
    entity_t    *entities;

    int         num_dlights;
    dlight_t    *dlights;

    int         num_particles;
    particle_t  *particles;
} refdef_t;

enum {
    QGL_PROFILE_NONE,
    QGL_PROFILE_CORE,
    QGL_PROFILE_ES,
};

typedef struct {
    uint8_t     colorbits;
    uint8_t     depthbits;
    uint8_t     stencilbits;
    uint8_t     multisamples;
    bool        debug;
    uint8_t     profile;
    uint8_t     major_ver;
    uint8_t     minor_ver;
} r_opengl_config_t;

typedef enum {
    QVF_FULLSCREEN      = BIT(0),
    QVF_GAMMARAMP       = BIT(1),
    QVF_VIDEOSYNC       = BIT(2),
} vidFlags_t;

typedef struct {
    int         width;
    int         height;
    vidFlags_t  flags;
} refcfg_t;

extern refcfg_t r_config;

typedef struct {
    int left, right, top, bottom;
} clipRect_t;

typedef enum {
    IF_NONE             = 0,
    IF_PERMANENT        = BIT(0),   // not freed by R_EndRegistration()
    IF_TRANSPARENT      = BIT(1),   // known to be transparent
    IF_PALETTED         = BIT(2),   // loaded from 8-bit paletted format
    IF_UPSCALED         = BIT(3),   // upscaled
    IF_SCRAP            = BIT(4),   // put in scrap texture
    IF_TURBULENT        = BIT(5),   // turbulent surface (don't desaturate, etc)
    IF_REPEAT           = BIT(6),   // tiling image
    IF_NEAREST          = BIT(7),   // don't bilerp
    IF_OPAQUE           = BIT(8),   // known to be opaque
    IF_DEFAULT_FLARE    = BIT(9),   // default flare hack
    IF_CUBEMAP          = BIT(10),  // cubemap (or part of it)
    IF_CLASSIC_SKY      = BIT(11),  // split in two halves
    IF_SPECIAL          = BIT(12),  // 1x1 pixel pure white image

    // these flags only affect R_RegisterImage() behavior,
    // and are not stored in image
    IF_OPTIONAL         = BIT(16),  // don't warn if not found
    IF_KEEP_EXTENSION   = BIT(17),  // don't override extension
} imageflags_t;

typedef enum {
    IT_PIC,
    IT_FONT,
    IT_SKIN,
    IT_SPRITE,
    IT_WALL,
    IT_SKY,

    IT_MAX
} imagetype_t;

// called when the library is loaded
bool    R_Init(bool total);

// called before the library is unloaded
void    R_Shutdown(bool total);

// All data that will be used in a level should be
// registered before rendering any frames to prevent disk hits,
// but they can still be registered at a later time
// if necessary.
//
// EndRegistration will free any remaining data that wasn't registered.
// Any model_s or skin_s pointers from before the BeginRegistration
// are no longer valid after EndRegistration.
//
// Skins and images need to be differentiated, because skins
// are flood filled to eliminate mip map edge errors, and pics have
// an implicit "pics/" prepended to the name. (a pic name that starts with a
// slash will not use the "pics/" prefix or the ".pcx" postfix)
void    R_BeginRegistration(const char *map);
qhandle_t R_RegisterModel(const char *name);
qhandle_t R_RegisterImage(const char *name, imagetype_t type,
                          imageflags_t flags);
void    R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis);
void    R_EndRegistration(void);

#define R_RegisterPic(name)     R_RegisterImage(name, IT_PIC, IF_PERMANENT)
#define R_RegisterTempPic(name) R_RegisterImage(name, IT_PIC, IF_NONE)
#define R_RegisterFont(name)    R_RegisterImage(name, IT_FONT, IF_PERMANENT)
#define R_RegisterSkin(name)    R_RegisterImage(name, IT_SKIN, IF_NONE)
#define R_RegisterSprite(name)  R_RegisterImage(name, IT_SPRITE, IF_NONE)

void    R_RenderFrame(const refdef_t *fd);
void    R_LightPoint(const vec3_t origin, vec3_t light);

void    R_SetClipRect(const clipRect_t *clip);
float   R_ClampScale(cvar_t *var);
void    R_SetScale(float scale);
void    R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font);
void    R_DrawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font);
int     R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                            const char *string, color_t color, qhandle_t font);  // returns advanced x coord
static inline int R_DrawString(int x, int y, int flags, size_t maxChars,
                               const char *string, color_t color, qhandle_t font)
{
    return R_DrawStringStretch(x, y, 1, flags, maxChars, string, color, font);
}


// kfont stuff
typedef struct {
    uint16_t    x, y, w, h;
} kfont_char_t;

#define KFONT_ASCII_MIN         32
#define KFONT_ASCII_MAX         126

typedef struct {
    qhandle_t       pic;
    kfont_char_t    chars[KFONT_ASCII_MAX - KFONT_ASCII_MIN + 1];
    uint16_t        line_height;
    float           sw, sh;
} kfont_t;

const kfont_char_t *SCR_KFontLookup(const kfont_t *kfont, uint32_t codepoint);
void    SCR_LoadKFont(kfont_t *font, const char *filename);
int     R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont);

bool    R_GetPicSize(int *w, int *h, qhandle_t pic);   // returns transparency bit
void    R_DrawPic(int x, int y, color_t color, qhandle_t pic);
void    R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
void    R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle, int pivot_x, int pivot_y, qhandle_t pic);
void    R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
void    R_DrawStretchRaw(int x, int y, int w, int h);
void    R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic);
void    R_TileClear(int x, int y, int w, int h, qhandle_t pic);
void    R_DrawFill8(int x, int y, int w, int h, int c);
void    R_DrawFill32(int x, int y, int w, int h, color_t color);

// video mode and renderer state management entry points
void    R_BeginFrame(void);
void    R_EndFrame(void);
void    R_ModeChanged(int width, int height, int flags);
bool    R_VideoSync(void);

void    GL_ExpireDebugObjects(void);
bool    R_SupportsPerPixelLighting(void);

r_opengl_config_t R_GetGLConfig(void);

typedef struct renderer_export_s {
    bool (*Init)(bool total);
    void (*Shutdown)(bool total);

    void (*BeginRegistration)(const char *map);
    qhandle_t (*RegisterModel)(const char *name);
    qhandle_t (*RegisterImage)(const char *name, imagetype_t type, imageflags_t flags);
    void (*SetSky)(const char *name, float rotate, bool autorotate, const vec3_t axis);
    void (*EndRegistration)(void);

    void (*RenderFrame)(const refdef_t *fd);
    void (*LightPoint)(const vec3_t origin, vec3_t light);

    void (*SetClipRect)(const clipRect_t *clip);
    float (*ClampScale)(cvar_t *var);
    void (*SetScale)(float scale);
    void (*DrawChar)(int x, int y, int flags, int ch, color_t color, qhandle_t font);
    void (*DrawStretchChar)(int x, int y, int w, int h, int flags, int ch,
                            color_t color, qhandle_t font);
    int (*DrawStringStretch)(int x, int y, int scale, int flags, size_t maxChars,
                             const char *string, color_t color, qhandle_t font);

    const kfont_char_t *(*KFontLookup)(const kfont_t *kfont, uint32_t codepoint);
    void (*LoadKFont)(kfont_t *font, const char *filename);
    int (*DrawKFontChar)(int x, int y, int scale, int flags, uint32_t codepoint,
                         color_t color, const kfont_t *kfont);

    bool (*GetPicSize)(int *w, int *h, qhandle_t pic);
    void (*DrawPic)(int x, int y, color_t color, qhandle_t pic);
    void (*DrawStretchPic)(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void (*DrawStretchRotatePic)(int x, int y, int w, int h, color_t color, float angle,
                                 int pivot_x, int pivot_y, qhandle_t pic);
    void (*DrawKeepAspectPic)(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void (*DrawStretchRaw)(int x, int y, int w, int h);
    void (*UpdateRawPic)(int pic_w, int pic_h, const uint32_t *pic);
    void (*TileClear)(int x, int y, int w, int h, qhandle_t pic);
    void (*DrawFill8)(int x, int y, int w, int h, int c);
    void (*DrawFill32)(int x, int y, int w, int h, color_t color);

    void (*BeginFrame)(void);
    void (*EndFrame)(void);
    void (*ModeChanged)(int width, int height, int flags);
    bool (*VideoSync)(void);

    void (*ExpireDebugObjects)(void);
    bool (*SupportsPerPixelLighting)(void);
    r_opengl_config_t (*GetGLConfig)(void);

    refcfg_t *Config;
} renderer_export_t;

#if USE_EXTERNAL_RENDERERS && !defined(RENDERER_DLL)
extern renderer_export_t re;

#define R_Init re.Init
#define R_Shutdown re.Shutdown
#define R_BeginRegistration re.BeginRegistration
#define R_RegisterModel re.RegisterModel
#define R_RegisterImage re.RegisterImage
#define R_SetSky re.SetSky
#define R_EndRegistration re.EndRegistration

#define R_RenderFrame re.RenderFrame
#define R_LightPoint re.LightPoint

#define R_SetClipRect re.SetClipRect
#define R_ClampScale re.ClampScale
#define R_SetScale re.SetScale
#define R_DrawChar re.DrawChar
#define R_DrawStretchChar re.DrawStretchChar
#define R_DrawStringStretch re.DrawStringStretch

#define SCR_KFontLookup re.KFontLookup
#define SCR_LoadKFont re.LoadKFont
#define R_DrawKFontChar re.DrawKFontChar

#define R_GetPicSize re.GetPicSize
#define R_DrawPic re.DrawPic
#define R_DrawStretchPic re.DrawStretchPic
#define R_DrawStretchRotatePic re.DrawStretchRotatePic
#define R_DrawKeepAspectPic re.DrawKeepAspectPic
#define R_DrawStretchRaw re.DrawStretchRaw
#define R_UpdateRawPic re.UpdateRawPic
#define R_TileClear re.TileClear
#define R_DrawFill8 re.DrawFill8
#define R_DrawFill32 re.DrawFill32

#define R_BeginFrame re.BeginFrame
#define R_EndFrame re.EndFrame
#define R_ModeChanged re.ModeChanged
#define R_VideoSync re.VideoSync

#define GL_ExpireDebugObjects re.ExpireDebugObjects
#define R_SupportsPerPixelLighting re.SupportsPerPixelLighting
#define R_GetGLConfig re.GetGLConfig

#define r_config (*re.Config)
#endif
