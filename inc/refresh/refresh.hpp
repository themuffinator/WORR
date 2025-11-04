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

#include <type_traits>
#include <algorithm>

#include "common/cvar.hpp"
#include "common/error.hpp"
#include "shared/game.hpp"

// Some client UI constants are required by inline helpers in this header. They
// are normally provided by client headers, but a few refresh translation units
// include this header without pulling those in. Provide compatible fallbacks so
// the renderer can be built in isolation.
#ifndef CONCHAR_WIDTH
#define CONCHAR_WIDTH 8
#endif

#ifndef CONCHAR_HEIGHT
#define CONCHAR_HEIGHT 8
#endif

#ifndef UI_MULTILINE
#define UI_MULTILINE (1u << 9)
#endif

#if USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
struct ref_freetype_font_t {
    FT_Face face { nullptr };
    int     pixelHeight { 0 };
    void   *driverData { nullptr };
    int     ascent { 0 };
    int     descent { 0 };
    int     lineHeight { 0 };
};
using ftfont_t = ref_freetype_font_t;
#else
struct ref_freetype_font_t;
struct ftfont_t;
#endif

#define MAX_DLIGHTS     64
#define MAX_ENTITIES    2048
#define MAX_PARTICLES   8192
#define MAX_LIGHTSTYLES 256
#define MAX_SHADOW_VIEWS 256

#define POWERSUIT_SCALE     4.0f
#define WEAPONSHELL_SCALE   0.5f

#define RF_TRACKER          BIT_ULL(32)

#define RF_SHELL_MASK       (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | \
                             RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM | RF_SHELL_LITE_GREEN)

#define DLIGHT_CUTOFF       64

typedef struct entity_s {
    qhandle_t           model;          // opaque type outside refresh
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
    vec3_t                  origin;
    vec3_t                  direction;
    color_t                 color;
    shadow_light_type_t     lighttype;
    float                   radius;
    int                     resolution;
    float                   intensity;
    float                   fade_start;
    float                   fade_end;
    int                     lightstyle;
    float                   coneangle;
    float                   bias;
    int                     entity_number;
    bool                    casts_shadow;
} shadow_light_submission_t;

struct shadow_view_parameters_t {
    mat4_t view_projection;
    vec4_t viewport_rect;
    vec4_t source_position;
    float  bias;
    float  shade_amount;
};

static_assert(sizeof(shadow_view_parameters_t) == 104,
    "shadow_view_parameters_t must remain 104 bytes (compatible with quakeUBShadowStruct_s)");

struct shadow_view_assignment_t {
    shadow_view_parameters_t parameters;
    vec4_t cube_face_offset;
    int    atlas_index;
    int    face;
    int    resolution;
    bool   valid;
};

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

    bool        depth_of_field;
    float       dof_blur_range;
    float       dof_focus_distance;
    float       dof_focus_range;
    float       dof_luma_strength;
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

constexpr vidFlags_t operator|(vidFlags_t lhs, vidFlags_t rhs) noexcept
{
    return static_cast<vidFlags_t>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr vidFlags_t operator&(vidFlags_t lhs, vidFlags_t rhs) noexcept
{
    return static_cast<vidFlags_t>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr vidFlags_t operator~(vidFlags_t value) noexcept
{
    return static_cast<vidFlags_t>(~static_cast<int>(value));
}

constexpr vidFlags_t &operator|=(vidFlags_t &lhs, vidFlags_t rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr vidFlags_t &operator&=(vidFlags_t &lhs, vidFlags_t rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

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

constexpr imageflags_t operator|(imageflags_t lhs, imageflags_t rhs) noexcept
{
    using U = std::underlying_type_t<imageflags_t>;
    return static_cast<imageflags_t>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr imageflags_t operator&(imageflags_t lhs, imageflags_t rhs) noexcept
{
    using U = std::underlying_type_t<imageflags_t>;
    return static_cast<imageflags_t>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr imageflags_t operator^(imageflags_t lhs, imageflags_t rhs) noexcept
{
    using U = std::underlying_type_t<imageflags_t>;
    return static_cast<imageflags_t>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

constexpr imageflags_t operator~(imageflags_t value) noexcept
{
    using U = std::underlying_type_t<imageflags_t>;
    return static_cast<imageflags_t>(~static_cast<U>(value));
}

constexpr imageflags_t &operator|=(imageflags_t &lhs, imageflags_t rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr imageflags_t &operator&=(imageflags_t &lhs, imageflags_t rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

constexpr imageflags_t &operator^=(imageflags_t &lhs, imageflags_t rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

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

void    R_ClearShadowLights(void);
void    R_QueueShadowLight(const shadow_light_submission_t &light);
size_t  R_CollectShadowLights(const vec3_t vieworg, const lightstyle_t *styles,
                              dlight_t *dlights, size_t max_dlights);
const shadow_light_submission_t *R_GetQueuedShadowLights(size_t *count);
bool    R_ShadowAtlasInit(void);
void    R_ShadowAtlasShutdown(void);
void    R_ShadowAtlasBeginFrame(void);
bool    R_ShadowAtlasAllocateView(const shadow_view_parameters_t &params,
                                  int face, int resolution,
                                  shadow_view_assignment_t *out_assignment);
size_t  R_ShadowAtlasViewCount(void);
const shadow_view_assignment_t *R_ShadowAtlasViews(void);

void    R_SetClipRect(const clipRect_t *clip);
float   R_ClampScale(cvar_t *var);
void    R_SetScale(float scale);
int     get_auto_scale(void);
void    R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font);
void    R_DrawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font);
int     R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                            const char *string, color_t color, qhandle_t font,
                            const struct ftfont_t *ftFont = nullptr);  // returns advanced x coord
static inline int R_DrawString(int x, int y, int flags, size_t maxChars,
                               const char *string, color_t color, qhandle_t font,
                               const struct ftfont_t *ftFont = nullptr)
{
    return R_DrawStringStretch(x, y, 1, flags, maxChars, string, color, font, ftFont);
}

#if USE_FREETYPE
bool    R_AcquireFreeTypeFont(qhandle_t font, struct ftfont_t *outFont);
void    R_ReleaseFreeTypeFont(struct ftfont_t *font);
int     R_DrawFreeTypeString(int x, int y, int scale, int flags, size_t maxChars,
                             const char *string, color_t color, qhandle_t font,
                             const struct ftfont_t *ftFont = nullptr);
int     R_MeasureFreeTypeString(int scale, int flags, size_t maxChars,
                                const char *string, qhandle_t font,
                                const struct ftfont_t *ftFont = nullptr);
float   R_FreeTypeFontLineHeight(int scale, const struct ftfont_t *ftFont = nullptr);
#else
inline bool R_AcquireFreeTypeFont(qhandle_t, struct ftfont_t *)
{
    return false;
}

inline void R_ReleaseFreeTypeFont(struct ftfont_t *)
{
}

inline int R_DrawFreeTypeString(int x, int y, int scale, int flags, size_t maxChars,
                                const char *string, color_t color, qhandle_t font,
                                const struct ftfont_t *ftFont = nullptr)
{
    return R_DrawStringStretch(x, y, scale, flags, maxChars, string, color, font, ftFont);
}

inline int R_MeasureFreeTypeString(int scale, int flags, size_t maxChars,
                                   const char *string, qhandle_t, const struct ftfont_t * = nullptr)
{
    int width = 0;
    int maxWidth = 0;

    if (!string)
        return 0;

    while (maxChars-- && *string) {
        char ch = *string++;

        if ((flags & UI_MULTILINE) && ch == '\n') {
            maxWidth = (std::max)(maxWidth, width);
            width = 0;
            continue;
        }

        width += CONCHAR_WIDTH * scale;
    }

    return (std::max)(maxWidth, width);
}

inline float R_FreeTypeFontLineHeight(int scale, const struct ftfont_t * = nullptr)
{
    return CONCHAR_HEIGHT * (std::max)(scale, 1);
}
#endif


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

// video mode and refresh state management entry points
void    R_BeginFrame(void);
void    R_EndFrame(void);
void    R_ModeChanged(int width, int height, int flags);
bool    R_VideoSync(void);

void    GL_ExpireDebugObjects(void);
bool    R_SupportsPerPixelLighting(void);

r_opengl_config_t R_GetGLConfig(void);
