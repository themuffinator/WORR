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

#include "gl.hpp"
#include "font_freetype.hpp"
#include <array>

#if USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>
#endif

drawStatic_t draw;

#if USE_FREETYPE

namespace {

constexpr int FT_ATLAS_SIZE = 512;
constexpr int FT_GLYPH_PADDING = 1;
constexpr int FT_BASE_PIXEL_HEIGHT = CONCHAR_HEIGHT * 4;

struct FtAtlas {
    GLuint texnum = 0;
    int width = FT_ATLAS_SIZE;
    int height = FT_ATLAS_SIZE;
    int pen_x = 0;
    int pen_y = 0;
    int row_height = 0;
};

struct FtGlyph {
    int atlas_index = -1;
    float s0 = 0.0f;
    float t0 = 0.0f;
    float s1 = 0.0f;
    float t1 = 0.0f;
    int width = 0;
    int height = 0;
    int bearing_x = 0;
    int bearing_y = 0;
    int advance = 0;
};

struct FtFont {
    image_t *image = nullptr;
    byte *file_buffer = nullptr;
    size_t file_size = 0;
    FT_Face face = nullptr;
    int ascent = 0;
    int descent = 0;
    int line_height = 0;
    int pixel_height = FT_BASE_PIXEL_HEIGHT;
    std::vector<FtAtlas> atlases;
    std::unordered_map<uint32_t, FtGlyph> glyphs;
};

FT_Library ft_library = nullptr;
std::unordered_map<const image_t *, std::unique_ptr<FtFont>> ft_fonts;

static void Ft_DestroyFont(FtFont &font)
{
    for (FtAtlas &atlas : font.atlases) {
        if (atlas.texnum) {
            qglDeleteTextures(1, &atlas.texnum);
            atlas.texnum = 0;
        }
    }

    font.atlases.clear();
    font.glyphs.clear();

    if (font.face) {
        FT_Done_Face(font.face);
        font.face = nullptr;
    }

    if (font.file_buffer) {
        FS_FreeFile(font.file_buffer);
        font.file_buffer = nullptr;
        font.file_size = 0;
    }
}

static FtAtlas &Ft_CreateAtlas(FtFont &font)
{
    FtAtlas atlas{};

    qglGenTextures(1, &atlas.texnum);
    GL_ForceTexture(TMU_TEXTURE, atlas.texnum);

    qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_RED)
    GLenum internal_format = GL_RED;
    GLenum format = GL_RED;
#else
    GLenum internal_format = GL_ALPHA;
    GLenum format = GL_ALPHA;
#endif
    qglTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                  atlas.width, atlas.height, 0,
                  format, GL_UNSIGNED_BYTE, nullptr);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_CLAMP_TO_EDGE
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    font.atlases.push_back(atlas);
    return font.atlases.back();
}

struct FtAtlasPlacement {
    FtAtlas *atlas = nullptr;
    int atlas_index = -1;
    int x = 0;
    int y = 0;
};

static FtAtlasPlacement Ft_AllocateAtlasSpace(FtFont &font, int width, int height)
{
    if (width <= 0 || height <= 0)
        return {};

    for (size_t i = 0; i < font.atlases.size(); ++i) {
        FtAtlas &atlas = font.atlases[i];

        if (width + FT_GLYPH_PADDING > atlas.width || height + FT_GLYPH_PADDING > atlas.height)
            continue;

        if (atlas.pen_x + width + FT_GLYPH_PADDING > atlas.width) {
            atlas.pen_x = 0;
            atlas.pen_y += atlas.row_height;
            atlas.row_height = 0;
        }

        if (atlas.pen_y + height + FT_GLYPH_PADDING > atlas.height)
            continue;

        const int x = atlas.pen_x;
        const int y = atlas.pen_y;

        atlas.pen_x += width + FT_GLYPH_PADDING;
        atlas.row_height = max(atlas.row_height, height + FT_GLYPH_PADDING);

        return { &atlas, static_cast<int>(i), x, y };
    }

    FtAtlas &atlas = Ft_CreateAtlas(font);
    return Ft_AllocateAtlasSpace(font, width, height);
}

static FtGlyph *Ft_EmitGlyph(FtFont &font, uint32_t codepoint)
{
    auto it = font.glyphs.find(codepoint);
    if (it != font.glyphs.end())
        return &it->second;

    if (!font.face)
        return nullptr;

    FT_Error err = FT_Load_Char(font.face, codepoint, FT_LOAD_RENDER);
    if (err)
        return nullptr;

    FT_GlyphSlot slot = font.face->glyph;

    FtGlyph glyph{};
    glyph.advance = slot->advance.x >> 6;
    glyph.bearing_x = slot->bitmap_left;
    glyph.bearing_y = slot->bitmap_top;
    glyph.width = slot->bitmap.width;
    glyph.height = slot->bitmap.rows;

    if (glyph.width > 0 && glyph.height > 0 && slot->bitmap.buffer) {
        FtAtlasPlacement placement = Ft_AllocateAtlasSpace(font, glyph.width, glyph.height);
        if (placement.atlas) {
            glyph.atlas_index = placement.atlas_index;

            const float inv_w = 1.0f / placement.atlas->width;
            const float inv_h = 1.0f / placement.atlas->height;

            glyph.s0 = placement.x * inv_w;
            glyph.t0 = placement.y * inv_h;
            glyph.s1 = (placement.x + glyph.width) * inv_w;
            glyph.t1 = (placement.y + glyph.height) * inv_h;

            GL_BindTexture(TMU_TEXTURE, placement.atlas->texnum);
            qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_RED)
            GLenum format = GL_RED;
#else
            GLenum format = GL_ALPHA;
#endif
            qglTexSubImage2D(GL_TEXTURE_2D, 0, placement.x, placement.y,
                             glyph.width, glyph.height,
                             format, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
            qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }
    }

    auto res = font.glyphs.emplace(codepoint, glyph);
    return &res.first->second;
}

static FtGlyph *Ft_LookupGlyph(FtFont &font, uint32_t codepoint)
{
    FtGlyph *glyph = Ft_EmitGlyph(font, codepoint);
    if (!glyph || (glyph->atlas_index < 0 && glyph->width == 0)) {
        if (codepoint != '?')
            glyph = Ft_EmitGlyph(font, '?');
    }
    return glyph;
}

static FtFont *Ft_FontForImage(const image_t *image)
{
    auto it = ft_fonts.find(image);
    if (it == ft_fonts.end())
        return nullptr;
    return it->second.get();
}

static int Ft_DrawString(FtFont &font, int x, int y, int scale, int flags,
                         size_t maxlen, const char *s, color_t color)
{
    if (!s)
        return x;

    const float target_height = CONCHAR_HEIGHT * max(scale, 1);
    const float scale_factor = target_height / static_cast<float>(font.pixel_height);
    const float line_advance = (font.line_height ? font.line_height : font.pixel_height) * scale_factor;
    const float ascent = (font.ascent ? font.ascent : font.pixel_height) * scale_factor;

    float pen_x = static_cast<float>(x);
    float pen_y = static_cast<float>(y);
    const float start_x = pen_x;

    const bool drop_shadow = (flags & UI_DROPSHADOW) != 0;
    const int shadow_offset = drop_shadow ? max(scale, 1) : 0;

    color_t shadow_color = ColorA(color.a);

    while (maxlen-- && *s) {
        unsigned char ch = static_cast<unsigned char>(*s++);

        if ((flags & UI_MULTILINE) && ch == '\n') {
            pen_y += line_advance + (1.0f / draw.scale);
            pen_x = start_x;
            continue;
        }

        if (ch < 32)
            continue;

        FtGlyph *glyph = Ft_LookupGlyph(font, ch);
        if (!glyph)
            continue;

        float adv = glyph->advance * scale_factor;
        if (glyph->width > 0 && glyph->height > 0 && glyph->atlas_index >= 0) {
            const float xpos = pen_x + glyph->bearing_x * scale_factor;
            const float ypos = pen_y + ascent - glyph->bearing_y * scale_factor;
            const float gw = glyph->width * scale_factor;
            const float gh = glyph->height * scale_factor;

            const FtAtlas &atlas = font.atlases[glyph->atlas_index];

            if (drop_shadow) {
                GL_StretchPic_(xpos + shadow_offset, ypos + shadow_offset, gw, gh,
                               glyph->s0, glyph->t0, glyph->s1, glyph->t1,
                               shadow_color, atlas.texnum, IF_TRANSPARENT);

                if (gl_fontshadow->integer > 1) {
                    GL_StretchPic_(xpos + shadow_offset * 2, ypos + shadow_offset * 2, gw, gh,
                                   glyph->s0, glyph->t0, glyph->s1, glyph->t1,
                                   shadow_color, atlas.texnum, IF_TRANSPARENT);
                }
            }

            GL_StretchPic_(xpos, ypos, gw, gh,
                           glyph->s0, glyph->t0, glyph->s1, glyph->t1,
                           color, atlas.texnum, IF_TRANSPARENT);
        }

        pen_x += adv;
    }

    return static_cast<int>(std::lround(pen_x));
}

} // namespace

bool Draw_LoadFreeTypeFont(image_t *image, const char *filename)
{
    if (!ft_library)
        return false;

    if (!image || !filename)
        return false;

    auto font = std::make_unique<FtFont>();
    font->image = image;

    byte *buffer = nullptr;
    int length = FS_LoadFile(filename, reinterpret_cast<void **>(&buffer));
    if (length < 0) {
        Com_SetLastError(va("Failed to load font '%s'", filename));
        return false;
    }

    font->file_buffer = buffer;
    font->file_size = static_cast<size_t>(length);

    FT_Face face = nullptr;
    FT_Error err = FT_New_Memory_Face(ft_library, reinterpret_cast<const FT_Byte *>(buffer), length, 0, &face);
    if (err) {
        Com_SetLastError("FreeType failed to create font face");
        FS_FreeFile(buffer);
        return false;
    }

    font->face = face;
    font->pixel_height = FT_BASE_PIXEL_HEIGHT;

    err = FT_Set_Pixel_Sizes(face, 0, font->pixel_height);
    if (err) {
        Com_SetLastError("FreeType failed to set pixel size");
        FT_Done_Face(face);
        FS_FreeFile(buffer);
        font->face = nullptr;
        font->file_buffer = nullptr;
        font->file_size = 0;
        return false;
    }

    font->ascent = face->size->metrics.ascender >> 6;
    font->descent = -(face->size->metrics.descender >> 6);
    font->line_height = face->size->metrics.height >> 6;

    if (font->line_height <= 0)
        font->line_height = font->pixel_height;
    if (font->ascent <= 0)
        font->ascent = font->pixel_height - font->descent;

    Draw_FreeFreeTypeFont(image);

    ft_fonts[image] = std::move(font);

    image->flags |= IF_TRANSPARENT;
    image->width = CONCHAR_WIDTH;
    image->height = CONCHAR_HEIGHT;
    image->upload_width = image->width;
    image->upload_height = image->height;
    image->texnum = 0;
    image->texnum2 = 0;
    image->aspect = 1.0f;
    image->sl = 0;
    image->sh = 1;
    image->tl = 0;
    image->th = 1;

    return true;
}

void Draw_FreeFreeTypeFont(image_t *image)
{
    if (!image)
        return;

    auto it = ft_fonts.find(image);
    if (it == ft_fonts.end())
        return;

    Ft_DestroyFont(*it->second);
    ft_fonts.erase(it);
}

void Draw_InitFreeTypeFonts(void)
{
    if (ft_library)
        return;

    if (FT_Init_FreeType(&ft_library)) {
        Com_SetLastError("Failed to initialize FreeType");
        ft_library = nullptr;
    }
}

void Draw_ShutdownFreeTypeFonts(void)
{
    if (!ft_library)
        return;

    for (auto &entry : ft_fonts)
        Ft_DestroyFont(*entry.second);

    ft_fonts.clear();
    FT_Done_FreeType(ft_library);
    ft_library = nullptr;
}

#endif // USE_FREETYPE

// the final process in drawing any pic
static inline void GL_DrawPic(
    vec2_t vertices[4], vec2_t texcoords[4],
    color_t color, int texnum, int flags)
{
    glVertexDesc2D_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 4 > TESS_MAX_VERTICES ||
        tess.numindices + 6 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != texnum))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = texnum;

    dst_vert = ((glVertexDesc2D_t *) tess.vertices) + tess.numverts;

    for (int i = 0; i < 4; i++, dst_vert++) {
        Vector2Copy(vertices[i], dst_vert->xy);
        Vector2Copy(texcoords[i], dst_vert->st);
        dst_vert->c = color.u32;
    }

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    if (flags & IF_TRANSPARENT) {
        if ((flags & IF_PALETTED) && draw.scale == 1)
            tess.flags |= GLS_ALPHATEST_ENABLE;
        else
            tess.flags |= GLS_BLEND_BLEND;
    }

    if (color.a != 255)
        tess.flags |= GLS_BLEND_BLEND;

    tess.numverts += 4;
    tess.numindices += 6;
}

static inline void GL_StretchPic_(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    color_t color, int texnum, int flags)
{
    std::array<vec2_t, 4> vertices{};
    std::array<vec2_t, 4> texcoords{};

    Vector2Set(vertices[0], x,     y    );
    Vector2Set(vertices[1], x + w, y    );
    Vector2Set(vertices[2], x + w, y + h);
    Vector2Set(vertices[3], x,     y + h);

    Vector2Set(texcoords[0], s1, t1);
    Vector2Set(texcoords[1], s2, t1);
    Vector2Set(texcoords[2], s2, t2);
    Vector2Set(texcoords[3], s1, t2);

    GL_DrawPic(vertices.data(), texcoords.data(), color, texnum, flags);
}

#define GL_StretchPic(x,y,w,h,s1,t1,s2,t2,color,image) \
    GL_StretchPic_(x,y,w,h,s1,t1,s2,t2,color,(image)->texnum,(image)->flags)

static inline void GL_StretchRotatePic_(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    float angle, float pivot_x, float pivot_y,
    color_t color, int texnum, int flags)
{
    std::array<vec2_t, 4> vertices{};
    std::array<vec2_t, 4> texcoords{};

    float hw = w / 2.0f;
    float hh = h / 2.0f;

    Vector2Set(vertices[0], -hw + pivot_x, -hh + pivot_y);
    Vector2Set(vertices[1],  hw + pivot_x, -hh + pivot_y);
    Vector2Set(vertices[2],  hw + pivot_x,  hh + pivot_y);
    Vector2Set(vertices[3], -hw + pivot_x,  hh + pivot_y);

    Vector2Set(texcoords[0], s1, t1);
    Vector2Set(texcoords[1], s2, t1);
    Vector2Set(texcoords[2], s2, t2);
    Vector2Set(texcoords[3], s1, t2);

    float s = sinf(angle);
    float c = cosf(angle);

    for (int i = 0; i < 4; i++) {
        float vert_x = vertices[i][0];
        float vert_y = vertices[i][1];
        
        vertices[i][0] = (vert_x * c - vert_y * s) + x;
        vertices[i][1] = (vert_x * s + vert_y * c) + y;
    }

    GL_DrawPic(vertices.data(), texcoords.data(), color, texnum, flags);
}

#define GL_StretchRotatePic(x,y,w,h,s1,t1,s2,t2,angle,px,py,color,image) \
    GL_StretchRotatePic_(x,y,w,h,s1,t1,s2,t2,angle,px,py,color,(image)->texnum,(image)->flags)

static void GL_DrawVignette(float frac, color_t outer, color_t inner)
{
    static const byte indices[24] = {
        0, 5, 4, 0, 1, 5, 1, 6, 5, 1, 2, 6, 6, 2, 3, 6, 3, 7, 0, 7, 3, 0, 4, 7
    };
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 8 > TESS_MAX_VERTICES ||
        tess.numindices + 24 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != TEXNUM_WHITE))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = TEXNUM_WHITE;

    int x = 0, y = 0;
    int w = glr.fd.width, h = glr.fd.height;
    int distance = min(w, h) * frac;

    // outer vertices
    dst_vert = tess.vertices + tess.numverts * 5;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, outer.u32);
    WN32(dst_vert +  9, outer.u32);
    WN32(dst_vert + 14, outer.u32);
    WN32(dst_vert + 19, outer.u32);

    // inner vertices
    x += distance;
    y += distance;
    w -= distance * 2;
    h -= distance * 2;

    dst_vert += 20;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, inner.u32);
    WN32(dst_vert +  9, inner.u32);
    WN32(dst_vert + 14, inner.u32);
    WN32(dst_vert + 19, inner.u32);

    /*
    0             1
        4     5

        7     6
    3             2
    */

    dst_indices = tess.indices + tess.numindices;
    for (int i = 0; i < 24; i++)
        dst_indices[i] = tess.numverts + indices[i];

    tess.flags |= GLS_BLEND_BLEND | GLS_SHADE_SMOOTH;

    tess.numverts += 8;
    tess.numindices += 24;
}

void GL_Blend(void)
{
    if (glr.fd.screen_blend[3]) {
        color_t color;

        color.r = glr.fd.screen_blend[0] * 255;
        color.g = glr.fd.screen_blend[1] * 255;
        color.b = glr.fd.screen_blend[2] * 255;
        color.a = glr.fd.screen_blend[3] * 255;

        GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                       color, TEXNUM_WHITE, 0);
    }

    if (glr.fd.damage_blend[3]) {
        color_t outer, inner;

        outer.r = glr.fd.damage_blend[0] * 255;
        outer.g = glr.fd.damage_blend[1] * 255;
        outer.b = glr.fd.damage_blend[2] * 255;
        outer.a = glr.fd.damage_blend[3] * 255;

        inner = ColorSetAlpha(outer, static_cast<uint8_t>(0));

        if (gl_damageblend_frac->value > 0)
            GL_DrawVignette(Cvar_ClampValue(gl_damageblend_frac, 0, 0.5f), outer, inner);
        else
            GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                           outer, TEXNUM_WHITE, 0);
    }
}

void R_SetClipRect(const clipRect_t *clip)
{
    clipRect_t rc;
    float scale;

    GL_Flush2D();

    if (!clip) {
clear:
        if (draw.scissor) {
            qglDisable(GL_SCISSOR_TEST);
            draw.scissor = false;
        }
        return;
    }

    scale = 1 / draw.scale;

    rc.left = clip->left * scale;
    rc.top = clip->top * scale;
    rc.right = clip->right * scale;
    rc.bottom = clip->bottom * scale;

    if (rc.left < 0)
        rc.left = 0;
    if (rc.top < 0)
        rc.top = 0;
    if (rc.right > r_config.width)
        rc.right = r_config.width;
    if (rc.bottom > r_config.height)
        rc.bottom = r_config.height;
    if (rc.right < rc.left)
        goto clear;
    if (rc.bottom < rc.top)
        goto clear;

    qglEnable(GL_SCISSOR_TEST);
    qglScissor(rc.left, r_config.height - rc.bottom,
               rc.right - rc.left, rc.bottom - rc.top);
    draw.scissor = true;
}

int get_auto_scale(void)
{
    // Define the base vertical resolution the UI was designed for.
    const int scale_base_height = SCREEN_HEIGHT;
    int scale;

    if (r_config.height < r_config.width) { // Landscape mode
        scale = r_config.height / scale_base_height;
    }
    else { // Portrait mode
        // For portrait, use a width that maintains the 4:3 aspect ratio.
        const int scale_base_width = scale_base_height * 4 / 3; // SCREEN_HEIGHT * 4/3 = SCREEN_WIDTH
        scale = r_config.width / scale_base_width;
    }

    // Ensure the scale factor is at least 1.
    if (scale < 1) {
        scale = 1;
    }

    if (vid && vid->get_dpi_scale) {
        int min_scale = vid->get_dpi_scale();
        return max(scale, min_scale);
    }

    return scale;
}

float R_ClampScale(cvar_t *var)
{
    if (!var)
        return 1.0f;

    if (var->value)
        return 1.0f / Cvar_ClampValue(var, 1.0f, 6.0f);

    return 1.0f / get_auto_scale();
}

void R_SetScale(float scale)
{
    if (draw.scale == scale)
        return;

    GL_Flush2D();

    GL_Ortho(0, Q_rint(r_config.width * scale),
             Q_rint(r_config.height * scale), 0, -1, 1);

    draw.scale = scale;
}

void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                  color, image);
}

void R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle, int pivot_x, int pivot_y, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    GL_StretchRotatePic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                        angle, pivot_x, pivot_y, color, image);
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    if (image->flags & IF_SCRAP) {
        R_DrawStretchPic(x, y, w, h, color, pic);
        return;
    }

    float scale_w = w;
    float scale_h = h * image->aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    GL_StretchPic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, color, image);
}

void R_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, image->width, image->height,
                  image->sl, image->tl, image->sh, image->th, color, image);
}

void R_DrawStretchRaw(int x, int y, int w, int h)
{
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, COLOR_WHITE, TEXNUM_RAW, 0);
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_RAW);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic_w, pic_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
}

#define DIV64 (1.0f / 64.0f)

void R_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    GL_StretchPic(x, y, w, h, x * DIV64, y * DIV64,
                  (x + w) * DIV64, (y + h) * DIV64, COLOR_WHITE, IMG_ForHandle(pic));
}

void R_DrawFill8(int x, int y, int w, int h, int c)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, ColorU32(d_8to24table[c & 0xff]), TEXNUM_WHITE, 0);
}

void R_DrawFill32(int x, int y, int w, int h, color_t color)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, color, TEXNUM_WHITE, 0);
}

static inline void draw_char(int x, int y, int w, int h, int flags, int c, color_t color, const image_t *image)
{
    float s, t;

    if ((c & 127) == 32)
        return;

    if (flags & UI_ALTCOLOR)
        c |= 0x80;

    if (flags & UI_XORCOLOR)
        c ^= 0x80;

    s = (c & 15) * 0.0625f;
    t = (c >> 4) * 0.0625f;

    if (flags & UI_DROPSHADOW && c != 0x83) {
        color_t black = ColorA(color.a);

        GL_StretchPic(x + 1, y + 1, w, h, s, t,
                      s + 0.0625f, t + 0.0625f, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + 2, y + 2, w, h, s, t,
                          s + 0.0625f, t + 0.0625f, black, image);
    }

    if (c >> 7)
        color = ColorSetAlpha(COLOR_WHITE, color.a);

    GL_StretchPic(x, y, w, h, s, t,
                  s + 0.0625f, t + 0.0625f, color, image);
}

void R_DrawChar(int x, int y, int flags, int c, color_t color, qhandle_t font)
{
    if (gl_fontshadow->integer > 0)
        flags |= UI_DROPSHADOW;

    draw_char(x, y, CONCHAR_WIDTH, CONCHAR_HEIGHT, flags, c & 255, color, IMG_ForHandle(font));
}

void R_DrawStretchChar(int x, int y, int w, int h, int flags, int c, color_t color, qhandle_t font)
{
    draw_char(x, y, w, h, flags, c & 255, color, IMG_ForHandle(font));
}

int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, qhandle_t font)
{
    const image_t *image = IMG_ForHandle(font);

#if USE_FREETYPE
    if (FtFont *ft_font = Ft_FontForImage(image)) {
        if (gl_fontshadow->integer > 0)
            flags |= UI_DROPSHADOW;
        return Ft_DrawString(*ft_font, x, y, scale, flags, maxlen, s, color);
    }
#endif

    if (gl_fontshadow->integer > 0)
        flags |= UI_DROPSHADOW;

    int sx = x;

    while (maxlen-- && *s) {
        byte c = *s++;

        if ((flags & UI_MULTILINE) && c == '\n') {
            y += CONCHAR_HEIGHT * scale + (1.0 / draw.scale);
            x = sx;
            continue;
        }

        draw_char(x, y, CONCHAR_WIDTH * scale, CONCHAR_HEIGHT * scale, flags, c, color, image);
        x += CONCHAR_WIDTH * scale;
    }

    return x;
}

static inline int draw_kfont_char(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);

    if (!ch)
        return 0;
    
    image_t *image = IMG_ForHandle(kfont->pic);

    float s = ch->x * kfont->sw;
    float t = ch->y * kfont->sh;
    
    float sw = ch->w * kfont->sw;
    float sh = ch->h * kfont->sh;

    int w = ch->w * scale;
    int h = ch->h * scale;

    int shadow_offset = 0;

    if ((flags & UI_DROPSHADOW) || gl_fontshadow->integer > 0) {
        shadow_offset = (1 * scale);
        
        color_t black = ColorA(color.a);

        GL_StretchPic(x + shadow_offset, y + shadow_offset, w, h, s, t,
                      s + sw, t + sh, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + (shadow_offset * 2), y + (shadow_offset * 2), w, h, s, t,
                          s + sw, t + sh, black, image);
    }

    GL_StretchPic(x, y, w, h, s, t,
                  s + sw, t + sh, color, image);

    return ch->w * scale;
}

int R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    return draw_kfont_char(x, y, scale, flags, codepoint, color, kfont);
}

const kfont_char_t *SCR_KFontLookup(const kfont_t *kfont, uint32_t codepoint)
{
    if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX)
        return NULL;

    const kfont_char_t *ch = &kfont->chars[codepoint - KFONT_ASCII_MIN];

    if (!ch->w)
        return NULL;

    return ch;
}

void SCR_LoadKFont(kfont_t *font, const char *filename)
{
    memset(font, 0, sizeof(*font));

    char *buffer;

    if (FS_LoadFile(filename, reinterpret_cast<void **>(&buffer)) < 0)
        return;

    const char *data = buffer;

    while (true) {
        const char *token = COM_Parse(&data);

        if (!*token)
            break;

        if (!strcmp(token, "texture")) {
            token = COM_Parse(&data);
            font->pic = R_RegisterFont(va("/%s", token));
        } else if (!strcmp(token, "unicode")) {
        } else if (!strcmp(token, "mapchar")) {
            token = COM_Parse(&data);

            while (true) {
                token = COM_Parse(&data);

                if (!strcmp(token, "}"))
                    break;

                uint32_t codepoint = strtoul(token, NULL, 10);
                uint32_t x, y, w, h;
                
                x = strtoul(COM_Parse(&data), NULL, 10);
                y = strtoul(COM_Parse(&data), NULL, 10);
                w = strtoul(COM_Parse(&data), NULL, 10);
                h = strtoul(COM_Parse(&data), NULL, 10);
                COM_Parse(&data);

                codepoint -= KFONT_ASCII_MIN;

                if (codepoint < KFONT_ASCII_MAX) {
                    font->chars[codepoint].x = x;
                    font->chars[codepoint].y = y;
                    font->chars[codepoint].w = w;
                    font->chars[codepoint].h = h;

                    font->line_height = max(font->line_height, h);
                }
            }
        }
    }
    
    font->sw = 1.0f / IMG_ForHandle(font->pic)->width;
    font->sh = 1.0f / IMG_ForHandle(font->pic)->height;

    FS_FreeFile(buffer);
}

qhandle_t r_charset;

#if USE_DEBUG

void Draw_Lightmaps(void)
{
    int block = lm.block_size;
    int rows = 0, cols = 0;

    while (block) {
        rows = max(r_config.height / block, 1);
        cols = max(lm.nummaps / rows, 1);
        if (cols * block <= r_config.width)
            break;
        block >>= 1;
    }

    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            int k = j * cols + i;
            if (k < lm.nummaps)
                GL_StretchPic_(block * i, block * j, block, block,
                               0, 0, 1, 1, COLOR_WHITE, lm.texnums[k], 0);
        }
    }
}

void Draw_Scrap(void)
{
    GL_StretchPic_(0, 0, 256, 256,
                   0, 0, 1, 1, COLOR_WHITE, TEXNUM_SCRAP, IF_PALETTED | IF_TRANSPARENT);
}

#endif
